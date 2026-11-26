#include "spice/ui.hpp"
#include <algorithm>
#include <string>
#include <vector>
namespace spice {
  namespace {
    const Color White{0.94f,0.97f,1.f,1.f};
    const Color Muted{0.60f,0.67f,0.79f,1.f};
    const Color Ink{0.035f,0.05f,0.09f,1.f};
    const Color Panel{0.10f,0.14f,0.22f,0.94f};
    const Color Line{1.f,1.f,1.f,0.12f};
    std::vector<std::string> wrap(Renderer& r,const std::string& text,float size,float max,size_t max_lines){
      std::vector<std::string> lines;
      std::string line;
      size_t i=0;
      while(i<text.size()&&lines.size()<max_lines){
        while(i<text.size()&&text[i]==' ')++i;
        size_t j=i;
        while(j<text.size()&&text[j]!=' ')++j;
        const std::string word=text.substr(i,j-i);
        const std::string candidate=line.empty()?word:line+" "+word;
        if(r.width(candidate,size)>max&&!line.empty()){
          lines.push_back(line);
          line=word;
        }
        else line=candidate;
        i=j;
      }
      if(!line.empty()&&lines.size()<max_lines)lines.push_back(line);
      if(i<text.size()&&!lines.empty()){
        auto& last=lines.back();
        last=r.ellipsis(last+" ...",size,max);
      }
      return lines;
    }
    void chip(Renderer& r,const Rect& box,const std::string& name,bool on){
      const Color base=on?Color{0.30f,0.40f,0.60f,0.98f}:Color{0.11f,0.15f,0.23f,0.92f};
      r.rect(box,base,14);
      if(on)r.rect(box,{0.62f,0.75f,1.f,0.55f},14,1.4f);
      r.text(name,box.x+18,box.y+15,16,on?White:Muted);
    }
    void tile(Renderer& r,const Design& d,const Rect& box){
      const Color accent=Color::hex(d.accent);
      const Color secondary=Color::hex(d.secondary);
      r.gradient(box,accent.opacity(.40f),secondary.opacity(.18f),18);
      r.transform(0,0,.60f);
      draw_design_scene(r,d.scene,{box.x+8,box.y+4,box.w-16,box.h*.76f},d);
      r.transform();
      r.gradient({box.x,box.y+box.h*.55f,box.w,box.h*.45f},{0,0,0,0},{0.02f,0.03f,0.06f,.88f},18);
      r.rect(box,Line,18,1.f);
      r.rect({box.x+16,box.y+box.h-72,36,2},accent.opacity(.9f),1);
      r.rect({box.x+16,box.y+16,72,30},{0,0,0,.30f},8);
      const std::string mark=d.mark.empty()?d.name.substr(0,3):d.mark;
      r.text(r.ellipsis(mark,15,60),box.x+26,box.y+22,15,White);
      r.text(r.ellipsis(d.name,19,box.w-32),box.x+16,box.y+box.h-62,19,White);
      r.text(r.ellipsis(d.scene+" / "+d.category,13,box.w-32),box.x+16,box.y+box.h-34,13,Muted);
    }
  }
  void Deck::draw_designs(){
    auto& r=renderer_;
    r.rect({0,0,1920,1080},Ink);
    r.gradient({0,0,1920,1080},{0.06f,0.09f,0.16f,1},{0.015f,0.025f,0.05f,1});
    r.transform();
    const std::string focused=nodes_.empty()?"":nodes_[static_cast<size_t>(focus_)].id;
    if(design_keyboard_){
      label("SPICE HOME / DESIGN COLLECTION",104,54,17,Muted);
      label("Find a concept",104,92,56,White);
      label("Search by name, scene or keyword. Category: "+design_category_,106,170,20,Muted);
      r.gradient({360,270,1200,650},Panel,{0.04f,0.065f,0.12f,1},24);
      r.rect({360,270,1200,650},Line,24,1);
      label("Search concepts",408,296,28,White);
      const auto matches=designs_.filter(design_category_,query_).size();
      const std::string count=std::to_string(matches)+" matches";
      label(count,1512-r.width(count,20),300,20,Muted);
      r.rect({408,342,1104,64},{0.025f,0.04f,0.08f,1},12);
      label(query_.empty()?"Choose letters below or type on your keyboard":r.ellipsis(query_,24,1052),434,361,24,query_.empty()?Muted:White);
      for(const auto& n:nodes_){
        if(n.id.rfind("design-key:",0)==0&&n.id!="design-key:space"){
          const auto key=n.id.substr(11);
          r.rect(n.rect,n.id==focused?Color{0.29f,0.37f,0.51f,.95f}:Color{0.12f,0.16f,0.24f,.95f},10);
          label(key,n.rect.x+(n.rect.w-r.width(key,24))*.5f,n.rect.y+18,24,White);
        }
        else if(n.id=="design-key:space")button(n,"Space");
        else if(n.id=="design-delete")button(n,"Delete letter");
        else if(n.id=="design-key-clear")button(n,"Clear");
        else if(n.id=="design-search-done")button(n,"Show "+count,"search");
      }
      label("D-pad moves  /  OK selects  /  Back closes search and keeps your query",408,866,19,Muted);
      if(!nodes_.empty())draw_focus(focus_motion_.value());
      return;
    }
    if(design_inspector_){
      const Design* d=designs_.find(design_selected_);
      r.rect({0,0,1920,1080},{0.02f,0.03f,0.06f,.99f});
      if(!d){
        label("Design not found in the catalog.",330,320,30,White);
        for(auto& n:nodes_)if(n.id=="design-back")button(n,"Back");
        return;
      }
      auto pose=presentation();
      r.transform(0,pose.offset_y,pose.opacity);
      r.rect({0,0,1920,10},Color::hex(d->accent).opacity(.9f));
      label("DESIGN COLLECTION / "+d->name,104,54,17,Muted);
      float y=96;
      for(const auto& line:wrap(r,d->hero.empty()?d->name:d->hero,48,780,3)){
        label(line,104,y,48,White);
        y+=58;
      }
      y+=10;
      for(const auto& line:wrap(r,d->tagline,24,780,3)){
        label(line,104,y,24,Color{0.82f,0.88f,1.f,1.f});
        y+=34;
      }
      y+=8;
      for(const auto& line:wrap(r,d->description,19,780,6)){
        label(line,104,y,19,Muted);
        y+=28;
      }
      {
        const float art_y=y+22;
        const float art_h=std::clamp(880.f-art_y,140.f,360.f);
        Rect art{104,art_y,800,art_h};
        r.gradient(art,{0.07f,0.10f,0.18f,.98f},{0.02f,0.03f,0.06f,.98f},22);
        r.rect(art,Line,22,1.f);
        draw_design_scene(r,d->scene,{art.x+16,art.y+14,art.w-32,art.h-40},*d);
        label("PROCEDURAL SCENE / FICTIONAL",art.x+18,art.y+art.h-30,14,Muted);
      }
      Rect meta{980,120,760,250};
      r.gradient(meta,Panel,{0.06f,0.09f,0.15f,.96f},18);
      r.rect(meta,Line,18,1.f);
      label("Sample details   /   "+d->scene,1010,146,15,Muted);
      float my=182;
      for(const auto& fact:d->metadata){
        label(fact.label,1010,my,15,Muted);
        label(r.ellipsis(fact.value,18,430),1180,my-3,18,White);
        my+=42;
      }
      if(d->metadata.empty())label("No sample metadata supplied.",1010,182,18,Muted);
      Rect cards{980,390,760,320};
      r.gradient(cards,Panel,{0.06f,0.09f,0.15f,.96f},18);
      r.rect(cards,Line,18,1.f);
      label("Example cards",1010,416,15,Muted);
      float cy=452;
      for(size_t i=0;i<d->cards.size()&&i<3;++i){
        const auto& card=d->cards[i];
        label(r.ellipsis(card.title,20,470),1010,cy,20,White);
        label(r.ellipsis(card.subtitle,15,470),1010,cy+26,15,Muted);
        r.rect({1500,cy+4,200,8},{1,1,1,.12f},4);
        r.rect({1500,cy+4,200*static_cast<float>(card.progress),8},Color::hex(d->accent).opacity(.9f),4);
        cy+=76;
      }
      Rect facts{980,730,760,150};
      r.gradient(facts,Panel,{0.06f,0.09f,0.15f,.96f},18);
      r.rect(facts,Line,18,1.f);
      label("Expected fields",1010,756,15,Muted);
      std::string fields;
      for(const auto& field:d->fields)fields+=(fields.empty()?"":" / ")+field;
      float fy=786;
      for(const auto& line:wrap(r,fields.empty()?"None declared":fields,16,700,3)){
        label(line,1010,fy,16,Color{0.82f,0.88f,1.f,1.f});
        fy+=24;
      }
      r.rect({104,900,900,54},{0.10f,0.14f,0.22f,.92f},14);
      label("Design concept / fictional artwork and sample content",128,916,18,Color{1.f,.84f,.52f,1.f});
      {
        const auto list=designs_.filter(design_category_,query_);
        int index=0;
        for(size_t i=0;i<list.size();++i)if(list[i]->key==design_selected_){
          index=static_cast<int>(i)+1;
          break;
        }
        const auto position=std::to_string(index)+" / "+std::to_string(list.size())+"   "+design_category_+(query_.empty()?"":"  /  "+query_);
        label(r.ellipsis(position,16,330),1060,948,16,Muted);
      }
      for(auto& n:nodes_){
        if(n.id=="design-back")button(n,"Back to catalog");
        if(n.id=="design-step:prev")button(n,"Prev");
        if(n.id=="design-step:next")button(n,"Next");
      }
      if(!nodes_.empty())draw_focus(focus_motion_.value());
      r.transform();
      return;
    }
    label("SPICE HOME / DESIGN COLLECTION",104,54,17,Muted);
    r.line(104,207,1816,207,{.72f,.82f,1.f,.14f},1);
    label("Design collection",104,92,56,White);
    const auto list=designs_.filter(design_category_,query_);
    std::string subtitle;
    if(!designs_.ok())subtitle="Design catalog unavailable: "+designs_.error();
    else if(design_category_=="All"&&query_.empty())
      subtitle=std::to_string(designs_.size())+" concepts to explore - fictional artwork and sample content";
    else{
      subtitle="Showing "+std::to_string(list.size())+" of "+std::to_string(designs_.size())+" concepts  -  ";
      if(design_category_!="All")subtitle+=design_category_;
      if(!query_.empty())subtitle+=std::string(design_category_!="All"?" + ":"")+"\""+query_+"\"";
      subtitle+="  -  design concepts";
    }
    label(r.ellipsis(subtitle,20,1400),106,170,20,designs_.ok()?Muted:Color{1,.7f,.5f,1});
    for(auto& n:nodes_)if(n.id=="close")button(n,"Back","back");
    for(auto& n:nodes_)if(n.id.rfind("design-cat:",0)==0){
      const std::string name=n.id.substr(11);
      chip(r,n.rect,name+"  "+std::to_string(designs_.filter(name,"").size()),n.id=="design-cat:"+design_category_);
    }
    for(const auto& n:nodes_)if(n.id=="design-search"){
      r.rect(n.rect,{0.10f,0.14f,0.22f,.94f},14);
      r.rect(n.rect,Line,14,1.f);
      r.icon("search",{n.rect.x+16,n.rect.y+15,22,22},Muted);
      label(query_.empty()?"Search concepts":r.ellipsis(query_,18,n.rect.w-70),n.rect.x+50,n.rect.y+15,18,query_.empty()?Muted:White);
    }
    for(const auto& n:nodes_)if(n.id=="design-clear-search"){
      r.rect(n.rect,{0.16f,0.21f,0.31f,.94f},14);
      label("Clear",n.rect.x+28,n.rect.y+15,18,White);
    }
    const int pages=list.empty()?1:static_cast<int>((list.size()+14)/15);
    for(auto& n:nodes_)if(n.id.rfind("design:",0)==0){
      if(const Design* d=designs_.find(n.id.substr(7))){
        tile(r,*d,n.rect);
      }
    }
    if(list.empty()){
      const std::string title=designs_.ok()?"No concepts found":"Collection unavailable";
      label(title,(1920-r.width(title,32))*.5f,436,32,White);
      const std::string detail=designs_.ok()?"Try another search or reset to explore the whole collection.":"The design catalog could not be loaded. Back returns to the launcher.";
      label(detail,(1920-r.width(detail,20))*.5f,492,20,Muted);
      for(const auto& n:nodes_)if(n.id=="design-reset")button(n,"Reset search and categories","refresh");
    }
    const int first=list.empty()?0:design_page_*15+1;
    const int last=std::min((design_page_+1)*15,static_cast<int>(list.size()));
    label(std::to_string(first)+"-"+std::to_string(last)+" of "+std::to_string(list.size())+" concepts   /   Page "+std::to_string(design_page_+1)+" of "+std::to_string(pages),536,1002,18,Muted);
    const Design* current=focused.rfind("design:",0)==0?designs_.find(focused.substr(7)):nullptr;
    if(current)label(r.ellipsis(current->name+" / "+current->tagline,18,1680),106,948,18,Muted);
    for(int i=0;i<2;++i){
      const Node n{i==0?"design-previous":"design-next",{104.f+i*202,986,190,52}};
      const std::string name=i==0?"Previous":"Next";
      if((i==0&&design_page_>0)||(i==1&&design_page_+1<pages))button(n,name);
      else{
        r.rect(n.rect,{.10f,.13f,.19f,.5f},10);
        label(name,n.rect.x+20,n.rect.y+15,21,Muted.opacity(.40f));
      }
    }
    label("D-pad moves  /  OK inspects  /  Search opens the keyboard  /  Back returns",104,1048,15,Muted);
    label("Concept previews / sample content",1450,1048,15,Muted);
    if(!nodes_.empty())draw_focus(focus_motion_.value());
  }
}
