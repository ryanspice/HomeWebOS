#pragma once
#include "spice/core.hpp"
#include "spice/json.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace spice {
  // Authored presentation metadata only. A design is never a launchable app:
  // it carries no URL, command, credential or file path. The catalog is the
  // reviewed POC from the Rust 0.3.5 work, used here as the C++ port guideline.
  struct DesignFact {
    std::string label,value;
  };
  struct DesignCard {
    std::string title,subtitle,detail;
    double progress=0;
  };
  struct Design {
    std::string key,name,category,surface,mark,accent,secondary,scene;
    std::string tagline,description,availability,hero;
    std::vector<DesignFact> metadata;
    std::vector<DesignCard> cards;
    std::vector<std::string> fields,aliases,matched;
    bool sample_only=true;
  };
  class DesignCatalog {
    std::vector<Design> designs_;
    std::vector<std::string> categories_;
    std::string notice_,error_;
    public:
    bool load(const std::filesystem::path& path);
    bool ok()const{
      return error_.empty();
    }
    const std::string& error()const{
      return error_;
    }
    const std::string& notice()const{
      return notice_;
    }
    const std::vector<Design>& all()const{
      return designs_;
    }
    const std::vector<std::string>& categories()const{
      return categories_;
    }
    const Design* find(const std::string& key)const;
    std::vector<const Design*> filter(const std::string& category,const std::string& query)const;
    size_t size()const{
      return designs_.size();
    }
  };
}