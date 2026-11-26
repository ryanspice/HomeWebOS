#include "spice/render.hpp"
#include "spice/prepared.hpp"
#include "prepared_fixture.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace spice;
int main(){
  Sdl sdl; sdl.SetMainReady();
  if(sdl.Init(SDL_INIT_VIDEO)!=0)throw std::runtime_error("SDL init");
  sdl.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  sdl.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); sdl.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  auto* window=sdl.CreateWindow("renderer test",0,0,640,360,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!window)throw std::runtime_error("window");
  auto context=sdl.GL_CreateContext(window);
  if(!context)throw std::runtime_error(std::string("context: ")+sdl.GetError());
  {
    Renderer r(sdl,find_font(""),"assets");
    PreparedData prepared;
    prepared.images.push_back({"covers/prepared/com.example.fixture",prepared_fixture_png(),1,1});
    auto startup=r.counters().texture_uploads;
    r.preload_prepared(prepared);
    if(r.counters().texture_uploads!=startup+1)throw std::runtime_error("prepared startup upload count");
    prepared.images[0].key="hero/prepared/com.example.transparent";
    bool transparent_rejected=false;
    try{r.preload_prepared(prepared);}catch(const std::exception&){transparent_rejected=true;}
    if(!transparent_rejected)throw std::runtime_error("transparent prepared hero accepted");
    auto view=Viewport::make(1920,1080,1920,1080);
    r.begin(view);
    prepared.images[0].key="covers/prepared/com.example.late";
    bool late_rejected=false;
    try{r.preload_prepared(prepared);}catch(const std::exception& e){late_rejected=std::string(e.what()).find("before the first frame")!=std::string::npos;}
    if(!late_rejected)throw std::runtime_error("interactive prepared upload accepted");
    const auto before=r.counters();
    for(const auto& text:{"Hello world", "AVATAR", "jiffy", "", "Mélange", "a long sequence with punctuation..."}){
      auto measured=r.width(text,29);
      if(r.counters().texture_uploads!=before.texture_uploads||r.counters().rasterizations!=before.rasterizations)throw std::runtime_error("measurement uploaded or rasterized");
      (void)measured;
    }
    for(const auto& text:{"Hello world", "AVATAR", "jiffy", "", "Mélange", "a long sequence with punctuation..."}){
      auto measured=r.width(text,29);
      auto drawn=r.text(text,0,0,29);
      if(std::abs(measured-drawn)>.01f){std::cerr<<text<<" measured="<<measured<<" drawn="<<drawn<<"\n";throw std::runtime_error("measurement mismatch");}
    }
    auto before_ellipsis=r.counters();
    const auto shortened=r.ellipsis("Long text requiring truncation",29,100);
    if(shortened=="Long text requiring truncation"||r.counters().texture_uploads!=before_ellipsis.texture_uploads||r.counters().rasterizations!=before_ellipsis.rasterizations)throw std::runtime_error("ellipsis measurement");
    auto measured_before_repeat=r.counters();
    if(r.ellipsis("Long text requiring truncation",29,100)!=shortened||r.counters().ellipsis_hits!=measured_before_repeat.ellipsis_hits+1||r.counters().metric_misses!=measured_before_repeat.metric_misses)throw std::runtime_error("ellipsis cache repeat");
    const auto wider=r.ellipsis("Long text requiring truncation",29,500);
    if(wider==shortened)throw std::runtime_error("ellipsis width invalidation");
    for(int i=0;i<520;++i)r.ellipsis("bounded layout "+std::to_string(i),17,60.f+i%10);
    if(r.ellipsis_cache_size()>512)throw std::runtime_error("ellipsis admission bound");
    for(int drawable:{1280,1920,2160,3840}){
      r.end();r.begin(Viewport::make(1920,1080,drawable,drawable*9/16));
      for(float size:{11.f,17.f,23.f,29.f,37.f})for(const auto& sample:{"AVATAR", "jiffy", "Mélange", "A gypsy jumps!", "W."}){
        auto measured=r.width(sample,size);
        auto drawn=r.text(sample,0,0,size);
        if(std::abs(measured-drawn)>.01f)throw std::runtime_error("density measurement mismatch");
      }
    }
    r.text("Hello world",0,0,29);
    auto c=r.counters();
    r.text("Hello world",0,0,29);
    if(r.counters().text_hits!=c.text_hits+1||r.counters().texture_uploads!=c.texture_uploads)throw std::runtime_error("text cache miss on repeat");
    for(int i=0;i<400;++i)r.text("cache admission "+std::to_string(i),0,0,20);
    if(r.text_cache_size()>384||r.counters().text_evictions==0)throw std::runtime_error("cache bound");
    r.end();
    std::cout<<"PASS metrics-only width, raster agreement, cache hits and bounded admission; evictions="<<r.counters().text_evictions<<"\n";
  }
  sdl.GL_DeleteContext(context);sdl.DestroyWindow(window);sdl.Quit();
}

