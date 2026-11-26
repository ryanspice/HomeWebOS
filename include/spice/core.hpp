#pragma once
#include "spice/json.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace spice {
  constexpr float DesignWidth=1920,DesignHeight=1080;
  struct Point {
    float x=0,y=0;
  };
  struct Rect {
    float x=0,y=0,w=0,h=0;
    bool contains(Point p)const{
      return p.x>=x&&p.x<x+w&&p.y>=y&&p.y<y+h;
    }
    Point center()const{
      return {
        x+w*.5f,y+h*.5f
      };
    }
  };
  struct Viewport {
    int window_w=1920,window_h=1080,drawable_w=1920,drawable_h=1080;
    float scale=1,left=0,top=0;
    static Viewport make(int ww,int wh,int dw,int dh);
    std::optional<Point> pointer(float x,float y)const;
  };
  struct Node {
    std::string id;
    Rect rect;
  };
  enum class Direction {
    Left,Right,Up,Down
  };
  int neighbor(const std::vector<Node>& nodes,int from,Direction d);
  std::vector<Node> home_nodes(bool expanded);
  float approach(float from,float target,float dt,float speed=16);
  float ease(float t);
  struct Presentation {
    float offset_y=0,opacity=1;
    Rect bounds(Rect r)const{r.y+=offset_y;return r;}
    Point unmap(Point p)const{p.y-=offset_y;return p;}
  };
  Presentation panel_presentation(float progress,bool reduced_motion=false);
  class FocusMotion {
    Rect from_{},target_{},value_{};
    double elapsed_=.14;
    public:
    static constexpr double Duration=.14;
    void snap(Rect target);
    void retarget(Rect target);
    bool update(double dt,bool active,bool reduced_motion);
    Rect value()const{return value_;}
    bool settled()const{return elapsed_>=Duration;}
    float progress()const{return static_cast<float>(elapsed_/Duration);}
  };
  class ArtworkMotion {
    std::string from_="welcome",to_="welcome",pending_;
    double elapsed_=.3;
    public:
    static constexpr double Duration=.3;
    void request(const std::string& target);
    bool update(double dt,bool active,bool reduced_motion);
    const std::string& from()const{return from_;}
    const std::string& to()const{return to_;}
    const std::string& pending()const{return pending_;}
    bool animating()const{return elapsed_<Duration;}
    float progress()const{return static_cast<float>(elapsed_/Duration);}
  };
  // Bounded, deterministic ambient wallpaper motion. This is a concept-only
  // port of the drifting particles, parallax and breathing vignette seen in the
  // PS5 home references: no external assets, decoders or per-frame allocations.
  struct AmbientParticle {
    float x=0,y=0,r=2,vx=0,vy=0,phase=0,alpha=0;
    float cr=1,cg=1,cb=1;
  };
  class AmbientMotion {
    std::vector<AmbientParticle> particles_;
    uint32_t rng_=0x9e3779b9u;
    double time_=0,pulse_=1;
    float px_=0,py_=0,target_x_=0,target_y_=0;
    bool enabled_=true;
    float rand01();
    void respawn(AmbientParticle& p,bool initial);
  public:
    static constexpr size_t MaxParticles=28;
    static constexpr double PulseDuration=.5;
    static constexpr float MaxParallax=14;
    explicit AmbientMotion(bool enabled=true);
    void enabled(bool value){enabled_=value;}
    bool enabled()const{return enabled_;}
    void pulse(){pulse_=0;}
    // Returns true when a visible value changed and a frame should be presented.
    bool advance(double dt,bool active,bool reduced_motion,Point focus);
    double time()const{return time_;}
    const std::vector<AmbientParticle>& particles()const{return particles_;}
    float parallax_x()const{return px_;}
    float parallax_y()const{return py_;}
    float vignette()const{return static_cast<float>(.5+.5*std::sin(time_*.6));}
    float pulse_progress()const{return static_cast<float>(std::clamp(pulse_/PulseDuration,0.,1.));}
  };
  struct FrameSummary {
    double median_ms=0,p95_ms=0,p99_ms=0,rate=0;
    size_t samples=0;
    double max_ms=0;
  };
  class FrameStats {
    std::array<double,240> values_{};
    size_t size_=0,next_=0;
    public:
    void push(double ms);
    void reset(){
      size_=next_=0;
    }
    FrameSummary summary()const;
  };
  bool safe_app_id(const std::string& id);
  bool safe_setting(const std::string& id);
  bool private_ipv4(const std::string& host);
  bool safe_token(const std::string& value);
  void pop_utf8(std::string& value);
  bool native_launch_argument(const std::string& value);
  std::string clean_text(std::string_view value,size_t max=100);
  std::string read_file(const std::filesystem::path& path,size_t limit=131072);
  void atomic_write(const std::filesystem::path& path,const std::string& value);
  std::filesystem::path user_data_dir();
  struct AppEntry {
    std::string id,title,art,kind;
    bool installed=false,sample=false;
    std::string icon_key{},hero_key{},description{},vendor{},version{},observed_at{};
    bool prepared=false;
  };
  std::vector<AppEntry> sample_apps();
  std::vector<AppEntry> fallback_apps();
  std::vector<AppEntry> parse_apps(const Json& reply);
  struct Preferences {
    std::string theme="ps5";
    bool expanded=true,reduced_motion=false,handoff=false,ambient_motion=true;
    int ambient_idle_hz=60;
    // Render target. "max" asks for the largest mode the display offers; the
    // fixed presets exist so an owner can trade sharpness for headroom and
    // because software-rendered QA wants a bounded, deterministic drawable.
    std::string render_resolution="max";
    double dwell=8;
    std::string retropie_host,processor_app_id;
    int retropie_port=8767,retropie_hdmi=1;
    std::vector<std::string> favorites,pins,custom_settings;
    std::map<std::string,std::string> remote_keys,profiles;
    Json extra=Json::object();
    static Preferences from_json(const Json& value);
    Json json()const;
  };
  // Fixed pixel target for a render_resolution preset; false means "max" (the
  // display decides). Unknown strings behave like "max".
  bool resolution_size(const std::string& preset,int& width,int& height);
  constexpr const char* ResolutionPresets[]={
    "max","2160","1440","1080"
  };
  class PreviewState {
    std::string focused_,suppressed_,pending_;
    double elapsed_=0;
    std::map<std::string,double> retry_at_;
    public:
    void focus(const std::string& id);
    void pause(){
      elapsed_=0;
    }
    void suppress(){
      suppressed_=focused_;
      elapsed_=0;
    }
    std::optional<std::string> tick(double dt,double now,bool active,bool covered,const Preferences& prefs);
    void complete(const std::string& id,bool success,double now);
    void retry(double now);
    double elapsed()const{
      return elapsed_;
    }
    bool pending()const{
      return !pending_.empty();
    }
  };
  struct Processor {
    bool available=false;
    double cpu=-1,memory=-1,load1=0,load5=0,load15=0;
    uint64_t total_kib=0,available_kib=0,total_ticks=0,idle_ticks=0;
  };
  Processor parse_processor(const std::string& stat,const std::string& mem,const std::string& load,const Processor* previous=nullptr);
}
