// Procedural scene art for the design catalog, ported from the Rust 0.3.5
// console/design.rs painters. Everything is drawn with the native primitive
// renderer inside a caller-supplied rect, so tiles and the inspector share one
// implementation and no per-design texture, image or decoder is needed.
//
// Every painter is explicitly a placeholder: it must stay readable as fictional
// art, never as live EPG, real telemetry or provider artwork.
#include "spice/ui.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
namespace spice {
  namespace {
    const Color White{1.f,1.f,1.f,1.f};
    struct Pen {
      Renderer& r;
      Rect b;
      float unit()const{
        return std::min(b.w,b.h);
      }
      float cx()const{
        return b.x+b.w*.5f;
      }
      float cy()const{
        return b.y+b.h*.5f;
      }
      bool rich()const{
        return b.h>=220;
      }
      Rect box(float x,float y,float w,float h)const{
        return {
          b.x+x*b.w,b.y+y*b.h,w*b.w,h*b.h
        };
      }
      void fill(float x,float y,float w,float h,Color c,float radius=0)const{
        r.rect(box(x,y,w,h),c,radius*unit());
      }
      void grad(float x,float y,float w,float h,Color top,Color bottom,float radius=0)const{
        r.gradient(box(x,y,w,h),top,bottom,radius*unit());
      }
      void frame(float x,float y,float w,float h,Color c,float thickness=1.f)const{
        r.rect(box(x,y,w,h),c,0,thickness);
      }
      void disc(float x,float y,float radius,Color c)const{
        const Rect q=box(x,y,0,0);
        const float R=radius*unit();
        r.rect({
          q.x-R,q.y-R,R*2,R*2
        },c,R);
      }
      void ring(float x,float y,float radius,Color c,float thickness)const{
        const Rect q=box(x,y,0,0);
        const float R=radius*unit();
        r.rect({
          q.x-R,q.y-R,R*2,R*2
        },c,R,thickness);
      }
      void line(float x1,float y1,float x2,float y2,Color c,float thickness=1.f)const{
        r.line(b.x+x1*b.w,b.y+y1*b.h,b.x+x2*b.w,b.y+y2*b.h,c,thickness);
      }
      void label(const std::string& s,float x,float y,float rel,Color c)const{
        if(!rich())return;
        r.text(s,b.x+x*b.w,b.y+y*b.h,rel*b.h,c);
      }
      void bar(float x,float y,float w,float h,float amount,Color track,Color fill_color)const{
        const float radius=h*.5f*b.h/unit();
        r.rect(box(x,y,w,h),track,radius);
        r.rect(box(x,y,w*std::clamp(amount,0.f,1.f),h),fill_color,radius);
      }
    };
    Color tint(Color c,float a){
      return {
        c.r,c.g,c.b,a
      };
    }
    void broadcast(const Pen& p,Color accent){
      p.grad(.02f,.03f,.96f,.94f,{
        .07f,.11f,.19f,.60f
      },
      {
        .03f,.05f,.10f,.85f
      },.04f);
      p.frame(.03f,.05f,.94f,.90f,tint(White,.07f),1.f);
      p.label("PLACEHOLDER FEED / NO LIVE CONTENT",.05f,.08f,.055f,tint(White,.55f));
      for(int n=0;n<3;++n){
        const float y=.22f+n*.25f;
        p.fill(.05f,y,.90f,.19f,n==0?tint(accent,.24f):tint(White,.05f),.025f);
        p.disc(.10f,y+.095f,.028f,n==0?accent:tint(White,.25f));
        p.fill(.15f,y+.05f,.34f,.038f,tint(White,n==0?.78f:.32f),.012f);
        p.fill(.15f,y+.115f,.22f,.024f,tint(White,.18f),.012f);
        p.label(n==0?"NOW / EXAMPLE":n==1?"NEXT / EXAMPLE":"DISCOVER / EXAMPLE",.15f,y+.145f,.05f,tint(White,.42f));
      }
    }
    void arcade(const Pen& p,Color accent){
      p.ring(.5f,.5f,.40f,tint(White,.10f),1.5f);
      p.ring(.5f,.5f,.30f,tint(accent,.22f),1.5f);
      // Fixed registration marks make the controller read as an instrument,
      // not a live signal. The motif remains bounded by the supplied artwork.
      p.line(.12f,.50f,.19f,.50f,tint(White,.24f),1.f);
      p.line(.81f,.50f,.88f,.50f,tint(White,.24f),1.f);
      p.fill(.20f,.30f,.60f,.32f,{
        .10f,.15f,.24f,.95f
      },.16f);
      p.fill(.34f,.435f,.045f,.10f,tint(White,.8f),.01f);
      p.fill(.2975f,.475f,.13f,.045f,tint(White,.8f),.01f);
      static const float dots[4][2]={
        {.66f,.38f},{.70f,.45f},{.62f,.45f},{.66f,.52f}
      };
      for(const auto& dot:dots)p.disc(dot[0],dot[1],.028f,tint(accent,.9f));
    }
    void library(const Pen& p,Color accent){
      for(int n=0;n<3;++n){
        const float x=.06f+n*.31f;
        const float y=n==1?.10f:.18f;
        p.fill(x,y,.26f,.74f,{
          .09f,.13f,.21f,.95f
        },.035f);
        p.ring(x+.13f,y+.26f,.075f,tint(White,.30f),1.5f);
        p.fill(x+.03f,y+.56f,.20f,.035f,tint(White,.70f),.01f);
        p.fill(x+.03f,y+.63f,.14f,.025f,tint(accent,.55f),.01f);
      }
    }
    void cinema(const Pen& p,Color accent,Color secondary){
      p.grad(.06f,.10f,.88f,.58f,tint(secondary,.30f),{
        .02f,.03f,.07f,.95f
      },.035f);
      p.frame(.06f,.10f,.88f,.58f,tint(White,.14f),1.f);
      p.ring(.5f,.39f,.17f,tint(White,.28f),2.f);
      p.disc(.5f,.39f,.10f,tint(accent,.30f));
      for(int n=0;n<6;++n)p.fill(.09f+n*.145f,.74f,.10f,.16f,{
        .10f,.13f,.20f,.9f
      },.02f);
    }
    void rows(const Pen& p,Color accent,const char* const* names,int count){
      p.grad(.02f,.03f,.96f,.94f,{
        .07f,.11f,.19f,.60f
      },
      {
        .03f,.05f,.10f,.85f
      },.04f);
      p.frame(.03f,.05f,.94f,.90f,tint(White,.07f),1.f);
      p.label("DESIGN / STATUS NOT QUERIED",.05f,.08f,.055f,tint(White,.5f));
      for(int n=0;n<count;++n){
        const float y=.24f+n*.24f;
        p.fill(.05f,y,.90f,.17f,tint(White,.05f),.02f);
        p.disc(.10f,y+.085f,.026f,tint(accent,.85f));
        p.fill(.15f,y+.055f,.30f,.038f,tint(White,.72f),.012f);
        p.bar(.62f,y+.07f,.28f,.045f,(n+1)/static_cast<float>(count),tint(White,.14f),tint(accent,.85f));
        p.label(names[n],.15f,y+.13f,.05f,tint(White,.42f));
      }
    }
    void sport(const Pen& p,Color accent){
      p.fill(.06f,.16f,.88f,.64f,tint(White,.05f),.03f);
      p.frame(.10f,.30f,.80f,.44f,tint(White,.22f),1.5f);
      p.line(.5f,.30f,.5f,.74f,tint(White,.16f),1.5f);
      p.ring(.5f,.52f,.09f,tint(White,.20f),1.5f);
      p.fill(.20f,.02f,.60f,.22f,{
        .10f,.14f,.22f,.95f
      },.05f);
      p.label("EXAMPLE SCOREBOARD / NOT LIVE",.22f,.045f,.05f,tint(White,.5f));
      p.label("HOME  /  AWAY",.22f,.125f,.075f,tint(White,.85f));
      p.fill(.30f,.50f,.06f,.05f,tint(accent,.9f),.01f);
      p.fill(.64f,.50f,.06f,.05f,tint(White,.5f),.01f);
    }
    void orbit(const Pen& p,Color accent,Color secondary){
      for(int n=0;n<5;++n)p.ring(.5f,.5f,.16f+n*.055f,tint(accent,.18f-n*.012f),1.5f);
      p.disc(.5f,.5f,.085f,tint(accent,.92f));
      p.disc(.5f,.5f,.035f,tint(White,.85f));
      p.disc(.74f,.30f,.030f,tint(secondary,.95f));
      p.disc(.24f,.70f,.020f,tint(White,.75f));
      p.line(.10f,.18f,.22f,.18f,tint(White,.22f),1.f);
      p.line(.78f,.82f,.90f,.82f,tint(White,.22f),1.f);
      p.line(.06f,.92f,.94f,.92f,tint(White,.08f),1.f);
    }
    void music(const Pen& p,Color accent){
      for(int n=0;n<4;++n)p.ring(.5f,.46f,.30f+n*.05f,tint(accent,.14f),1.5f);
      p.fill(.34f,.20f,.32f,.50f,{
        .15f,.20f,.31f,.98f
      },.03f);
      p.ring(.5f,.45f,.10f,tint(White,.30f),2.f);
      p.label("ORIGINAL SAMPLE",.06f,.62f,.055f,tint(White,.5f));
      for(int n=0;n<22;++n){
        const float t=n/21.f;
        const float h=.05f+std::fabs(std::sin(t*3.1f+n*.35f))*.16f;
        p.fill(.06f+n*.041f,.94f-h,.016f,h,tint(accent,.6f),.006f);
      }
    }
    void gallery(const Pen& p,Color accent,Color secondary){
      p.fill(.20f,.06f,.60f,.82f,tint(secondary,.85f),.03f);
      p.fill(.25f,.11f,.50f,.72f,tint(White,.10f),.02f);
      p.disc(.5f,.45f,.28f,{
        .44f,.40f,.39f,1.f
      });
      p.disc(.5f,.45f,.20f,{
        .72f,.63f,.55f,1.f
      });
      p.disc(.5f,.45f,.10f,tint(accent,.8f));
    }
    void telemetry(const Pen& p,Color accent){
      p.grad(.02f,.03f,.96f,.94f,{
        .07f,.11f,.19f,.60f
      },
      {
        .03f,.05f,.10f,.85f
      },.04f);
      p.frame(.03f,.05f,.94f,.90f,tint(White,.07f),1.f);
      p.label("ILLUSTRATIVE / NOT A MEASUREMENT",.05f,.08f,.055f,tint(White,.5f));
      for(int n=0;n<22;++n){
        const float h=.10f+std::fabs(std::sin(n*.42f))*.52f;
        p.fill(.06f+n*.041f,.84f-h,.022f,h,tint(accent,.65f),.008f);
      }
      p.line(.05f,.85f,.95f,.85f,tint(White,.16f),1.f);
    }
    void devices(const Pen& p,Color accent){
      const float nx[3]={.26f,.74f,.5f};
      const float ny[3]={.30f,.30f,.72f};
      for(int n=0;n<3;++n)p.disc(nx[n],ny[n],.085f,{
        .11f,.16f,.25f,.95f
      });
      p.line(nx[0],ny[0],nx[1],ny[1],tint(accent,.55f),1.5f);
      p.line((nx[0]+nx[1])*.5f,(ny[0]+ny[1])*.5f,nx[2],ny[2],tint(accent,.55f),1.5f);
      for(int n=0;n<3;++n)p.ring(nx[n],ny[n],.085f,tint(accent,.8f),2.f);
    }
    void input(const Pen& p,Color accent){
      p.fill(.06f,.16f,.88f,.60f,tint(White,.05f),.03f);
      p.frame(.06f,.16f,.88f,.60f,tint(White,.16f),1.f);
      p.disc(.34f,.46f,.075f,{
        .11f,.16f,.25f,.95f
      });
      p.ring(.34f,.46f,.075f,tint(accent,.9f),2.f);
      p.fill(.62f,.30f,.10f,.10f,{
        .11f,.16f,.25f,.95f
      },.02f);
      p.fill(.62f,.50f,.10f,.10f,{
        .11f,.16f,.25f,.95f
      },.02f);
      p.fill(.74f,.30f,.10f,.10f,{
        .11f,.16f,.25f,.95f
      },.02f);
      p.fill(.74f,.50f,.10f,.10f,tint(accent,.4f),.02f);
      p.label("NO INPUT SIGNAL QUERIED",.08f,.80f,.055f,tint(White,.5f));
    }
    // Thumbnail motif: a deliberately reduced version of each family, ~6 draws
    // instead of ~20, used for the catalog grid. The inspector draws the full
    // painter. Keeps the grid cheap without giving up a distinct look per design.
    void motif(const Pen& p,const std::string& scene,Color accent,Color secondary){
      p.grad(.01f,.01f,.98f,.98f,{
        .07f,.11f,.19f,.66f
      },
      {
        .025f,.04f,.08f,.90f
      },.04f);
      if(scene=="broadcast"||scene=="guide"||scene=="creator"){
        p.fill(.08f,.20f,.84f,.20f,tint(accent,.55f),.03f);
        p.fill(.08f,.48f,.60f,.17f,tint(White,.10f),.03f);
        p.fill(.08f,.73f,.40f,.17f,tint(White,.07f),.03f);
      }
      else if(scene=="arcade"){
        p.ring(.5f,.5f,.36f,tint(accent,.30f),1.5f);
        p.fill(.22f,.34f,.56f,.32f,{
          .10f,.15f,.24f,.95f
        },.16f);
        p.fill(.335f,.465f,.05f,.10f,tint(White,.8f),.01f);
        p.fill(.31f,.495f,.10f,.04f,tint(White,.8f),.01f);
      }
      else if(scene=="library"){
        for(int n=0;n<3;++n)p.fill(.10f+n*.30f,.18f,.22f,.64f,{
          .09f,.13f,.21f,.95f
        },.04f);
        p.ring(.21f,.40f,.06f,tint(accent,.7f),1.5f);
      }
      else if(scene=="cinema"){
        p.fill(.10f,.14f,.80f,.50f,tint(secondary,.45f),.03f);
        p.ring(.5f,.39f,.15f,tint(White,.30f),2.f);
        p.disc(.5f,.39f,.09f,tint(accent,.5f));
        p.fill(.10f,.72f,.80f,.16f,tint(White,.06f),.03f);
      }
      else if(scene=="settings"||scene=="browser"){
        for(int n=0;n<3;++n){
          p.fill(.08f,.22f+n*.24f,.84f,.11f,tint(White,.08f),.03f);
          p.fill(.08f,.22f+n*.24f,(n==0?.52f:.78f),.11f,tint(accent,.7f),.03f);
        }
      }
      else if(scene=="sport"){
        p.frame(.10f,.20f,.80f,.58f,tint(White,.25f),1.5f);
        p.line(.5f,.20f,.5f,.78f,tint(White,.18f),1.5f);
        p.ring(.5f,.49f,.11f,tint(White,.22f),1.5f);
      }
      else if(scene=="orbit"){
        for(int n=0;n<3;++n)p.ring(.5f,.5f,.18f+n*.10f,tint(accent,.24f),1.5f);
        p.disc(.5f,.5f,.09f,tint(accent,.95f));
      }
      else if(scene=="music"){
        for(int n=0;n<3;++n)p.ring(.5f,.40f,.26f+n*.08f,tint(accent,.16f),1.5f);
        p.fill(.36f,.20f,.28f,.46f,{
          .15f,.20f,.31f,.98f
        },.03f);
        for(int n=0;n<5;++n){
          const float h=.10f+std::fabs(std::sin(n*1.3f))*.20f;
          p.fill(.20f+n*.13f,.90f-h,.06f,h,tint(accent,.65f),.01f);
        }
      }
      else if(scene=="gallery"){
        p.fill(.16f,.10f,.68f,.76f,tint(secondary,.85f),.03f);
        p.disc(.5f,.45f,.28f,{
          .72f,.63f,.55f,1.f
        });
        p.disc(.5f,.45f,.14f,tint(accent,.8f));
      }
      else if(scene=="telemetry"){
        for(int n=0;n<7;++n){
          const float h=.14f+std::fabs(std::sin(n*1.1f))*.46f;
          p.fill(.11f+n*.115f,.84f-h,.065f,h,tint(accent,.7f),.01f);
        }
        p.line(.08f,.86f,.92f,.86f,tint(White,.16f),1.f);
      }
      else if(scene=="devices"){
        p.line(.28f,.32f,.72f,.32f,tint(accent,.60f),1.5f);
        p.line(.5f,.32f,.5f,.68f,tint(accent,.60f),1.5f);
        p.disc(.28f,.32f,.075f,{
          .11f,.16f,.25f,.95f
        });
        p.disc(.72f,.32f,.075f,{
          .11f,.16f,.25f,.95f
        });
        p.disc(.5f,.68f,.075f,tint(accent,.55f));
      }
      else if(scene=="input"){
        p.frame(.10f,.22f,.80f,.52f,tint(White,.18f),1.f);
        p.disc(.34f,.48f,.09f,{
          .11f,.16f,.25f,.95f
        });
        p.fill(.58f,.32f,.13f,.13f,{
          .11f,.16f,.25f,.95f
        },.02f);
        p.fill(.58f,.54f,.13f,.13f,tint(accent,.5f),.02f);
      }
      else{
        p.fill(.08f,.24f,.84f,.18f,tint(accent,.55f),.03f);
        p.fill(.08f,.52f,.56f,.16f,tint(White,.10f),.03f);
      }
    }
  }
  void draw_design_scene(Renderer& renderer,const std::string& scene,const Rect& bounds,const Design& design){
    if(bounds.w<=1||bounds.h<=1)return;
    const Pen p{renderer,bounds};
    const Color accent=Color::hex(design.accent);
    const Color secondary=Color::hex(design.secondary);
    if(bounds.h<220){
      motif(p,scene,accent,secondary);
      return;
    }
    if(scene=="broadcast"||scene=="guide"||scene=="creator"){
      broadcast(p,accent);
    }
    else if(scene=="arcade"){
      arcade(p,accent);
    }
    else if(scene=="library"){
      library(p,accent);
    }
    else if(scene=="cinema"){
      cinema(p,accent,secondary);
    }
    else if(scene=="settings"){
      static const char* const names[]={
        "Picture","Sound","Connection"
      };
      rows(p,accent,names,3);
    }
    else if(scene=="browser"){
      static const char* const names[]={
        "Read","Build","Explore"
      };
      rows(p,accent,names,3);
    }
    else if(scene=="sport"){
      sport(p,accent);
    }
    else if(scene=="orbit"){
      orbit(p,accent,secondary);
    }
    else if(scene=="music"){
      music(p,accent);
    }
    else if(scene=="gallery"){
      gallery(p,accent,secondary);
    }
    else if(scene=="telemetry"){
      telemetry(p,accent);
    }
    else if(scene=="devices"){
      devices(p,accent);
    }
    else if(scene=="input"){
      input(p,accent);
    }
    else{
      broadcast(p,accent);
    }
  }
}
