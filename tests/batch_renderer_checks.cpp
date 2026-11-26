#include "spice/render.hpp"
#include <png.h>
#include <iostream>
#include <stdexcept>
using namespace spice;

static std::vector<unsigned char> pixels(const std::filesystem::path& path){
  png_image image{};image.version=PNG_IMAGE_VERSION;
  if(!png_image_begin_read_from_file(&image,path.string().c_str()))throw std::runtime_error("read comparison PNG");
  image.format=PNG_FORMAT_RGBA;
  std::vector<unsigned char> data(PNG_IMAGE_SIZE(image));
  if(!png_image_finish_read(&image,nullptr,data.data(),0,nullptr))throw std::runtime_error("decode comparison PNG");
  png_image_free(&image);return data;
}

static RenderCounters paint(Sdl& sdl,const std::filesystem::path& output,bool batched,int width,int height){
  Renderer r(sdl,find_font(""),"assets",batched);
  r.begin(Viewport::make(width,height,width,height));
  r.gradient({0,0,1920,1080},{.04f,.07f,.15f,1},{.01f,.015f,.025f,1});
  for(int i=0;i<40;++i){
    float x=55.f+(i%10)*177.f,y=55.f+(i/10)*150.f;
    r.gradient({x,y,150,122},{.22f,.35f,.68f,.75f},{.06f,.11f,.22f,.9f},static_cast<float>(i%17));
    r.rect({x+12,y+12,130,100},{.9f,.4f,.2f,.27f},12,1.5f);
    r.line(x-7,y+4,x+160,y+112,{.7f,.85f,1,.7f},1.f+i%5);
  }
  r.image("hero/eclipse",{64,660,430,250},.57f,20);
  r.image("covers/aether",{340,700,230,210},.6f,12);
  r.transform(30.25f,14.5f,.47f);
  r.gradient({780,650,600,200},{.2f,.8f,.6f,.8f},{.7f,.2f,.9f,.45f},24);
  r.text("AVATAR / Mélange / punctuation...",800,690,25,{1,.9f,.7f,1});
  r.line(760,660,1450,900,{1,1,1,.5f},3);
  r.transform();
  // Overflow the fixed 512-quad CPU page, then cross multiple GPU pages.
  for(int i=0;i<700;++i)r.rect({15.f+(i%100)*18,960.f+(i/100)*13,12,9},{.2f,.8f,.9f,.5f},2);
  for(int i=0;i<420;++i){
    r.text("eviction "+std::to_string(i),20.f+(i%20)*90,40.f+(i%24)*40,14,{.9f,.9f,1,.12f});
  }
  // Uncached temporary textures must be consumed before deletion.
  r.text(std::string(4100,'W'),10,1010,11,{1,1,1,.2f});
  r.end();
  r.screenshot(output);
  if(r.text_cache_size()>384||r.counters().text_evictions==0)throw std::runtime_error("eviction fixture did not exercise admission");
  if(batched&&r.geometry_capacity_bytes()!=221184)throw std::runtime_error("geometry bound changed");
  return r.counters();
}

int main(int argc,char** argv){
  if(argc!=2&&argc!=4)throw std::runtime_error("output directory and optional width height required");
  int width=argc==4?std::stoi(argv[2]):1280,height=argc==4?std::stoi(argv[3]):720;
  if(width<640||height<360||width>3840||height>2160)throw std::runtime_error("fixture dimensions");
  auto output=std::filesystem::path(argv[1]);std::filesystem::create_directories(output);
  Sdl sdl;sdl.SetMainReady();
  if(sdl.Init(SDL_INIT_VIDEO)!=0)throw std::runtime_error("SDL init");
  sdl.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
  sdl.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);sdl.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  auto* window=sdl.CreateWindow("batch comparison",0,0,width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
  if(!window)throw std::runtime_error("window");
  auto context=sdl.GL_CreateContext(window);if(!context)throw std::runtime_error("context");
  auto immediate=paint(sdl,output/"immediate.png",false,width,height);
  auto batched=paint(sdl,output/"batched.png",true,width,height);
  auto a=pixels(output/"immediate.png"),b=pixels(output/"batched.png");
  if(a.size()!=b.size())throw std::runtime_error("comparison size");
  size_t changed=0;unsigned max_delta=0;
  for(size_t i=0;i<a.size();++i)if(a[i]!=b[i]){++changed;max_delta=std::max(max_delta,static_cast<unsigned>(std::abs(static_cast<int>(a[i])-b[i])));}
  std::cout<<"comparison changed_channels="<<changed<<" max_delta="<<max_delta
    <<" immediate_draws="<<immediate.draws<<" batched_draws="<<batched.draws
    <<" immediate_primitives="<<immediate.primitives<<" batched_primitives="<<batched.primitives
    <<" batch_orphans="<<batched.buffer_orphans<<"\n";
  if(max_delta>1||changed>a.size()/1000)throw std::runtime_error("batch pixel equivalence failed");
  if(immediate.primitives!=batched.primitives||batched.draws>=immediate.draws||batched.buffer_orphans<2||immediate.buffer_orphans<2)throw std::runtime_error("batch/order/overflow fixture failed");
  sdl.GL_DeleteContext(context);sdl.DestroyWindow(window);sdl.Quit();
  std::cout<<"PASS painter order, mixed shader modes, transforms, overflow, eviction, temporary lifetime and bounded batching\n";
}
