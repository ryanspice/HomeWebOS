#include "spice/inventory.hpp"
#include <cctype>
namespace spice {
  static std::string lower_ascii(std::string text){for(char& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return text;}
  std::string launcher_build_reason(const AppEntry& app){
    if(app.sample||!safe_app_id(app.id)||app.id=="com.spice.home.cpp"||app.id.starts_with("com.webos.")||app.id=="org.webosbrew.hbchannel")return {};
    static const std::map<std::string,std::string> known{
      {"com.spice.home","Earlier Spice Home app ID"},
      {"com.ryanspice.ps5home","PS5 Home build"},
      {"com.ryanspice.ps5home.flutter","PS5 Home Flutter build"},
      {"com.ryanspice.xmb","XMB launcher build"},
      {"com.ryanspice.ogxdashboard","Dashboard launcher build"},
      {"com.canopy.orbishome","Orbis Home build"},
      {"com.boring.tvhome","TV Home launcher"},
      {"com.homebrew.openlauncher","Open Launcher build"},
      {"com.wiios.launcher","Launcher build"},
      {"io.github.webosconsole.home","Console Home build"},
      {"io.metrohome.webos","Metro Home build"},
      {"dev.hurrspice.tvdock","TV dock build"}
    };
    if(auto found=known.find(app.id);found!=known.end())return found->second;
    const bool owner=app.id.starts_with("com.ryanspice.")||app.id.starts_with("com.spice.")||app.id.starts_with("dev.hurrspice.")||app.id.starts_with("io.metrohome.")||app.id.starts_with("com.canopy.");
    if(!owner)return {};
    auto title=lower_ascii(app.title);
    for(char& c:title)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))c=' ';
    title=" "+title+" ";
    for(auto* word:{"home","start","launcher","dashboard"})if(title.find(" "+std::string(word)+" ")!=std::string::npos)return "Owner app with a Home/Start/launcher name";
    return {};
  }
  static std::set<std::string> real_ids(const std::vector<AppEntry>& apps){
    std::set<std::string> ids;
    for(const auto& app:apps)if(!app.sample&&safe_app_id(app.id)&&app.id!="com.spice.home.cpp")ids.insert(app.id);
    return ids;
  }
  void InventoryReview::classify(const std::vector<AppEntry>& apps){
    candidates_.clear();
    for(const auto& app:apps){auto reason=launcher_build_reason(app);if(!reason.empty())candidates_.emplace(app.id,std::move(reason));}
  }
  void InventoryReview::seed(const std::vector<AppEntry>& apps,const std::string& scope){
    baseline_=real_ids(apps);scope_=scope;known_=!scope.empty()&&scope!="unknown";compared_=false;new_ids_.clear();classify(apps);
  }
  void InventoryReview::scan(const std::vector<AppEntry>& apps,const std::string& scope){
    auto current=real_ids(apps);
    compared_=known_&&scope==scope_&&!scope.empty()&&scope!="unknown";
    if(compared_){
      for(auto it=new_ids_.begin();it!=new_ids_.end();)if(!current.contains(*it))it=new_ids_.erase(it);else ++it;
      for(const auto& id:current)if(!baseline_.contains(id))new_ids_.insert(id);
    }else new_ids_.clear();
    baseline_=std::move(current);scope_=scope;known_=!scope.empty()&&scope!="unknown";classify(apps);
  }
  Json InventoryReview::report()const{
    auto out=Json::object(),fresh=Json::array(),candidates=Json::array();
    for(const auto& id:new_ids_)fresh.add(Json::string(id));
    for(const auto& [id,reason]:candidates_){auto row=Json::object();row.set("id",Json::string(id));row.set("reason",Json::string(reason));candidates.add(row);}
    out.set("new_ids",fresh);out.set("candidates",candidates);out.set("scope",Json::string(scope_));out.set("compared_with_previous",Json::boolean(compared_));
    out.set("new_count",Json::number(static_cast<double>(new_ids_.size())));out.set("candidate_count",Json::number(static_cast<double>(candidates_.size())));
    out.set("classification",Json::string("Naming-based review candidates; no removal operation"));return out;
  }
}
