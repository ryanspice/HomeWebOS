#include "spice/core.hpp"
#include "spice/window_activity.hpp"
#include "spice/services.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <cstring>
#ifndef _WIN32
#include <unistd.h>
#endif
using namespace spice;
static int passed=0, failed=0;
static void check(bool ok,const char* expr,int line){if(!ok)throw std::runtime_error(std::string("line ")+std::to_string(line)+": "+expr);}
#define CHECK(e) check(bool(e),#e,__LINE__)
static bool near(double a,double b,double epsilon=.001){return std::abs(a-b)<epsilon;}
static void rejects(const std::function<void()>& f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
static void test(const char* title,const std::function<void()>& f){try{f();++passed;std::cout<<"PASS "<<title<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<title<<": "<<e.what()<<'\n';}}
static int index(const std::vector<Node>& n,const std::string& id){for(size_t i=0;i<n.size();++i)if(n[i].id==id)return static_cast<int>(i);throw std::runtime_error("Node absent: "+id);}
static bool dwell(PreviewState& s,const Preferences& p,double now=0){for(int i=0;i<62;++i)if(s.tick(.05,now+i*.05,true,false,p))return true;return false;}
int main(int argc,char** argv){
 // Self-hosted subprocess fixtures avoid shell parsing and external test dependencies.
 if(argc>1){std::string mode=argv[1];
  if(mode=="--echo"){if(argc>2)std::cout<<argv[2];return 0;}
  if(mode=="--flood"){std::cout<<std::string(65536,'x');return 0;}
  if(mode=="--sleep"){std::this_thread::sleep_for(std::chrono::seconds(2));return 0;}
#ifndef _WIN32
  if(mode=="--descendant"){auto pid=fork();if(pid==0){std::this_thread::sleep_for(std::chrono::seconds(2));_exit(0);}return 0;}
#endif
  return 2;
 }
 test("1080p identity pointer",[]{auto v=Viewport::make(1920,1080,1920,1080);auto p=v.pointer(500,400);CHECK(p&&near(p->x,500)&&near(p->y,400));});
 test("2x DPI pointer maps once",[]{auto v=Viewport::make(1920,1080,3840,2160);auto p=v.pointer(500,400);CHECK(p&&near(p->x,500)&&near(p->y,400)&&near(v.scale,2));});
 test("4K window pointer",[]{auto v=Viewport::make(3840,2160,3840,2160);auto p=v.pointer(1000,800);CHECK(p&&near(p->x,500)&&near(p->y,400));});
 test("720p pointer",[]{auto p=Viewport::make(1280,720,1280,720).pointer(320,360);CHECK(p&&near(p->x,480)&&near(p->y,540));});
 test("letterbox bars reject pointer",[]{auto v=Viewport::make(1920,1200,1920,1200);CHECK(!v.pointer(100,20));CHECK(near(v.pointer(100,160)->y,100));CHECK(!v.pointer(1920,500));});
 test("zero viewport is finite",[]{auto v=Viewport::make(0,0,0,0);CHECK(v.scale>0&&std::isfinite(v.scale));});
 test("staircase 4-3-2 right alignment",[]{auto n=home_nodes(true);CHECK(n.size()==30);for(auto row:{0,1,2}){int k=row==0?0:row==1?4:7;auto r=n[index(n,"extra:"+std::to_string(k))].rect;CHECK(near(r.x,486+130*row));CHECK(near(r.y,340+144*row));}CHECK(n[index(n,"extra:3")].rect.x==n[index(n,"extra:6")].rect.x);CHECK(n[index(n,"extra:6")].rect.x==n[index(n,"extra:8")].rect.x);});
 test("collapsed layout preserves bottom cards",[]{auto a=home_nodes(true),b=home_nodes(false);CHECK(a.size()-b.size()==9);for(int i=0;i<5;++i){auto id="card:"+std::to_string(i);auto x=a[index(a,id)].rect,y=b[index(b,id)].rect;CHECK(x.x==y.x&&x.y==y.y&&x.w==y.w&&x.h==y.h);}});
 test("D-pad staircase tracks nearest column",[]{auto n=home_nodes(true);CHECK(n[neighbor(n,index(n,"rail:3"),Direction::Down)].id=="extra:0");CHECK(n[neighbor(n,index(n,"extra:0"),Direction::Down)].id=="extra:4");CHECK(n[neighbor(n,index(n,"extra:4"),Direction::Down)].id=="extra:7");CHECK(n[neighbor(n,index(n,"extra:7"),Direction::Up)].id=="extra:5");});
 test("all focus nodes in logical screen",[]{for(auto n:home_nodes(true)){CHECK(n.rect.x>=0&&n.rect.y>=0&&n.rect.x+n.rect.w<=1920&&n.rect.y+n.rect.h<=1080);}});
 test("navigation handles missing focus",[]{CHECK(neighbor({},0,Direction::Left)==-1);CHECK(neighbor(home_nodes(false),-9,Direction::Down)==0);});
 test("60 and 120 Hz animation agree",[]{float a=0,b=0;for(int i=0;i<60;++i)a=approach(a,1,1.f/60);for(int i=0;i<120;++i)b=approach(b,1,1.f/120);CHECK(near(a,b,.00001));CHECK(approach(0,1,-1)==0);CHECK(ease(-1)==0&&ease(2)==1);});
 test("focus has a rendered exact endpoint and then stops",[]{
   FocusMotion m;m.snap({10,20,100,80});m.retarget({410,160,200,120});
   CHECK(m.update(.1,true,false)&&!m.settled());
   CHECK(m.update(.05,true,false)&&m.settled());
   CHECK(m.value().x==410&&m.value().y==160&&m.value().w==200&&m.value().h==120);
   CHECK(!m.update(.05,true,false));
 });
 test("focus retarget preserves current pose and identical target keeps time",[]{
   FocusMotion m;m.snap({0,0,10,10});m.retarget({300,100,30,40});m.update(.07,true,false);
   auto before=m.value();auto progress=m.progress();m.retarget({300,100,30,40});
   CHECK(m.progress()==progress&&m.value().x==before.x);
   m.retarget({-100,0,20,20});CHECK(m.progress()==0&&m.value().x==before.x);
   m.update(FocusMotion::Duration,true,false);CHECK(m.value().x==-100&&m.settled());
 });
 test("focus freezes inactive and reduced motion resolves on next update",[]{
   FocusMotion m;m.snap({0,0,10,10});m.retarget({200,80,50,60});m.update(.02,true,false);
   auto before=m.value();CHECK(!m.update(4,false,false)&&m.value().x==before.x);
   CHECK(m.update(0,false,true)&&m.settled()&&m.value().x==200);
 });
 test("focus matching elapsed time agrees at 60 120 and 240",[]{
   float first=0;
   for(int hz:{60,120,240}){
     FocusMotion m;m.snap({10,20,100,80});m.retarget({1010,220,300,120});
     for(int i=0;i<hz/10;++i)m.update(1./hz,true,false);
     if(hz==60)first=m.value().x;
     CHECK(near(first,m.value().x,.0002));
   }
 });
 test("motion ignores invalid time without poisoning state",[]{
   FocusMotion f;f.snap({1,2,3,4});f.retarget({10,20,30,40});ArtworkMotion a;a.request("eclipse");
   for(double dt:{-1.,0.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
     CHECK(!f.update(dt,true,false)&&f.value().x==1);
     CHECK(!a.update(dt,true,false)&&a.progress()==0);
   }
 });
 test("panel painted bounds and pointer inverse share fractional translation",[]{
   Rect control{1578,134,56,56};
   for(float progress:{0.f,.01f,.25f,.5f,.9f,1.f}){
     auto p=panel_presentation(progress);auto painted=p.bounds(control);
     CHECK(control.contains(p.unmap({painted.x+1,painted.y+1})));
     CHECK(!control.contains(p.unmap({painted.x+1,painted.y+painted.h+.1f})));
     CHECK(near(painted.y-control.y,p.offset_y,.0001));
     CHECK(p.offset_y>=0&&p.offset_y<=12&&p.opacity>=0&&p.opacity<=1);
   }
   auto reduced=panel_presentation(0,true);CHECK(reduced.offset_y==0&&reduced.opacity==1);
 });
 test("artwork coalesces rapid targets without changing displayed blend",[]{
   ArtworkMotion a;a.request("eclipse");a.update(.06,true,false);auto progress=a.progress();
   a.request("aether");a.request("ember");
   CHECK(a.from()=="welcome"&&a.to()=="eclipse"&&a.progress()==progress&&a.pending()=="ember");
   a.update(.04,true,false);CHECK(a.progress()>progress&&a.to()=="eclipse");
 });
 test("artwork carries elapsed time into the one queued transition",[]{
   ArtworkMotion a,b;a.request("eclipse");b.request("eclipse");a.request("ember");b.request("ember");
   a.update(.35,true,false);for(int i=0;i<35;++i)b.update(.01,true,false);
   CHECK(a.from()=="eclipse"&&a.to()=="ember"&&a.pending().empty());
   CHECK(a.from()==b.from()&&a.to()==b.to()&&near(a.progress(),b.progress(),.00001));
   a.update(.3,true,false);CHECK(!a.animating()&&a.from()=="ember"&&a.to()=="ember"&&a.pending().empty());
   CHECK(!a.update(.1,true,false));
 });
 test("artwork can cancel pending target and reduced motion chooses latest",[]{
   ArtworkMotion a;a.request("eclipse");a.request("ember");a.request("eclipse");CHECK(a.pending().empty());
   a.update(.03,true,false);auto progress=a.progress();CHECK(!a.update(2,false,false)&&a.progress()==progress);
   a.request("tidal");CHECK(a.update(0,false,true));
   CHECK(!a.animating()&&a.from()=="tidal"&&a.to()=="tidal"&&a.pending().empty());
 });
 test("frame intervals bounded and quantified",[]{FrameStats s;for(int i=0;i<300;++i)s.push(1000./120);s.push(0);s.push(999);s.push(std::numeric_limits<double>::quiet_NaN());CHECK(s.summary().samples==240);CHECK(near(s.summary().rate,120));CHECK(near(s.summary().max_ms,999));s.reset();s.push(999);CHECK(near(s.summary().median_ms,999));s.reset();CHECK(s.summary().samples==0);});
  test("ambient motion is bounded deterministic and tracks focus",[]{
    AmbientMotion a,b;
    CHECK(a.particles().size()==AmbientMotion::MaxParticles);
    CHECK(a.particles().size()==b.particles().size());
    for(size_t i=0;i<a.particles().size();++i){
      CHECK(a.particles()[i].x==b.particles()[i].x&&a.particles()[i].y==b.particles()[i].y);
      CHECK(a.particles()[i].r>=2&&a.particles()[i].r<=6&&a.particles()[i].alpha>=0&&a.particles()[i].alpha<=1);
    }
    for(int i=0;i<120;++i){
      CHECK(a.advance(1./120,true,false,{1620.f,800.f}));
      b.advance(1./120,true,false,{1620.f,800.f});
    }
    CHECK(near(a.time(),1.)&&a.time()==b.time());
    CHECK(near(a.parallax_x(),b.parallax_x())&&near(a.parallax_y(),b.parallax_y()));
    CHECK(a.parallax_x()>1&&a.parallax_y()>0&&a.parallax_x()<=AmbientMotion::MaxParallax);
    for(size_t i=0;i<a.particles().size();++i)CHECK(a.particles()[i].x==b.particles()[i].x&&a.particles()[i].y==b.particles()[i].y);
    CHECK(a.vignette()>=0&&a.vignette()<=1);
  });
  test("ambient motion freezes for reduced motion opt-out and inactive frames",[]{
    AmbientMotion a;a.advance(.05,true,false,{960.f,540.f});
    float x=a.particles()[0].x;
    double t=a.time();
    CHECK(!a.advance(.05,true,true,{0.f,0.f}));
    CHECK(a.time()==t&&a.particles()[0].x==x&&a.parallax_x()==0&&a.parallax_y()==0);
    CHECK(!a.advance(.05,false,false,{0.f,0.f}));
    CHECK(a.time()==t&&a.particles()[0].x==x);
    AmbientMotion off(false);CHECK(!off.advance(.05,true,false,{0.f,0.f})&&off.time()==0&&off.parallax_x()==0);
    for(double dt:{-1.,0.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
      CHECK(!a.advance(dt,true,false,{960.f,540.f})&&a.time()==t);
  });
  test("focus shine pulse advances once then settles",[]{
    AmbientMotion a;CHECK(a.pulse_progress()==1);
    a.pulse();CHECK(a.pulse_progress()==0);
    CHECK(a.advance(.1,true,false,{960.f,540.f}));
    CHECK(a.pulse_progress()>.1&&a.pulse_progress()<1);
    CHECK(a.advance(1,true,false,{960.f,540.f})&&a.pulse_progress()==1);
  });
  test("ambient particles respawn in bounds and parallax recenters",[]{
    AmbientMotion a;
    for(int i=0;i<3000;++i)a.advance(1./120,true,false,{i%2?1919.f:1.f,i%2?1079.f:1.f});
    for(const auto& p:a.particles()){
      CHECK(p.x>=-8.f&&p.x<=1928.f&&p.y>=480.f&&p.y<=1088.f);
      CHECK(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.alpha));
    }
    CHECK(std::abs(a.parallax_x())<=AmbientMotion::MaxParallax);
    for(int i=0;i<240;++i)a.advance(1./120,true,false,{960.f,540.f});
    CHECK(std::abs(a.parallax_x())<.01f&&std::abs(a.parallax_y())<.01f);
  });
 test("JSON objects roundtrip",[]{auto j=Json::parse("{\"a\":[1,true,\"ok\"],\"b\":null}");CHECK(j.get("a").size()==3);CHECK(j.get("a").at(1).flag());CHECK(Json::parse(j.dump()).get("a").at(2).str()=="ok");CHECK(j.get("b").empty());});
 test("JSON rejects trailing and oversized input",[]{rejects([]{Json::parse("{}{} ");});rejects([]{Json::parse("{\"a\":1}",3);});rejects([]{Json::parse("{\"a\":}");});});
 test("JSON nesting and UTF8 limited",[]{rejects([]{Json::parse(std::string(50,'[')+"0"+std::string(50,']'));});rejects([]{Json::parse(std::string("{\"a\":\"")+char(0xff)+"\"}");});});
 test("JSON type confusion blocked",[]{auto j=Json::parse("{\"bool\":\"true\",\"n\":\"9\"}");CHECK(!j.get("bool").flag());CHECK(j.get("n").num(5)==5);CHECK(j.at(1).empty());rejects([&]{j.add(Json());});});
 test("app IDs cannot become shell commands",[]{CHECK(safe_app_id("com.webos.app.hdmi1"));for(auto* s:{"","a;id","a b","../x","a\n","$HOME","a/b"})CHECK(!safe_app_id(s));CHECK(!safe_app_id(std::string(161,'a')));});
 test("settings category key validation",[]{CHECK(safe_setting("picture.pictureMode"));for(auto* s:{"picture",".mode","picture.","a.b.c","a.$()"})CHECK(!safe_setting(s));});
 test("RetroPie only literal private IPv4",[]{for(auto* s:{"10.0.0.1","172.16.1.2","172.31.255.2","192.168.1.10"})CHECK(private_ipv4(s));for(auto* s:{"127.0.0.1","localhost","8.8.8.8","172.32.0.1","169.254.169.254","10.01.2.3","10.0.0.256","10.0.0.1:80","[::1]","10.0.0.1/path","10.0.0.1\n"})CHECK(!private_ipv4(s));});
 test("bearer token has bounded printable bytes",[]{CHECK(safe_token(std::string(24,'x')));CHECK(!safe_token("short"));CHECK(!safe_token(std::string(257,'x')));CHECK(!safe_token(std::string(24,'x')+"\r\nInjected"));});
 test("UTF8 clipping preserves codepoints",[]{CHECK(clean_text("A\nB\tC")=="ABC");CHECK(clean_text("é☺",3)=="é");auto s=std::string("Aé☺");pop_utf8(s);CHECK(s=="Aé");pop_utf8(s);CHECK(s=="A");pop_utf8(s);pop_utf8(s);CHECK(s.empty());CHECK(clean_text(std::string("X")+char(0xff)+"Y")=="X?Y");});
 test("native launch envelope identity checked",[]{CHECK(native_launch_argument("{\"@system_native_app\":true,\"nid\":\"com.spice.home.cpp\"}"));CHECK(!native_launch_argument("{\"@system_native_app\":true,\"nid\":\"com.spice.home\"}"));CHECK(!native_launch_argument("{\"@system_native_app\":\"true\",\"nid\":\"com.spice.home.cpp\"}"));});
  test("preference defaults conservative",[]{
    auto p=Preferences::from_json(Json::object());
    CHECK(p.expanded&&!p.handoff&&!p.reduced_motion&&p.ambient_motion);
    CHECK(p.dwell==8&&p.theme=="ps5"&&p.ambient_idle_hz==60);
    rejects([]{Preferences::from_json(Json::array());});
  });
  test("render resolution preset defaults to max and validates",[]{
    auto p=Preferences::from_json(Json::object());
    CHECK(p.render_resolution=="max");
    CHECK(Preferences::from_json(Json::parse(R"({"render_resolution":"1080"})")).render_resolution=="1080");
    CHECK(Preferences::from_json(Json::parse(R"({"render_resolution":"4k"})")).render_resolution=="max");
    auto j=p.json();
    CHECK(j.get("render_resolution").str()=="max");
    int w=0,h=0;
    CHECK(!resolution_size("max",w,h));
    CHECK(resolution_size("2160",w,h)&&w==3840&&h==2160);
    CHECK(resolution_size("1440",w,h)&&w==2560&&h==1440);
    CHECK(resolution_size("1080",w,h)&&w==1920&&h==1080);
  });
  test("ambient motion opt-out persists through preference roundtrip",[]{    auto p=Preferences::from_json(Json::parse(R"({"ambient_motion":false})"));CHECK(!p.ambient_motion);
    auto j=p.json();CHECK(!j.get("ambient_motion").flag(true));CHECK(!Preferences::from_json(j).ambient_motion);
    auto slow=Preferences::from_json(Json::parse(R"({"ambient_idle_hz":30})"));CHECK(slow.ambient_idle_hz==30);
    CHECK(Preferences::from_json(Json::parse(R"({"ambient_idle_hz":1})")).ambient_idle_hz==60);
    CHECK(Preferences::from_json(slow.json()).ambient_idle_hz==30);
  });
 test("preferences validate boundaries",[]{auto p=Preferences::from_json(Json::parse(R"({"theme":"bogus","preview_dwell_seconds":999,"retropie_host":"8.8.8.8","retropie_port":1,"retropie_hdmi":8,"processor_app_id":"bad;cmd","favorites":["com.test","com.test","a;id"],"remote_scancodes":{"F5":"ok","F6":"exec"}})"));CHECK(p.theme=="ps5"&&p.dwell==30&&p.retropie_host.empty());CHECK(p.retropie_port==8767&&p.retropie_hdmi==1&&p.processor_app_id.empty());CHECK(p.favorites.size()==1&&p.remote_keys.size()==1);});
 test("preferences preserve unknown fields but drop known tokens",[]{auto p=Preferences::from_json(Json::parse(R"({"unrelated":{"a":1},"retropie_token":"do-not-copy","token":"no","theme":"metro"})"));auto j=p.json();CHECK(j.get("unrelated").get("a").num()==1);CHECK(j.get("retropie_token").empty()&&j.get("token").empty());CHECK(Preferences::from_json(j).theme=="metro");});
 test("inventory removes hidden duplicate and self entries",[]{auto v=parse_apps(Json::parse(R"({"launchPoints":[{"id":"com.a","title":"A"},{"id":"com.a","title":"Again"},{"id":"com.b","visible":false},{"id":"com.spice.home.cpp"},{"id":"a;id"}]})"));CHECK(v.size()==1&&v[0].installed&&!v[0].sample);});
 test("fallbacks are not claimed installed",[]{for(auto a:fallback_apps())CHECK(!a.installed&&!a.sample);for(auto a:sample_apps())CHECK(a.sample&&!a.installed);});
 test("dwell disabled unless user opts in",[]{Preferences p;p.dwell=3;p.profiles["com.webos.app.hdmi1"]="native_handoff";PreviewState s;s.focus("com.webos.app.hdmi1");CHECK(!dwell(s,p));p.handoff=true;CHECK(dwell(s,p));CHECK(s.pending());CHECK(!dwell(s,p));});
 test("dwell never auto-opens arbitrary app",[]{Preferences p;p.handoff=true;p.dwell=3;p.profiles["com.streaming.app"]="native_handoff";PreviewState s;s.focus("com.streaming.app");CHECK(!dwell(s,p));});
 test("covered or unfocused resets countdown",[]{Preferences p;p.handoff=true;p.dwell=3;p.profiles["com.webos.app.hdmi1"]="native_handoff";PreviewState s;s.focus("com.webos.app.hdmi1");for(int i=0;i<30;++i)s.tick(.05,i*.05,true,false,p);CHECK(s.elapsed()>1);s.tick(.05,2,true,true,p);CHECK(s.elapsed()==0);s.tick(.05,2,false,false,p);CHECK(!s.pending());});
 test("Back suppresses automatic reentry",[]{Preferences p;p.handoff=true;p.dwell=3;p.profiles["com.webos.app.hdmi1"]="native_handoff";PreviewState s;s.focus("com.webos.app.hdmi1");s.suppress();CHECK(!dwell(s,p));s.focus("");s.focus("com.webos.app.hdmi1");CHECK(dwell(s,p));});
 test("failure cooldown then manual retry",[]{Preferences p;p.handoff=true;p.dwell=3;p.profiles["com.webos.app.hdmi1"]="native_handoff";PreviewState s;s.focus("com.webos.app.hdmi1");CHECK(dwell(s,p));s.complete("com.webos.app.hdmi1",false,4);CHECK(!dwell(s,p,10));s.retry(14);CHECK(dwell(s,p,14));});
 test("late launch completion cannot clear different request",[]{Preferences p;p.handoff=true;p.dwell=3;p.profiles["com.webos.app.hdmi1"]="native_handoff";PreviewState s;s.focus("com.webos.app.hdmi1");CHECK(dwell(s,p));s.complete("com.webos.app.hdmi2",true,4);CHECK(s.pending());s.complete("com.webos.app.hdmi1",true,4);CHECK(!s.pending());CHECK(!dwell(s,p,5));});
 test("CPU usage is measured delta not boot average",[]{auto a=parse_processor("cpu 100 0 100 800 0 0 0 0","MemTotal: 1000 kB\nMemAvailable: 250 kB","1 2 3");CHECK(a.cpu==-1&&a.memory==75);auto b=parse_processor("cpu 150 0 100 850 0 0 0 0","MemTotal: 1000 kB\nMemAvailable: 500 kB","1 2 3",&a);CHECK(b.cpu==50&&b.memory==50);});
 test("bad CPU memory load samples rejected",[]{rejects([]{parse_processor("wrong 1","","1 2 3");});rejects([]{parse_processor("cpu 1 1 1 1 1 1 1 1","MemTotal: 100\nMemAvailable: 200","1 2 3");});rejects([]{parse_processor("cpu 1 1 1 1 1 1 1 1","MemTotal: 100\nMemAvailable: 20","-1 2 3");});});
 test("Luna allowlist distinguishes private reads",[]{CHECK(luna_access("luna://com.webos.applicationManager/launch")==Access::Public);CHECK(luna_access("luna://com.webos.applicationManager/listLaunchPoints")==Access::PrivateRead);CHECK(luna_access("luna://org.webosbrew.hbchannel.service/exec")==Access::Denied);CHECK(luna_access("luna://com.webos.settingsservice/setSystemSettings")==Access::Denied);});
 test("desktop worker refuses TV commands",[]{Services s(false);CHECK(!s.private_reads());CHECK(s.post({RequestKind::Launch,"com.webos.app.livetv",{}}));std::vector<Response> out;for(int i=0;i<100&&out.empty();++i){std::this_thread::sleep_for(std::chrono::milliseconds(5));out=s.drain();}CHECK(out.size()==1&&!out[0].ok);CHECK(out[0].message.find("Desktop mode")!=std::string::npos);});
 test("atomic preference file bounded read",[]{auto dir=std::filesystem::temp_directory_path()/(std::string("spice-cpp-test-")+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));auto file=dir/"settings.json";atomic_write(file,"{\"a\":1}");CHECK(read_file(file)=="{\"a\":1}");atomic_write(file,"{}");CHECK(read_file(file)=="{}");rejects([&]{read_file(file,1);});std::filesystem::remove_all(dir);});
#ifndef _WIN32
 std::string self=std::filesystem::absolute(argv[0]).string();
 test("subprocess passes arguments literally",[&]{auto r=bounded_process(self,{"--echo","$(touch /no-file); & ' \""},1000,1024);CHECK(r.exit_code==0&&!r.timed_out&&r.output=="$(touch /no-file); & ' \"");});
 test("subprocess response limit enforced",[&]{auto r=bounded_process(self,{"--flood"},1000,256);CHECK(r.too_large&&r.output.size()<=256);});
 test("subprocess deadline enforced",[&]{auto t=std::chrono::steady_clock::now();auto r=bounded_process(self,{"--sleep"},100,1024);CHECK(r.timed_out);CHECK(std::chrono::steady_clock::now()-t<std::chrono::seconds(1));});
 test("descendant-held pipe cannot deadlock",[&]{auto r=bounded_process(self,{"--descendant"},100,1024);CHECK(r.timed_out);});
 test("subprocess missing executable and invalid limits fail",[&]{rejects([&]{bounded_process("/does-not-exist",{},100,10);});rejects([&]{bounded_process(self,{},0,10);});rejects([&]{bounded_process(self,{std::string("x\0y",3)},100,10);});});
#endif
 test("popup focus loss recovers on new delivered TV key",[]{WindowActivity a;a.blur(100);CHECK(!a.active());CHECK(!a.recover_input(true,99));CHECK(a.recover_input(true,101));CHECK(a.active());});
 test("stale hidden input stays paused but new delivered input recovers",[]{WindowActivity a;a.hide(100);CHECK(!a.recover_input(true,99));CHECK(!a.recover_input(true,100));CHECK(!a.recover_keyboard_focus(true,true));CHECK(a.recover_input(true,101));CHECK(a.active());});
 test("desktop blur and unrelated keyboard focus stay paused",[]{WindowActivity a;a.blur(100);CHECK(!a.recover_input(false,101));CHECK(!a.recover_keyboard_focus(true,false));CHECK(a.recover_keyboard_focus(true,true));});
 test("focus recovery handles timestamp rollover",[]{WindowActivity a;a.blur(0xfffffff0u);CHECK(a.recover_input(true,10));a.blur(10);CHECK(!a.recover_input(true,0xfffffff0u));});
 test("TV monitoring is rejected without starting a service worker",[]{Services s(true);CHECK(!s.worker_started());CHECK(!s.post({RequestKind::Processor,"",{}}));CHECK(!s.worker_started());CHECK(s.request_counts().get("processor_local").num()==0);});
 std::cout<<"RESULT "<<passed<<" passed / "<<failed<<" failed\n";return failed?1:0;
}
