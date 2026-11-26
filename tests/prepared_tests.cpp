#include "spice/prepared.hpp"
#include "prepared_fixture.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace spice;
static int passed=0,failed=0;
static void check(bool result){if(!result)throw std::runtime_error("check failed");}
static void rejects(const std::function<void()>& action){try{action();}catch(const std::exception&){return;}throw std::runtime_error("unexpected acceptance");}
static Json metadata(){
  return Json::parse(R"({"schema":"spice-home.prepared.v1","captured_at":"2026-11-05T00:00:00Z","source":{"kind":"import"},"device":{"modelName":"fixture only"},"apps":[{"id":"com.example.fixture","title":"Fixture application","description":"Owner supplied description","vendor":"Example","version":"1.0","observed_at":"2026-11-05T00:00:00Z","inventory_confirmed":false,"provenance":"owner-import","icon_key":"covers/prepared/com.example.fixture","hero_key":""}]})");
}
static std::filesystem::path root(){
  static int serial=0;
  auto folder=std::filesystem::temp_directory_path()/("spice-prepared-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial));
  std::filesystem::create_directories(folder/"snapshots"/"fixture"/"images");
  return folder;
}
static Json fixture(const std::filesystem::path& folder,const Json& data){
  auto bytes=data.dump();auto png=prepared_fixture_png();auto digest=sha256(png);
  atomic_write(folder/"snapshots/fixture/data.json",bytes);
  atomic_write(folder/"snapshots/fixture/images"/(digest+".png"),png);
  auto manifest=Json::parse(R"({"schema":"spice-home.prepared.v1","assets":[]})");
  manifest.set("data_sha256",Json::string(sha256(bytes)));
  auto asset=Json::object();asset.set("key",Json::string("covers/prepared/com.example.fixture"));
  asset.set("sha256",Json::string(digest));asset.set("path",Json::string("images/"+digest+".png"));
  asset.set("bytes",Json::number(static_cast<double>(png.size())));asset.set("width",Json::number(1));asset.set("height",Json::number(1));
  manifest.get("assets").add(asset);
  return manifest;
}
static void publish(const std::filesystem::path& folder,const Json& manifest){
  auto bytes=manifest.dump();atomic_write(folder/"snapshots/fixture/manifest.json",bytes);
  auto pointer=Json::object();pointer.set("schema",Json::string("spice-home.prepared-pointer.v1"));
  pointer.set("manifest",Json::string("snapshots/fixture/manifest.json"));pointer.set("sha256",Json::string(sha256(bytes)));
  atomic_write(folder/"current.json",pointer.dump());
}
static void test(const char* name,const std::function<void()>& action){try{action();++passed;std::cout<<"PASS "<<name<<"\n";}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<"\n";}}
int main(){
  test("SHA256 standard vectors including multi-block padding",[]{
    check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    check(sha256(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  });
  test("valid prepared data, imported evidence semantics and bound PNG",[]{auto r=root();publish(r,fixture(r,metadata()));auto p=load_prepared(r);check(p.loaded&&p.apps.size()==1&&p.images.size()==1&&p.rgba_bytes==4&&p.apps[0].prepared&&!p.apps[0].installed&&p.device.get("modelName").empty());});
  test("metadata tampering rejected before use",[]{auto r=root();publish(r,fixture(r,metadata()));atomic_write(r/"snapshots/fixture/data.json","{}");rejects([&]{load_prepared(r);});});
  test("manifest tampering rejected",[]{auto r=root();publish(r,fixture(r,metadata()));atomic_write(r/"snapshots/fixture/manifest.json","{}");rejects([&]{load_prepared(r);});});
  test("PNG tampering rejected",[]{auto r=root();auto m=fixture(r,metadata());publish(r,m);atomic_write(r/"snapshots/fixture"/m.get("assets").at(0).get("path").str(),"different image");rejects([&]{load_prepared(r);});});
  test("metadata-only scan validates metadata without opening artwork",[]{auto r=root();auto m=fixture(r,metadata());publish(r,m);std::filesystem::remove(r/"snapshots/fixture"/m.get("assets").at(0).get("path").str());auto data=load_prepared(r,false);check(data.loaded&&!data.graphics_validated&&data.apps.size()==1&&data.images.empty()&&data.image_count==1);rejects([&]{load_prepared(r);});atomic_write(r/"snapshots/fixture/data.json","{}");rejects([&]{load_prepared(r,false);});});
  test("path traversal in manifest rejected",[]{auto r=root();auto m=fixture(r,metadata());m.get("assets").at(0).set("path",Json::string("../outside.png"));publish(r,m);rejects([&]{load_prepared(r);});});
  test("advertised dimensions must match PNG header",[]{auto r=root();auto m=fixture(r,metadata());m.get("assets").at(0).set("width",Json::number(256));publish(r,m);rejects([&]{load_prepared(r);});});
  test("import cannot claim confirmed TV inventory",[]{auto r=root();auto d=metadata();d.get("apps").at(0).set("inventory_confirmed",Json::boolean(true));publish(r,fixture(r,d));rejects([&]{load_prepared(r);});});
  test("duplicate app IDs rejected",[]{auto r=root();auto d=metadata();d.get("apps").add(d.get("apps").at(0));publish(r,fixture(r,d));rejects([&]{load_prepared(r);});});
  test("missing artwork reference rejected",[]{auto r=root();auto d=metadata();d.get("apps").at(0).set("hero_key",Json::string("hero/prepared/com.example.fixture"));publish(r,fixture(r,d));rejects([&]{load_prepared(r);});});
  test("unknown source cannot claim a TV snapshot",[]{auto r=root();auto d=metadata();d.get("source").set("kind",Json::string("unknown"));publish(r,fixture(r,d));rejects([&]{load_prepared(r);});});
  test("invalid text and app count fail closed",[]{auto r=root();auto d=metadata();d.get("apps").at(0).set("title",Json::string("bad\ncontrol"));publish(r,fixture(r,d));rejects([&]{load_prepared(r);});d=metadata();for(int i=0;i<200;++i)d.get("apps").add(d.get("apps").at(0));publish(r,fixture(r,d));rejects([&]{load_prepared(r);});});
#ifndef _WIN32
  test("symlink escape rejected before file content read",[]{auto r=root();auto m=fixture(r,metadata());publish(r,m);auto target=r/"snapshots/fixture"/m.get("assets").at(0).get("path").str();std::filesystem::remove(target);std::filesystem::create_symlink("/etc/os-release",target);rejects([&]{load_prepared(r);});});
#endif
  std::cout<<"RESULT "<<passed<<" passed / "<<failed<<" failed\n";
  return failed?1:0;
}
