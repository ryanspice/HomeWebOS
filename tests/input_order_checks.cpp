#include "spice/ui.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace spice;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){
  if(argc!=2)throw std::runtime_error("owned test output directory required");
  const auto out=std::filesystem::path(argv[1])/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  std::filesystem::create_directories(out);
  Sdl s;s.SetMainReady();require(s.Init(SDL_INIT_VIDEO)==0,"SDL init");
  s.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  s.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);s.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  auto* window=s.CreateWindow("input order checks",0,0,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);require(window,"window");
  auto context=s.GL_CreateContext(window);require(context,"context");
  {
    Renderer renderer(s,find_font(""),"assets",false,true,false);
    PreparedData prepared;prepared.loaded=true;prepared.source_kind="import";prepared.source_label="Synthetic input-order fixture";
    prepared.apps={{"com.webos.app.hdmi1","HDMI fixture 1","welcome","input"},{"com.webos.app.hdmi2","HDMI fixture 2","welcome","input"}};
    for(const auto* input:{"none","back","blur","right"}){
      const auto config=out/(std::string(input)+".json");
      Preferences p;p.handoff=true;p.dwell=3;
      p.profiles["com.webos.app.hdmi1"]="native_handoff";p.profiles["com.webos.app.hdmi2"]="native_handoff";
      atomic_write(config,p.json().dump());
      Services services(false);
      Deck deck(renderer,services,config,"assets",false,&prepared);
      for(int i=0;i<59;++i)deck.update(.05);
      deck.update(.04); // 2.99 seconds, just before the real minimum 3s threshold.
      require(services.request_counts().get("launch").num()==0,"fixture launched before threshold");
      deck.advance_presentation(.02);
      require(services.request_counts().get("launch").num()==0,"presentation phase performed a handoff");
      const std::string action=input;
      if(action=="blur")deck.active(false);
      else if(action!="none")deck.action(action);
      deck.update(.02,false);
      const auto launches=services.request_counts().get("launch").num();
      if(launches!=(action=="none"?1:0))throw std::runtime_error("Dwell ordering for "+action+": posted "+std::to_string(launches));
      require(deck.finish_settings(),"unexpected save failure");
      std::cout<<"PASS presentation-before-input and dwell-after-"<<input<<std::endl;
    }
    {
      const auto config=out/"writer-success.json";
      atomic_write(config,"{\"unrelated\":{\"keep\":7}}\n");
      Services services(false);Deck deck(renderer,services,config,"assets",false);
      for(int i=0;i<21;++i)deck.action("motion");
      require(deck.finish_settings(),"latest settings failed to drain");
      const auto saved=Json::parse(read_file(config));
      require(Preferences::from_json(saved).reduced_motion,"latest preference missing");
      require(saved.get("unrelated").get("keep").num()==7,"unknown settings data lost");
      const auto status=deck.report().get("settings_persistence");
      require(status.get("state").str()=="saved"&&status.get("sequence").num()==21&&status.get("shutdown_drained").flag(),"final acknowledgment wrong");
      std::cout<<"PASS real Deck rapid writes retain extras and final sequence\n";
    }
    {
      const auto config=out/"invalid-original.json";const std::string original="{invalid-original";atomic_write(config,original);
      Services services(false);Deck deck(renderer,services,config,"assets",false);
      deck.action("motion");require(deck.finish_settings(),"blocked original should not submit a failed job");
      require(read_file(config)==original,"malformed original overwritten");
      require(deck.report().get("settings_persistence").get("state").str()=="blocked-invalid-original","invalid-file gate not reported");
      std::cout<<"PASS real Deck preserves malformed original and blocks submission\n";
    }
    {
      const auto config=out/"replace-failure.json";atomic_write(config,"{}\n");
      Services services(false);Deck deck(renderer,services,config,"assets",false);
      // Only this owned fixture changes after startup; no permissions/global path edits.
      std::filesystem::rename(config,out/"replace-failure-preimage.json");std::filesystem::create_directory(config);
      deck.action("motion");require(!deck.finish_settings(),"failed final write was reported saved");
      const auto status=deck.report().get("settings_persistence");
      require(status.get("state").str()=="failed"&&!status.get("error").str().empty()&&status.get("shutdown_drained").flag(),"write failure missing from final report");
      std::cout<<"PASS real Deck reports final replace failure without false success\n";
    }
    {
      Services services(false);Deck deck(renderer,services,out/"tv-back-home.json","assets",true);
      for(int i=0;i<100;++i)deck.update(.05);
      require(services.request_counts().get("processor_local").num()==0,"TV Home started background processor polling");
      deck.action("library");deck.action("settings");deck.action("back");
      require(deck.view()=="home"&&!deck.quit(),"TV Back failed to return directly Home");
      deck.action("back");require(deck.view()=="home"&&!deck.quit(),"Home Back requested exit or opened a panel");
      deck.action("control");deck.action("home");
      require(deck.view()=="home"&&!deck.quit(),"native Back/Exit Home action failed");
      deck.action("control");deck.update(.3);deck.pointer({1455,900},true);
      require(deck.view()=="home"&&!deck.quit(),"on-screen Control Center Back opened exit confirmation");
      deck.action("processor");deck.update(.05);
      require(deck.view()=="home"&&services.request_counts().get("processor_local").num()==0,"Disabled TV Processor Deck started sampling");
      deck.pointer({1500,400},true);require(deck.report().get("focus").str()!="widget:0","Disabled Home processor remained focusable");
      deck.action("control");deck.update(.3);deck.pointer({550,640},true);
      require(deck.report().get("focus").str()!="cc-card:0","Disabled processor card remained focusable");
      deck.pointer({870,900},true);require(deck.report().get("focus").str()!="cc:3","Disabled processor bar item remained focusable");
      deck.action("back");const auto polls=services.request_counts().get("processor_local").num();
      for(int i=0;i<100;++i)deck.update(.05);
      require(services.request_counts().get("processor_local").num()==polls,"TV Home kept processor polling after return");
      require(deck.finish_settings(),"unexpected TV Back save failure");
      std::cout<<"PASS TV Back returns Home and disabled processor has no sampling or input targets\n";
    }
  }
  s.GL_DeleteContext(context);s.DestroyWindow(window);s.Quit();
  std::cout<<"8 integrated native checks passed; no TV commands executed\n";
}
