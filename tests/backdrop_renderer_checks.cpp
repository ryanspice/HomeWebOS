#include "spice/render.hpp"
#include <png.h>
#include <iostream>
#include <stdexcept>
using namespace spice;
static std::vector<unsigned char> pixels(const std::filesystem::path& file){
  png_image image{};image.version=PNG_IMAGE_VERSION;
  if(!png_image_begin_read_from_file(&image,file.string().c_str()))throw std::runtime_error("PNG read");
  image.format=PNG_FORMAT_RGBA;std::vector<unsigned char> data(PNG_IMAGE_SIZE(image));
  if(!png_image_finish_read(&image,nullptr,data.data(),0,nullptr))throw std::runtime_error("PNG decode");
  png_image_free(&image);return data;
}
int main(int argc,char** argv){
  if(argc!=2)throw std::runtime_error("fixture output required");
  auto output=std::filesystem::path(argv[1]);std::filesystem::create_directories(output);
  Sdl s;s.SetMainReady();if(s.Init(SDL_INIT_VIDEO)!=0)throw std::runtime_error("SDL init");
  s.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  s.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);s.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  auto* w=s.CreateWindow("backdrop comparison",0,0,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!w)throw std::runtime_error("window");
  auto context=s.GL_CreateContext(w);
  if(!context)throw std::runtime_error("context");
  for(int step=0;step<3;++step){
    float blend=step==0?0.f:step==1?.47f:1.f;
    int submissions[2]{};
    for(int mode=0;mode<2;++mode){
      Renderer r(s,find_font(""),"assets",false,mode==1);r.begin(Viewport::make(1280,720,1280,720));
      r.backdrop("hero/welcome","hero/eclipse",blend);r.end();submissions[mode]=r.draws;
      r.screenshot(output/(std::to_string(step)+(mode?"-composite.png":"-legacy.png")));
    }
    auto a=pixels(output/(std::to_string(step)+"-legacy.png"));auto b=pixels(output/(std::to_string(step)+"-composite.png"));
    size_t changed=0;unsigned max_interior=0,max_edge=0;
    for(int y=0;y<720;++y)for(int x=0;x<1280;++x)for(int c=0;c<4;++c){
      size_t at=(static_cast<size_t>(y)*1280+x)*4+c;
      unsigned delta=static_cast<unsigned>(std::abs(static_cast<int>(a[at])-b[at]));
      if(delta)++changed;
      if(x>=2&&y>=2&&x<1278&&y<718)max_interior=std::max(max_interior,delta);else max_edge=std::max(max_edge,delta);
    }
    std::cout<<"step="<<step<<" legacy_draws="<<submissions[0]<<" composite_draws="<<submissions[1]
      <<" changed_channels="<<changed<<" max_interior_delta="<<max_interior<<" max_edge_delta="<<max_edge<<"\n";
    if(max_interior>2||submissions[0]-submissions[1]!=(step==0?3:4))throw std::runtime_error("backdrop composition comparison failed");
  }
  s.GL_DeleteContext(context);s.DestroyWindow(w);s.Quit();
  std::cout<<"PASS composited lighting, crossfade endpoints and three removed backdrop passes; edge/rounding deltas recorded\n";
}
