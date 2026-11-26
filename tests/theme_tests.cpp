#include "spice/theme.hpp"
#include "spice/prepared.hpp"
#include <png.h>
#include <zlib.h>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace spice;
namespace fs=std::filesystem;
namespace {
  int passed=0,failed=0,skipped=0,serial=0;
  fs::path fixture_root,output_root;
  void check(bool condition,const std::string& why="check failed"){if(!condition)throw std::runtime_error(why);}
  void rejects(const std::function<void()>& action,const std::string& contains={}){
    try{action();}catch(const std::exception& e){check(contains.empty()||std::string(e.what()).find(contains)!=std::string::npos,e.what());return;}
    throw std::runtime_error("unexpected acceptance");
  }
  fs::path copy_fixture(){
    auto target=output_root/("case-"+std::to_string(++serial));
    fs::copy(fixture_root,target,fs::copy_options::recursive);return target;
  }
  Json pointer(const fs::path& root){return Json::parse(read_file(root/"current.json"));}
  fs::path manifest_path(const fs::path& root){return root/pointer(root).get("selected").get("manifest").str();}
  void publish_raw(const fs::path& root,const std::string& raw){
    auto ptr=pointer(root);auto path=root/ptr.get("selected").get("manifest").str();
    atomic_write(path,raw);ptr.get("selected").set("sha256",Json::string(sha256(raw)));
    atomic_write(root/"current.json",ptr.dump());
  }
  void publish(const fs::path& root,const Json& manifest){publish_raw(root,manifest.dump());}
  void test(const char* name,const std::function<void()>& action){
    try{action();++passed;std::cout<<"PASS "<<name<<"\n";}
    catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<"\n";}
  }
  void replace_image(const fs::path& root,Json& manifest,size_t index,const std::string& bytes){
    auto row=manifest.get("images").at(index);auto old=row.get("sha256").str(),digest=sha256(bytes);
    row.set("sha256",Json::string(digest));row.set("path",Json::string("images/"+digest+".png"));
    row.set("bytes",Json::parse("{\"n\":"+std::to_string(bytes.size())+"}").get("n"));
    atomic_write(manifest_path(root).parent_path()/"images"/(digest+".png"),bytes);
    if(manifest.get("background").str()==old)manifest.set("background",Json::string(digest));
    auto icons=manifest.get("icons");
    for(size_t i=0;i<icons.size();++i)if(icons.at(i).get("image").str()==old)icons.at(i).set("image",Json::string(digest));
    auto apps=manifest.get("apps");
    for(size_t i=0;i<apps.size();++i)for(auto* key:{"icon","hero"})if(apps.at(i).get(key).str()==old)apps.at(i).set(key,Json::string(digest));
    size_t total=0;auto images=manifest.get("images");
    for(size_t i=0;i<images.size();++i)total+=static_cast<size_t>(images.at(i).get("bytes").num());
    manifest.set("compressed_bytes",Json::parse("{\"n\":"+std::to_string(total)+"}").get("n"));
  }
  std::string make_background(unsigned char shade){
    png_image image{};image.version=PNG_IMAGE_VERSION;image.width=1600;image.height=900;image.format=PNG_FORMAT_RGB;
    std::vector<unsigned char> pixels(static_cast<size_t>(1600)*900*3,shade);
    png_alloc_size_t size=0;
    check(png_image_write_to_memory(&image,nullptr,&size,0,pixels.data(),0,nullptr)!=0,"PNG sizing failed");
    std::string result(static_cast<size_t>(size),'\0');
    check(png_image_write_to_memory(&image,result.data(),&size,0,pixels.data(),0,nullptr)!=0,"PNG encoding failed");
    result.resize(static_cast<size_t>(size));
    // libpng's writer can add ancillary chunks; fixtures use the admitted profile.
    std::string stripped=result.substr(0,8);
    for(size_t pos=8;pos+12<=result.size();){
      uint32_t length=0;for(size_t i=0;i<4;++i)length=(length<<8)|static_cast<unsigned char>(result[pos+i]);
      check(static_cast<size_t>(length)<=result.size()-pos-12);
      auto kind=result.substr(pos+4,4);
      if(kind=="IHDR"||kind=="IDAT"||kind=="IEND")stripped.append(result,pos,static_cast<size_t>(length)+12);
      pos+=static_cast<size_t>(length)+12;
    }
    return stripped;
  }
  uint32_t read32(const std::string& s,size_t pos){uint32_t v=0;for(size_t i=0;i<4;++i)v=(v<<8)|static_cast<unsigned char>(s[pos+i]);return v;}
  void put32(std::string& s,uint32_t v){for(int shift=24;shift>=0;shift-=8)s.push_back(static_cast<char>(v>>shift));}
  std::string truncated_idat(const std::string& png){
    auto kind=png.find("IDAT");check(kind!=std::string::npos&&kind>=4);
    auto start=kind-4;auto size=static_cast<size_t>(read32(png,start));check(size>4&&start+size+12<=png.size());
    auto short_size=size/2;std::string chunk;put32(chunk,static_cast<uint32_t>(short_size));
    chunk+="IDAT";chunk.append(png,kind+4,short_size);
    auto crc=crc32(0L,Z_NULL,0);crc=crc32(crc,reinterpret_cast<const Bytef*>(chunk.data()+4),static_cast<uInt>(short_size+4));
    put32(chunk,static_cast<uint32_t>(crc));return png.substr(0,start)+chunk+png.substr(start+size+12);
  }
}
int main(int argc,char** argv){
  if(argc!=3){std::cerr<<"usage: theme_tests FIXTURE_THEMES_ROOT OWNED_OUTPUT_DIR\n";return 2;}
  fixture_root=argv[1];
  auto parent=fs::path(argv[2]);fs::create_directories(parent);
  output_root=parent/("run-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  if(!fs::create_directory(output_root)){std::cerr<<"Unable to allocate fresh test output\n";return 2;}
  test("valid fixture loads startup buffers and missing mappings return fallback",[]{
    auto t=load_theme(fixture_root,true);
    check(t.loaded&&t.name=="Geometric Blue"&&t.background_policy=="missing");
    check(t.global_background()&&t.semantic_icon("navigation.search")&&t.app_icon("com.example.geometric")&&t.app_hero("com.example.geometric"));
    check(!t.semantic_icon("unknown")&&!t.app_icon("not.an.app")&&!t.app_hero("not.an.app"));
    check(t.background_for("com.example.geometric",true)==t.app_hero("com.example.geometric"));
    check(!t.background_for("com.example.other",true)&&t.background_for("com.example.other",false)==t.global_background());
    t.background_policy="all";check(t.background_for("com.example.other",true)==t.global_background());
    check(t.rgba_bytes<=24*1024*1024&&t.rgba_bytes>=1600*900*4*2);
  });
  test("missing implicit selection uses defaults and explicit missing gives guidance",[]{
    auto absent=output_root/"absent";auto t=load_theme(absent);check(!t.loaded&&t.error.empty());
    rejects([&]{(void)load_theme(absent,true);},"select the built-in default");
  });
  test("selected built-in default succeeds explicitly",[]{
    auto root=copy_fixture();atomic_write(root/"current.json",R"({"schema":"spice-home.theme-pointer.v1","selected":null,"previous":null})");
    auto t=load_theme(root,true);check(!t.loaded&&t.error.empty());
  });
  test("pointer traversal fails explicitly and implicit call preserves defaults",[]{
    auto root=copy_fixture();auto p=pointer(root);p.get("selected").set("manifest",Json::string("../manifest.json"));atomic_write(root/"current.json",p.dump());
    rejects([&]{(void)load_theme(root,true);},"generation path");auto t=load_theme(root);check(!t.loaded&&!t.error.empty());
  });
  test("manifest hash corruption rejected",[]{auto root=copy_fixture();atomic_write(manifest_path(root),"{}");rejects([&]{(void)load_theme(root,true);},"manifest hash");});
  test("image hash corruption rejected",[]{
    auto root=copy_fixture();auto m=Json::parse(read_file(manifest_path(root)));auto path=manifest_path(root).parent_path()/m.get("images").at(0).get("path").str();
    atomic_write(path,"corrupt");rejects([&]{(void)load_theme(root,true);},"hash/length");
  });
  test("PNG CRC corruption rejected even with recomputed content hashes",[]{
    auto root=copy_fixture();auto m=Json::parse(read_file(manifest_path(root)));auto path=manifest_path(root).parent_path()/m.get("images").at(0).get("path").str();
    auto bytes=read_file(path,8*1024*1024);bytes.back()^=1;replace_image(root,m,0,bytes);publish(root,m);
    rejects([&]{(void)load_theme(root,true);},"CRC");
  });
  test("incomplete zlib stream rejected after valid PNG CRC and hash",[]{
    auto root=copy_fixture();auto m=Json::parse(read_file(manifest_path(root)));auto path=manifest_path(root).parent_path()/m.get("images").at(0).get("path").str();
    replace_image(root,m,0,truncated_idat(read_file(path,8*1024*1024)));publish(root,m);
    rejects([&]{(void)load_theme(root,true);},"decode");
  });
  test("duplicate escaped JSON keys rejected at pointer and manifest",[]{
    auto root=copy_fixture();auto raw=read_file(root/"current.json");auto pos=raw.find("\"schema\"");check(pos!=std::string::npos);
    raw.insert(pos,"\"\\u0073chema\":\"evil\",");atomic_write(root/"current.json",raw);
    rejects([&]{(void)load_theme(root,true);},"Duplicate");
    root=copy_fixture();raw=read_file(manifest_path(root));pos=raw.find("\"name\"");check(pos!=std::string::npos);
    raw.insert(pos,"\"name\":\"evil\",");publish_raw(root,raw);rejects([&]{(void)load_theme(root,true);},"Duplicate");
  });
  test("NUL in JSON key cannot alias a parsed field",[]{
    auto root=copy_fixture();auto raw=read_file(root/"current.json");auto pos=raw.find("\"schema\"");check(pos!=std::string::npos);
    raw.replace(pos,8,"\"schema\\u0000ignored\"");atomic_write(root/"current.json",raw);
    rejects([&]{(void)load_theme(root,true);},"NUL");
  });
  test("unsupported layers and duplicate app mapping rejected",[]{
    auto root=copy_fixture();auto m=Json::parse(read_file(manifest_path(root)));m.set("layers",Json::array());publish(root,m);
    rejects([&]{(void)load_theme(root,true);},"Unsupported theme field");
    root=copy_fixture();m=Json::parse(read_file(manifest_path(root)));m.get("apps").add(m.get("apps").at(0));publish(root,m);
    rejects([&]{(void)load_theme(root,true);},"duplicate");
  });
  test("invalid optional fields and image destinations rejected",[]{
    auto root=copy_fixture();auto m=Json::parse(read_file(manifest_path(root)));m.set("accent",Json());publish(root,m);
    rejects([&]{(void)load_theme(root,true);},"string");
    root=copy_fixture();m=Json::parse(read_file(manifest_path(root)));m.get("images").at(0).set("path",Json::string("../outside.png"));publish(root,m);
    rejects([&]{(void)load_theme(root,true);},"theme image");
  });
  test("more than 24 MiB including crossfade reserve rejected",[]{
    auto root=copy_fixture();auto m=Json::parse(read_file(manifest_path(root)));
    for(unsigned char shade:{17,39}){
      auto bytes=make_background(shade),digest=sha256(bytes);
      atomic_write(manifest_path(root).parent_path()/"images"/(digest+".png"),bytes);
      auto row=Json::parse("{\"kind\":\"background\",\"width\":1600,\"height\":900,\"bytes\":"+std::to_string(bytes.size())+"}");
      row.set("sha256",Json::string(digest));row.set("path",Json::string("images/"+digest+".png"));m.get("images").add(row);
    }
    publish(root,m);rejects([&]{(void)load_theme(root,true);},"budget");
  });
  test("symlinked asset denied even when target remains inside store",[]{
    auto root=copy_fixture();auto m=Json::parse(read_file(manifest_path(root)));auto target=manifest_path(root).parent_path()/m.get("images").at(0).get("path").str();
    auto alternate=target.parent_path()/"alternate.png";fs::copy_file(target,alternate);fs::remove(target);
    std::error_code ec;fs::create_symlink(alternate,target,ec);
    if(ec){++skipped;std::cout<<"SKIP symlink creation: "<<ec.message()<<"\n";return;}
    rejects([&]{(void)load_theme(root,true);},"link");
  });
  std::cout<<"RESULT "<<passed<<" passed / "<<failed<<" failed / "<<skipped<<" skipped subchecks\n";
  return failed?1:0;
}
