#include "spice/ui.hpp"
namespace spice {
  void Deck::rebuild_library(const std::string& preferred){
    rebuild();
    auto find=[&](const std::string& id){
      for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id==id)return static_cast<int>(i);
      return -1;
    };
    int next=find(preferred);
    if(next<0&&!library_keyboard_)for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id.starts_with("app:")){
      next=static_cast<int>(i);break;
    }
    if(next<0)next=find(library_keyboard_?"library-key:A":"library-search");
    focus_=next<0?0:next;
    if(!nodes_.empty())focus_motion_.snap(nodes_[static_cast<size_t>(focus_)].rect);
    focus_changed();
    dirty_=true;
  }
  void Deck::restore_previous(){
    ViewContext context;
    if(navigation_.empty()){
      context.view="home";
      const auto home=home_nodes(prefs_.expanded);
      if(home_focus_>=0&&home_focus_<static_cast<int>(home.size()))context.focus=home[static_cast<size_t>(home_focus_)].id;
      context.selected=selected_;
    }
    else{
      context=std::move(navigation_.back());
      navigation_.pop_back();
    }
    view_=context.view;
    query_=context.query;selected_=context.selected;category_=context.category;
    page_=context.page;settings_page_=context.settings_page;review_only_=context.review_only;
    library_keyboard_=false;
    panel_progress_=1;
    rebuild();
    bool found=false;
    for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id==context.focus){focus_=static_cast<int>(i);found=true;break;}
    if(!found){
      focus_=0;
      for(size_t i=0;i<nodes_.size();++i)if(nodes_[i].id.starts_with("app:")){focus_=static_cast<int>(i);break;}
    }
    if(!nodes_.empty())focus_motion_.snap(nodes_[static_cast<size_t>(focus_)].rect);
    focus_changed();preview_.pause();dirty_=true;
  }
  void Deck::draw_library_search(){
    auto& r=renderer_;
    const Color white{.94f,.97f,1,1},muted{.60f,.67f,.79f,1};
    r.rect({0,0,1920,1080},{.025f,.04f,.075f,1});
    r.gradient({0,0,1920,1080},{.06f,.09f,.16f,1},{.015f,.025f,.05f,1});
    const std::string focused=nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id;
    label("SPICE HOME / YOUR APP LIBRARY",104,54,17,muted);
    label("Find an app",104,92,56,white);
    label(review_only_?"Searching build candidates by title or app ID":"Search your local app inventory by title or app ID",106,170,20,muted);
    r.gradient({360,270,1200,650},{.10f,.14f,.22f,.94f},{.04f,.065f,.12f,1},24);
    r.rect({360,270,1200,650},{1,1,1,.12f},24,1);
    label("Search apps",408,296,28,white);
    const std::string count=std::to_string(visible_apps().size())+" matches";
    label(count,1512-r.width(count,20),300,20,muted);
    r.rect({408,342,1104,64},{.025f,.04f,.08f,1},12);
    label(query_.empty()?"Choose letters below or type on your keyboard":r.ellipsis(query_,24,1052),434,361,24,query_.empty()?muted:white);
    for(const auto& n:nodes_){
      if(n.id.starts_with("library-key:")&&n.id!="library-key:space"){
        const auto key=n.id.substr(12);
        r.rect(n.rect,n.id==focused?Color{.29f,.37f,.51f,.95f}:Color{.12f,.16f,.24f,.95f},10);
        label(key,n.rect.x+(n.rect.w-r.width(key,24))*.5f,n.rect.y+18,24,white);
      }
      else if(n.id=="library-key:space")button(n,"Space");
      else if(n.id=="library-delete")button(n,"Delete letter");
      else if(n.id=="library-clear")button(n,"Clear");
      else if(n.id=="library-search-done")button(n,"Show "+count,"search");
    }
    label("D-pad moves / OK selects / Back shows results and keeps your query",408,866,19,muted);
    if(!nodes_.empty())draw_focus(focus_motion_.value());
  }
}
