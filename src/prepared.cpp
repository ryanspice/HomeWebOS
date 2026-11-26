#include "spice/prepared.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
namespace spice {
  std::string sha256(std::string_view input){
    static constexpr std::array<uint32_t,64> k{
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    std::array<uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::string padded(input);
    padded.push_back(static_cast<char>(0x80));
    while(padded.size()%64!=56)padded.push_back(0);
    const auto bits=static_cast<uint64_t>(input.size())*8;
    for(int n=7;n>=0;--n)padded.push_back(static_cast<char>(bits>>(n*8)));
    for(size_t at=0;at<padded.size();at+=64){
      std::array<uint32_t,64> w{};
      for(size_t i=0;i<16;++i)for(size_t b=0;b<4;++b)w[i]=(w[i]<<8)|static_cast<unsigned char>(padded[at+i*4+b]);
      for(size_t i=16;i<64;++i){
        auto s0=std::rotr(w[i-15],7)^std::rotr(w[i-15],18)^(w[i-15]>>3);
        auto s1=std::rotr(w[i-2],17)^std::rotr(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
      }
      auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
      for(size_t i=0;i<64;++i){
        auto s1=std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25);
        auto ch=(e&f)^(~e&g);
        auto t1=hh+s1+ch+k[i]+w[i];
        auto s0=std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22);
        auto maj=(a&b)^(a&c)^(b&c);
        auto t2=s0+maj;
        hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
      }
      h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
    }
    static constexpr char hex[]="0123456789abcdef";
    std::string out;out.reserve(64);
    for(auto value:h)for(int n=7;n>=0;--n)out.push_back(hex[(value>>(n*4))&15]);
    return out;
  }
  static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
  static bool hex_hash(const std::string& value){return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
  static bool generation_name(const std::string& value){return !value.empty()&&value.size()<=80&&std::all_of(value.begin(),value.end(),[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
  static std::filesystem::path beneath(const std::filesystem::path& root,const std::string& relative){
    require(!relative.empty()&&relative.find('\0')==std::string::npos&&relative.find('\\')==std::string::npos&&relative.find(':')==std::string::npos,"Invalid prepared path");
    std::filesystem::path part(relative);
    require(!part.is_absolute(),"Prepared absolute path denied");
    for(auto& component:part)require(component!=".."&&component!=".","Prepared path traversal denied");
    auto base=std::filesystem::canonical(root),target=std::filesystem::canonical(base/part);
    auto i=base.begin(),j=target.begin();
    for(;i!=base.end()&&j!=target.end()&&*i==*j;++i,++j){}
    require(i==base.end()&&j!=target.end()&&std::filesystem::is_regular_file(target),"Prepared path escapes root");
    return target;
  }
  static size_t integer(const Json& value,size_t max){
    auto number=value.num(-1);
    require(std::isfinite(number)&&number>=0&&number<=static_cast<double>(max)&&std::floor(number)==number,"Prepared integer exceeds limit");
    return static_cast<size_t>(number);
  }
  static std::string string(const Json& value,size_t max,bool needed=false){
    require(value.is(json_type_string),"Prepared string has wrong type");
    auto out=value.str();
    require((!needed||!out.empty())&&clean_text(out,max)==out,"Prepared string exceeds limit or contains invalid text");
    return out;
  }
  PreparedData load_prepared(const std::filesystem::path& root,bool read_graphics){
    PreparedData out;
    auto pointer=Json::parse(read_file(beneath(root,"current.json"),4096),4096);
    require(pointer.get("schema").str()=="spice-home.prepared-pointer.v1","Unsupported prepared pointer");
    auto relative=pointer.get("manifest").str();
    const std::string prefix="snapshots/",suffix="/manifest.json";
    require(relative.starts_with(prefix)&&relative.ends_with(suffix)&&relative.size()>prefix.size()+suffix.size(),"Invalid prepared generation path");
    out.generation=relative.substr(prefix.size(),relative.size()-prefix.size()-suffix.size());
    require(generation_name(out.generation),"Invalid prepared generation name");
    auto manifest_path=beneath(root,relative);
    auto manifest_bytes=read_file(manifest_path,131072);
    require(hex_hash(pointer.get("sha256").str())&&sha256(manifest_bytes)==pointer.get("sha256").str(),"Prepared manifest hash mismatch");
    auto manifest=Json::parse(manifest_bytes);
    require(manifest.get("schema").str()=="spice-home.prepared.v1","Unsupported prepared manifest");
    auto bytes=read_file(beneath(manifest_path.parent_path(),"data.json"),1048576);
    require(hex_hash(manifest.get("data_sha256").str())&&sha256(bytes)==manifest.get("data_sha256").str(),"Prepared metadata hash mismatch");
    auto data=Json::parse(bytes,1048576);
    require(data.get("schema").str()=="spice-home.prepared.v1","Unsupported prepared metadata");
    out.captured_at=string(data.get("captured_at"),40,true);
    out.source_kind=string(data.get("source").get("kind"),16,true);
    require(out.source_kind=="tv"||out.source_kind=="import","Invalid prepared source");
    out.source_label=out.source_kind=="tv"?"Prepared TV snapshot":"Owner-provided offline metadata";
    out.inventory_scope=out.source_kind=="import"?"owner-import":data.get("source_availability").get("inventory_scope").str("unknown");
    require(out.inventory_scope=="developer"||out.inventory_scope=="private-full"||out.inventory_scope=="package-files-only"||out.inventory_scope=="owner-import"||out.inventory_scope=="unknown","Invalid prepared inventory scope");
    auto assets=manifest.get("assets");
    require(assets.is(json_type_array)&&assets.size()<=128,"Prepared graphics count exceeds limit");
    std::set<std::string> keys;
    for(size_t i=0;i<assets.size();++i){
      auto asset=assets.at(i);
      auto key=string(asset.get("key"),160,true),digest=asset.get("sha256").str();
      auto slash=key.find("/prepared/");
      require(slash!=std::string::npos&&(key.substr(0,slash)=="covers"||key.substr(0,slash)=="hero")&&safe_app_id(key.substr(slash+10))&&keys.insert(key).second,"Invalid/duplicate prepared image key");
      require(hex_hash(digest)&&asset.get("path").str()=="images/"+digest+".png","Invalid prepared image path");
      size_t width=integer(asset.get("width"),1920),height=integer(asset.get("height"),1080),count=integer(asset.get("bytes"),8388608);
      require(width>0&&height>0&&count>0,"Empty prepared image descriptor");
      require(!key.starts_with("covers/")||(width<=256&&height<=256),"Prepared icon dimensions exceed limit");
      out.rgba_bytes+=width*height*4;out.compressed_bytes+=count;
      require(out.rgba_bytes<=48*1048576&&out.compressed_bytes<=16*1048576,"Prepared graphics memory budget exceeded");
      if(!read_graphics)continue;
      auto image=read_file(beneath(manifest_path.parent_path(),asset.get("path").str()),8388608);
      require(sha256(image)==digest&&image.size()==count,"Prepared image hash/length mismatch");
      static constexpr unsigned char signature[]{137,80,78,71,13,10,26,10};
      require(image.size()>=33&&std::memcmp(image.data(),signature,8)==0&&image.substr(12,4)=="IHDR","Invalid prepared PNG header");
      auto u32=[&](size_t at){uint32_t value=0;for(size_t n=0;n<4;++n)value=(value<<8)|static_cast<unsigned char>(image[at+n]);return value;};
      require(image[24]==8&&(image[25]==2||image[25]==6),"Prepared image format exceeds limit");
      require(width==u32(16)&&height==u32(20),"Prepared image header/manifest mismatch");
      out.images.push_back({key,std::move(image),static_cast<int>(width),static_cast<int>(height)});
    }
    auto apps=data.get("apps");
    require(apps.is(json_type_array)&&apps.size()<=200,"Prepared app count exceeds limit");
    std::set<std::string> ids;
    for(size_t i=0;i<apps.size();++i){
      auto row=apps.at(i);
      auto id=string(row.get("id"),128,true);
      require(safe_app_id(id)&&ids.insert(id).second,"Invalid/duplicate prepared app ID");
      require(row.get("inventory_confirmed").is(json_type_boolean),"Prepared inventory evidence state missing");
      bool confirmed=row.get("inventory_confirmed").flag();
      require(out.source_kind=="tv"||!confirmed,"Imported metadata cannot claim TV inventory");
      if(id=="com.spice.home.cpp")continue;
      AppEntry app{id,string(row.get("title"),96,true),"library","app",confirmed,false};
      app.description=string(row.get("description"),384);
      app.vendor=string(row.get("vendor"),96);app.version=string(row.get("version"),64);
      app.observed_at=string(row.get("observed_at"),40,true);
      app.icon_key=string(row.get("icon_key"),160);app.hero_key=string(row.get("hero_key"),160);
      require((app.icon_key.empty()||(app.icon_key=="covers/prepared/"+id&&keys.contains(app.icon_key)))&&(app.hero_key.empty()||(app.hero_key=="hero/prepared/"+id&&keys.contains(app.hero_key))),"Prepared app has unbound artwork");
      app.prepared=true;
      if(id=="com.webos.app.livetv")app.kind="tv";
      else if(id.starts_with("com.webos.app.hdmi"))app.kind="input";
      else if(id=="org.webosbrew.hbchannel")app.kind="homebrew";
      out.apps.push_back(std::move(app));
    }
    auto device=data.get("device");
    if(out.source_kind=="tv"){
      for(auto* name:{"modelName","firmwareVersion","sdkVersion","UHD"})if(device.get(name).is(json_type_string))out.device.set(name,Json::string(string(device.get(name),96)));
      out.device_captured_at=data.get("device_captured_at").is(json_type_string)?string(data.get("device_captured_at"),40):out.captured_at;
    }
    out.image_count=assets.size();out.loaded=true;out.graphics_validated=read_graphics;
    return out;
  }
  Json PreparedData::report()const{
    auto out=Json::object();
    out.set("loaded",Json::boolean(loaded));out.set("generation",Json::string(generation));
    out.set("captured_at",Json::string(captured_at));out.set("source_kind",Json::string(source_kind));
    out.set("device_captured_at",Json::string(device_captured_at));
    out.set("source_label",Json::string(source_label));out.set("error",Json::string(error));
    out.set("inventory_scope",Json::string(inventory_scope));
    out.set("apps",Json::number(static_cast<double>(apps.size())));out.set("graphics",Json::number(static_cast<double>(image_count)));
    out.set("rgba_bytes",Json::number(static_cast<double>(rgba_bytes)));out.set("compressed_bytes",Json::number(static_cast<double>(compressed_bytes)));
    out.set("automatic_live_refresh",Json::boolean(false));out.set("startup_graphics_only",Json::boolean(true));
    out.set("graphics_validated",Json::boolean(graphics_validated));
    return out;
  }
}
