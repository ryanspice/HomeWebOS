#include "spice/render.hpp"
#include "spice/theme.hpp"
#include <iostream>
#include <stdexcept>
using namespace spice;
int main(int argc,char** argv){
  if(argc!=3)throw std::runtime_error("theme fixture and output directory required");
  const auto theme=load_theme(argv[1],true);
  const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
  Sdl s;s.SetMainReady();if(s.Init(SDL_INIT_VIDEO)!=0)throw std::runtime_error("SDL init");
  s.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  s.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);s.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  auto* window=s.CreateWindow("theme renderer checks",0,0,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!window)throw std::runtime_error("window");
  auto context=s.GL_CreateContext(window);if(!context)throw std::runtime_error("context");
  {
    Renderer r(s,find_font(""),"assets",false,true,false);
    const auto before=r.texture_bytes();
    auto bad=theme;bad.images.back().png="not a PNG";bad.compressed_bytes=0;
    for(const auto& image:bad.images)bad.compressed_bytes+=image.png.size();
    bool rejected=false;try{r.preload_theme(bad);}catch(const std::exception&){rejected=true;}
    if(!rejected||r.texture_bytes()!=before||r.theme_bytes()!=0)throw std::runtime_error("Partial theme upload was not rolled back");
    for(const auto& image:theme.images)if(r.has_image("theme/"+image.sha256))throw std::runtime_error("Failed upload published a theme key");
    std::cout<<"PASS partial PNG failure preserves texture table and accounting\n";
    bad=theme;bad.rgba_bytes=25u*1024u*1024u;rejected=false;
    try{r.preload_theme(bad);}catch(const std::exception&){rejected=true;}
    if(!rejected||r.texture_bytes()!=before)throw std::runtime_error("Invalid admission reached texture publication");
    std::cout<<"PASS texture admission fails before publication\n";
    r.preload_theme(theme);
    const auto actual=theme.rgba_bytes-1600u*900u*4u;
    if(r.theme_bytes()!=actual||r.texture_bytes()!=before+actual)throw std::runtime_error("Unique theme texture accounting mismatch");
    for(const auto& image:theme.images)if(!r.has_image("theme/"+image.sha256))throw std::runtime_error("Validated theme image missing");
    std::cout<<"PASS all unique images published once; reserve is not allocated twice\n";
    r.begin(Viewport::make(1280,720,1280,720));
    r.backdrop("theme/"+theme.global_background()->sha256,"theme/"+theme.global_background()->sha256,0);
    int index=0;
    for(const auto& [slot,digest]:theme.icons){
      (void)slot;r.image("theme/"+digest,{100.f+index*180,300,128,128});++index;
    }
    r.end();r.screenshot(out/"theme-textures.png");
    std::cout<<"PASS actual GLES2 theme background and colored icon draw/capture\n";
    const auto bytes=r.texture_bytes();rejected=false;
    try{r.preload_theme(theme);}catch(const std::exception&){rejected=true;}
    if(!rejected||r.texture_bytes()!=bytes)throw std::runtime_error("Theme reload during interaction was allowed");
    std::cout<<"PASS late upload blocked without changing active textures\n";
  }
  s.GL_DeleteContext(context);s.DestroyWindow(window);s.Quit();
}
