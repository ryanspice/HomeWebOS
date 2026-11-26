#include "spice/designs.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_set>
namespace spice {
  namespace {
    constexpr size_t MaxDesigns=96;
    constexpr size_t MaxCatalogBytes=256*1024;
    std::string lower(std::string value){
      std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
        return static_cast<char>(std::tolower(c));
      });
      return value;
    }
    std::string bounded(const Json& value,size_t limit){
      auto text=clean_text(value.str(),limit);
      if(text.empty())throw std::runtime_error("Design field is empty or invalid");
      return text;
    }
    std::string options(const Json& value,size_t limit){
      return clean_text(value.str(),limit);
    }
    bool hex(const std::string& value){
      if(value.size()!=7||value[0]!='#')return false;
      return std::all_of(value.begin()+1,value.end(),[](unsigned char c){
        return std::isxdigit(c)!=0;
      });
    }
    void strings(const Json& value,std::vector<std::string>& out,size_t count,size_t limit){
      out.clear();
      for(size_t i=0;i<value.size()&&i<count;++i){
        auto text=clean_text(value.at(i).str(),limit);
        if(!text.empty())out.push_back(std::move(text));
      }
    }
  }
  bool DesignCatalog::load(const std::filesystem::path& path){
    designs_.clear();
    categories_.clear();
    notice_.clear();
    error_.clear();
    try{
      const std::string raw=read_file(path,MaxCatalogBytes);
      if(raw.empty())throw std::runtime_error("Design catalog missing or empty");
      Json root=Json::parse(raw,MaxCatalogBytes);
      if(root.get("schema").num(0)!=1)throw std::runtime_error("Unsupported design schema");
      if(!root.get("design_only").flag())throw std::runtime_error("Catalog is not design-only");
      notice_=clean_text(root.get("notice").str(),600);
      for(size_t i=0;i<root.get("categories").size()&&i<16;++i){
        auto name=clean_text(root.get("categories").at(i).str(),48);
        if(!name.empty())categories_.push_back(std::move(name));
      }
      const Json entries=root.get("entries");
      if(entries.size()==0||entries.size()>MaxDesigns)throw std::runtime_error("Design count outside 1..96");
      std::unordered_set<std::string> keys;
      for(size_t i=0;i<entries.size();++i){
        const Json source=entries.at(i);
        Design d;
        d.key=bounded(source.get("key"),48);
        if(!keys.insert(d.key).second)throw std::runtime_error("Duplicate design key: "+d.key);
        d.name=bounded(source.get("name"),60);
        d.category=bounded(source.get("category"),48);
        d.surface=options(source.get("surface"),32);
        d.mark=options(source.get("mark"),24);
        d.accent=bounded(source.get("accent"),7);
        d.secondary=bounded(source.get("secondary"),7);
        if(!hex(d.accent)||!hex(d.secondary))throw std::runtime_error("Design colour is not #rrggbb: "+d.key);
        d.scene=options(source.get("scene"),32);
        d.tagline=options(source.get("tagline"),160);
        d.description=options(source.get("description"),400);
        d.availability=options(source.get("availability_note"),240);
        d.hero=options(source.get("hero_title"),120);
        d.sample_only=source.get("sample_only").flag(true);
        strings(source.get("aliases"),d.aliases,8,48);
        strings(source.get("matched_ids"),d.matched,8,96);
        strings(source.get("expected_fields"),d.fields,12,96);
        for(size_t n=0;n<source.get("metadata").size()&&n<8;++n){
          const Json fact=source.get("metadata").at(n);
          d.metadata.push_back({options(fact.get("label"),40),options(fact.get("value"),120)});
        }
        for(size_t n=0;n<source.get("cards").size()&&n<8;++n){
          const Json card=source.get("cards").at(n);
          d.cards.push_back({
            options(card.get("title"),80),options(card.get("subtitle"),100),
            options(card.get("detail"),100),std::clamp(card.get("progress").num(0),0.0,1.0)
          });
        }
        designs_.push_back(std::move(d));
      }
      if(categories_.empty()){
        for(const auto& d:designs_)if(std::find(categories_.begin(),categories_.end(),d.category)==categories_.end())categories_.push_back(d.category);
      }
    }
    catch(const std::exception& e){
      error_=e.what();
      designs_.clear();
      categories_.clear();
      return false;
    }
    return true;
  }
  const Design* DesignCatalog::find(const std::string& key)const{
    auto it=std::find_if(designs_.begin(),designs_.end(),[&](const Design& d){
      return d.key==key;
    }
    );
    return it==designs_.end()?nullptr:&*it;
  }
  std::vector<const Design*> DesignCatalog::filter(const std::string& category,const std::string& query)const{
    const std::string needle=lower(clean_text(query,64));
    std::vector<const Design*> out;
    for(const auto& d:designs_){
      if(!category.empty()&&category!="All"&&d.category!=category)continue;
      if(!needle.empty()){
        std::string haystack=lower(d.name+" "+d.category+" "+d.scene+" "+d.tagline+" "+d.key);
        for(const auto& alias:d.aliases)haystack+=" "+lower(alias);
        if(haystack.find(needle)==std::string::npos)continue;
      }
      out.push_back(&d);
    }
    return out;
  }
}