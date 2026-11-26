#include "spice/ui.hpp"
namespace spice {
  void Deck::glyph(const std::string& slot,const std::string& fallback,Rect rect,Color color){
    if(theme_&&theme_->loaded&&!slot.empty())if(const auto* image=theme_->semantic_icon(slot)){
      const auto key="theme/"+image->sha256;
      if(renderer_.has_image(key)){renderer_.image(key,rect,color.a);return;}
    }
    renderer_.icon(fallback,rect,color);
  }
  std::string Deck::app_icon_key(const AppEntry& app)const{
    if(theme_&&theme_->loaded)if(const auto* image=theme_->app_icon(app.id)){
      const auto key="theme/"+image->sha256;
      if(renderer_.has_image(key))return key;
    }
    if(!app.icon_key.empty()&&renderer_.has_image(app.icon_key))return app.icon_key;
    const auto sample="covers/"+app.art;
    return app.sample&&renderer_.has_image(sample)?sample:std::string();
  }
  std::string Deck::app_artwork_key(const AppEntry& app)const{
    if(theme_&&theme_->loaded)if(const auto* image=theme_->background_for(app.id,!app.hero_key.empty()&&renderer_.has_image(app.hero_key))){
      const auto key="theme/"+image->sha256;
      if(renderer_.has_image(key))return key;
    }
    return !app.hero_key.empty()&&renderer_.has_image(app.hero_key)?app.hero_key.substr(5):app.art;
  }
}
