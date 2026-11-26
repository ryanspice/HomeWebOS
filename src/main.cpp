#include "spice/ui.hpp"
#include "spice/frame_clock.hpp"
#include "spice/window_activity.hpp"
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <map>
#include <csignal>
#include <limits>
namespace spice {
  static volatile std::sig_atomic_t shutdown_requested=0;
  static void request_shutdown(int){shutdown_requested=1;}
  struct Options {
    int width=1920,height=1080,frames=0,fps=120;
    bool width_set=false,height_set=false,resolution_pinned=false;
    double step_ms=0;
    bool fullscreen=false,vsync=true,webos=false,batched=false,legacy_background=false,profile=false;
    std::string font,view,script;
    std::string resolution="max";
    std::filesystem::path assets,config,screenshot,report,prepared,theme;
  };
  static int integer(const std::string& text,int lo,int hi){
    size_t used=0;
    int n=std::stoi(text,&used);
    if(used!=text.size()||n<lo||n>hi)throw std::runtime_error("Invalid numeric option: "+text);
    return n;
  }
  static Options options(int argc,char** argv){
    Options o;
#ifdef SPICE_WEBOS
    o.webos=true;
    o.fullscreen=true;
    o.width=3840;o.height=2160;
    o.vsync=false;o.fps=60;
#endif
    for(int i=1;i<argc;++i){
      std::string a=argv[i];
      auto value=[&](){
        if(i+1>=argc)throw std::runtime_error("Missing value for "+a);
        return std::string(argv[++i]);
      };
      if(i==1&&native_launch_argument(a)){
        // Native deep links choose an existing local page only. All file,
        // command and network options remain outside the launch envelope.
        const auto launch=Json::parse(a,4096);
        if(launch.get("spiceHomeView").is(json_type_string))o.view=launch.get("spiceHomeView").str();
        continue;
      }
      if(a=="--help"){
        std::cout<<"Spice Home C++ 0.1.0\nNative SDL2 + shader renderer. Desktop preview by default.\n\n--assets DIR --font FILE --config FILE\n--width N --height N --fullscreen\n--no-vsync --fps 30..240\n--batch-renderer (experimental, preserves painter order)\n--legacy-background (comparison lighting path)\n--frames N --screenshot FILE --report FILE\n--step-ms N (0 < N <= 50; bounded desktop animation QA, not refresh)\n--view home|control|library|designs|processor|retro|homebrew|inputs|tv|settings|diagnostics|preview\n--script FILE (desktop-only input regression script)\n\nArrows: navigate. Enter: OK. Esc: Back. C: Control Center.\nE: staircase. M: reduced motion. T: palette. F: pin.\nF2: diagnostics. F3: apps. F4: settings. F11: fullscreen.\nLibrary: type to filter; Backspace edits. Controller D-pad/A/B supported.\n";
        std::cout<<"Prepared offline data: --prepared-data DIR\nDefaults: <config-directory>/prepared, then <assets>/prepared.\nMetadata/graphics load at startup; live metadata refresh is explicit.\n";
        std::cout<<"Render resolution: settings key render_resolution = max|2160|1440|1080\nmax asks the display for its largest mode; --width/--height still win.\n";
        std::cout<<"Local native theme: --theme-data DIR\nDefaults: <config-directory>/themes, then <assets>/themes.\nImport/restore with tools/prepare_theme.py, then relaunch to apply.\n";
        std::exit(0);
      }
      else if(a=="--width"){o.width=integer(value(),640,7680);o.width_set=true;}
      else if(a=="--height"){o.height=integer(value(),360,4320);o.height_set=true;}
      else if(a=="--fps")o.fps=integer(value(),30,240);
      else if(a=="--frames")o.frames=integer(value(),1,100000);
      else if(a=="--step-ms"){
        auto text=value();
        size_t used=0;
        o.step_ms=std::stod(text,&used);
        if(used!=text.size()||!std::isfinite(o.step_ms)||o.step_ms<=0||o.step_ms>50)throw std::runtime_error("Invalid animation QA step");
      }
      else if(a=="--assets")o.assets=value();
      else if(a=="--prepared-data")o.prepared=value();
      else if(a=="--theme-data")o.theme=value();
      else if(a=="--font")o.font=value();
      else if(a=="--config")o.config=value();
      else if(a=="--screenshot")o.screenshot=value();
      else if(a=="--report"){o.report=value();o.profile=true;}
      else if(a=="--view")o.view=value();
      else if(a=="--script")o.script=value();
      else if(a=="--fullscreen")o.fullscreen=true;
      else if(a=="--no-vsync")o.vsync=false;
      else if(a=="--vsync")o.vsync=true;
      else if(a=="--batch-renderer")o.batched=true;
      else if(a=="--legacy-background")o.legacy_background=true;
      else if(a=="--demo")o.webos=false;
      else if(a=="--webos"){
#ifdef SPICE_WEBOS
        o.webos=true;
#else
        throw std::runtime_error("This host binary cannot enter webOS mode; cross-build with SPICE_WEBOS=ON");
#endif
      }
      else throw std::runtime_error("Unknown option: "+a);
    }
    if(o.assets.empty()){
      auto exe=std::filesystem::absolute(argv[0]).parent_path();
      for(auto& dir:{
        exe/"assets",exe.parent_path()/"assets",std::filesystem::current_path()/"assets"
      }
      )if(std::filesystem::is_regular_file(dir/"hero/welcome.png")){
        o.assets=dir;
        break;
      }
    }
    if(o.assets.empty())throw std::runtime_error("Artwork not found. Run from project directory or pass --assets PATH");
    if(o.config.empty())o.config=user_data_dir()/"settings.json";
    // Resolution comes from the settings file unless the CLI pinned a size.
    // Only this one field is read here: the window must exist before the deck
    // loads preferences, and a bad or missing file must not block startup.
    if(!o.width_set&&!o.height_set){
      // A preset supplies a coherent 16:9 pair; never mix one CLI-pinned axis
      // with the other axis from the settings file.
      std::string preset="max";
      try{
        if(std::filesystem::is_regular_file(o.config))preset=Preferences::from_json(Json::parse(read_file(o.config))).render_resolution;
      }catch(const std::exception&){}
      o.resolution=preset;
      int w=0,h=0;
      if(resolution_size(preset,w,h)){
        o.width=w;
        o.height=h;
        o.resolution_pinned=true;
      }
    }
    if(o.webos&&o.report.empty())o.report=o.config.parent_path()/"last-run.json";
    if(o.webos&&!o.script.empty())throw std::runtime_error("Automated event scripts are desktop-only; TV actions need live input");
    if(o.step_ms>0&&(o.webos||o.frames==0))throw std::runtime_error("Fixed animation steps require bounded desktop --frames");
    if(o.view=="home")o.view.clear();
    const std::vector<std::string> views{
      "", "control", "library", "designs", "processor", "retro", "homebrew", "inputs", "tv", "settings", "diagnostics", "preview"
    };
    if(std::find(views.begin(),views.end(),o.view)==views.end()){
      // A bad deeplink must not prevent the launcher from starting; relaunch
      // arguments are host-controlled but can be stale after an app update.
      std::cerr<<"Unknown spiceHomeView '"<<o.view<<"'; starting at Home\n";
      o.view.clear();
    }
    return o;
  }
  static std::string key_action(int scan){
    switch(scan){
      case 80:return "left";
      case 79:return "right";
      case 82:return "up";
      case 81:return "down";
      case 40:case 44:return "ok";
      case 41:case 270:return "back";
      case 482:case 505:return "home"; // SDL-webOS short Back / held Back.
      case 6:return "control";
      case 8:return "expand";
      case 16:return "motion";
      case 23:return "theme";
      case 21:return "refresh";
      case 9:return "favorite";
      case 59:return "diagnostics";
      case 60:return "library";
      case 61:return "settings";
      default:return "";
    }
  }
  static int action_scan(const std::string& action){
    static const std::map<std::string,int> scans{
      {
        "left",80
      },
      {
        "right",79
      },
      {
        "up",82
      },
      {
        "down",81
      },
      {
        "ok",40
      },
      {
        "back",41
      },
      {
        "control",6
      },
      {
        "expand",8
      },
      {
        "motion",16
      },
      {
        "theme",23
      },
      {
        "favorite",9
      },
      {
        "diagnostics",59
      },
      {
        "library",60
      },
      {
        "settings",61
      },
      {
        "refresh",21
      }
    };
    auto it=scans.find(action);
    return it==scans.end()?0:it->second;
  }
  int run(int argc,char** argv){
    auto opt=options(argc,argv);
    auto prepared_root=opt.prepared;
    PreparedData prepared;
    if(!opt.prepared.empty())prepared=load_prepared(opt.prepared);
    else for(const auto& path:{opt.config.parent_path()/"prepared",opt.assets/"prepared"}){
      if(!std::filesystem::exists(path/"current.json"))continue;
      prepared_root=path;
      try{prepared=load_prepared(path);}
      catch(...){prepared.error="Prepared snapshot invalid; local shortcuts retained without live metadata requests";}
      break;
    }
    Theme theme;
    if(!opt.theme.empty())theme=load_theme(opt.theme,true);
    else for(const auto& path:{opt.config.parent_path()/"themes",opt.assets/"themes"}){
      std::error_code error;
      const auto status=std::filesystem::symlink_status(path,error);
      if(error==std::errc::no_such_file_or_directory)continue;
      if(!error&&!std::filesystem::exists(status)&&!std::filesystem::is_symlink(status))continue;
      theme=load_theme(path,false);break;
    }
    Sdl sdl;
    sdl.SetMainReady();
    if(opt.webos){
      sdl.SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_BACK","true");
      sdl.SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_EXIT","true");
      sdl.SetHint("SDL_WEBOS_ACCESS_POLICY_RIBBON","false");
      sdl.SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS","0");
    }
    if(sdl.Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)!=0)throw std::runtime_error(std::string("SDL initialization failed: ")+sdl.GetError());
    struct SdlQuit{
      Sdl& s;
      ~SdlQuit(){
        s.Quit();
      }
    }
    quit{
      sdl
    };
    sdl.GL_SetAttribute(SDL_GL_RED_SIZE,8);
    sdl.GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
    sdl.GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
    sdl.GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    sdl.GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    sdl.GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
    sdl.GL_SetAttribute(SDL_GL_STENCIL_SIZE,0);
    SDL_Window* window=nullptr;
    SDL_GLContext context=nullptr;
    // "max" on the desktop means the display's own mode; the C3 path already
    // defaults to its 4K card. SDL must be initialised before asking.
    if(!opt.webos&&!opt.width_set&&!opt.height_set&&!opt.resolution_pinned){
      SDL_DisplayMode desktop{};
      if(sdl.GetDesktopDisplayMode(0,&desktop)==0&&desktop.w>=640&&desktop.h>=360){
        opt.width=desktop.w;
        opt.height=desktop.h;
      }
    }
    std::string context_path="OpenGL ES 2 requested";
    for(int attempt=0;attempt<(opt.webos?1:2);++attempt){
      sdl.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,attempt==0?SDL_GL_CONTEXT_PROFILE_ES:0);
      sdl.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
      sdl.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,attempt==0?0:1);
      // webOS already presents this as a fullscreen card. Desktop-fullscreen
      // would replace our requested 4K buffer with its 1080p UI-plane mode.
      window=sdl.CreateWindow("Spice Home C++ | Native System Deck",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,opt.width,opt.height,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(opt.fullscreen&&!opt.webos?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
      if(window)context=sdl.GL_CreateContext(window);
      if(context){
        if(attempt)context_path="Desktop OpenGL 2.1 compatibility fallback (not GLES hardware proof)";
        break;
      }
      std::cerr<<"Context attempt "<<attempt+1<<": "<<sdl.GetError()<<"\n";
      if(window){
        sdl.DestroyWindow(window);
        window=nullptr;
      }
    }
    if(!window||!context)throw std::runtime_error("No usable GL context; TV build does not silently fall back to a different renderer");
    struct ContextScope{
      Sdl& s;
      SDL_Window* w;
      SDL_GLContext c;
      ~ContextScope(){
        s.GL_DeleteContext(c);
        s.DestroyWindow(w);
      }
    }
    context_scope{
      sdl,window,context
    };
    bool swap_ok=false;
    if(opt.vsync){
      swap_ok=sdl.GL_SetSwapInterval(-1)==0;
      if(!swap_ok)swap_ok=sdl.GL_SetSwapInterval(1)==0;
    }
    else swap_ok=sdl.GL_SetSwapInterval(0)==0;
    int swap=sdl.GL_GetSwapInterval();
    if(!swap_ok)std::cerr<<"Swap interval unavailable: explicit "<<opt.fps<<" fps software ceiling (not panel refresh)\n";
    auto font=find_font(opt.font);
    Renderer renderer(sdl,font,opt.assets,opt.batched,!opt.legacy_background,!prepared.loaded);
    if(prepared.loaded){
      try{renderer.preload_prepared(prepared);}
      catch(...){
        if(!opt.prepared.empty())throw;
        prepared=PreparedData{};
        prepared.error="Prepared graphics unavailable; local shortcuts retained without live metadata requests";
      }
      // Compressed source buffers are no longer needed after the startup upload.
      std::vector<PreparedImage>().swap(prepared.images);
    }
    if(theme.loaded){
      try{renderer.preload_theme(theme);}
      catch(const std::exception& e){
        if(!opt.theme.empty())throw;
        theme=Theme{};theme.error="Theme graphics unavailable; defaults retained: "+clean_text(e.what(),160);
      }
      for(auto& image:theme.images)std::string().swap(image.png);
    }
    Services services(opt.webos,prepared_root);
    Deck deck(renderer,services,opt.config,opt.assets,opt.webos,&prepared,&theme);
    if(!opt.view.empty())deck.action(opt.view);
    Json script=Json::array();
    Json motion_trace=Json::array();
    if(!opt.script.empty()){
      script=Json::parse(read_file(opt.script));
      if(!script.is(json_type_array)||script.size()>1000)throw std::runtime_error("Script must be an array with at most 1000 actions");
    }
    std::vector<SDL_GameController*> controllers;
    auto open_controller=[&](int index){
      if(controllers.size()<8&&sdl.IsGameController(index)){
        auto* c=sdl.GameControllerOpen(index);
        if(c)controllers.push_back(c);
      }
    };
    for(int i=0;i<sdl.NumJoysticks();++i)open_controller(i);
    struct ControllersScope{
      Sdl& s;
      std::vector<SDL_GameController*>& list;
      ~ControllersScope(){
        for(auto* c:list)s.GameControllerClose(c);
      }
    }
    controllers_scope{
      sdl,controllers
    };
    auto get_view=[&](){
      int ww=0,wh=0,dw=0,dh=0;
      sdl.GetWindowSize(window,&ww,&wh);
      sdl.GL_GetDrawableSize(window,&dw,&dh);
      return Viewport::make(ww,wh,dw,dh);
    };
    Viewport viewport=get_view();
    FrameStats stats,render_cpu_stats,swap_stats,capture_stats,event_cpu_stats,update_cpu_stats,loop_cpu_stats,pacing_wait_stats,idle_wait_stats;
    bool previous_present_active=false;
    uint64_t idle_waits=0;
    bool running=true,active=true,text_mode=false;
    WindowActivity activity;
    uint64_t frame=0;
    // FrameStats::summary() allocates and sorts up to 240 samples. The HUD only
    // needs a quarter-second refresh, so do not do that work on every frame.
    FrameSummary cached_summary{};
    uint64_t summary_frame=0;
    const auto clock_origin=std::chrono::steady_clock::now();
    FrameClock frame_clock(clock_origin);
    auto last_present=clock_origin,next_frame=clock_origin;
    double since_start=0,last_axis_time=-1;
    bool axis_neutral=true;
    std::cout<<"Spice Home C++ 0.1.0\n"<<context_path<<"\n"<<renderer.vendor<<" / "<<renderer.renderer<<" / "<<renderer.version<<"\nWindow "<<viewport.window_w<<"x"<<viewport.window_h<<" | drawable "<<viewport.drawable_w<<"x"<<viewport.drawable_h<<" | swap "<<swap<<"\n";
    // Install after native window/lifecycle setup, which may install its own
    // process handlers. Orderly shutdown must drain app-owned persistence.
    const auto old_term=std::signal(SIGTERM,request_shutdown);
    const auto old_int=std::signal(SIGINT,request_shutdown);
    struct SignalScope{
      decltype(old_term) term;decltype(old_int) interrupt;
      ~SignalScope(){std::signal(SIGTERM,term);std::signal(SIGINT,interrupt);}
    } signals{old_term,old_int};
    bool force=true;
    uint64_t script_tick=0;
    while(running&&!deck.quit()&&!shutdown_requested){
      const bool collect_metrics=opt.profile||opt.frames>0||deck.view()=="diagnostics";
      renderer.profiling(collect_metrics);
      auto now=std::chrono::steady_clock::now();
      const auto work_start=now;
      const auto tick=frame_clock.advance(now);
      double dt=opt.step_ms>0?opt.step_ms/1000:tick.animation_seconds;
      since_start=std::chrono::duration<double>(now-clock_origin).count();
      bool trace_frame=false;
      const uint64_t trace_tick=script_tick;
      bool dirty=false;
      double presentation_ms=0;
      auto update_start=std::chrono::steady_clock::now();
      auto update_done=update_start;
      // Advance existing presentation tracks, with no service/dwell side effects.
      // Queued input is dispatched before the one service/dwell update below.
      if(opt.step_ms==0){
        dirty=deck.advance_presentation(dt);
        update_done=std::chrono::steady_clock::now();
        presentation_ms=std::chrono::duration<double,std::milli>(update_done-update_start).count();
      }
      const auto events_start=std::chrono::steady_clock::now();
      bool input_batch=false;
      for(size_t i=0;i<script.size();++i){
        auto item=script.at(i);
        if(static_cast<uint64_t>(item.get("frame").num(-1))!=script_tick)continue;
        auto action=item.get("action").str();
        int scan=action_scan(action);
        if(scan){
          SDL_Event e{};
          e.type=SDL_KEYDOWN;
          e.key.keysym.scancode=static_cast<SDL_Scancode>(scan);
          sdl.PushEvent(&e);
        }
        else if(action=="click"||action=="hover"){
          SDL_Event e{};
          if(action=="click"){
            e.type=SDL_MOUSEBUTTONDOWN;
            e.button.button=1;
            e.button.x=static_cast<int>(item.get("x").num());
            e.button.y=static_cast<int>(item.get("y").num());
          }
          else{
            e.type=SDL_MOUSEMOTION;
            e.motion.x=static_cast<int>(item.get("x").num());
            e.motion.y=static_cast<int>(item.get("y").num());
            e.motion.xrel=1;
          }
          sdl.PushEvent(&e);
        }
        else if(action=="blur"||action=="focus"){
          SDL_Event e{};
          e.type=SDL_WINDOWEVENT;
          e.window.event=action=="blur"?SDL_WINDOWEVENT_FOCUS_LOST:SDL_WINDOWEVENT_FOCUS_GAINED;
          sdl.PushEvent(&e);
        }
        else if(action=="text"){
          SDL_Event e{};
          e.type=SDL_TEXTINPUT;
          auto text=item.get("text").str();
          std::copy_n(text.c_str(),std::min<size_t>(text.size(),31),e.text.text);
          sdl.PushEvent(&e);
        }
        else if(action=="backspace"){
          SDL_Event e{};
          e.type=SDL_KEYDOWN;
          e.key.keysym.scancode=static_cast<SDL_Scancode>(42);
          sdl.PushEvent(&e);
        }
        else if(action=="trace")trace_frame=true;
        else if(action.rfind("open:",0)==0){deck.action(action.substr(5));input_batch=true;}
        else throw std::runtime_error("Unknown scripted action: "+action);
      }
      ++script_tick;
      SDL_Event event{};
      SDL_Event waited_event{};
      bool have_waited_event=false;
      auto resume_presentation=[&](){
        if(active)return;
        active=true;deck.active(true);dt=0;
        const auto resumed=std::chrono::steady_clock::now();
        frame_clock.resume(resumed);last_present=resumed;stats.reset();force=true;
      };
      while(running&&!deck.quit()&&!shutdown_requested&&(have_waited_event||sdl.PollEvent(&event))){
        if(have_waited_event){event=waited_event;have_waited_event=false;}
        input_batch=true;
        const bool navigation_input=event.type==SDL_KEYDOWN||event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEWHEEL||event.type==SDL_CONTROLLERBUTTONDOWN||(event.type==SDL_CONTROLLERAXISMOTION&&std::abs(static_cast<int>(event.caxis.value))>18000);
        if(navigation_input){
          if(activity.recover_input(opt.webos,event.common.timestamp))resume_presentation();
        }
        if(event.type==SDL_QUIT)running=false;
        else if(event.type==SDL_APP_TERMINATING){
          // webOS can terminate without a signal; flush the ordered settings
          // writer before the process goes away.
          deck.finish_settings();running=false;
        }
        else if(event.type==SDL_APP_LOWMEMORY){
          deck.release_caches();force=true;
        }
        else if(event.type==SDL_APP_WILLENTERBACKGROUND||event.type==SDL_APP_DIDENTERBACKGROUND){
          activity.hide(event.common.timestamp);active=false;deck.active(false);
        }
        else if(event.type==SDL_APP_DIDENTERFOREGROUND){activity.show();resume_presentation();force=true;}
        else if(event.type==SDL_WINDOWEVENT){
          auto ev=event.window.event;
          if(ev==SDL_WINDOWEVENT_CLOSE)running=false;
          if(ev==SDL_WINDOWEVENT_HIDDEN||ev==SDL_WINDOWEVENT_MINIMIZED||ev==SDL_WINDOWEVENT_FOCUS_LOST){
            if(ev==SDL_WINDOWEVENT_FOCUS_LOST)activity.blur(event.window.timestamp);
            else activity.hide(event.window.timestamp);
            active=false;
            deck.active(false);
          }
          if(ev==SDL_WINDOWEVENT_SHOWN||ev==SDL_WINDOWEVENT_RESTORED||ev==SDL_WINDOWEVENT_FOCUS_GAINED){
            activity.show();
            bool resuming=!active;
            active=true;
            deck.active(true);
            if(resuming){
              dt=0;
              frame_clock.resume(std::chrono::steady_clock::now());
            }
            last_present=std::chrono::steady_clock::now();
            stats.reset();
            force=true;
          }
          if(ev==SDL_WINDOWEVENT_EXPOSED)force=true;
          if(ev==SDL_WINDOWEVENT_RESIZED||ev==SDL_WINDOWEVENT_SIZE_CHANGED){
            viewport=get_view();
            force=true;
          }
        }
        else if(event.type==SDL_KEYDOWN){
          int scan=static_cast<int>(event.key.keysym.scancode);
          const char* raw=sdl.GetScancodeName(event.key.keysym.scancode);
          std::string name=raw?raw:"Unknown";
          deck.key_diagnostic(name+" / scan "+std::to_string(scan)+" / key "+std::to_string(event.key.keysym.sym));
          if(scan==68&&!event.key.repeat){
            opt.fullscreen=!opt.fullscreen;
            if(sdl.SetWindowFullscreen(window,opt.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0)!=0)std::cerr<<"Fullscreen request rejected\n";
            viewport=get_view();
            force=true;
            continue;
          }
          auto mapped=deck.remote_action(name);
          if(!mapped.empty()){
            deck.action(mapped);
            continue;
          }
          if(scan==42){
            deck.backspace();
            continue;
          }
          if(deck.accepts_text()&&scan>=4&&scan<=56&&scan!=40&&scan!=41)continue;
          auto action=key_action(scan);
          if(!action.empty()&&(!event.key.repeat||(scan>=79&&scan<=82)))deck.action(action);
        }
        else if(event.type==SDL_TEXTINPUT)deck.text_input(event.text.text);
        else if(event.type==SDL_MOUSEMOTION){
          if(event.motion.xrel||event.motion.yrel)if(auto p=viewport.pointer(static_cast<float>(event.motion.x),static_cast<float>(event.motion.y)))deck.pointer(*p,false);
        }
        else if(event.type==SDL_MOUSEBUTTONDOWN){
          if(event.button.button==1){
            if(auto p=viewport.pointer(static_cast<float>(event.button.x),static_cast<float>(event.button.y)))deck.pointer(*p,true);
          }else if(event.button.button==3||event.button.button==4)deck.action(opt.webos?"home":"back");
        }
        else if(event.type==SDL_MOUSEWHEEL){
          int y=event.wheel.direction==1?-event.wheel.y:event.wheel.y;
          int x=event.wheel.direction==1?-event.wheel.x:event.wheel.x;
          // P6: wheel scrolls up/down in grid/list views; horizontal only on the home rail.
          if(y)deck.action(deck.view()=="home"?(y>0?"left":"right"):(y>0?"up":"down"));
          else if(x)deck.action(x>0?"right":"left");
        }
        else if(event.type==SDL_CONTROLLERDEVICEADDED)open_controller(event.cdevice.which);
        else if(event.type==SDL_CONTROLLERDEVICEREMOVED){
          auto id=event.cdevice.which;
          for(auto it=controllers.begin();it!=controllers.end();){
            if(sdl.JoystickInstanceID(sdl.GameControllerGetJoystick(*it))==id){
              sdl.GameControllerClose(*it);
              it=controllers.erase(it);
            }
            else ++it;
          }
        }
        else if(event.type==SDL_CONTROLLERBUTTONDOWN){
          switch(event.cbutton.button){
            case 0:deck.action("ok");
            break;
            case 1:deck.action("back");
            break;
            case 4:deck.action("back");
            break;
            case 5:case 6:deck.action("control");
            break;
            case 11:deck.action("up");
            break;
            case 12:deck.action("down");
            break;
            case 13:deck.action("left");
            break;
            case 14:deck.action("right");
            break;
            default:break;
          }
        }
        else if(event.type==SDL_CONTROLLERAXISMOTION&&event.caxis.axis<2){
          int val=event.caxis.value;
          if(std::abs(val)<8000)axis_neutral=true;
          else if(std::abs(val)>18000&&(axis_neutral||since_start-last_axis_time>.19)){
            deck.action(event.caxis.axis==0?(val<0?"left":"right"):(val<0?"up":"down"));
            last_axis_time=since_start;
            axis_neutral=false;
          }
        }
      }

      if(activity.recover_keyboard_focus(opt.webos,sdl.GetKeyboardFocus()==window))resume_presentation();
      if(!running||deck.quit()||shutdown_requested){
        active=false;deck.active(false);break;
      }
      if(deck.accepts_text()!=text_mode){
        text_mode=deck.accepts_text();
        if(text_mode)sdl.StartTextInput();
        else sdl.StopTextInput();
      }
      const auto events_done=std::chrono::steady_clock::now();
      update_start=events_done;
      // Fixed-step QA keeps event-then-step behavior. Live input starts at age
      // zero; the zero-time presentation update also resolves reduced motion.
      dirty=deck.update(dt,opt.step_ms>0)||dirty;
      update_done=std::chrono::steady_clock::now();
      if(opt.step_ms==0&&input_batch)frame_clock.event_batch_end(update_done);
      if(trace_frame){
        if(motion_trace.size()>=256)throw std::runtime_error("Animation QA trace limit exceeded");
        auto sample=Json::object();
        sample.set("tick",Json::number(static_cast<double>(trace_tick)));
        sample.set("motion",deck.motion_report());
        motion_trace.add(sample);
      }
      bool render=active&&(dirty||force||opt.frames>0);
      if(render){
        force=false;
        viewport=get_view();
        auto render_start=std::chrono::steady_clock::now();
        if(collect_metrics&&(summary_frame++%15==0))cached_summary=stats.summary();
        renderer.begin(viewport);
        deck.draw(viewport,cached_summary,swap);
        renderer.end();
        auto render_done=std::chrono::steady_clock::now();
        if(collect_metrics){
        event_cpu_stats.push(std::chrono::duration<double,std::milli>(events_done-events_start).count());
        update_cpu_stats.push(presentation_ms+std::chrono::duration<double,std::milli>(update_done-update_start).count());
        loop_cpu_stats.push(std::chrono::duration<double,std::milli>(render_done-work_start).count());
        render_cpu_stats.push(std::chrono::duration<double,std::milli>(render_done-render_start).count());
        }
        ++frame;
        bool final=opt.frames>0&&frame>=static_cast<uint64_t>(opt.frames);
        bool captured=final&&!opt.screenshot.empty();
        if(captured){
          auto capture_start=std::chrono::steady_clock::now();
          renderer.screenshot(opt.screenshot);
          capture_stats.push(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-capture_start).count());
        }
        auto swap_start=std::chrono::steady_clock::now();
        sdl.GL_SwapWindow(window);
        auto presented=std::chrono::steady_clock::now();
        if(collect_metrics)swap_stats.push(std::chrono::duration<double,std::milli>(presented-swap_start).count());
        if(collect_metrics&&previous_present_active&&!captured)stats.push(std::chrono::duration<double,std::milli>(presented-last_present).count());
        previous_present_active=!captured;
        last_present=presented;
        if(final)running=false;
        if(swap==0||!swap_ok){
          next_frame=std::max(next_frame+std::chrono::microseconds(1000000/opt.fps),presented);
          auto pacing_start=std::chrono::steady_clock::now();
          const auto wait_ms=std::chrono::duration_cast<std::chrono::milliseconds>(next_frame-pacing_start).count();
          const int timeout=static_cast<int>(std::max<int64_t>(0,std::min<int64_t>(wait_ms,std::numeric_limits<int>::max())));
          if(timeout>0&&sdl.WaitEventTimeout(&waited_event,timeout))have_waited_event=true;
          if(collect_metrics)pacing_wait_stats.push(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-pacing_start).count());
        }
      }
      else{
        previous_present_active=false;
        ++idle_waits;
        auto idle_start=std::chrono::steady_clock::now();
        if(sdl.WaitEventTimeout(&waited_event,active?deck.idle_wait_ms():200))have_waited_event=true;
        if(collect_metrics)idle_wait_stats.push(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-idle_start).count());
        last_present=std::chrono::steady_clock::now();
      }
    }
    const bool settings_saved=deck.finish_settings();
    auto report=deck.report();
    if(!settings_saved)std::cerr<<"Settings save failed on exit: "<<report.get("settings_persistence").get("error").str()<<"\n";
    auto performance=Json::object();
    const auto& rc=renderer.counters();
    performance.set("profiling_enabled",Json::boolean(renderer.profiling()));
    performance.set("render_path",Json::string(renderer.batched()?"batched-experimental":"immediate"));
    performance.set("backdrop_path",Json::string(renderer.composite_background()?"single-pass-composite":"legacy-layered"));
    performance.set("fragment_precision",Json::string(renderer.is_es?(renderer.fragment_highp()?"highp":"mediump-fallback"):"desktop-float"));
    performance.set("logical_primitives_total",Json::number(static_cast<double>(rc.primitives)));
    performance.set("vertex_upload_calls_total",Json::number(static_cast<double>(rc.vertex_upload_calls)));
    performance.set("buffer_orphans_total",Json::number(static_cast<double>(rc.buffer_orphans)));
    performance.set("geometry_capacity_bytes",Json::number(static_cast<double>(renderer.geometry_capacity_bytes())));
    performance.set("home_cache_bytes",Json::number(static_cast<double>(renderer.home_cache_bytes())));
    performance.set("home_cache_hits",Json::number(static_cast<double>(rc.home_cache_hits)));
    performance.set("home_cache_repaints",Json::number(static_cast<double>(rc.home_cache_repaints)));
    performance.set("draw_calls_total",Json::number(static_cast<double>(rc.draws)));
    performance.set("vertex_upload_bytes_total",Json::number(static_cast<double>(rc.vertex_upload_bytes)));
    performance.set("uniform_submit_ms_total",Json::number(rc.uniform_submit_ms));
    performance.set("vertex_upload_ms_total",Json::number(rc.vertex_upload_ms));
    performance.set("draw_submit_ms_total",Json::number(rc.draw_submit_ms));
    performance.set("texture_uploads_total_including_startup",Json::number(static_cast<double>(rc.texture_uploads)));
    performance.set("texture_upload_bytes_total_including_startup",Json::number(static_cast<double>(rc.texture_upload_bytes)));
    performance.set("text_rasterizations",Json::number(static_cast<double>(rc.rasterizations)));
    performance.set("text_cache_hits",Json::number(static_cast<double>(rc.text_hits)));
    performance.set("text_cache_misses",Json::number(static_cast<double>(rc.text_misses)));
    performance.set("text_cache_evictions",Json::number(static_cast<double>(rc.text_evictions)));
    performance.set("metric_cache_hits",Json::number(static_cast<double>(rc.metric_hits)));
    performance.set("metric_cache_misses",Json::number(static_cast<double>(rc.metric_misses)));
    performance.set("metric_cache_entries",Json::number(static_cast<double>(renderer.metric_cache_size())));
    performance.set("ellipsis_cache_hits",Json::number(static_cast<double>(rc.ellipsis_hits)));
    performance.set("ellipsis_cache_misses",Json::number(static_cast<double>(rc.ellipsis_misses)));
    performance.set("ellipsis_cache_entries",Json::number(static_cast<double>(renderer.ellipsis_cache_size())));
    performance.set("raster_scratch_capacity_bytes",Json::number(static_cast<double>(renderer.raster_scratch_bytes())));
    performance.set("idle_waits",Json::number(static_cast<double>(idle_waits)));
    performance.set("rolling_sample_capacity",Json::number(240));
    performance.set("gpu_time_available",Json::boolean(false));
    performance.set("physical_presentation_verified",Json::boolean(false));
    performance.set("requested_fps_ceiling",Json::number(opt.fps));
    performance.set("requested_vsync",Json::boolean(opt.vsync));
    performance.set("swap_request_accepted",Json::boolean(swap_ok));
    auto add_stats=[&](const char* name,const FrameStats& source){
      auto summary=source.summary();
      auto item=Json::object();
      item.set("samples",Json::number(static_cast<double>(summary.samples)));
      item.set("median_ms",Json::number(summary.median_ms));
      item.set("p95_ms",Json::number(summary.p95_ms));
      item.set("p99_ms",Json::number(summary.p99_ms));
      item.set("max_ms",Json::number(summary.max_ms));
      performance.set(name,item);
    };
    add_stats("active_present_interval",stats);
    add_stats("cpu_render_submission",render_cpu_stats);
    add_stats("cpu_events_for_rendered_frames",event_cpu_stats);
    add_stats("cpu_update_for_rendered_frames",update_cpu_stats);
    add_stats("cpu_active_loop_work",loop_cpu_stats);
    add_stats("software_pacing_wait",pacing_wait_stats);
    add_stats("idle_event_wait",idle_wait_stats);
    add_stats("swap_call",swap_stats);
    add_stats("screenshot_capture",capture_stats);
    report.set("performance",performance);
#ifdef SPICE_BUILD_CONFIGURATION
    report.set("build_configuration",Json::string(SPICE_BUILD_CONFIGURATION));
#endif
#ifdef SPICE_SANITIZER_BUILD
    report.set("sanitizer_build",Json::boolean(SPICE_SANITIZER_BUILD!=0));
#endif
#ifdef _MSC_VER
    report.set("compiler",Json::string("MSVC "+std::to_string(_MSC_VER)));
#else
    report.set("compiler",Json::string(__VERSION__));
#endif
    report.set("context_path",Json::string(context_path));
    report.set("capture_frames",Json::number(static_cast<double>(frame)));
    report.set("bounded_capture",Json::boolean(opt.frames>0));
    // Resolution provenance: the preset, whether the CLI pinned the size, and
    // the size actually requested. The drawable is separately reported.
    report.set("render_resolution",Json::string(opt.resolution));
    report.set("render_size_pinned",Json::boolean(opt.width_set||opt.height_set));
    report.set("requested_width",Json::number(opt.width));
    report.set("requested_height",Json::number(opt.height));
    report.set("tv_verified",Json::boolean(false));
    report.set("animation_clock",Json::string(opt.step_ms>0?"fixed-step-desktop-qa":"wall-clock"));
    report.set("animation_step_ms",Json::number(opt.step_ms));
    report.set("input_update_order",Json::string(opt.step_ms>0?"fixed-step events then presentation/services/dwell":"presentation, input, services/dwell"));
    report.set("motion_trace",motion_trace);
    if(!opt.report.empty()){
      try{atomic_write(opt.report,report.dump(true)+"\n");}
      catch(const std::exception&){if(!opt.webos)throw;std::cerr<<"Native diagnostic report could not be saved\n";}
    }
    std::cout<<"EXIT: "<<frame<<" native frames, view="<<deck.view()<<", estimated RGBA cache="<<renderer.texture_bytes()/1048576.<<" MiB\n";
    return settings_saved?0:2;
  }
}
int main(int argc,char** argv){
  try{
    return spice::run(argc,argv);
  }
  catch(const std::exception& e){
    std::cerr<<"Spice Home C++ stopped: "<<e.what()<<"\n";
    return 1;
  }
}
