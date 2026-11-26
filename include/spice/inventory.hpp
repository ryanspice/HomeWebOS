#pragma once
#include "spice/core.hpp"
#include <set>
namespace spice {
  std::string launcher_build_reason(const AppEntry& app);
  class InventoryReview {
    std::set<std::string> baseline_,new_ids_;
    std::map<std::string,std::string> candidates_;
    std::string scope_;
    bool known_=false,compared_=false;
    void classify(const std::vector<AppEntry>& apps);
    public:
    void seed(const std::vector<AppEntry>& apps,const std::string& scope);
    void scan(const std::vector<AppEntry>& apps,const std::string& scope);
    bool compared()const{return compared_;}
    bool is_new(const std::string& id)const{return new_ids_.contains(id);}
    const std::set<std::string>& new_ids()const{return new_ids_;}
    const std::map<std::string,std::string>& candidates()const{return candidates_;}
    const std::string& scope()const{return scope_;}
    Json report()const;
  };
}
