#include "spice/core.hpp"
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace spice {
  Viewport Viewport::make(int ww,int wh,int dw,int dh){
    Viewport v;
    v.window_w=std::max(1,ww);
    v.window_h=std::max(1,wh);
    v.drawable_w=std::max(1,dw);
    v.drawable_h=std::max(1,dh);
    v.scale=std::min(v.drawable_w/DesignWidth,v.drawable_h/DesignHeight);
    v.left=(v.drawable_w-DesignWidth*v.scale)*.5f;
    v.top=(v.drawable_h-DesignHeight*v.scale)*.5f;
    return v;
  }
  std::optional<Point> Viewport::pointer(float x,float y)const{
    Point p{
      (x*drawable_w/window_w-left)/scale,(y*drawable_h/window_h-top)/scale
    };
    if(p.x<0||p.y<0||p.x>=DesignWidth||p.y>=DesignHeight)return std::nullopt;
    return p;
  }
  int neighbor(const std::vector<Node>& nodes,int from,Direction d){
    if(nodes.empty())return -1;
    if(from<0||static_cast<size_t>(from)>=nodes.size())return 0;
    auto a=nodes[static_cast<size_t>(from)].rect.center();
    double best=std::numeric_limits<double>::max();
    int found=from;
    for(size_t i=0;i<nodes.size();++i){
      if(static_cast<int>(i)==from)continue;
      auto b=nodes[i].rect.center();
      double dx=b.x-a.x,dy=b.y-a.y,forward=0,side=0;
      switch(d){
        case Direction::Left:forward=-dx;
        side=std::abs(dy);
        break;
        case Direction::Right:forward=dx;
        side=std::abs(dy);
        break;
        case Direction::Up:forward=-dy;
        side=std::abs(dx);
        break;
        case Direction::Down:forward=dy;
        side=std::abs(dx);
        break;
      }
      if(forward<1)continue;
      // Row/column alignment beats shorter diagonals; empty staircase cells stay empty.
      double score=forward+side*2.5+(side>forward*1.2?1000:0);
      if(score<best){
        best=score;
        found=static_cast<int>(i);
      }
    }
    return found;
  }
  std::vector<Node> home_nodes(bool expanded){
    std::vector<Node> out{
      {
        "games",{
          96,44,116,50
        }
      },
      {
        "media",{
          242,44,116,50
        }
      },
      {
        "library",{
          1456,42,54,54
        }
      },
      {
        "expand",{
          1526,42,54,54
        }
      },
      {
        "settings",{
          1596,42,54,54
        }
      },
      {
        "control",{
          1666,42,54,54
        }
      }
    };
    for(int i=0;i<7;++i)out.push_back({
      "rail:"+std::to_string(i),{
        96.f+i*130,150,110,110
      }
    }
    );
    if(expanded){
      int k=0;
      for(int row=0;row<3;++row)for(int col=0;col<4-row;++col)out.push_back({
        "extra:"+std::to_string(k++),{
          96.f+(3+row+col)*130,340.f+row*144,110,104
        }
      }
      );
    }
    for(int i=0;i<3;++i)out.push_back({
      "widget:"+std::to_string(i),{
        1340,354.f+i*130,484,108
      }
    }
    );
    for(int i=0;i<5;++i)out.push_back({
      "card:"+std::to_string(i),{
        96.f+i*284,832,270,140
      }
    }
    );
    return out;
  }
  float approach(float a,float b,float dt,float speed){
    if(!std::isfinite(dt)||dt<=0)return a;
    return b+(a-b)*std::exp(-speed*dt);
  }
  float ease(float t){
    t=std::clamp(t,0.f,1.f);
    return 1-std::pow(1-t,3.f);
  }
  Presentation panel_presentation(float progress,bool reduced_motion){
    float opacity=reduced_motion?1.f:ease(progress);
    return {12*(1-opacity),opacity};
  }
  void FocusMotion::snap(Rect target){
    from_=target_=value_=target;
    elapsed_=Duration;
  }
  void FocusMotion::retarget(Rect target){
    if(target.x==target_.x&&target.y==target_.y&&target.w==target_.w&&target.h==target_.h)return;
    from_=value_;
    target_=target;
    elapsed_=0;
  }
  bool FocusMotion::update(double dt,bool active,bool reduced_motion){
    if(settled())return false;
    if(reduced_motion){snap(target_);return true;}
    if(!active||!std::isfinite(dt)||dt<=0)return false;
    elapsed_=std::min(Duration,elapsed_+dt);
    float t=ease(progress());
    value_={from_.x+(target_.x-from_.x)*t,from_.y+(target_.y-from_.y)*t,
      from_.w+(target_.w-from_.w)*t,from_.h+(target_.h-from_.h)*t};
    if(settled())value_=target_;
    return true; // Includes the endpoint frame; the next idle update returns false.
  }
  void ArtworkMotion::request(const std::string& target){
    if(target.empty())return;
    if(animating()){
      if(target==to_)pending_.clear();
      else pending_=target;
    }
    else if(target!=to_){
      from_=to_;
      to_=target;
      elapsed_=0;
    }
  }
  bool ArtworkMotion::update(double dt,bool active,bool reduced_motion){
    if(!animating())return false;
    if(reduced_motion){
      if(!pending_.empty())to_=std::move(pending_);
      from_=to_;
      pending_.clear();
      elapsed_=Duration;
      return true;
    }
    if(!active||!std::isfinite(dt)||dt<=0)return false;
    // One pending target bounds both storage and work. Carry excess time into
    // that segment so matching elapsed times agree across 60/120/240 steps.
    double remaining=dt;
    while(animating()&&remaining>0){
      double used=std::min(remaining,Duration-elapsed_);
      elapsed_+=used;
      remaining-=used;
      if(elapsed_>=Duration){
        from_=to_;
        if(!pending_.empty()&&pending_!=to_){
          to_=std::move(pending_);
          pending_.clear();
          elapsed_=0;
        }
        else pending_.clear();
      }
    }
    return true;
  }
  float AmbientMotion::rand01(){
    rng_^=rng_<<13;
    rng_^=rng_>>17;
    rng_^=rng_<<5;
    return (rng_&0x00ffffffu)/16777216.f;
  }
  void AmbientMotion::respawn(AmbientParticle& p,bool initial){
    // Match the reference composition: particles live in the middle band and
    // drift in short arcs with white / pale-yellow / dim-blue tints.
    p.x=rand01()*1920.f;
    p.y=540.f+rand01()*300.f;
    p.r=2.f+rand01()*4.f;
    p.vx=(rand01()-.5f)*10.f;
    p.vy=(rand01()-.5f)*6.f;
    p.phase=rand01()*6.2831853f;
    p.alpha=initial?rand01()*.5f:0.f;
    const float tint=rand01();
    if(tint<.45f){p.cr=1;p.cg=1;p.cb=1;}
    else if(tint<.75f){p.cr=1;p.cg=1;p.cb=.69f;}
    else{p.cr=.45f;p.cg=.55f;p.cb=.75f;}
  }
  AmbientMotion::AmbientMotion(bool enabled):enabled_(enabled){
    particles_.reserve(MaxParticles);
    for(size_t i=0;i<MaxParticles;++i){
      AmbientParticle p;
      respawn(p,true);
      particles_.push_back(p);
    }
  }
  bool AmbientMotion::advance(double dt,bool active,bool reduced_motion,Point focus){
    if(reduced_motion||!enabled_){
      // Neutral pose is exact; no frames are spent on a hidden effect.
      px_=py_=0;
      pulse_=PulseDuration;
      return false;
    }
    if(!active||!std::isfinite(dt)||dt<=0)return false;
    time_+=dt;
    if(pulse_<PulseDuration)pulse_=std::min(PulseDuration,pulse_+dt);
    if(std::isfinite(focus.x)&&std::isfinite(focus.y)){
      target_x_=std::clamp((focus.x-960.f)/960.f,-1.f,1.f)*MaxParallax;
      target_y_=std::clamp((focus.y-540.f)/540.f,-1.f,1.f)*MaxParallax*.6f;
    }
    else target_x_=target_y_=0;
    px_=approach(px_,target_x_,static_cast<float>(dt),3.f);
    py_=approach(py_,target_y_,static_cast<float>(dt),3.f);
    for(auto& p:particles_){
      p.phase+=dt*.8;
      p.alpha=.18f+.16f*(.5f+.5f*std::sin(p.phase));
      p.x+=p.vx*static_cast<float>(dt)*(.6f+.4f*std::sin(p.phase*.7f));
      p.y+=p.vy*static_cast<float>(dt);
      if(p.x<-8||p.x>1928||p.y<480||p.y>1088)respawn(p,false);
    }
    return true;
  }
  void FrameStats::push(double ms){
    if(!std::isfinite(ms)||ms<=0)return;
    values_[next_++%values_.size()]=ms;
    size_=std::min(size_+1,values_.size());
  }
  FrameSummary FrameStats::summary()const{
    if(!size_)return {};
    std::vector<double> v(values_.begin(),values_.begin()+static_cast<ptrdiff_t>(size_));
    std::sort(v.begin(),v.end());
    auto p=[&](double q){
      return v[static_cast<size_t>(std::floor((v.size()-1)*q))];
    };
    return {
      p(.5),p(.95),p(.99),1000/p(.5),v.size(),v.back()
    };
  }
  bool safe_app_id(const std::string& id){
    return !id.empty()&&id.size()<=160&&std::all_of(id.begin(),id.end(),[](unsigned char c){
      return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';
    }
    );
  }
  bool safe_setting(const std::string& id){
    auto p=id.find('.');
    if(p==std::string::npos||p==0||p>40||p+1==id.size()||id.size()-p-1>60||id.find('.',p+1)!=std::string::npos)return false;
    return std::all_of(id.begin(),id.end(),[](unsigned char c){
      return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.';
    }
    );
  }
  bool private_ipv4(const std::string& host){
    if(host.empty()||host.size()>15)return false;
    std::array<int,4> a{};
    size_t start=0;
    for(int i=0;i<4;++i){
      auto end=host.find('.',start);
      if(i==3){
        if(end!=std::string::npos)return false;
        end=host.size();
      }
      else if(end==std::string::npos)return false;
      auto s=std::string_view(host).substr(start,end-start);
      if(s.empty()||(s.size()>1&&s[0]=='0'))return false;
      auto [ptr,ec]=std::from_chars(s.data(),s.data()+s.size(),a[static_cast<size_t>(i)]);
      if(ec!=std::errc{}
      ||ptr!=s.data()+s.size()||a[static_cast<size_t>(i)]<0||a[static_cast<size_t>(i)]>255)return false;
      start=end+1;
    }
    return a[0]==10||(a[0]==192&&a[1]==168)||(a[0]==172&&a[1]>=16&&a[1]<=31);
  }
  bool safe_token(const std::string& s){
    return s.size()>=24&&s.size()<=256&&std::all_of(s.begin(),s.end(),[](unsigned char c){
      return c>=33&&c<=126;
    }
    );
  }
  void pop_utf8(std::string& value) {
    if(value.empty()) return;
    size_t i=value.size()-1;
    while(i>0&&(static_cast<unsigned char>(value[i])&0xc0)==0x80) --i;
    value.resize(i);
  }
  bool native_launch_argument(const std::string& value) {
    if(value.empty()||value[0]!='{'||value.size()>4096) return false;
    try {
      auto j=Json::parse(value,4096);
      return j.object_type()&&j.get("@system_native_app").flag()&&j.get("nid").str()=="com.spice.home.cpp";
    }
    catch(...) {
      return false;
    }
  }
  std::string clean_text(std::string_view value,size_t max) {
    std::string out;
    for(size_t i=0;i<value.size();) {
      auto b=static_cast<unsigned char>(value[i]);
      size_t n=b<128?1:b>=0xc2&&b<=0xdf?2:b>=0xe0&&b<=0xef?3:b>=0xf0&&b<=0xf4?4:0;
      unsigned cp=n==1?b:b&((1u<<(7-n))-1);
      bool valid=n>0&&i+n<=value.size();
      for(size_t k=1;k<n&&valid;++k) {
        auto c=static_cast<unsigned char>(value[i+k]);
        valid=(c&0xc0)==0x80;
        cp=(cp<<6)|(c&63);
      }
      valid=valid&&!(cp>=0xd800&&cp<=0xdfff)&&cp<=0x10ffff&&!(n==2&&cp<128)&&!(n==3&&cp<2048)&&!(n==4&&cp<65536);
      if(!valid){
        if(out.size()+1>max)break;
        out+='?';
        ++i;
        continue;
      }
      if(cp<32||(cp>=127&&cp<=159)){
        i+=n;
        continue;
      }
      if(out.size()+n>max)break;
      out.append(value.substr(i,n));
      i+=n;
    }
    return out;
  }
  std::string read_file(const std::filesystem::path& p,size_t limit){
    std::ifstream in(p,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot read file: "+p.filename().string());
    std::string out;
    std::array<char,4096> buf{};
    while(in){
      in.read(buf.data(),buf.size());
      auto n=in.gcount();
      if(out.size()+static_cast<size_t>(n)>limit)throw std::runtime_error("File exceeds size limit");
      out.append(buf.data(),static_cast<size_t>(n));
    }
    if(!in.eof())throw std::runtime_error("File read failed");
    return out;
  }
  void atomic_write(const std::filesystem::path& p,const std::string& text){
    if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path());
    auto temp=p;
    temp += ".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try{
      {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        out.write(text.data(),static_cast<std::streamsize>(text.size()));
        out.flush();
        if(!out)throw std::runtime_error("Settings write failed");
      }
#ifdef _WIN32
      if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Atomic settings replace failed");
#else
      std::filesystem::rename(temp,p);
#endif
    }
    catch(...){
      std::error_code ec;
      std::filesystem::remove(temp,ec);
      throw;
    }
  }
  std::filesystem::path user_data_dir(){
    if(auto* env=std::getenv("SPICE_CPP_DATA_DIR"))return env;
#ifdef SPICE_WEBOS
    return "/media/internal/spice-home-cpp";
#endif
#ifdef _WIN32
    if(auto* base=std::getenv("LOCALAPPDATA"))return std::filesystem::path(base)/"SpiceHomeCpp";
#else
    if(auto* base=std::getenv("XDG_CONFIG_HOME"))return std::filesystem::path(base)/"spice-home-cpp";
    if(auto* base=std::getenv("HOME"))return std::filesystem::path(base)/".config/spice-home-cpp";
#endif
    return ".spice-home-cpp";
  }
  std::vector<AppEntry> sample_apps(){
    return {
      {
        "sample.welcome","Welcome","welcome","welcome",false,true
      },
      {
        "sample.eclipse","Eclipse","eclipse","game",false,true
      },
      {
        "sample.aether","Aether","aether","game",false,true
      },
      {
        "sample.ember","Emberline","ember","game",false,true
      },
      {
        "sample.nova","Nova Drift","nova","game",false,true
      },
      {
        "sample.tidal","Tidal","tidal","game",false,true
      },
      {
        "sample.library","Library","library","library",false,true
      },
      {
        "sample.cinema","Cinema","cinema","media",false,true
      }
    };
  }
  std::vector<AppEntry> fallback_apps(){
    return {
      {
        "com.webos.app.livetv","Live TV","welcome","tv"
      },
      {
        "com.webos.app.hdmi1","HDMI 1","aether","input"
      },
      {
        "com.webos.app.hdmi2","HDMI 2","tidal","input"
      },
      {
        "com.webos.app.hdmi3","HDMI 3","ember","input"
      },
      {
        "com.webos.app.hdmi4","HDMI 4","nova","input"
      },
      {
        "org.webosbrew.hbchannel","Homebrew Channel","library","homebrew"
      }
    };
  }
  std::vector<AppEntry> parse_apps(const Json& reply){
    std::vector<AppEntry> out;
    auto list=reply.get("launchPoints");
    if(!list.is(json_type_array))list=reply.get("apps");
    std::set<std::string> ids;
    for(size_t i=0;i<std::min<size_t>(list.size(),1000)&&out.size()<200;++i){
      auto a=list.at(i);
      auto id=a.get("id").str();
      if(!safe_app_id(id)||id=="com.spice.home.cpp"||!a.get("visible").flag(true)||!ids.insert(id).second)continue;
      AppEntry entry{id,clean_text(a.get("title").str(id),96),"library","app",reply.get("source_kind").str()=="prepared"?a.get("inventory_confirmed").flag():true,false};
      entry.description=clean_text(a.get("appDescription").str(a.get("description").str()),384);
      entry.vendor=clean_text(a.get("vendor").str(),96);entry.version=clean_text(a.get("version").str(),64);
      entry.observed_at=clean_text(a.get("observed_at").str(),40);
      out.push_back(std::move(entry));
    }
    return out;
  }
  static std::vector<std::string> read_strings(const Json& j,size_t max,bool(*valid)(const std::string&)){
    std::vector<std::string> v;
    for(size_t i=0;i<std::min(j.size(),max);++i){
      auto s=j.at(i).str();
      if(valid(s)&&std::find(v.begin(),v.end(),s)==v.end())v.push_back(s);
    }
    return v;
  }
  Preferences Preferences::from_json(const Json& j){    if(!j.object_type())throw std::runtime_error("Settings must be an object");
    Preferences p;
    p.extra=j;
    const std::set<std::string> themes{
      "spice","metro","xmb","nxe","ps4","ps5"
    };
    auto theme=j.get("theme").str("ps5");
    if(themes.contains(theme))p.theme=theme;
    p.expanded=j.get("expanded_icons").flag(true);
    p.reduced_motion=j.get("reduced_motion").flag();
    p.ambient_motion=j.get("ambient_motion").flag(true);
    auto idle=j.get("ambient_idle_hz").num(60);
    if(std::isfinite(idle)&&idle>=10&&idle<=240&&std::floor(idle)==idle)p.ambient_idle_hz=static_cast<int>(idle);
    {
      // Unknown presets fall back to "max" so an older or hand-edited file
      // never degrades to a random size.
      auto resolution=j.get("render_resolution").str("max");
      p.render_resolution="max";
      for(auto preset:ResolutionPresets)if(resolution==preset)p.render_resolution=preset;
    }
    p.handoff=j.get("handoff_on_dwell").flag();
    auto dwell=j.get("preview_dwell_seconds").num(8);
    p.dwell=std::isfinite(dwell)?std::clamp(dwell,3.,30.):8;
    auto host=j.get("retropie_host").str();
    if(private_ipv4(host))p.retropie_host=host;
    auto port=j.get("retropie_port").num(8767);
    if(std::isfinite(port)&&port>=1024&&port<=65535)p.retropie_port=static_cast<int>(port);
    auto hdmi=j.get("retropie_hdmi").num(1);
    if(std::isfinite(hdmi)&&hdmi>=1&&hdmi<=4)p.retropie_hdmi=static_cast<int>(hdmi);
    auto id=j.get("processor_app_id").str();
    if(safe_app_id(id))p.processor_app_id=id;
    p.favorites=read_strings(j.get("favorites"),64,safe_app_id);
    p.pins=read_strings(j.get("pinned_settings"),8,safe_setting);
    p.custom_settings=read_strings(j.get("custom_settings"),20,safe_setting);
    const std::set<std::string> actions{
      "left","right","up","down","ok","back","control","settings","library","diagnostics"
    };
    for(auto& [key,value]:j.get("remote_scancodes").members()){
      auto action=value.str();
      if(key.size()<40&&actions.contains(action)&&p.remote_keys.size()<32)p.remote_keys[key]=action;
    }
    for(auto& [app,value]:j.get("preview_profiles").members()){
      auto mode=value.get("mode").str("poster");
      if(safe_app_id(app)&&(mode=="poster"||mode=="native_handoff")&&p.profiles.size()<200)p.profiles[app]=mode;
    }
    return p;
  }
  Json Preferences::json()const{
    auto j=Json::parse(extra.dump());
    j.set("schema",Json::number(1));
    j.set("theme",Json::string(theme));
    j.set("expanded_icons",Json::boolean(expanded));
    j.set("reduced_motion",Json::boolean(reduced_motion));
    j.set("ambient_motion",Json::boolean(ambient_motion));
    j.set("ambient_idle_hz",Json::number(ambient_idle_hz));
    j.set("render_resolution",Json::string(render_resolution));
    j.set("handoff_on_dwell",Json::boolean(handoff));
    j.set("preview_dwell_seconds",Json::number(dwell));
    j.set("retropie_host",Json::string(retropie_host));
    j.set("retropie_port",Json::number(retropie_port));
    j.set("retropie_hdmi",Json::number(retropie_hdmi));
    j.set("processor_app_id",Json::string(processor_app_id));
    auto strings=[&](const char* name,const auto& values){
      auto a=Json::array();
      for(auto& v:values)a.add(Json::string(v));
      j.set(name,a);
    };
    strings("favorites",favorites);
    strings("pinned_settings",pins);
    strings("custom_settings",custom_settings);
    auto keys=Json::object();
    for(auto& [key,value]:remote_keys)keys.set(key.c_str(),Json::string(value));
    j.set("remote_scancodes",keys);
    auto profiles_json=Json::object();
    for(auto& [key,value]:profiles){
      auto entry=Json::object();
      entry.set("mode",Json::string(value));
      profiles_json.set(key.c_str(),entry);
    }
    j.set("preview_profiles",profiles_json);
    // Known legacy credential fields are deliberately not migrated.
    j.set("retropie_token",Json());
    j.set("token",Json());
    return j;
  }
  bool resolution_size(const std::string& preset,int& width,int& height){
    // 16:9 ladder that matches the panel classes this app targets. "max" and
    // anything unrecognized leave the caller to consult the display.
    const std::pair<const char*,std::pair<int,int>> sizes[]{
      {"2160",{3840,2160}},{"1440",{2560,1440}},{"1080",{1920,1080}}
    };
    for(auto& [name,size]:sizes){
      if(preset==name){
        width=size.first;
        height=size.second;
        return true;
      }
    }
    return false;
  }
  void PreviewState::focus(const std::string& id){
    if(focused_==id)return;
    focused_=id;
    suppressed_.clear();
    elapsed_=0;
  }
  std::optional<std::string> PreviewState::tick(double dt,double now,bool active,bool covered,const Preferences& p){
    if(!active||covered){
      elapsed_=0;
      return std::nullopt;
    }
    if(!p.handoff||focused_==suppressed_||!pending_.empty())return std::nullopt;
    auto it=p.profiles.find(focused_);
    if(it==p.profiles.end()||it->second!="native_handoff")return std::nullopt;
    if(focused_!="com.webos.app.livetv"&&focused_!="com.webos.app.hdmi1"&&focused_!="com.webos.app.hdmi2"&&focused_!="com.webos.app.hdmi3"&&focused_!="com.webos.app.hdmi4")return std::nullopt;
    auto retry=retry_at_.find(focused_);
    if(retry!=retry_at_.end()&&now<retry->second){
      elapsed_=0;
      return std::nullopt;
    }
    if(!std::isfinite(dt)||dt<=0)return std::nullopt;
    elapsed_+=std::min(dt,.05);
    if(elapsed_<p.dwell)return std::nullopt;
    pending_=focused_;
    elapsed_=0;
    return pending_;
  }
  void PreviewState::complete(const std::string& id,bool success,double now){
    if(id!=pending_)return;
    pending_.clear();
    if(success){
      suppressed_=id;
    }
    else{
      if(retry_at_.size()>=32)retry_at_.erase(retry_at_.begin());
      retry_at_[id]=now+45;
    }
    elapsed_=0;
  }
  void PreviewState::retry(double now){
    retry_at_[focused_]=now;
    suppressed_.clear();
    elapsed_=0;
  }
  Processor parse_processor(const std::string& stat,const std::string& mem,const std::string& load,const Processor* previous){
    Processor p;
    std::istringstream cpu(stat);
    std::string label;
    cpu>>label;
    if(label!="cpu")throw std::runtime_error("Missing processor counters");
    std::array<uint64_t,8> ticks{};
    for(auto& v:ticks)if(!(cpu>>v))throw std::runtime_error("Invalid processor counters");
    for(auto v:ticks){
      if(v>UINT64_MAX-p.total_ticks)throw std::runtime_error("Processor counter overflow");
      p.total_ticks+=v;
    }
    p.idle_ticks=ticks[3]+ticks[4];
    if(previous&&p.total_ticks>previous->total_ticks&&p.idle_ticks>=previous->idle_ticks){
      auto d=p.total_ticks-previous->total_ticks;
      auto idle=p.idle_ticks-previous->idle_ticks;
      p.cpu=100.*(1.-static_cast<double>(std::min(idle,d))/static_cast<double>(d));
    }
    std::istringstream mi(mem);
    std::string line;
    bool total=false,avail=false;
    while(std::getline(mi,line)){
      std::istringstream s(line);
      std::string key;
      uint64_t v=0;
      s>>key>>v;
      if(key=="MemTotal:"){
        p.total_kib=v;
        total=true;
      }
      if(key=="MemAvailable:"){
        p.available_kib=v;
        avail=true;
      }
    }
    if(!total||!avail||p.total_kib==0||p.available_kib>p.total_kib)throw std::runtime_error("Memory counters unavailable");
    p.memory=100.*(1.-static_cast<double>(p.available_kib)/p.total_kib);
    std::istringstream li(load);
    li>>p.load1>>p.load5>>p.load15;
    if(!li||!std::isfinite(p.load1)||!std::isfinite(p.load5)||!std::isfinite(p.load15)||p.load1<0||p.load5<0||p.load15<0)throw std::runtime_error("Invalid load average");
    p.available=true;
    return p;
  }
}
