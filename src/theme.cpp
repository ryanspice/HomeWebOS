#include "spice/theme.hpp"
#include "spice/prepared.hpp"
#include <png.h>
#include <zlib.h>
#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string_view>

namespace spice {
  namespace {
    constexpr size_t MiB=1024*1024, MaxRgba=24*MiB, Reserve=1600*900*4;
    constexpr size_t MaxCompressed=16*MiB, MaxImage=8*MiB;
    void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
    bool whitespace(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}

    // json-c overwrites duplicate keys. Walk the already parsed JSON tokens and
    // compare decoded object names, including equivalent Unicode escape forms.
    class UniqueKeys {
      std::string_view in_;
      size_t at_=0;
      void space(){while(at_<in_.size()&&whitespace(in_[at_]))++at_;}
      std::string token(){
        require(at_<in_.size()&&in_[at_]=='"',"Expected JSON string");
        size_t start=at_++;
        bool escaped=false;
        while(at_<in_.size()){
          char c=in_[at_++];
          if(escaped){escaped=false;continue;}
          if(c=='\\'){escaped=true;continue;}
          if(c=='"')return Json::parse(in_.substr(start,at_-start)).str();
        }
        throw std::runtime_error("Unterminated JSON string");
      }
      void value(unsigned depth){
        require(depth<=32,"Theme JSON nesting exceeds limit");
        space();require(at_<in_.size(),"Invalid theme JSON");
        if(in_[at_]=='{'){
          ++at_;space();std::set<std::string> keys;
          if(at_<in_.size()&&in_[at_]=='}'){++at_;return;}
          for(;;){
            space();auto key=token();require(key.find('\0')==std::string::npos,"NUL in theme JSON key");
            require(keys.insert(key).second,"Duplicate theme JSON key");
            space();require(at_<in_.size()&&in_[at_++]==':',"Invalid JSON object");
            value(depth+1);space();require(at_<in_.size(),"Invalid JSON object");
            char sep=in_[at_++];if(sep=='}')return;require(sep==',',"Invalid JSON object");
          }
        }
        if(in_[at_]=='['){
          ++at_;space();if(at_<in_.size()&&in_[at_]==']'){++at_;return;}
          for(;;){value(depth+1);space();require(at_<in_.size(),"Invalid JSON array");
            char sep=in_[at_++];if(sep==']')return;require(sep==',',"Invalid JSON array");}
        }
        if(in_[at_]=='"'){(void)token();return;}
        size_t start=at_;
        while(at_<in_.size()&&!whitespace(in_[at_])&&in_[at_]!=','&&in_[at_]!=']'&&in_[at_]!='}')++at_;
        require(at_>start,"Invalid JSON value");
      }
      public:
      explicit UniqueKeys(std::string_view in):in_(in){}
      void check(){value(0);space();require(at_==in_.size(),"Trailing theme JSON");}
    };
    Json parse(const std::string& raw,size_t limit){
      auto out=Json::parse(raw,limit);UniqueKeys(raw).check();return out;
    }
    void fields(const Json& obj,std::initializer_list<const char*> required,
                std::initializer_list<const char*> optional={}){
      require(obj.object_type(),"Expected theme object");
      std::set<std::string> allowed;
      for(auto* key:required)allowed.insert(key);
      for(auto* key:optional)allowed.insert(key);
      std::set<std::string> found;
      for(const auto& [key,value]:obj.members()){
        (void)value;require(allowed.contains(key),"Unsupported theme field/layer: "+key);found.insert(key);
      }
      for(auto* key:required)require(found.contains(key),"Missing theme field: "+std::string(key));
    }
    bool has(const Json& obj,const std::string& key){
      for(const auto& [name,value]:obj.members()){(void)value;if(name==key)return true;}
      return false;
    }
    std::string str(const Json& v,size_t limit,bool required=true){
      require(v.is(json_type_string),"Theme string has wrong type");
      auto s=v.str();require((!required||!s.empty())&&clean_text(s,limit)==s,"Invalid theme text");return s;
    }
    bool hex(const std::string& s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
    bool generation(const std::string& s){return s.size()==32&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
    size_t integer(const Json& v,size_t max){
      require(v.is(json_type_int),"Theme number must be an integer");
      auto n=v.num(-1);require(n>=0&&n<=static_cast<double>(max)&&std::floor(n)==n,"Theme integer exceeds limit");return static_cast<size_t>(n);
    }
    void no_links(const std::filesystem::path& path){
      auto absolute=std::filesystem::absolute(path).lexically_normal();
      std::filesystem::path cursor;
      for(const auto& part:absolute){
        cursor/=part;auto status=std::filesystem::symlink_status(cursor);
        require(std::filesystem::exists(status)&&!std::filesystem::is_symlink(status),"Theme path missing or contains a link");
      }
    }
    std::filesystem::path file_at(const std::filesystem::path& root,const std::string& rel){
      require(!rel.empty()&&rel.find('\0')==std::string::npos&&rel.find('\\')==std::string::npos&&rel.find(':')==std::string::npos,"Invalid theme path");
      require(rel.front()!='/'&&rel.back()!='/'&&rel.find("//")==std::string::npos,"Invalid theme path");
      std::filesystem::path part(rel);
      for(const auto& p:part)require(p!="."&&p!="..","Theme path traversal denied");
      auto target=root/part;no_links(target);
      require(std::filesystem::is_regular_file(target)&&std::filesystem::hard_link_count(target)==1,"Theme file must be an unlinked regular file");
      return target;
    }
    uint32_t u32(const std::string& s,size_t at){
      uint32_t n=0;for(size_t i=0;i<4;++i)n=(n<<8)|static_cast<unsigned char>(s[at+i]);return n;
    }
    void validate_png(const std::string& bytes,bool background){
      static constexpr unsigned char sig[]{137,80,78,71,13,10,26,10};
      require(bytes.size()>=57&&std::memcmp(bytes.data(),sig,8)==0,"Invalid theme PNG signature");
      size_t pos=8;bool header=false,data=false,ended=false,after_data=false;
      while(pos+12<=bytes.size()){
        auto n=static_cast<size_t>(u32(bytes,pos));
        require(n<=MaxImage&&n<=bytes.size()-pos-12,"Invalid theme PNG chunk size");
        auto kind=bytes.substr(pos+4,4);
        auto crc=crc32(0L,Z_NULL,0);
        crc=crc32(crc,reinterpret_cast<const Bytef*>(bytes.data()+pos+4),static_cast<uInt>(n+4));
        require(static_cast<uint32_t>(crc)==u32(bytes,pos+8+n),"Theme PNG CRC mismatch");
        if(kind=="IHDR"){
          require(!header&&pos==8&&n==13,"Invalid theme PNG header");header=true;
        }else if(kind=="IDAT"){
          require(header&&!after_data&&!ended,"Invalid theme PNG data ordering");data=true;
        }else if(kind=="IEND"){
          require(header&&data&&!ended&&n==0,"Invalid theme PNG end");ended=true;after_data=true;
        }else throw std::runtime_error("Unsupported theme PNG chunk");
        pos+=n+12;
        if(ended)break;
      }
      require(header&&data&&ended&&pos==bytes.size(),"Incomplete theme PNG");
      const uint32_t w=background?1600u:128u,h=background?900u:128u;
      require(u32(bytes,16)==w&&u32(bytes,20)==h&&static_cast<unsigned char>(bytes[24])==8&&
              static_cast<unsigned char>(bytes[25])==(background?2:6)&&bytes[26]==0&&bytes[27]==0&&bytes[28]==0,
              "Theme PNG dimensions or RGB/RGBA profile mismatch");
      png_image image{};image.version=PNG_IMAGE_VERSION;
      require(png_image_begin_read_from_memory(&image,bytes.data(),bytes.size())!=0,"Theme PNG decode header failed");
      image.format=PNG_FORMAT_RGBA;
      std::vector<unsigned char> pixels(static_cast<size_t>(w)*h*4);
      const bool ok=png_image_finish_read(&image,nullptr,pixels.data(),0,nullptr)!=0;
      png_image_free(&image);
      require(ok,"Theme PNG decode failed");
    }
    struct Reference {std::string path,digest,gen;};
    Reference reference(const Json& value){
      if(value.empty())return {};
      fields(value,{"manifest","sha256"});
      auto path=str(value.get("manifest"),128),digest=str(value.get("sha256"),64);
      constexpr std::string_view prefix="snapshots/",suffix="/manifest.json";
      require(path.starts_with(prefix)&&path.ends_with(suffix)&&path.size()==prefix.size()+32+suffix.size(),"Invalid theme generation path");
      auto gen=path.substr(prefix.size(),32);
      require(generation(gen)&&hex(digest),"Invalid theme generation reference");
      return {path,digest,gen};
    }
    Theme load(const std::filesystem::path& root,bool explicit_selection){
      Theme out;
      std::error_code ec;
      auto root_status=std::filesystem::symlink_status(root,ec);
      if(!ec&&(std::filesystem::exists(root_status)||std::filesystem::is_symlink(root_status)))no_links(root);
      ec.clear();
      auto status=std::filesystem::symlink_status(root/"current.json",ec);
      require(!ec||ec==std::errc::no_such_file_or_directory,"Cannot inspect theme pointer");
      if((ec==std::errc::no_such_file_or_directory||!std::filesystem::exists(status))&&!std::filesystem::is_symlink(status)){
        require(!explicit_selection,"No theme pointer; import a theme or select the built-in default");return out;
      }
      auto pointer=parse(read_file(file_at(root,"current.json"),4096),4096);
      fields(pointer,{"schema","selected","previous"});
      require(pointer.get("schema").str()=="spice-home.theme-pointer.v1","Unsupported theme pointer");
      auto selected=reference(pointer.get("selected"));(void)reference(pointer.get("previous"));
      if(selected.path.empty())return out;
      auto manifest_path=file_at(root,selected.path);
      auto raw=read_file(manifest_path,131072);
      require(sha256(raw)==selected.digest,"Theme manifest hash mismatch");
      auto manifest=parse(raw,131072);
      fields(manifest,{"schema","name","version","provenance","background_policy","background","icons","apps","images","rgba_bytes","compressed_bytes"},{"accent"});
      require(manifest.get("schema").str()=="spice-home.theme.v1","Unsupported theme schema or p5theme layers");
      out.generation=selected.gen;out.name=str(manifest.get("name"),96);out.version=str(manifest.get("version"),64);
      out.provenance=str(manifest.get("provenance"),384);out.background_policy=str(manifest.get("background_policy"),16);
      require(out.background_policy=="all"||out.background_policy=="missing","Invalid theme background policy");
      if(has(manifest,"accent")){
        out.accent=str(manifest.get("accent"),7);
        require(out.accent.size()==7&&out.accent[0]=='#'&&std::all_of(out.accent.begin()+1,out.accent.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}),"Invalid theme accent");
      }
      auto images=manifest.get("images");
      require(images.is(json_type_array)&&images.size()>0&&images.size()<=128,"Invalid theme image count");
      std::set<std::string> hashes;
      out.rgba_bytes=Reserve;
      for(size_t i=0;i<images.size();++i){
        auto item=images.at(i);fields(item,{"sha256","path","bytes","width","height","kind"});
        auto digest=str(item.get("sha256"),64),path=str(item.get("path"),80),kind=str(item.get("kind"),16);
        require(hex(digest)&&hashes.insert(digest).second&&path=="images/"+digest+".png"&&
                (kind=="background"||kind=="icon"),"Invalid or duplicate theme image");
        bool bg=kind=="background";
        auto width=integer(item.get("width"),1600),height=integer(item.get("height"),900),count=integer(item.get("bytes"),MaxImage);
        require(width==(bg?1600u:128u)&&height==(bg?900u:128u)&&count>0,"Invalid theme image dimensions/length");
        out.rgba_bytes+=width*height*4;out.compressed_bytes+=count;
        require(out.rgba_bytes<=MaxRgba&&out.compressed_bytes<=MaxCompressed,"Theme graphics budget exceeded");
        auto bytes=read_file(file_at(manifest_path.parent_path(),path),MaxImage);
        require(bytes.size()==count&&sha256(bytes)==digest,"Theme image hash/length mismatch");
        validate_png(bytes,bg);
        out.images.push_back({digest,std::move(bytes),static_cast<int>(width),static_cast<int>(height),bg});
      }
      require(integer(manifest.get("rgba_bytes"),MaxRgba)==out.rgba_bytes&&integer(manifest.get("compressed_bytes"),MaxCompressed)==out.compressed_bytes,"Theme graphics accounting mismatch");
      std::set<std::string> used;
      auto bind=[&](const Json& value,bool background){
        auto digest=str(value,64);const auto* image=out.image(digest);
        require(image&&image->background==background,"Unbound theme image");used.insert(digest);return digest;
      };
      out.background=bind(manifest.get("background"),true);
      const std::set<std::string> slots{"navigation.search","navigation.home","navigation.control-center","actions.refresh","actions.settings","library.collection"};
      auto icons=manifest.get("icons");require(icons.is(json_type_array)&&icons.size()<=slots.size(),"Invalid theme icon count");
      for(size_t i=0;i<icons.size();++i){
        auto item=icons.at(i);fields(item,{"slot","image"});auto slot=str(item.get("slot"),64);
        require(slots.contains(slot)&&!out.icons.contains(slot),"Invalid or duplicate semantic icon");out.icons.emplace(slot,bind(item.get("image"),false));
      }
      auto apps=manifest.get("apps");require(apps.is(json_type_array)&&apps.size()<=64,"Invalid theme app override count");
      for(size_t i=0;i<apps.size();++i){
        auto item=apps.at(i);fields(item,{"id"},{"icon","hero"});auto id=str(item.get("id"),128);
        const char first=id.empty()?'\0':id[0];
        require(id.size()>=2&&((first>='a'&&first<='z')||(first>='A'&&first<='Z')||(first>='0'&&first<='9'))&&
                safe_app_id(id)&&!out.apps.contains(id),"Invalid or duplicate theme app ID");
        ThemeAppOverride app;
        if(has(item,"icon"))app.icon=bind(item.get("icon"),false);
        if(has(item,"hero"))app.hero=bind(item.get("hero"),true);
        require(!app.icon.empty()||!app.hero.empty(),"Empty theme app override");out.apps.emplace(id,std::move(app));
      }
      require(used==hashes,"Unreferenced theme image");
      out.loaded=true;return out;
    }
  }
  const ThemeImage* Theme::image(const std::string& digest)const{
    auto found=std::find_if(images.begin(),images.end(),[&](const auto& row){return row.sha256==digest;});
    return found==images.end()?nullptr:&*found;
  }
  const ThemeImage* Theme::global_background()const{return image(background);}
  const ThemeImage* Theme::semantic_icon(const std::string& slot)const{
    auto found=icons.find(slot);return found==icons.end()?nullptr:image(found->second);
  }
  const ThemeImage* Theme::app_icon(const std::string& id)const{
    auto found=apps.find(id);return found==apps.end()?nullptr:image(found->second.icon);
  }
  const ThemeImage* Theme::app_hero(const std::string& id)const{
    auto found=apps.find(id);return found==apps.end()?nullptr:image(found->second.hero);
  }
  const ThemeImage* Theme::background_for(const std::string& id,bool has_prepared_hero)const{
    if(const auto* hero=app_hero(id))return hero;
    if(background_policy=="all"||!has_prepared_hero)return global_background();
    return nullptr;
  }
  Theme load_theme(const std::filesystem::path& root,bool explicit_selection){
    try{return load(root,explicit_selection);}
    catch(const std::exception& e){
      std::string message="Theme invalid: "+std::string(e.what())+"; verify the store or select the built-in default";
      if(explicit_selection)throw std::runtime_error(message);
      Theme fallback;fallback.error=std::move(message);return fallback;
    }
  }
}
