#include "spice/render.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace spice;
int main(){
  Sdl sdl;sdl.SetMainReady();
  if(sdl.Init(SDL_INIT_VIDEO)!=0)throw std::runtime_error("SDL init");
  sdl.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  sdl.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);sdl.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  auto* window=sdl.CreateWindow("text measurement probe",0,0,640,360,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!window)throw std::runtime_error("window");
  auto context=sdl.GL_CreateContext(window);if(!context)throw std::runtime_error("context");
  {
    Renderer r(sdl,find_font(""),"assets");r.begin(Viewport::make(640,360,640,360));
    auto bytes=r.texture_bytes(),entries=r.text_cache_size();
    double total=0;auto start=std::chrono::steady_clock::now();
    for(int i=0;i<50;++i){
      auto text="AVATAR / a native measurement sample "+std::to_string(i)+" with punctuation...";
      total+=r.width(text,29);
      if(i%10==0)total+=r.width(r.ellipsis(text,29,150),29);
    }
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"{\"measurements\":50,\"texture_cache_entries_delta\":"<<(r.text_cache_size()-entries)
      <<",\"texture_bytes_delta\":"<<(r.texture_bytes()-bytes)<<",\"elapsed_host_ms\":"<<ms
      <<",\"width_checksum\":"<<total<<",\"gpu_time_measured\":false,\"tv_tested\":false}\n";
    r.end();
  }
  sdl.GL_DeleteContext(context);sdl.DestroyWindow(window);sdl.Quit();
}
