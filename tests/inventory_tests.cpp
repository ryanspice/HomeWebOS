#include "spice/inventory.hpp"
#include "spice/services.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace spice;
static int passed=0,failed=0;
static void check(bool yes){if(!yes)throw std::runtime_error("check failed");}
static AppEntry app(std::string id,std::string title){return {std::move(id),std::move(title),"library","app",true,false};}
static void test(const char* title,const std::function<void()>& f){try{f();++passed;std::cout<<"PASS "<<title<<"\n";}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<title<<": "<<e.what()<<"\n";}}
int main(){
  test("prior launcher families are review candidates",[]{check(!launcher_build_reason(app("com.spice.home","Spice Home")).empty());check(!launcher_build_reason(app("com.ryanspice.ps5home.flutter","PS5 Home")).empty());check(!launcher_build_reason(app("com.ryanspice.start","Start")).empty());});
  test("current app, system Home, Homebrew and ordinary apps stay neutral",[]{for(const auto& a:{app("com.spice.home.cpp","Spice Home"),app("com.webos.app.home","Home"),app("org.webosbrew.hbchannel","Homebrew Channel"),app("com.ryanspice.processdeck","Process Deck"),app("com.thirdparty.homeassistant","Home Assistant")})check(launcher_build_reason(a).empty());auto a=app("com.spice.home","Sample");a.sample=true;check(launcher_build_reason(a).empty());});
  test("new IDs compared only against an established matching source",[]{InventoryReview r;auto old=app("com.example.old","Old");r.seed({old},"developer");r.scan({old,app("com.example.new","New")},"developer");check(r.compared()&&r.new_ids().size()==1&&r.is_new("com.example.new"));r.scan({old,app("com.example.new","New")},"developer");check(r.new_ids().size()==1);});
  test("source changes establish a baseline without invented new items",[]{InventoryReview r;r.seed({app("com.example.old","Old")},"developer");r.scan({app("com.example.store","Store")},"private-full");check(!r.compared()&&r.new_ids().empty());r.scan({app("com.example.store","Store"),app("com.example.next","Next")},"private-full");check(r.is_new("com.example.next"));});
  test("removed IDs lose their new badge and candidates reconcile",[]{InventoryReview r;r.seed({},"developer");r.scan({app("com.spice.home","Spice")},"developer");check(r.new_ids().size()==1&&r.candidates().size()==1);r.scan({},"developer");check(r.new_ids().empty()&&r.candidates().empty());});
  test("first unknown scan and sample data do not fabricate discovery",[]{InventoryReview r;r.seed(sample_apps(),"");r.scan({app("com.example.app","App")},"developer");check(!r.compared()&&r.new_ids().empty());r.scan({app("com.example.other","Other")},"unknown");check(!r.compared()&&r.new_ids().empty());});
  test("SDK app list parses metadata without accepting launch parameters",[]{auto j=Json::parse(R"({"apps":[{"id":"com.example.app","title":"App","vendor":"Vendor","version":"2","appDescription":"Description","params":{"command":"ignored"}},{"id":"bad;command","title":"Bad"}]})");auto a=parse_apps(j);check(a.size()==1&&a[0].version=="2"&&a[0].description=="Description"&&a[0].installed);check(luna_access("luna://com.webos.applicationManager/dev/listApps")==Access::Public);auto p=Json::parse(R"({"source_kind":"prepared","apps":[{"id":"com.example.import","title":"Import","inventory_confirmed":false}]})");check(!parse_apps(p)[0].installed);});
  std::cout<<"RESULT "<<passed<<" passed / "<<failed<<" failed\n";return failed?1:0;
}
