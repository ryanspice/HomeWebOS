#include "spice/ui.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_set>
namespace spice {
  static constexpr Color white{
    .96f,.97f,1,1
  },
  muted{
    .66f,.72f,.82f,1
  },
  aqua{
    .44f,.88f,.78f,1
  };
  static std::string number(double v,int digits=1){
    if(v<0||!std::isfinite(v))return "--";
    std::ostringstream s;
    s<<std::fixed<<std::setprecision(digits)<<v;
    return s.str();
  }
  static std::string clock_text(){
    std::time_t now=std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&now);
#else
    localtime_r(&now,&tm);
#endif
    char buf[32]{};
    std::strftime(buf,sizeof(buf),"%I:%M %p",&tm);
    return buf[0]=='0'?std::string(buf+1):buf;
  }
  static std::string lower(std::string s){
    for(char& c:s)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return s;
  }
  static Color accent_for(const std::string& id){
    if(id=="spice")return Color::hex("#68e0c8");
    if(id=="metro")return Color::hex("#23b6df");
    if(id=="xmb")return Color::hex("#a7daff");
    if(id=="nxe")return Color::hex("#b8eb65");
    if(id=="ps4")return Color::hex("#67b9fa");
    return Color::hex("#a8c5fe");
  }
  Color Deck::accent()const{
    return theme_&&theme_->loaded&&!theme_->accent.empty()?Color::hex(theme_->accent):accent_for(prefs_.theme);
  }
  Deck::Deck(Renderer& renderer,Services& services,std::filesystem::path config,std::filesystem::path assets,bool webos,const PreparedData* prepared,const Theme* theme):renderer_(renderer),services_(services),config_(std::move(config)),assets_(std::move(assets)),webos_(webos),settings_writer_(config_),theme_(theme){
    try{
      if(std::filesystem::exists(config_))prefs_=Preferences::from_json(Json::parse(read_file(config_)));
    }
    catch(const std::exception& e){
      config_writable_=false;
      note(std::string("Settings not loaded; original preserved: ")+e.what());
    }
    ambience_.enabled(prefs_.ambient_motion);
    apps_=webos_?fallback_apps():sample_apps();
    backend_notice_=webos_?"Configured TV shortcuts / not yet verified installed":"DESKTOP DEMO / illustrated app tiles are sample content";
    if(prepared){
      prepared_report_=prepared->report();
      if(prepared->loaded){
        apps_=prepared->apps;
        device_=prepared->device;
        device_snapshot_=prepared->source_kind=="tv";
        backend_notice_=prepared->source_label+" / "+prepared->captured_at+" / refresh explicitly";
      }
    }
    if(webos_){
      for(auto& id:prefs_.favorites)if(std::none_of(apps_.begin(),apps_.end(),[&](auto& a){
        return a.id==id;
      }
      ))apps_.push_back({
        id,id,"library","app"
      }
      );
      selected_=apps_.empty()?"":apps_[0].id;
    }
    inventory_review_.seed(apps_,prepared&&prepared->loaded?prepared->inventory_scope:"");
    last_inventory_notice_=backend_notice_;
    // Load the bounded catalog at startup, before any interactive frame can open it.
    ensure_designs();
    rebuild();
    focus_=std::min(6,static_cast<int>(nodes_.size())-1);
    focus_motion_.snap(nodes_[static_cast<size_t>(focus_)].rect);
    focus_changed();
    if(theme_&&theme_->loaded)artwork_motion_.update(0,true,true);
    if(!webos_)services_.post({
      RequestKind::Processor,"",prefs_
    }
    );
    // Metadata never triggers live calls at startup, even if a bundle is missing.
    // Native processor counters are sampled only while Processor Deck is open.
    note(prepared&&prepared->loaded?"Prepared local data and graphics loaded. Live metadata refresh is explicit.":webos_?"Local shortcuts ready. Read TV information explicitly to refresh.":"Native desktop demo. No TV command will be executed.");
  }
  void Deck::note(const std::string& message){
    // Toasts and the event log are live overlays; they must not invalidate the
    // cached home composition.
    toast_=clean_text(message,140);
    toast_until_=time_+5;
    events_.push_back(toast_);
    while(events_.size()>32)events_.pop_front();
    dirty_=true;
  }
  void Deck::save(){
    if(!config_writable_){note("Original settings file is invalid; move it aside before saving. It has not been overwritten.");return;}
    try{
      save_error_.clear();
      save_sequence_=settings_writer_.submit(prefs_.json().dump(true)+"\n");
      observe_save(settings_writer_.poll(save_sequence_));
      dirty_=true;
    }
    catch(const std::exception& e){
      save_error_=clean_text(e.what(),512);
      note("Settings save failed: "+save_error_);
    }
  }
  std::vector<AppEntry> Deck::visible_apps()const{
    std::vector<AppEntry> out;
    for(auto& a:apps_){
      bool match=query_.empty()||lower(a.title).find(lower(query_))!=std::string::npos||lower(a.id).find(lower(query_))!=std::string::npos;
      if(match&&(view_!="library"||!review_only_||inventory_review_.candidates().contains(a.id)))out.push_back(a);
    }
    return out;
  }
  AppEntry Deck::entry(const std::string& node)const{
    if(node.rfind("rail:",0)==0){
      int i=std::stoi(node.substr(5));
      if(media_=="Media"){
        auto list=fallback_apps();
        for(auto& item:list){auto found=std::find_if(apps_.begin(),apps_.end(),[&](const auto& app){return app.id==item.id&&app.prepared;});if(found!=apps_.end())item=*found;}
        if(!webos_&&!prepared_report_.get("loaded").flag())list.insert(list.begin(),AppEntry{
          "sample.cinema","Cinema","cinema","media",false,true
        }
        );
        return list[static_cast<size_t>(i)%list.size()];
      }
      if(!apps_.empty())return apps_[static_cast<size_t>(i)%apps_.size()];
    }
    if(node.rfind("extra:",0)==0){
      int i=std::stoi(node.substr(6));
      if(i<6&&!apps_.empty()){
        size_t idx=static_cast<size_t>(i)+1;
        if(!webos_&&i==5)idx=7;
        return apps_[idx%apps_.size()];
      }
      if(i==6)return {
        "@library","All apps","library","library"
      };
      if(i==7)return {
        "@inputs","HDMI & TV","aether","input"
      };
      return {
        "@tv","TV settings","welcome","settings"
      };
    }
    return {};
  }
  void Deck::ensure_designs(){
    if(designs_loaded_)return;
    designs_loaded_=true;
    const auto path=assets_/"designs"/"catalog.json";
    if(!designs_.load(path))note(std::string("Design catalog unavailable: ")+designs_.error());
  }
  void Deck::rebuild_designs(std::string preferred){
    rebuild();
    auto find=[&](const std::string& id){
      for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id==id)return static_cast<int>(i);
      return -1;
    };
    int next=find(preferred);
    if(next<0)for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id.rfind("design:",0)==0){
      next=static_cast<int>(i);
      break;
    }
    if(next<0)next=find("design-reset");
    if(next<0)next=find("design-search");
    focus_=next<0?0:next;
    if(!nodes_.empty())focus_motion_.snap(nodes_[static_cast<size_t>(focus_)].rect);
    focus_changed();
  }
  void Deck::rebuild(){
    ++home_content_revision_;
    nodes_.clear();
    if(view_=="home"){
      nodes_=home_nodes(prefs_.expanded);
      if(webos_)std::erase_if(nodes_,[](const auto& n){return n.id=="widget:0";});
      nodes_.push_back({"refresh-items",{1490,984,334,44}});
    }
    else if(view_=="designs"){
      ensure_designs();
      if(design_keyboard_){
        for(size_t i=0;i<sizeof(design_keys_)-1;++i)nodes_.push_back({
          std::string("design-key:")+design_keys_[i],{
            408.f+static_cast<float>(i%10)*112,438.f+static_cast<float>(i/10)*76,100,62
          }
        });
        nodes_.push_back({"design-key:space",{408,770,190,62}});
        nodes_.push_back({"design-delete",{616,770,220,62}});
        nodes_.push_back({"design-key-clear",{854,770,190,62}});
        nodes_.push_back({"design-search-done",{1062,770,450,62}});
      }
      else if(design_inspector_){
        nodes_.push_back({
          "design-back",{
            104,986,230,56
          }
        }
        );
        nodes_.push_back({
          "design-step:prev",{
            1440,932,140,52
          }
        }
        );
        nodes_.push_back({
          "design-step:next",{
            1596,932,140,52
          }
        }
        );
      }
      else{
        nodes_.push_back({
          "close",{
            1648,104,168,56
          }
        }
        );
        nodes_.push_back({
          "design-cat:All",{
            104,216,176,52
          }
        }
        );
        int chip=1;
        for(const auto& category:designs_.categories()){
          nodes_.push_back({
            "design-cat:"+category,{
              104.f+chip*186,216,176,52
            }
          }
          );
          ++chip;
        }
        nodes_.push_back({"design-search",{1420,216,query_.empty()?396.f:274.f,52}});
        if(!query_.empty())nodes_.push_back({"design-clear-search",{1706,216,110,52}});
        const auto list=designs_.filter(design_category_,query_);
        int max_page=list.empty()?0:static_cast<int>((list.size()-1)/15);
        design_page_=std::clamp(design_page_,0,max_page);
        for(size_t i=static_cast<size_t>(design_page_)*15;i<list.size()&&i<static_cast<size_t>(design_page_+1)*15;++i){
          int local=static_cast<int>(i%15);
          nodes_.push_back({
            "design:"+list[i]->key,{
              104.f+(local%5)*345,292.f+(local/5)*218,325,198
            }
          }
          );
        }
        if(list.empty()&&designs_.ok())nodes_.push_back({"design-reset",{728,572,464,60}});
        if(design_page_>0)nodes_.push_back({
          "design-previous",{
            104,986,190,52
          }
        }
        );
        if(design_page_<max_page)nodes_.push_back({
          "design-next",{
            306,986,190,52
          }
        }
        );
      }
    }
    else if(view_=="library"&&library_keyboard_){
      for(size_t i=0;i<sizeof(design_keys_)-1;++i)nodes_.push_back({
        std::string("library-key:")+design_keys_[i],{
          408.f+static_cast<float>(i%10)*112,438.f+static_cast<float>(i/10)*76,100,62
        }
      });
      nodes_.push_back({"library-key:space",{408,770,190,62}});
      nodes_.push_back({"library-delete",{616,770,220,62}});
      nodes_.push_back({"library-clear",{854,770,190,62}});
      nodes_.push_back({"library-search-done",{1062,770,450,62}});
    }
    else if(view_=="control"){
      for(int i=webos_?1:0;i<3;++i)nodes_.push_back({
        "cc-card:"+std::to_string(i),{
          380.f+i*400,586,376,188
        }
      }
      );
      for(int i=0;i<8;++i){if(webos_&&i==3)continue;nodes_.push_back({
        "cc:"+std::to_string(i),{
          382.f+i*146,854,112,90
        }
      }
      );
      }
    }
    else {
      nodes_.push_back({
        "close",{
          1578,134,56,56
        }
      }
      );
      if(view_=="library"||view_=="pins"||view_=="inputs"){
        auto list=view_=="inputs"?fallback_apps():visible_apps();
        if(view_=="pins")list.erase(std::remove_if(list.begin(),list.end(),[&](auto& a){
          return std::find(prefs_.favorites.begin(),prefs_.favorites.end(),a.id)==prefs_.favorites.end();
        }
        ),list.end());
        int max_page=list.empty()?0:static_cast<int>((list.size()-1)/9);
        page_=std::clamp(page_,0,max_page);
        for(size_t i=static_cast<size_t>(page_)*9;i<list.size()&&i<static_cast<size_t>(page_+1)*9;++i){
          int local=static_cast<int>(i%9);
          nodes_.push_back({
            "app:"+list[i].id,{
              322.f+(local%3)*430,332.f+(local/3)*148,402,120
            }
          }
          );
        }
        if(page_>0)nodes_.push_back({
          "previous",{
            322,830,190,56
          }
        }
        );
        if(page_<max_page)nodes_.push_back({
          "next",{
            530,830,190,56
          }
        }
        );
        nodes_.push_back({
          "refresh-apps",{
            1040,830,252,56
          }
        }
        );
        nodes_.push_back({
          "toggle-pin",{
            1310,830,270,56
          }
        }
        );
        if(view_=="library"){
          nodes_.push_back({"review-candidates",{740,830,280,56}});
          nodes_.push_back({"library-search",{1320,218,260,52}});
        }
      }
      else if(view_=="settings"){
        for(int i=0;i<6;++i)nodes_.push_back({
          "pref:"+std::to_string(i),{
            360,320.f+i*80,1190,64
          }
        }
        );
        nodes_.push_back({"designs",{360,820,510,56}});
      }
      else if(view_=="tv"){
        int i=0;
        for(auto* category:{
          "picture","sound","general","accessibility","privacy","network"
        }
        )nodes_.push_back({
          std::string("cat:")+category,{
            330.f+i++*210,290,194,50
          }
        }
        );
        auto settings=settings_.get(category_.c_str()).get("settings").members();
        int start=settings_page_*6;
        for(int n=start;n<static_cast<int>(settings.size())&&n<start+6;++n)nodes_.push_back({
          "pin-setting:"+category_+"."+settings[static_cast<size_t>(n)].first,{
            348,380.f+(n-start)*62,1230,50
          }
        }
        );
        nodes_.push_back({
          "refresh-settings",{
            340,830,260,56
          }
        }
        );
        nodes_.push_back({
          "settings-next",{
            618,830,210,56
          }
        }
        );
        nodes_.push_back({
          "native-settings",{
            1180,830,380,56
          }
        }
        );
      }
      else if(view_=="processor"){
        nodes_.push_back({
          "refresh-cpu",{
            360,820,250,56
          }
        }
        );
        nodes_.push_back({
          "processor-app",{
            1140,820,400,56
          }
        }
        );
      }
      else if(view_=="retro"){
        nodes_.push_back({
          "refresh-retro",{
            360,820,280,56
          }
        }
        );
        nodes_.push_back({
          "retro-hdmi",{
            1160,820,380,56
          }
        }
        );
      }
      else if(view_=="homebrew"){
        nodes_.push_back({
          "homebrew-probe",{
            360,820,300,56
          }
        }
        );
        nodes_.push_back({
          "homebrew-open",{
            1110,820,430,56
          }
        }
        );
      }
      else if(view_=="diagnostics"){
        nodes_.push_back({
          "refresh-device",{
            360,830,300,56
          }
        }
        );
        nodes_.push_back({
          "preview-lab",{
            1190,830,350,56
          }
        }
        );
      }
      else if(view_=="preview"){
        nodes_.push_back({
          "retry-preview",{
            360,830,280,56
          }
        }
        );
        nodes_.push_back({
          "pref:2",{
            1150,830,390,56
          }
        }
        );
      }
      else if(view_=="confirm"){
        nodes_.push_back({
          "cancel",{
            360,740,350,62
          }
        }
        );
        nodes_.push_back({
          "confirm",{
            1180,740,350,62
          }
        }
        );
      }
    }
    focus_=std::clamp(focus_,0,std::max(0,static_cast<int>(nodes_.size())-1));
    sync_library_selection();
  }
  void Deck::sync_library_selection(){
    if(view_!="library"&&view_!="pins"&&view_!="inputs"){selected_identity_.clear();identity_key_.clear();selected_title_.clear();return;}
    auto list=view_=="inputs"?fallback_apps():visible_apps();
    if(view_=="pins")list.erase(std::remove_if(list.begin(),list.end(),[&](const auto& app){return std::find(prefs_.favorites.begin(),prefs_.favorites.end(),app.id)==prefs_.favorites.end();}),list.end());
    const std::string node=nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id;
    if(node.starts_with("app:")){
      auto focused=std::find_if(list.begin(),list.end(),[&](const auto& app){return app.id==node.substr(4);});
      if(focused!=list.end())selected_=focused->id;
    }
    auto chosen=std::find_if(list.begin(),list.end(),[&](const auto& app){return app.id==selected_;});
    if(chosen==list.end()){
      if(list.empty()){selected_.clear();selected_title_.clear();selected_identity_.clear();identity_key_.clear();return;}
      chosen=list.begin();selected_=chosen->id;
    }
    selected_title_=chosen->title;
    auto reason=inventory_review_.candidates().find(selected_);
    const std::string why=reason==inventory_review_.candidates().end()?"":reason->second;
    const std::string key=chosen->id+"\n"+chosen->version+"\n"+why;
    if(key==identity_key_)return;
    identity_key_=key;selected_identity_.clear();
    std::string remaining="ID: "+chosen->id+(chosen->version.empty()?"":" / Version "+chosen->version);
    while(!remaining.empty()&&selected_identity_.size()<3){
      std::string line=remaining;
      while(!line.empty()&&renderer_.width(line,14)>1238)pop_utf8(line);
      if(line.empty())break;
      selected_identity_.push_back(line);remaining.erase(0,line.size());
    }
    if(!why.empty()&&selected_identity_.size()<3)selected_identity_.push_back("Review: "+why);
  }
  void Deck::focus_changed(){
    // Tiles are fixed-size, but the cached hero/refresh still depend on the
    // selection, so a focus move repaints the layer once (not per animation
    // frame). Focus ring and shine remain live.
    // Only Home has a selection-dependent cached hero/refresh; focus moves in
    // panels must not repaint the hidden Home layer behind them.
    if(view_=="home")++home_content_revision_;
    if(nodes_.empty())return;
    focus_motion_.retarget(nodes_[static_cast<size_t>(focus_)].rect);
    dirty_=true;
    if(view_=="home")ambience_.pulse();
    sync_library_selection();
    auto e=entry(nodes_[static_cast<size_t>(focus_)].id);
    if(view_=="home"&&(e.id.empty()||e.id[0]=='@'))preview_.focus("");
    if(view_=="home"&&!e.id.empty()&&e.id[0]!='@'){
      selected_=e.id;
      preview_.focus(e.id);
      artwork_motion_.request(app_artwork_key(e));
    }
  }
  void Deck::open(const std::string& view){
    if(webos_&&view=="processor"){note("Processor Deck is off");return;}
    if(view==view_)return;
    if(view=="home")navigation_.clear();
    else{
      if(navigation_.size()==8)navigation_.erase(navigation_.begin());
      navigation_.push_back({view_,nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id,query_,selected_,category_,page_,settings_page_,review_only_});
    }
    if(view_=="home")home_focus_=focus_;
    if(view=="designs"){
      if(view_!="designs"){
        design_return_view_=view_;
        design_return_focus_=nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id;
      }
      design_page_=0;
      design_inspector_=false;
      design_keyboard_=false;
      design_selected_.clear();
      query_.clear();
      design_category_="All";
      toast_until_=0;
    }
    view_=view;
    library_keyboard_=false;
    query_.clear();
    page_=0;
    settings_page_=0;
    panel_progress_=view=="home"||view=="designs"?1.f:0.f;
    focus_=view=="home"?home_focus_:1;
    rebuild();
    if(webos_&&view=="control")focus_=0;
    if(view=="designs"&&!design_inspector_){
      for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id.rfind("design:",0)==0){
        focus_=static_cast<int>(i);
        break;
      }
    }
    if(!nodes_.empty())focus_motion_.snap(nodes_[static_cast<size_t>(focus_)].rect);
    focus_changed();
    preview_.pause();
    dirty_=true;
    if(view=="processor")services_.post({
      RequestKind::Processor,"",prefs_
    }
    );
  }
  void Deck::launch(const std::string& id){
    if(id.rfind("sample.",0)==0){
      note("Illustrated desktop sample, not an installed game or TV app");
      return;
    }
    if(!services_.post({
      RequestKind::Launch,id,prefs_
    }
    ))note("Launch already queued or service queue busy");
    else note(webos_?"Requesting native app launch...":"Desktop safety: native TV launches are disabled");
  }
  void Deck::activate(std::string id){
    if(id=="library-search"){
      library_search_focus_=nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id;
      library_keyboard_=true;
      rebuild_library("library-key:A");
      return;
    }
    if(id=="library-search-done"){
      library_keyboard_=false;
      rebuild_library();
      return;
    }
    if(id.starts_with("library-key:")){
      text_input(id=="library-key:space"?" ":lower(id.substr(12)));
      return;
    }
    if(id=="library-delete"){backspace();return;}
    if(id=="library-clear"){
      query_.clear();page_=0;rebuild_library(id);return;
    }
    if(id=="games"||id=="media"){
      media_=id=="games"?"Games":"Media";
      focus_=6;
      focus_changed();
      note(media_+" rail selected");
      return;
    }
    if(id=="close"||id=="cancel"){
      action("back");
      return;
    }
    if(id=="design-search"){
      design_keyboard_=true;
      rebuild_designs("design-key:A");
      return;
    }
    if(id=="design-search-done"){
      design_keyboard_=false;
      rebuild_designs();
      return;
    }
    if(id.rfind("design-key:",0)==0){
      text_input(id=="design-key:space"?" ":lower(id.substr(11)));
      return;
    }
    if(id=="design-delete"){
      backspace();
      return;
    }
    if(id=="design-key-clear"||id=="design-clear-search"||id=="design-reset"){
      query_.clear();
      design_page_=0;
      if(id=="design-reset")design_category_="All";
      rebuild_designs(id=="design-key-clear"?id:id=="design-clear-search"?"design-search":"");
      return;
    }
    if(id.rfind("design-cat:",0)==0){
      design_category_=id.substr(11);
      design_page_=0;
      design_inspector_=false;
      rebuild_designs(id);
      return;
    }
    if(id.rfind("design:",0)==0){
      design_selected_=id.substr(7);
      design_inspector_=true;
      panel_progress_=0;
      rebuild_designs("design-back");
      return;
    }
    if(id=="design-previous"||id=="design-next"){
      design_page_=std::max(0,design_page_+(id=="design-next"?1:-1));
      rebuild_designs();
      return;
    }
    if(id=="design-step:prev"||id=="design-step:next"){
      const auto list=designs_.filter(design_category_,query_);
      int index=-1;
      for(size_t i=0;i<list.size();++i)if(list[i]->key==design_selected_){
        index=static_cast<int>(i);
        break;
      }
      if(!list.empty()){
        const int n=static_cast<int>(list.size());
        index=index<0?0:(index+(id=="design-step:next"?1:n-1))%n;
        design_selected_=list[static_cast<size_t>(index)]->key;
        design_page_=index/15;
      }
      rebuild_designs(id);
      return;
    }
    if(id=="design-back"){
      design_inspector_=false;
      panel_progress_=1;
      rebuild_designs("design:"+design_selected_);
      return;
    }
    if(id=="library"||id=="settings"||id=="control"||id=="designs"){
      open(id);
      return;
    }
    if(id=="expand"){
      prefs_.expanded=!prefs_.expanded;
      rebuild();
      save();
      return;
    }
    if(id.rfind("rail:",0)==0||id.rfind("extra:",0)==0){
      auto e=entry(id);
      if(e.id=="@library"||e.kind=="library"){
        open("library");
        return;
      }
      if(e.id=="@inputs"){
        open("inputs");
        return;
      }
      if(e.id=="@tv"){
        open("tv");
        return;
      }
      if(e.kind=="welcome"){
        open("control");
        return;
      }
      launch(e.id);
      return;
    }
    if(id.rfind("widget:",0)==0){
      open(std::array<const char*,3>{
        "processor","retro","homebrew"
      }
      [static_cast<size_t>(std::stoi(id.substr(7)))]);
      return;
    }
    if(id.rfind("card:",0)==0){
      open(std::array<const char*,5>{
        "control","library","inputs","pins","tv"
      }
      [static_cast<size_t>(std::stoi(id.substr(5)))]);
      return;
    }
    if(id.rfind("cc-card:",0)==0){
      open(std::array<const char*,3>{
        "processor","retro","homebrew"
      }
      [static_cast<size_t>(std::stoi(id.substr(8)))]);
      return;
    }
    if(id.rfind("cc:",0)==0){
      int i=std::stoi(id.substr(3));
      if(i==0){
        action("back");
        return;
      }
      if(i==7){
        action("home");
        return;
      }
      open(std::array<const char*,8>{
        "home","library","inputs","processor","retro","settings","diagnostics","home"
      }
      [static_cast<size_t>(i)]);
      return;
    }
    if(id.rfind("app:",0)==0){
      selected_=id.substr(4);
      launch(selected_);
      return;
    }
    if(id=="next"||id=="previous"){
      page_+=id=="next"?1:-1;
      rebuild();
      focus_=std::min(1,static_cast<int>(nodes_.size())-1);
      focus_changed();
      return;
    }
    if(id=="toggle-pin"){
      auto it=std::find(prefs_.favorites.begin(),prefs_.favorites.end(),selected_);
      if(it!=prefs_.favorites.end()){
        prefs_.favorites.erase(it);
        note("Removed selected app from favorites");
      }
      else if(safe_app_id(selected_)&&selected_.rfind("sample.",0)!=0){
        if(prefs_.favorites.size()>=64){note("Favorite limit reached (64)");return;}
        prefs_.favorites.push_back(selected_);
        note("Selected app pinned");
      }
      else{note("Select a real TV shortcut first; sample games cannot be pinned");return;}
      save();
      return;
    }
    if(id=="review-candidates"){
      review_only_=!review_only_;query_.clear();page_=0;
      rebuild();
      for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id==id)focus_=static_cast<int>(i);
      focus_changed();return;
    }
    if(id=="refresh-apps"||id=="refresh-items"){
      if(view_=="inputs"&&id=="refresh-apps"){services_.post({RequestKind::Inputs,"",prefs_});note("Reading input status explicitly");return;}
      if(scan_pending_){note("An app scan is already running");return;}
      query_.clear();review_only_=false;page_=0;
      if(view_!="library")open("library");else rebuild();
      scan_pending_=services_.post({RequestKind::Apps,"scan",prefs_});
      scan_epoch_=interaction_epoch_;
      scan_message_=scan_pending_?"Scanning apps...":"Scan could not be queued";
      backend_notice_=scan_message_+" / previous data: "+last_inventory_notice_;
      note(scan_message_);
      return;
    }
    if(id.rfind("pref:",0)==0){
      int p=std::stoi(id.substr(5));
      if(p==0)prefs_.expanded=!prefs_.expanded;
      if(p==1)prefs_.reduced_motion=!prefs_.reduced_motion;
      if(p==2){
        if(prefs_.handoff){
          prefs_.handoff=false;
          save();
          note("Automatic native handoff disabled");
          return;
        }
        confirmation_="handoff";
        return_view_=view_;
        open("confirm");
        return;
      }
      if(p==3){
        const std::array<std::string,6> t{
          "ps5","spice","metro","xmb","nxe","ps4"
        };
        auto it=std::find(t.begin(),t.end(),prefs_.theme);
        prefs_.theme=t[(static_cast<size_t>(it-t.begin())+1)%t.size()];
      }
      if(p==4){
        open("diagnostics");
        return;
      }
      if(p==5){
        open("tv");
        return;
      }
      save();
      note("Launcher preference updated");
      return;
    }
    if(id=="confirm"){
      if(confirmation_=="exit"){
        quit_=true;
        return;
      }
      if(confirmation_=="handoff"){
        prefs_.handoff=true;
        for(auto& app:fallback_apps())if(app.kind=="tv"||app.kind=="input")prefs_.profiles[app.id]="native_handoff";
        save();
        note("Native TV/HDMI dwell handoff enabled, not inline video");
      }
      confirmation_.clear();
      restore_previous();
      return;
    }
    if(id.rfind("cat:",0)==0){
      category_=id.substr(4);
      settings_page_=0;
      rebuild();
      return;
    }
    if(id=="refresh-settings"){
      services_.post({
        RequestKind::Settings,category_,prefs_
      }
      );
      note("Reading supported keys; TV values are never changed");
      return;
    }
    if(id=="settings-next"){
      auto size=settings_.get(category_.c_str()).get("settings").members().size();
      settings_page_=size?((settings_page_+1)%static_cast<int>((size+5)/6)):0;
      rebuild();
      return;
    }
    if(id.rfind("pin-setting:",0)==0){
      auto key=id.substr(12);
      auto it=std::find(prefs_.pins.begin(),prefs_.pins.end(),key);
      if(it!=prefs_.pins.end())prefs_.pins.erase(it);
      else if(prefs_.pins.size()<8)prefs_.pins.push_back(key);
      else{
        note("Eight pinned setting values maximum");
        return;
      }
      save();
      return;
    }
    if(id=="native-settings"){
      launch("com.palm.app.settings");
      return;
    }
    if(id=="refresh-cpu"){
      services_.post({
        RequestKind::Processor,"manual",prefs_
      }
      );
      return;
    }
    if(id=="processor-app"){
      if(prefs_.processor_app_id.empty())note("Set processor_app_id in the separate C++ settings.json first");
      else launch(prefs_.processor_app_id);
      return;
    }
    if(id=="refresh-retro"){
      services_.post({
        RequestKind::RetroPie,"",prefs_
      }
      );
      note("Requesting companion status over trusted LAN only");
      return;
    }
    if(id=="retro-hdmi"){
      launch("com.webos.app.hdmi"+std::to_string(prefs_.retropie_hdmi));
      return;
    }
    if(id=="homebrew-probe"){
      services_.post({
        RequestKind::Probe,"org.webosbrew.hbchannel",prefs_
      }
      );
      return;
    }
    if(id=="homebrew-open"){
      launch("org.webosbrew.hbchannel");
      return;
    }
    if(id=="refresh-device"){
      services_.post({
        RequestKind::Device,"",prefs_
      }
      );
      return;
    }
    if(id=="preview-lab"){
      open("preview");
      return;
    }
    if(id=="retry-preview"){
      preview_.retry(time_);
      note("Preview cooldown cleared for current selection");
      return;
    }
  }
  void Deck::action(const std::string& name){
    ++interaction_epoch_;
    if(name=="home"||(webos_&&name=="back")){
      preview_.suppress();confirmation_.clear();
      if(view_!="home")open("home");
      dirty_=true;return;
    }
    if(name=="control"&&view_=="control"){restore_previous();return;}
    if(name=="refresh"){activate("refresh-items");return;}
    dirty_=true;
    visible_since_=time_;
    if(name=="left"||name=="right"||name=="up"||name=="down"){
      if(name=="down"&&view_=="library"&&!library_keyboard_&&!nodes_.empty()&&nodes_[static_cast<size_t>(focus_)].id=="library-search"){
        auto target=std::find_if(nodes_.begin(),nodes_.end(),[](const auto& node){return node.id.starts_with("app:");});
        if(target==nodes_.end())target=std::find_if(nodes_.begin(),nodes_.end(),[](const auto& node){return node.id=="refresh-apps";});
        if(target!=nodes_.end()){focus_=static_cast<int>(target-nodes_.begin());focus_changed();return;}
      }
      Direction d=name=="left"?Direction::Left:name=="right"?Direction::Right:name=="up"?Direction::Up:Direction::Down;
      int next=neighbor(nodes_,focus_,d);
      if(next!=focus_){
        focus_=next;
        if(view_!="home"&&nodes_[static_cast<size_t>(focus_)].id.rfind("app:",0)==0)selected_=nodes_[static_cast<size_t>(focus_)].id.substr(4);
        focus_changed();
      }
      return;
    }
    if(name=="ok"){
      if(!nodes_.empty())activate(nodes_[static_cast<size_t>(focus_)].id);
      return;
    }
    if(name=="back"){
      preview_.suppress();
      if(view_=="home"){
        return;
      }
      if(view_=="library"&&library_keyboard_){
        library_keyboard_=false;
        rebuild_library(library_search_focus_);
        return;
      }
      if(view_=="designs"&&design_keyboard_){
        design_keyboard_=false;
        rebuild_designs("design-search");
        return;
      }
      if(view_=="designs"&&design_inspector_){
        activate("design-back");
        return;
      }
      if(view_=="confirm"){
        confirmation_.clear();
      }
      restore_previous();
      return;
    }
    if(name=="expand"){
      prefs_.expanded=!prefs_.expanded;
      if(view_=="home")rebuild();
      save();
      return;
    }
    if(name=="motion"){
      prefs_.reduced_motion=!prefs_.reduced_motion;
      save();
      note(prefs_.reduced_motion?"Reduced motion enabled":"Motion enabled");
      return;
    }
    if(name=="favorite"){
      activate("toggle-pin");
      return;
    }
    if(name=="theme"){
      activate("pref:3");
      return;
    }
    if(name=="control"||name=="library"||name=="settings"||name=="diagnostics"||name=="processor"||name=="retro"||name=="homebrew"||name=="inputs"||name=="tv"||name=="preview"||name=="designs")open(name);
  }
  Presentation Deck::presentation()const{
    if(view_=="home"||(view_=="designs"&&!design_inspector_))return {};
    return panel_presentation(panel_progress_,prefs_.reduced_motion);
  }
  void Deck::pointer(Point p,bool click){
    ++interaction_epoch_;
    p=presentation().unmap(p);
    for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].rect.contains(p)){
      if(static_cast<int>(i)!=focus_){
        focus_=static_cast<int>(i);
        if(nodes_[i].id.rfind("app:",0)==0)selected_=nodes_[i].id.substr(4);
        focus_changed();
      }
      if(click)activate(nodes_[i].id);
      visible_since_=time_;
      if(click)dirty_=true;
      return;
    }
  }
  void Deck::text_input(const std::string& s){
    ++interaction_epoch_;
    if(!accepts_text())return;
    if(query_.size()+s.size()<=64){
      query_+=clean_text(s,64);
      page_=0;
      if(view_=="designs"){
        const auto previous=nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id;
        design_page_=0;
        rebuild_designs(previous);
      }
      else if(view_=="library")rebuild_library(nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id);
      else rebuild();
      dirty_=true;
    }
  }
  void Deck::backspace(){
    ++interaction_epoch_;
    if(view_=="library"&&library_keyboard_&&query_.empty())return;
    if(view_=="designs"&&!design_inspector_&&query_.empty())return;
    if(accepts_text()&&!query_.empty()){
      pop_utf8(query_);
      page_=0;
      design_page_=0;
      if(view_=="designs")rebuild_designs(nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id);
      else if(view_=="library")rebuild_library(nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id);
      else rebuild();
      dirty_=true;
    }
    else action("back");
  }
  std::string Deck::remote_action(const std::string& scan)const{
    auto it=prefs_.remote_keys.find(scan);
    return it==prefs_.remote_keys.end()?"":it->second;
  }
  int Deck::idle_wait_ms()const{
    // SDL wakes immediately on input, so a longer idle timeout costs no input
    // latency and lets ambient_idle_hz control presentation rate in live mode.
    if(!active_||view_!="home"||prefs_.reduced_motion||!prefs_.ambient_motion)return 50;
    return std::clamp(1000/std::max(1,prefs_.ambient_idle_hz),1,200);
  }
  void Deck::active(bool value){
    active_=value;
    preview_.pause();
    if(value){
      visible_since_=time_;
      dirty_=true;
    }
  }
  bool Deck::advance_presentation(double dt){
    bool animate=false;
    if(artwork_motion_.update(dt,active_,prefs_.reduced_motion)){
      // Backdrop is live; an artwork crossfade must not invalidate content.
      if(view_!="designs")animate=true;
    }
    if(active_||prefs_.reduced_motion){
      if(panel_progress_<1){
        panel_progress_=prefs_.reduced_motion?1:std::min(1.f,panel_progress_+static_cast<float>(dt/.2));
        animate=true;
      }
    }
    if(!nodes_.empty()){
      focus_motion_.retarget(nodes_[static_cast<size_t>(focus_)].rect);
      if(focus_motion_.update(dt,active_,prefs_.reduced_motion))animate=true;
    }
    // Ambient wallpaper is drawn outside the cached home layer, so it needs
    // its own presentation frames. Reduced motion or the opt-out freezes it.
    Point focus{960.f,540.f};
    if(!nodes_.empty())focus=nodes_[static_cast<size_t>(focus_)].rect.center();
    if(ambience_.advance(dt,active_,prefs_.reduced_motion||!prefs_.ambient_motion,focus)&&view_=="home"){
      // Ambient-only frames can be rate-limited independently of interaction.
      // Default 60 means no visible change; the owner may set 30 for OLED power.
      const double interval=1.0/static_cast<double>(std::max(1,prefs_.ambient_idle_hz));
      if(last_ambient_frame_<0||time_-last_ambient_frame_>=interval-.0001){
        last_ambient_frame_=time_;
        animate=true;
      }
    }
    if(animate)dirty_=true;
    return animate;
  }
  void Deck::release_caches(){
    // Called between frames only. Text and the Home layer are rebuilt from the
    // same source as the first frame; nothing user-visible changes.
    renderer_.release_caches();
    ++home_content_revision_;
    dirty_=true;
  }
  bool Deck::update(double dt,bool advance_tracks){
    time_+=dt;
    if(active_&&time_>=next_clock_){
      next_clock_=time_+1;
      const auto clock=clock_text();
      // The clock is painted live now; changing the minute only requests a
      // redraw instead of clearing and repainting the whole cached Home layer.
      if(clock!=home_clock_){home_clock_=clock;dirty_=true;}
    }
    const bool animate=advance_presentation(advance_tracks?dt:0);
    if(save_sequence_)observe_save(settings_writer_.poll(save_sequence_));
    for(auto& r:services_.drain()){
      // Only the app inventory is baked into the cached home layer. Widgets,
      // panels and toasts are painted live, so a device/input/settings/processor
      // reply must not spend a full-drawable home repaint.
      if(r.kind==RequestKind::Apps)++home_content_revision_;
      dirty_=true;
      if(r.kind==RequestKind::Apps){
        scan_pending_=false;
        if(!r.ok){scan_message_="Scan unavailable: "+r.message;backend_notice_=scan_message_+" / previous data: "+last_inventory_notice_;}
      }
      if(r.kind!=RequestKind::Processor||!r.ok||r.id=="manual")note(r.message);
      if(r.kind==RequestKind::Launch)preview_.complete(r.id,r.ok,time_);
      if(!r.ok)continue;
      switch(r.kind){
        case RequestKind::Processor:processor_=r.processor;
        if(processor_.cpu>=0){
          cpu_history_.push_back(processor_.cpu);
          while(cpu_history_.size()>60)cpu_history_.pop_front();
        }
        break;
        case RequestKind::Device:device_=r.data;device_snapshot_=false;
        break;
        case RequestKind::Inputs:inputs_=r.data;
        break;
        case RequestKind::RetroPie:retro_=r.data;
        break;
        case RequestKind::Settings:if(!settings_.object_type())settings_=Json::object();
        settings_.set(r.id.c_str(),r.data);
        if(view_=="tv")rebuild();
        break;
        case RequestKind::Apps:{
          const std::string previous_focus=nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id;
          auto apps=parse_apps(r.data);
          if(r.data.get("apps").is(json_type_array)||r.data.get("launchPoints").is(json_type_array)){
            for(auto& app:apps){
              auto old=std::find_if(apps_.begin(),apps_.end(),[&](const auto& before){return before.id==app.id&&before.prepared;});
              if(old!=apps_.end()){
                app.icon_key=old->icon_key;app.hero_key=old->hero_key;
                if(app.description.empty())app.description=old->description;
                if(app.vendor.empty())app.vendor=old->vendor;
                if(app.version.empty())app.version=old->version;
                if(app.observed_at.empty())app.observed_at=old->observed_at;
                app.prepared=true;
              }
            }
            apps_=apps;
            inventory_review_.scan(apps_,r.data.get("inventory_scope").str("unknown"));
            scan_source_=r.data.get("source_label").str("Explicit inventory");
            scan_message_=std::to_string(apps_.size())+" apps / "+std::to_string(inventory_review_.candidates().size())+" build candidates";
            scan_message_+=inventory_review_.compared()?" / "+std::to_string(inventory_review_.new_ids().size())+" new":" / baseline established";
            backend_notice_=scan_source_+" ("+inventory_review_.scope()+") / "+scan_message_;
            last_inventory_notice_=backend_notice_;
            if(view_=="library"&&query_.empty()&&interaction_epoch_==scan_epoch_){
              auto list=visible_apps();
              size_t index=0;
              auto fresh=std::find_if(list.begin(),list.end(),[&](const auto& app){return inventory_review_.is_new(app.id);});
              if(fresh!=list.end())index=static_cast<size_t>(fresh-list.begin());
              else{auto candidate=std::find_if(list.begin(),list.end(),[&](const auto& app){return inventory_review_.candidates().contains(app.id);});if(candidate!=list.end())index=static_cast<size_t>(candidate-list.begin());}
              page_=static_cast<int>(index/9);
              rebuild();
              if(index<list.size())for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id=="app:"+list[index].id)focus_=static_cast<int>(i);
              focus_changed();
            }else{
              if(view_=="library"&&previous_focus.starts_with("app:")){
                auto list=visible_apps();auto same=std::find_if(list.begin(),list.end(),[&](const auto& app){return app.id==previous_focus.substr(4);});
                if(same!=list.end())page_=static_cast<int>((same-list.begin())/9);
              }
              rebuild();
              for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id==previous_focus)focus_=static_cast<int>(i);
              focus_changed();
            }
            note(scan_message_);
          }
          else{scan_message_="Inventory response had no app list; previous entries retained";backend_notice_=scan_message_+" / "+last_inventory_notice_;note(scan_message_);}
          break;
        }
        case RequestKind::Probe:note(r.data.get("exist").flag()?"Homebrew reports installed":"Availability probe returned no confirmed install");
        break;
        case RequestKind::Launch:break;
      }
    }
    if(!webos_&&active_&&(view_=="processor"||view_=="home")&&time_>=next_cpu_){
      services_.post({
        RequestKind::Processor,"",prefs_
      }
      );
      next_cpu_=time_+2;
    }
    const double dwell_dt=interaction_epoch_==dwell_epoch_?dt:0;
    dwell_epoch_=interaction_epoch_;
    if(auto id=preview_.tick(dwell_dt,time_,active_,view_!="home",prefs_)){
      if(services_.post({
        RequestKind::Launch,*id,prefs_
      }
      ))note("Dwell threshold reached: requesting native input handoff");
      else preview_.complete(*id,false,time_);
    }
    if(toast_until_>0&&time_>toast_until_){
      toast_until_=0;
      dirty_=true;
    }
    bool draw=dirty_||animate;
    dirty_=false;
    motion_redraw_=draw&&active_;
    return motion_redraw_;
  }
  void Deck::label(const std::string& s,float x,float y,float size,Color c){
    renderer_.text(s,x,y,size,c);
  }
  void Deck::draw_focus(Rect r,float opacity){
    Color c=accent();
    // The activation target is immediate even while the decorative surround travels.
    if(!nodes_.empty()){
      const auto target=nodes_[static_cast<size_t>(focus_)].rect;
      if(std::abs(target.x-r.x)+std::abs(target.y-r.y)+std::abs(target.w-r.w)+std::abs(target.h-r.h)>.5f){
        renderer_.rect(target,c.opacity(.60f*opacity),12,1.5f);
        renderer_.rect({target.x+target.w*.5f-16,target.y+target.h-3,32,3},c.opacity(opacity),1.5f);
      }
    }
    r={
      r.x-5,r.y-5,r.w+10,r.h+10
    };
    renderer_.rect({
      r.x-4,r.y-4,r.w+8,r.h+8
    },
    c.opacity(.11f*opacity),18);
    // Focus "shine": two accent rings expand and fade after a move, echoing
    // the reference home-screen button pulse without adding a texture or shader.
    if(prefs_.ambient_motion&&!prefs_.reduced_motion&&!nodes_.empty()){
      const float progress=ambience_.pulse_progress();
      const auto target=nodes_[static_cast<size_t>(focus_)].rect;
      for(int i=0;i<2;++i){
        const float p=std::clamp((progress-float(i)*.22f)/.78f,0.f,1.f);
        if(p<=0||p>=1)continue;
        const float grow=ease(p)*20.f;
        renderer_.rect({
          target.x-5-grow,target.y-5-grow,target.w+10+grow*2,target.h+10+grow*2
        },
        c.opacity(.26f*(1-p)*opacity),18,1.5f);
      }
    }
    renderer_.rect(r,white.opacity(.94f*opacity),14,2.5f);
  }
  void Deck::button(const Node& n,const std::string& text,const std::string& icon,bool on,const std::string& slot){
    auto r=n.rect;
    bool selected=!nodes_.empty()&&nodes_[static_cast<size_t>(focus_)].id==n.id;
    renderer_.gradient(r,selected?Color{
      .29f,.37f,.51f,.95f
    }
    :Color{
      .12f,.16f,.24f,.88f
    },
    {
      .08f,.11f,.18f,.92f
    },
    10);
    renderer_.rect(r,{1,1,1,selected?.18f:.07f},10,1);
    if(on)renderer_.rect({
      r.x+4,r.y+12,3,r.h-24
    },
    aqua,1);
    float x=r.x+20;
    if(!icon.empty()){
      glyph(slot,icon,{
        x,r.y+(r.h-28)*.5f,28,28
      },
      white);
      x+=43;
    }
    label(renderer_.ellipsis(text,21,r.w-(x-r.x)-18),x,r.y+(r.h-23)*.5f,21,white);
  }
  void Deck::draw_ambience(){
    auto& r=renderer_;
    const float px=ambience_.parallax_x(),py=ambience_.parallax_y();
    const float breath=ambience_.vignette();
    const Color c=accent();
    // Two broad low-alpha fields drift against the focus direction to suggest
    // depth over the cached backdrop, as in the referenced PS5 wallpapers.
    r.gradient({-160+px*.9f,-120+py*.9f,1240,700},c.opacity(.07f),c.opacity(0.f),120);
    r.gradient({840-px*.9f,560-py*.9f,1240,700},Color{.12f,.36f,.8f,.07f},Color{.12f,.36f,.8f,0.f},120);
    // A breathing vignette adds depth at 4K DPI; kept faint over text and icons.
    const float v=.04f+.05f*breath;
    r.gradient({0,0,1920,160},{0,0,0,v},{0,0,0,0});
    r.gradient({0,920,1920,160},{0,0,0,0},{0,0,0,v});
    r.gradient({0,0,160,1080},{0,0,0,v*.7f},{0,0,0,0});
    r.gradient({1760,0,160,1080},{0,0,0,0},{0,0,0,v*.7f});
    for(const auto& p:ambience_.particles()){
      if(p.alpha<=.01f)continue;
      r.rect({p.x+px*.4f,p.y+py*.4f,p.r,p.r},Color{p.cr,p.cg,p.cb,p.alpha},p.r*.5f);
    }
  }
  static bool widget_motion_active(const Preferences& prefs,bool home){
    // The same gate the ambient layer and the focus shine use: reduced motion,
    // the "ambient_motion" opt-out and every non-home view keep the exact
    // neutral pose instead of a slower or hidden animation.
    return prefs.ambient_motion&&!prefs.reduced_motion&&home;
  }
  // The right-column widget cards draw from the shared home_nodes rectangles so
  // tiles, widgets and focus targets cannot drift apart geometrically. The
  // already-built node list is reused so this allocates nothing per frame.
  std::array<Rect,3> Deck::widget_rects()const{
    std::array<Rect,3> out{{
      {1340,354,484,108},{
        1340,484,484,108
      },{
        1340,614,484,108
      }
    }
    };
    for(const auto& n:nodes_){
      if(n.id.rfind("widget:",0)!=0)continue;
      const int index=std::stoi(n.id.substr(7));
      if(index>=0&&index<static_cast<int>(out.size()))out[static_cast<size_t>(index)]=n.rect;
    }
    return out;
  }
  // Bounded, deterministic breathing phase shared by every widget card. It is
  // derived from the fixed-step AmbientMotion clock, never from wall time.
  float Deck::widget_breath(int index)const{
    if(!widget_motion_active(prefs_,view_=="home"))return 0;
    return .5f+.5f*std::sin(static_cast<float>(ambience_.time())*.6f+static_cast<float>(index)*.9f);
  }
  // Widgets ride the same capped focus parallax as the ambient wallpaper, at a
  // shallower depth so a card never leaves its shared focus rectangle.
  Rect Deck::widget_pose(const Rect& base,int index)const{
    if(!widget_motion_active(prefs_,view_=="home"))return base;
    const float depth=.2f*(.75f+.25f*widget_breath(index));
    return {
      base.x+ambience_.parallax_x()*depth,base.y+ambience_.parallax_y()*depth,base.w,base.h
    };
  }
  void Deck::draw_home_widgets(){
    auto& r=renderer_;
    const auto rects=widget_rects();
    const bool motion=widget_motion_active(prefs_,view_=="home");
    // Static once: the widget cards are redrawn on every animated frame, so they
    // must not allocate titles or icon keys per frame.
    static const std::array<std::string,3> wtitle{
      "Processor Deck","RetroPie Link","Homebrew Channel"
    },
    wicon{
      "cpu","retropie","homebrew"
    };
    for(int i=0;i<3;++i){
      if(webos_&&i==0)continue;
      const auto index=static_cast<size_t>(i);
      const auto box=widget_pose(rects[index],i);
      r.gradient(box,{
        .11f,.16f,.26f,.94f
      },
      {
        .025f,.045f,.09f,.96f
      },
      14);
      if(motion){
        // The ambient wash that already drifts over the tiles, applied per card
        // and kept far below the text contrast.
        const float breath=widget_breath(i);
        r.gradient({
          box.x,box.y,box.w,box.h*.55f
        },
        accent().opacity(.035f+.03f*breath),accent().opacity(0.f),14);
      }
      r.rect({box.x+1,box.y+20,3,box.h-40},accent().opacity(.65f),1);
      r.rect(box,{
        1,1,1,.11f
      },
      14,1);
      if(motion)r.rect(box,accent().opacity(.05f+.05f*widget_breath(i)),14,1);
      r.icon(wicon[index],{
        box.x+24,box.y+25,33,33
      },
      accent());
      label(wtitle[index],box.x+76,box.y+21,21,white);
      std::string value=i==0?"CPU "+number(processor_.cpu,0)+"%   RAM "+number(processor_.memory,0)+"%":i==1?(retro_.empty()?"Pair a trusted LAN companion":"Connected / "+clean_text(retro_.get("hostname").str(),25)):"Open channel to manage updates";
      if(i==0&&webos_)value=processor_.available?"Last "+value:"Open live CPU / memory counters";
      label(value,box.x+76,box.y+58,17,muted);
    }
  }
  void Deck::draw_home_backdrop(){
    auto& r=renderer_;
    auto backdrop_key=[](const std::string& key){return key.starts_with("theme/")?key:"hero/"+key;};
    r.backdrop(backdrop_key(artwork_motion_.from()),backdrop_key(artwork_motion_.to()),
               artwork_motion_.animating()?ease(artwork_motion_.progress()):0);
  }
  void Deck::draw_home_content(){
    auto& r=renderer_;
    // Guide rules organize the canvas without changing shared node rectangles
    // or scheduling any additional frames.
    r.line(96,111,1824,111,{.72f,.82f,1.f,.13f},1);
    r.line(96,790,1500,790,{.72f,.82f,1.f,.14f},1);
    if(media_=="Games")r.rect({96,98,92,2},accent().opacity(.85f),1);
    if(media_=="Media")r.rect({242,98,79,2},accent().opacity(.85f),1);
    label("Games",96,55,28,media_=="Games"?white:muted);
    label("Media",242,55,28,media_=="Media"?white:muted);
    glyph("navigation.search","search",{
      1467,54,30,30
    },
    white);
    r.icon("layers",{
      1538,54,29,29
    },
    white);
    glyph("actions.settings","settings",{
      1608,54,29,29
    },
    white);
    glyph("navigation.control-center","controls",{
      1678,54,29,29
    },
    white);
    for(auto& n:home_nodes(prefs_.expanded)){
      if(n.id.rfind("rail:",0)!=0&&n.id.rfind("extra:",0)!=0)continue;
      auto e=entry(n.id);
      if(e.id.empty())continue;
      // Fixed-size tiles: selection emphasis is the live focus ring/shine,
      // so a focus change never dirties this cached content (owner decision).
      auto box=n.rect;
      const auto icon_key=e.sample&&e.kind=="library"?std::string():app_icon_key(e);
      if(!icon_key.empty())r.image(icon_key,box,1,11);
      else {
        r.gradient(box,{
          .16f,.24f,.4f,.95f
        },
        {
          .07f,.11f,.21f,.98f
        },
        11);
        glyph(e.kind=="library"?"library.collection":"",e.kind,{
          box.x+31,box.y+29,48,48
        },
        white);
      }
      r.rect(box,{
        1,1,1,.17f
      },
      11,1);
      if(n.id.rfind("extra:",0)==0)label(r.ellipsis(e.title,17,120),box.x,box.y+box.h+10,17,{
        .77f,.81f,.89f,1
      }
      );
    }
    if(prefs_.expanded)label("EXPLORE MORE   4  /  3  /  2",486,310,13,{
      .68f,.76f,.9f,1
    }
    );
    const std::array<std::string,5> titles{
      "Control Center","Your library","HDMI & TV","Pinned apps","Your TV"
    },
    icons{
      "controls","library","input","star","settings"
    },
    descs{
      "Your active home controls",std::to_string(apps_.size())+" apps and shortcuts","Native source handoff",std::to_string(prefs_.favorites.size())+" saved app shortcuts","Read-only LG settings"
    },
    foot{
      "Open controls","Browse and select","Inline video not available","Your preferences","No silent TV changes"
    };
    for(int i=0;i<5;++i){
      Rect box{
        96.f+i*284,832,270,140
      };
      r.gradient(box,{
        .105f,.14f,.21f,.97f
      },
      {
        .035f,.05f,.085f,.98f
      },
      12);
      r.line(box.x+18,box.y+90,box.x+box.w-18,box.y+90,{.72f,.82f,1.f,.12f},1);
      r.rect(box,{
        1,1,1,.14f
      },
      12,1);
      glyph(i==0?"navigation.control-center":i==1?"library.collection":"",icons[static_cast<size_t>(i)],{
        box.x+18,box.y+19,22,22
      },
      white);
      label(titles[static_cast<size_t>(i)],box.x+51,box.y+20,18,white);
      label(descs[static_cast<size_t>(i)],box.x+18,box.y+55,16,muted);
      label(foot[static_cast<size_t>(i)],box.x+18,box.y+110,13,{
        .53f,.62f,.76f,1
      }
      );
    }
    r.rect({
      96,1020,30,30
    },
    {
      1,1,1,.92f
    },
    15,1.4f);
    label("OK",100,1028,12,white);
    label("Select",140,1027,18,white);
    r.icon("back",{
      238,1021,26,26
    },
    white);
    label("Back",277,1027,18,white);
    r.icon("home",{
      364,1021,26,26
    },
    white);
    label("Control Center",405,1027,18,white);
    if(view_!="home"){
    label(webos_?"C++ NATIVE / TV LAB":"C++ NATIVE / DESKTOP DEMO",1550,1006,12,{
      .62f,.73f,.88f,1
    }
    );
    label("No inline TV texture",1550,1032,14,muted);
    }
    // Hero and Refresh are selection/view dependent but cheap to cache. Keeping
    // them in the layer meets the reduced-motion <=60 draws/frame gate; a focus
    // move repaints the layer at most once (still within the P1 acceptance).
    draw_home_hero();
    draw_home_refresh();
  }
  void Deck::draw_home_hero(){
    auto& r=renderer_;
    AppEntry current;
    for(auto& a:apps_)if(a.id==selected_)current=a;
    if(current.id.empty()){
      for(auto& app:fallback_apps())if(app.id==selected_)current=app;
    }
    if(current.id.empty()&&!apps_.empty())current=apps_[0];
    label(current.title.empty()?"Welcome":current.title,96,286,23,white);
    r.rect({98,344,42,3},accent().opacity(.9f),1);
    bool welcome=current.kind=="welcome"||current.id.empty();
    label(welcome?"Welcome back.":r.ellipsis(current.title,40,340),96,373,40,white);
    std::string primary_info,secondary_info;
    if(current.prepared){
      primary_info=current.description.empty()?(current.vendor.empty()?"No description provided":current.vendor):current.description;
      if(!current.description.empty())secondary_info=current.vendor;
      if(!current.version.empty()){
        if(!secondary_info.empty())secondary_info+=" / ";
        secondary_info+="Version "+current.version;
      }
    }
    label(welcome?"Your home. Your hardware.":current.prepared?r.ellipsis(primary_info,21,355):current.sample?"Original concept artwork.":"A native TV shortcut.",98,436,21,muted);
    label(welcome?"Ready when you are.":current.prepared?r.ellipsis(secondary_info,18,355):current.sample?"No game is installed here.":current.installed?"Discovered on this device.":"Installation not yet verified.",98,466,current.prepared?18.f:21.f,muted);
    if(current.prepared)label("PREPARED SNAPSHOT",98,496,12,accent());
    r.rect({98,529,178,38},{1,1,1,.07f},18);
    label("NATIVE SYSTEM DECK",110,540,12,{.7f,.8f,.96f,1});
  }
  void Deck::draw_home_refresh(){
    if(view_!="home")return;
    auto refresh=std::find_if(nodes_.begin(),nodes_.end(),[](const auto& node){return node.id=="refresh-items";});
    if(refresh!=nodes_.end())button(*refresh,scan_pending_?"Scanning...":"Refresh apps / R","refresh",false,"actions.refresh");
  }
  void Deck::draw_home(){
    // Direct, fully painted scene for non-Home callers and the cache fallback.
    draw_home_backdrop();
    draw_home_content();
    draw_home_widgets();
  }
  void Deck::draw_control(){
    auto& r=renderer_;
    auto pose=presentation();
    r.gradient({
      0,0,1920,1080
    },
    {
      .01f,.02f,.04f,.28f*pose.opacity
    },
    {
      .01f,.02f,.04f,.97f*pose.opacity
    }
    );
    r.transform(0,pose.offset_y,pose.opacity);
    label("CONTROL CENTER / SYSTEM DECK",380,498,13,accent());
    label("Control Center",380,524,29,white);
    for(int i=webos_?1:0;i<3;++i){
      Rect box{
        380.f+i*400,586,376,188
      };
      r.gradient(box,{
        .13f,.18f,.27f,.97f
      },
      {
        .06f,.085f,.14f,.99f
      },
      16);
      r.rect(box,{
        1,1,1,.14f
      },
      16,1);
      r.line(box.x+22,box.y+126,box.x+box.w-22,box.y+126,{.72f,.82f,1.f,.13f},1);
      r.icon(std::array<std::string,3>{
        "cpu","retropie","homebrew"
      }
      [static_cast<size_t>(i)],{
        box.x+22,box.y+22,33,33
      },
      accent());
      label(std::array<std::string,3>{
        "Processor Deck","RetroPie","Homebrew"
      }
      [static_cast<size_t>(i)],box.x+72,box.y+28,24,white);
      const auto cpu=webos_&&!processor_.available?std::string("Open live CPU / RAM counters"):(webos_?"Last CPU ":"CPU ")+number(processor_.cpu)+"%   RAM "+number(processor_.memory)+"%";
      label(i==0?cpu:i==1?(retro_.empty()?"Not connected":"Companion online"):"Updates stay in Homebrew Channel",box.x+22,box.y+91,18,muted);
      label(i==0?(webos_?"Read-only TV counters":"Actual desktop counters, not TV"):i==1?"Pairing and HDMI shortcut":"No unattended installs",box.x+22,box.y+144,15,muted);
    }
    r.rect({
      352,826,1216,146
    },
    {
      .045f,.06f,.09f,.98f
    },
    30);
    std::array<std::string,8> icons{
      "home","library","input","cpu","retropie","settings","display","back"
    };
    const bool returning_home=webos_||navigation_.empty()||navigation_.back().view=="home";
    if(!returning_home)icons[0]="back";
    for(int i=0;i<8;++i){
      if(webos_&&i==3)continue;
      glyph(i==0&&returning_home?"navigation.home":i==1?"library.collection":i==5?"actions.settings":"",icons[static_cast<size_t>(i)],{
        410.f+i*146,878,44,44
      },
      white);
    }
    if(!nodes_.empty()){
      draw_focus(focus_motion_.value());
      std::string title=nodes_[static_cast<size_t>(focus_)].id.rfind("cc:",0)==0?std::array<std::string,8>{
        returning_home?"Home":"Return","Apps","Inputs","Processor","RetroPie","Settings","Display lab","Back to Home"
      }
      [static_cast<size_t>(std::stoi(nodes_[static_cast<size_t>(focus_)].id.substr(3)))]:"Open system panel";
      label(title,827,985,21,white);
    }
    r.transform();
  }
  void Deck::draw_panel(){
    if(view_=="library"&&library_keyboard_){draw_library_search();return;}
    auto& r=renderer_;
    r.rect({
      0,0,1920,1080
    },
    {
      0,.005f,.02f,.75f*ease(panel_progress_)
    }
    );
    auto pose=presentation();
    r.transform(0,pose.offset_y,pose.opacity);
    Rect panel{
      286,112,1360,824
    };
    r.gradient(panel,{
      .09f,.13f,.21f,.99f
    },
    {
      .025f,.04f,.075f,1
    },
    24);
    r.rect(panel,{
      1,1,1,.12f
    },
    24,1);
    const std::map<std::string,std::string> names{
      {
        "library","Your app library"
      },
      {
        "pins","Pinned apps"
      },
      {
        "inputs","HDMI & Live TV"
      },
      {
        "processor","Processor Deck"
      },
      {
        "retro","RetroPie Link"
      },
      {
        "homebrew","Homebrew Channel"
      },
      {
        "settings","Make it your home"
      },
      {
        "tv","LG TV settings"
      },
      {
        "diagnostics","Display & native runtime"
      },
      {
        "preview","Preview Lab"
      },
      {
        "confirm","Confirm action"
      }
    };
    r.rect({330,133,46,3},accent().opacity(.9f),1);
    label(names.at(view_),330,159,38,white);
    r.icon("back",{
      1590,147,30,30
    },
    white);
    if(view_=="library"||view_=="pins"||view_=="inputs"){
      label(r.ellipsis(view_=="library"?(review_only_?"Candidates matched by app ID or Home/Start/launcher name.":query_.empty()?"Search with the controller or type to filter.":"Search: "+query_):view_=="inputs"?"Native source apps, not inline tuner or HDMI video.":"Favorites stay in the C++ edition's separate settings file.",20,view_=="library"?940:1250),330,228,20,muted);
      label(r.ellipsis(backend_notice_,15,1258),330,270,15,accent());
      auto list=view_=="inputs"?fallback_apps():visible_apps();
      for(auto& n:nodes_)if(n.id.rfind("app:",0)==0){
        auto it=std::find_if(list.begin(),list.end(),[&](auto& a){
          return a.id==n.id.substr(4);
        }
        );
        if(it==list.end())continue;
        r.gradient(n.rect,{
          .13f,.18f,.28f,1
        },
        {
          .075f,.1f,.16f,1
        },
        12);
        const auto icon_key=app_icon_key(*it);
        if(!icon_key.empty())r.image(icon_key,{
          n.rect.x+12,n.rect.y+14,90,90
        },
        1,8);
        else r.icon(it->kind,{
          n.rect.x+31,n.rect.y+35,46,46
        },
        accent());
        label(r.ellipsis(it->title,23,280),n.rect.x+121,n.rect.y+27,23,white);
        const bool fresh=inventory_review_.is_new(it->id),candidate=inventory_review_.candidates().contains(it->id);
        const Color badge=candidate?Color{1,.77f,.4f,1}:fresh?aqua:muted;
        if(candidate)r.rect(n.rect,badge.opacity(.6f),12,1);
        if(it->id==selected_)r.rect({n.rect.x+121,n.rect.y+n.rect.h-5,76,3},accent(),1.5f);
        if(fresh)r.rect({n.rect.x+4,n.rect.y+18,3,n.rect.h-36},aqua,1);
        std::string status=fresh?(candidate?"NEW / BUILD CANDIDATE":"NEW ITEM"):candidate?"BUILD CANDIDATE":it->prepared?"PREPARED SNAPSHOT":it->sample?"ILLUSTRATED SAMPLE":it->installed?"INSTALLED / DISCOVERED":"UNVERIFIED SHORTCUT";
        label(status,n.rect.x+122,n.rect.y+67,12,badge);
        if(!it->sample)label(r.ellipsis(it->version.empty()?it->id:it->id+" / "+it->version,11,263),n.rect.x+122,n.rect.y+91,11,muted);
      }
      if(std::none_of(nodes_.begin(),nodes_.end(),[](const auto& node){return node.id.starts_with("app:");}))label("No matching apps. Change the search or refresh.",360,410,24,muted);
      for(auto& n:nodes_){
        if(n.id=="library-search")button(n,query_.empty()?"Search apps":"Edit search","search",false,"navigation.search");
        if(n.id=="previous")button(n,"Previous");
        if(n.id=="next")button(n,"Next");
        if(n.id=="refresh-apps")button(n,scan_pending_?"Scanning...":"Refresh","refresh",false,"actions.refresh");
        if(n.id=="review-candidates")button(n,review_only_?"Show all apps":"Review builds ("+std::to_string(inventory_review_.candidates().size())+")","layers");
        if(n.id=="toggle-pin")button(n,selected_title_.empty()?"Select an app":(std::find(prefs_.favorites.begin(),prefs_.favorites.end(),selected_)!=prefs_.favorites.end()?"Unpin ":"Pin ")+selected_title_,"star");
      }
      for(size_t i=0;i<selected_identity_.size();++i)label(selected_identity_[i],322,764.f+static_cast<float>(i)*20,14,i+1==selected_identity_.size()?muted:white);
      auto visible=list;
      if(view_=="pins")visible.erase(std::remove_if(visible.begin(),visible.end(),[&](const auto& app){return std::find(prefs_.favorites.begin(),prefs_.favorites.end(),app.id)==prefs_.favorites.end();}),visible.end());
      const auto pages=std::max<size_t>(1,(visible.size()+8)/9);
      label(std::to_string(visible.size())+" items / Page "+std::to_string(page_+1)+" of "+std::to_string(pages)+" / Back returns to your previous view",322,910,16,muted);
    }
    else if(view_=="settings"){
      label("Same System Deck layout. Separate configuration. Nothing replaces your existing launcher.",330,231,20,muted);
      std::array<std::string,6> text{
        "Expanded 4 / 3 / 2 icon staircase    "+std::string(prefs_.expanded?"ON":"OFF"),"Reduced motion / disables PS5 ambience    "+std::string(prefs_.reduced_motion?"ON":"OFF"),"Native input handoff after "+number(prefs_.dwell,0)+" seconds    "+(prefs_.handoff?"ON":"OFF"),"Base color palette    "+prefs_.theme+(theme_&&theme_->loaded&&!theme_->accent.empty()?"  / local theme accent active":""),"Display diagnostics and frame intervals","Import supported LG TV settings (read only)"
      };
      for(auto& n:nodes_)if(n.id.rfind("pref:",0)==0){
        auto i=static_cast<size_t>(std::stoi(n.id.substr(5)));
        button(n,text[i],i==0?"layers":i==4?"display":"settings");
      }
      for(auto& n:nodes_)if(n.id=="designs")button(n,"Browse 50 design concepts","layers");
      label("Fictional concepts / presentation only",920,838,18,muted);
      if(theme_&&theme_->loaded)label(r.ellipsis("Local theme: "+theme_->name+" / "+theme_->version+" / changes apply on relaunch",15,1190),360,896,15,accent());
      else if(theme_&&!theme_->error.empty())label(r.ellipsis(theme_->error,15,1190),360,896,15,Color{1,.76f,.4f,1});
      else label("RetroPie address, processor app and remote mappings: settings.json; secrets stay in the session environment.",360,896,15,muted);
      label(r.ellipsis("Settings: "+settings_state()+(save_ack_.error.empty()?save_error_.empty()?"":" / "+save_error_:" / "+save_ack_.error),14,1190),360,918,14,settings_state()=="failed"?Color{1,.76f,.4f,1}:muted);
    }
    else if(view_=="processor"){
      label(webos_?"Read-only TV telemetry. First CPU sample needs a second reading.":"ACTUAL DESKTOP HOST COUNTERS / these are not readings from your LG TV.",330,231,19,accent());
      label("CPU UTILIZATION",360,310,15,muted);
      label(number(processor_.cpu)+"%",355,350,72,white);
      label("MEMORY USED",960,310,15,muted);
      label(number(processor_.memory)+"%",955,350,72,white);
      label("Load average   "+number(processor_.load1)+"  /  "+number(processor_.load5)+"  /  "+number(processor_.load15),360,454,21,muted);
      label("Available   "+number(processor_.available_kib/1024.,0)+" MiB",960,454,21,muted);
      Rect graph{
        360,524,1180,236
      };
      r.rect(graph,{
        .025f,.045f,.08f,1
      },
      12);
      for(int i=1;i<4;++i)r.line(graph.x,graph.y+i*graph.h/4,graph.x+graph.w,graph.y+i*graph.h/4,{
        1,1,1,.06f
      },
      1);
      if(cpu_history_.size()<2)label("Collecting real CPU deltas...",graph.x+30,graph.y+92,22,muted);
      else for(size_t i=1;i<cpu_history_.size();++i){
        float x1=graph.x+20+(i-1)*((graph.w-40)/59),x2=graph.x+20+i*((graph.w-40)/59);
        float y1=graph.y+graph.h-20-static_cast<float>(cpu_history_[i-1])/100*(graph.h-40),y2=graph.y+graph.h-20-static_cast<float>(cpu_history_[i])/100*(graph.h-40);
        r.line(x1,y1,x2,y2,aqua,3);
      }
      for(auto& n:nodes_){
        if(n.id=="refresh-cpu")button(n,"Refresh counters","refresh");
        if(n.id=="processor-app")button(n,"Open configured processor app","cpu");
      }
    }
    else if(view_=="retro"){
      label("An authenticated LAN companion, not a remote desktop or ROM downloader.",330,231,21,muted);
      r.icon("retropie",{
        370,325,140,140
      },
      accent());
      label(retro_.empty()?"Ready to pair":clean_text(retro_.get("hostname").str("RetroPie"),50),570,337,44,white);
      label("Address   "+(prefs_.retropie_host.empty()?std::string("NOT CONFIGURED"):prefs_.retropie_host)+":"+std::to_string(prefs_.retropie_port),570,414,22,muted);
      label("Set retropie_host in settings.json and SPICE_RETROPIE_TOKEN for this session.",360,526,21,muted);
      label("Trusted LAN HTTP only. No tokens are written to settings, screenshots or logs.",360,567,20,muted);
      if(!retro_.empty()){
        std::string systems;
        auto a=retro_.get("systems");
        for(size_t i=0;i<std::min<size_t>(a.size(),10);++i){
          if(i)systems+="   /   ";
          systems+=clean_text(a.at(i).str(),20);
        }
        label(r.ellipsis("Systems: "+systems,20,1180),360,635,20,white);
        label(retro_.get("emulationstation_running").flag()?"EmulationStation is running":"EmulationStation is not running",360,684,22,aqua);
      }
      else label("Status is unknown until the companion returns a valid authenticated response.",360,644,21,muted);
      for(auto& n:nodes_){
        if(n.id=="refresh-retro")button(n,"Connect / refresh","network");
        if(n.id=="retro-hdmi")button(n,"Switch to HDMI "+std::to_string(prefs_.retropie_hdmi),"input");
      }
    }
    else if(view_=="homebrew"){
      label("Keep package management with the app that owns it.",330,231,21,muted);
      r.icon("homebrew",{
        380,330,128,128
      },
      accent());
      label("Homebrew, without hidden changes.",570,355,37,white);
      label("Open Homebrew Channel to review, install and update your apps.",360,534,26,white);
      label("This launcher does not run privileged commands, install packages silently,",360,591,22,muted);
      label("or invent an update-all endpoint. You remain in control of every change.",360,628,22,muted);
      label("Launch target: org.webosbrew.hbchannel  /  availability must be verified on the TV.",360,714,17,muted);
      for(auto& n:nodes_){
        if(n.id=="homebrew-probe")button(n,"Check availability","search");
        if(n.id=="homebrew-open")button(n,"Open Homebrew Channel","homebrew");
      }
    }
    else if(view_=="tv"){
      label("Best-effort import. Unavailable keys stay unavailable. Select a value to pin it.",330,231,20,muted);
      for(auto& n:nodes_)if(n.id.rfind("cat:",0)==0)button(n,n.id.substr(4),"",category_==n.id.substr(4));
      auto values=settings_.get(category_.c_str()).get("settings").members();
      if(values.empty()){
        label("No values imported for this category.",360,416,28,white);
        label("Use Read category. Desktop mode never fabricates TV settings.",360,469,21,muted);
      }
      for(auto& n:nodes_)if(n.id.rfind("pin-setting:",0)==0){
        auto key=n.id.substr(12);
        auto dot=key.find('.');
        auto v=settings_.get(category_.c_str()).get("settings").get(key.substr(dot+1).c_str());
        auto text=v.is(json_type_string)?v.str():v.dump();
        button(n,key+"    "+clean_text(text,74),"",std::find(prefs_.pins.begin(),prefs_.pins.end(),key)!=prefs_.pins.end());
      }
      for(auto& n:nodes_){
        if(n.id=="refresh-settings")button(n,"Read category","refresh");
        if(n.id=="settings-next")button(n,"More values");
        if(n.id=="native-settings")button(n,"Open LG settings shortcut","settings");
      }
    }
    else if(view_=="diagnostics"){
      label("Measured framebuffer facts. Not a claim about the panel's 4K or 120 Hz output.",330,231,20,accent());
      std::vector<std::pair<std::string,std::string>> facts{
        {
          "Window coordinates",std::to_string(viewport_.window_w)+" x "+std::to_string(viewport_.window_h)
        },
        {
          "Drawable framebuffer",std::to_string(viewport_.drawable_w)+" x "+std::to_string(viewport_.drawable_h)
        },
        {
          "Logical layout / physical scale","1920 x 1080 / "+number(viewport_.scale,2)+"x"
        },
        {
          "Graphics API",renderer_.version
        },
        {
          "Renderer",renderer_.renderer
        },
        {
          "Swap interval",std::to_string(swap_)+" (driver reported, not a refresh-rate guarantee)"
        },
        {
          "Frame interval median / p95",number(frames_.median_ms,2)+" / "+number(frames_.p95_ms,2)+" ms ("+std::to_string(frames_.samples)+" samples)"
        },
        {
          "Estimated cached RGBA textures",number(renderer_.texture_bytes()/1048576.,1)+" MiB / "+std::to_string(renderer_.text_cache_size())+" text entries"
        },
        {
          "Last input",last_key_
        },
        {
          "Native permissions",services_.private_reads()?"Private read probes opted in; permissions still required":"Public requests only; private reads disabled"
        }
      };
      int i=0;
      for(auto& [key,value]:facts){
        float y=310.f+i++*47;
        label(key,354,y,18,muted);
        label(r.ellipsis(value,18,835),720,y,18,white);
      }
      std::string tv_info=device_.get("modelName").str();
      if(!tv_info.empty())tv_info=(device_snapshot_?"Prepared TV information: ":"Explicit TV read: ")+tv_info+" / firmware "+device_.get("firmwareVersion").str("unknown");
      else tv_info="TV information is unavailable locally. Read explicitly to refresh.";
      label(r.ellipsis(tv_info,15,1180),354,274,15,accent());
      for(auto& n:nodes_){
        if(n.id=="refresh-device")button(n,"Read TV identity","refresh");
        if(n.id=="preview-lab")button(n,"Open preview event log","display");
      }
    }
    else if(view_=="preview"){
      label("Posters and explicit native handoff. No tv:// element or decoder is embedded in SDL.",330,231,20,muted);
      label("Automatic TV / HDMI handoff: "+std::string(prefs_.handoff?"ON (opted in)":"OFF"),360,310,25,white);
      label("Dwell "+number(prefs_.dwell,0)+" s  /  failure cooldown 45 s  /  Back suppresses re-entry",360,359,21,muted);
      int i=0;
      size_t start=events_.size()>6?events_.size()-6:0;
      for(size_t n=start;n<events_.size();++n){
        label(r.ellipsis(events_[n],18,1170),360,432.f+i++*52,18,n==events_.size()-1?white:muted);
      }
      for(auto& n:nodes_){
        if(n.id=="retry-preview")button(n,"Retry selected source","refresh");
        if(n.id=="pref:2")button(n,prefs_.handoff?"Disable automatic handoff":"Enable automatic handoff","input");
      }
    }
    else if(view_=="confirm"){
      label(confirmation_=="exit"?"Close the C++ test launcher?":"Enable automatic native input handoff?",360,337,33,white);
      label(confirmation_=="exit"?"Your original TV home and Rust checkout remain unchanged.":"After the configured dwell, focusing Live TV or an HDMI shortcut",360,430,23,muted);
      if(confirmation_!="exit"){
        label("will request that input's native app. This is NOT background video.",360,474,23,muted);
        label("Back cancels re-entry; overlays and lost focus reset the countdown.",360,539,21,muted);
      }
      for(auto& n:nodes_){
        if(n.id=="cancel")button(n,"Cancel","back");
        if(n.id=="confirm")button(n,confirmation_=="exit"?"Exit C++ lab":"Enable handoff","ok");
      }
    }
    if(!nodes_.empty())draw_focus(focus_motion_.value());
    r.transform();
  }
  void Deck::draw(const Viewport& v,const FrameSummary& frames,int swap){
    viewport_=v;
    frames_=frames;
    swap_=swap;
    if(view_=="designs")draw_designs();
    else{
      if(view_=="home"){
        // Backdrop and artwork crossfade stay live; the cached layer holds the
        // stable content plus hero/refresh. The clock is a live label driven by
        // the once-per-second string so a minute tick cannot repaint the cache.
        // Ambience and widgets paint over it.
        draw_home_backdrop();
        renderer_.home_layer(home_content_revision_,[&]{draw_home_content();});
        // Painted live from the once-per-second cached string: a minute tick
        // no longer invalidates the static cache or runs localtime per frame.
        label(home_clock_,1750,58,23,white);
        if(prefs_.ambient_motion&&!prefs_.reduced_motion)draw_ambience();
        draw_home_widgets();
        if(!nodes_.empty())draw_focus(focus_motion_.value());
      }else{
        // Non-Home views keep the same live backdrop and cached stable content;
        // only the panel on top is uncached. This avoids a full draw_home() per
        // frame behind settings/control/library.
        draw_home_backdrop();
        renderer_.home_layer(home_content_revision_,[&]{draw_home_content();});
        label(home_clock_,1750,58,23,white);
        // Control Center paints cards in the same right-side region as the Home
        // widget strip; drawing both produces clipped duplicate labels.
        if(view_!="control")draw_home_widgets();
      }
      if(view_=="control")draw_control();
      else if(view_!="home")draw_panel();
    }
    if(!config_writable_)label("Settings file invalid: session-only changes; original file preserved.",96,805,18,Color{1,.76f,.4f,1});
    if(toast_until_>time_){
      const float cap=view_=="home"?1340.f:1450.f;
      const auto message=renderer_.ellipsis(toast_,18,cap-54);
      float width=std::min(cap,renderer_.width(message,18)+54);
      const float toast_y=view_=="designs"?12.f:view_=="control"?438.f:981.f;
      const float toast_x=view_=="home"?96.f:(1920-width)*.5f;
      renderer_.rect({
        toast_x,toast_y,width,39
      },
      {
        .04f,.065f,.11f,.97f
      },
      18);
      label(message,toast_x+27,toast_y+10,18,{
        .83f,.9f,1,1
      }
      );
    }
  }
  Json Deck::motion_report()const{
    auto rect_json=[](Rect r){
      auto j=Json::object();
      j.set("x",Json::number(r.x)); j.set("y",Json::number(r.y));
      j.set("w",Json::number(r.w)); j.set("h",Json::number(r.h));
      return j;
    };
    auto target=nodes_.empty()?Rect{}:nodes_[static_cast<size_t>(focus_)].rect;
    auto j=Json::object();
    j.set("focus_rect",rect_json(focus_motion_.value()));
    j.set("focus_presented",rect_json(presentation().bounds(focus_motion_.value())));
    j.set("focus_target",rect_json(target));
    j.set("focus_settled",Json::boolean(focus_motion_.settled()));
    j.set("panel_progress",Json::number(panel_progress_));
    j.set("panel_offset_y",Json::number(presentation().offset_y));
    j.set("panel_opacity",Json::number(presentation().opacity));
    j.set("artwork_from",Json::string(artwork_motion_.from()));
    j.set("artwork_to",Json::string(artwork_motion_.to()));
    j.set("artwork_pending",Json::string(artwork_motion_.pending()));
    j.set("artwork_progress",Json::number(artwork_motion_.progress()));
    j.set("artwork_active",Json::boolean(artwork_motion_.animating()));
    j.set("ambient_enabled",Json::boolean(prefs_.ambient_motion&&!prefs_.reduced_motion));
    j.set("ambient_particles",Json::number(static_cast<double>(ambience_.particles().size())));
    j.set("ambient_time",Json::number(ambience_.time()));
    j.set("ambient_parallax_x",Json::number(ambience_.parallax_x()));
    j.set("ambient_parallax_y",Json::number(ambience_.parallax_y()));
    j.set("ambient_vignette",Json::number(ambience_.vignette()));
    j.set("focus_pulse",Json::number(ambience_.pulse_progress()));
    j.set("text_rasterizations_this_frame",Json::number(static_cast<double>(renderer_.text_rasterizations_this_frame())));
    // Additive widget-motion fields; existing consumers keep reading the flat
    // keys above unchanged.
    const auto widgets=widget_rects();
    const auto posed=widget_pose(widgets.back(),2);
    auto widget_json=Json::object();
    widget_json.set("enabled",Json::boolean(widget_motion_active(prefs_,view_=="home")));
    widget_json.set("cards",Json::number(webos_?2:3));
    widget_json.set("drift_x",Json::number(posed.x-widgets.back().x));
    widget_json.set("drift_y",Json::number(posed.y-widgets.back().y));
    widget_json.set("breath",Json::number(widget_breath(2)));
    widget_json.set("layer",Json::string("outside-home-layer"));
    j.set("widget_motion",widget_json);
    j.set("active",Json::boolean(active_));
    j.set("redraw_requested",Json::boolean(motion_redraw_));
    return j;
  }
  Json Deck::report()const{
    auto j=Json::object();
    j.set("app",Json::string("com.spice.home.cpp"));
    j.set("runtime",Json::string(webos_?"webos":"desktop-demo"));
    j.set("view",Json::string(view_));
    j.set("active",Json::boolean(active_));
    j.set("motion",motion_report());
    j.set("config_writable",Json::boolean(config_writable_));
    j.set("settings_persistence",persistence_report());
    j.set("filter",Json::string(query_));
    j.set("library_search_open",Json::boolean(library_keyboard_));
    j.set("library_page",Json::number(page_+1));
    j.set("navigation_depth",Json::number(static_cast<double>(navigation_.size())));
    j.set("processor_available",Json::boolean(processor_.available));
    j.set("processor_enabled",Json::boolean(!webos_));
    j.set("service_worker_started",Json::boolean(services_.worker_started()));
    j.set("prepared_data",prepared_report_);
    j.set("service_requests_posted",services_.request_counts());
    j.set("automatic_live_metadata_refresh",Json::boolean(false));
    auto scan=inventory_review_.report();scan.set("pending",Json::boolean(scan_pending_));scan.set("source",Json::string(scan_source_));scan.set("summary",Json::string(scan_message_));scan.set("review_filter",Json::boolean(review_only_));scan.set("interaction_epoch",Json::number(static_cast<double>(interaction_epoch_)));scan.set("request_epoch",Json::number(static_cast<double>(scan_epoch_)));j.set("inventory_scan",scan);
    auto identity=Json::array();for(const auto& line:selected_identity_)identity.add(Json::string(line));j.set("selected_identity",identity);j.set("pin_target",Json::string(selected_));j.set("inventory_notice",Json::string(backend_notice_));
    auto tv_info=Json::object();
    for(auto* name:{"modelName","firmwareVersion","sdkVersion","UHD"})if(device_.get(name).is(json_type_string))tv_info.set(name,device_.get(name));
    j.set("tv_information",tv_info);
    j.set("tv_information_source",Json::string(device_snapshot_?"prepared-snapshot":device_.get("modelName").empty()?"unavailable":"explicit-query"));
    auto prepared_apps=Json::array();
    for(const auto& app:apps_)if(app.prepared){
      auto item=Json::object();
      item.set("id",Json::string(app.id));item.set("title",Json::string(app.title));
      item.set("icon_key",Json::string(app.icon_key));item.set("hero_key",Json::string(app.hero_key));
      item.set("observed_at",Json::string(app.observed_at));item.set("inventory_confirmed_at_capture",Json::boolean(app.installed));
      prepared_apps.add(item);
    }
    j.set("prepared_apps",prepared_apps);
    j.set("private_reads_enabled",Json::boolean(services_.private_reads()));
    j.set("last_notice",Json::string(toast_));
    j.set("focus",Json::string(nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id));
    j.set("selected_app",Json::string(selected_));
    j.set("expanded",Json::boolean(prefs_.expanded));
    j.set("reduced_motion",Json::boolean(prefs_.reduced_motion));
  j.set("ambient_motion",Json::boolean(prefs_.ambient_motion));
    j.set("handoff_enabled",Json::boolean(prefs_.handoff));
    j.set("inline_video",Json::boolean(false));
    j.set("settings_imported",Json::boolean(!settings_.empty()));
    j.set("theme",Json::string(prefs_.theme));
    auto theme=Json::object();
    theme.set("loaded",Json::boolean(theme_&&theme_->loaded));
    theme.set("interactive_io",Json::boolean(false));
    theme.set("actual_texture_bytes",Json::number(static_cast<double>(renderer_.theme_bytes())));
    if(theme_){
      size_t retained=0;for(const auto& image:theme_->images)retained+=image.png.size();theme.set("startup_compressed_bytes_retained",Json::number(static_cast<double>(retained)));
      theme.set("name",Json::string(theme_->name));theme.set("version",Json::string(theme_->version));theme.set("generation",Json::string(theme_->generation));theme.set("provenance",Json::string(theme_->provenance));theme.set("error",Json::string(theme_->error));theme.set("accent",Json::string(theme_->accent));theme.set("background_policy",Json::string(theme_->background_policy));
      theme.set("admitted_rgba_bytes",Json::number(static_cast<double>(theme_->rgba_bytes)));
      auto icons=Json::object();for(const auto& [slot,digest]:theme_->icons)icons.set(slot.c_str(),Json::string("theme/"+digest));theme.set("semantic_icons",icons);
      auto apps=Json::array();for(const auto& app:apps_){auto binding=Json::object();binding.set("id",Json::string(app.id));binding.set("icon",Json::string(app_icon_key(app)));binding.set("artwork",Json::string(app_artwork_key(app)));apps.add(binding);}theme.set("resolved_apps",apps);
    }
    j.set("theme_data",theme);
    j.set("designs",Json::number(static_cast<double>(designs_.size())));
    j.set("design_category",Json::string(design_category_));
    j.set("design_selected",Json::string(design_inspector_?design_selected_:std::string()));
    j.set("design_page",Json::number(design_page_+1));
    j.set("design_matches",Json::number(static_cast<double>(designs_.filter(design_category_,query_).size())));
    j.set("design_search_open",Json::boolean(design_keyboard_));
    j.set("gl_renderer",Json::string(renderer_.renderer));
    j.set("gl_version",Json::string(renderer_.version));
    j.set("gl_es",Json::boolean(renderer_.is_es));
    j.set("drawable_width",Json::number(viewport_.drawable_w));
    j.set("drawable_height",Json::number(viewport_.drawable_h));
    j.set("window_width",Json::number(viewport_.window_w));
    j.set("window_height",Json::number(viewport_.window_h));
    j.set("scale",Json::number(viewport_.scale));
    j.set("swap_interval",Json::number(swap_));
    j.set("median_interval_ms",Json::number(frames_.median_ms));
    j.set("p95_interval_ms",Json::number(frames_.p95_ms));
    j.set("interval_samples",Json::number(static_cast<double>(frames_.samples)));
    j.set("texture_bytes",Json::number(static_cast<double>(renderer_.texture_bytes())));
    j.set("draw_calls",Json::number(renderer_.draws));
    j.set("processor_source",Json::string(webos_?"TV /proc":"desktop host, not TV"));
    auto nodes=Json::array();
    auto presented=Json::array();
    for(auto& n:nodes_){
      auto a=Json::object();
      a.set("id",Json::string(n.id));
      a.set("x",Json::number(n.rect.x));
      a.set("y",Json::number(n.rect.y));
      a.set("w",Json::number(n.rect.w));
      a.set("h",Json::number(n.rect.h));
      nodes.add(a);
      auto bounds=presentation().bounds(n.rect);
      auto b=Json::object();
      b.set("id",Json::string(n.id));
      b.set("x",Json::number(bounds.x)); b.set("y",Json::number(bounds.y));
      b.set("w",Json::number(bounds.w)); b.set("h",Json::number(bounds.h));
      presented.add(b);
    }
    j.set("focus_targets",nodes);
    j.set("presented_focus_targets",presented);
    return j;
  }
}
