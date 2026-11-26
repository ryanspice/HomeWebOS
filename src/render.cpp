#include "spice/render.hpp"
#include <chrono>
#include "spice/prepared.hpp"
#include "spice/theme.hpp"
#include <png.h>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace spice {
  constexpr GLenum GL_VENDOR_=0x1f00,GL_RENDERER_=0x1f01,GL_VERSION_=0x1f02,GL_MAX_TEXTURE_SIZE_=0xd33,GL_BLEND_=0xbe2,GL_DEPTH_TEST_=0xb71,GL_SRC_ALPHA_=0x302,GL_ONE_MINUS_SRC_ALPHA_=0x303,GL_ONE_=1,GL_COLOR_BUFFER_BIT_=0x4000,GL_VERTEX_SHADER_=0x8b31,GL_FRAGMENT_SHADER_=0x8b30,GL_COMPILE_STATUS_=0x8b81,GL_LINK_STATUS_=0x8b82,GL_ARRAY_BUFFER_=0x8892,GL_DYNAMIC_DRAW_=0x88e8,GL_FLOAT_=0x1406,GL_TRIANGLES_=4,GL_TEXTURE_2D_=0xde1,GL_TEXTURE0_=0x84c0,GL_RGBA_=0x1908,GL_UNSIGNED_BYTE_=0x1401,GL_TEXTURE_MIN_FILTER_=0x2801,GL_TEXTURE_MAG_FILTER_=0x2800,GL_LINEAR_=0x2601,GL_TEXTURE_WRAP_S_=0x2802,GL_TEXTURE_WRAP_T_=0x2803,GL_CLAMP_TO_EDGE_=0x812f,GL_UNPACK_ALIGNMENT_=0xcf5,GL_PACK_ALIGNMENT_=0xd05;
  Gl::Gl(Sdl& sdl){
#define LOAD(name) name=reinterpret_cast<decltype(name)>(sdl.GL_GetProcAddress("gl" #name));if(!name)throw std::runtime_error("Missing GL symbol: gl" #name)
    LOAD(GetString);
    LOAD(GetIntegerv);
    GetShaderPrecisionFormat=reinterpret_cast<decltype(GetShaderPrecisionFormat)>(sdl.GL_GetProcAddress("glGetShaderPrecisionFormat"));
    LOAD(GetError);
    LOAD(Viewport);
    LOAD(ClearColor);
    LOAD(Clear);
    LOAD(Enable);
    LOAD(Disable);
    LOAD(BlendFunc);
    LOAD(BlendFuncSeparate);
    LOAD(CreateShader);
    LOAD(ShaderSource);
    LOAD(CompileShader);
    LOAD(GetShaderiv);
    LOAD(GetShaderInfoLog);
    LOAD(DeleteShader);
    LOAD(CreateProgram);
    LOAD(AttachShader);
    LOAD(BindAttribLocation);
    LOAD(LinkProgram);
    LOAD(GetProgramiv);
    LOAD(GetProgramInfoLog);
    LOAD(UseProgram);
    LOAD(DeleteProgram);
    LOAD(GetUniformLocation);
    LOAD(Uniform1i);
    LOAD(Uniform1f);
    LOAD(Uniform2f);
    LOAD(Uniform4f);
    LOAD(GenBuffers);
    LOAD(BindBuffer);
    LOAD(BufferData);
    LOAD(BufferSubData);
    LOAD(DeleteBuffers);
    LOAD(EnableVertexAttribArray);
    LOAD(DisableVertexAttribArray);
    LOAD(VertexAttribPointer);
    LOAD(DrawArrays);
    LOAD(GenTextures);
    LOAD(BindTexture);
    LOAD(TexParameteri);
    LOAD(TexImage2D);
    LOAD(DeleteTextures);
    LOAD(ActiveTexture);
    LOAD(PixelStorei);
    LOAD(ReadPixels);
    LOAD(Finish);
#undef LOAD
#define OPTIONAL_FBO(name) name=reinterpret_cast<decltype(name)>(sdl.GL_GetProcAddress("gl" #name));if(!name)name=reinterpret_cast<decltype(name)>(sdl.GL_GetProcAddress("gl" #name "EXT"))
    OPTIONAL_FBO(GenFramebuffers);OPTIONAL_FBO(DeleteFramebuffers);OPTIONAL_FBO(BindFramebuffer);
    OPTIONAL_FBO(FramebufferTexture2D);OPTIONAL_FBO(CheckFramebufferStatus);
#undef OPTIONAL_FBO
  }
  Color Color::hex(const std::string& s){
    if(s.size()!=7||s[0]!='#')return {};
    try{
      auto v=std::stoul(s.substr(1),nullptr,16);
      return {
        ((v>>16)&255)/255.f,((v>>8)&255)/255.f,(v&255)/255.f,1
      };
    }
    catch(...){
      return {};
    }
  }
  GLuint Renderer::compile(GLenum type,const std::string& src){
    auto id=gl_.CreateShader(type);
    const char* p=src.c_str();
    gl_.ShaderSource(id,1,&p,nullptr);
    gl_.CompileShader(id);
    int ok=0;
    gl_.GetShaderiv(id,GL_COMPILE_STATUS_,&ok);
    if(!ok){
      std::array<char,2048> log{};
      gl_.GetShaderInfoLog(id,log.size(),nullptr,log.data());
      gl_.DeleteShader(id);
      throw std::runtime_error(std::string("Shader failed: ")+log.data());
    }
    return id;
  }
  Renderer::Renderer(Sdl& sdl,const std::string& font,const std::filesystem::path& assets,bool batched,bool composite_background,bool sample_art):gl_(sdl),ft_(font),batched_(batched),composite_background_(composite_background){
    auto str=[&](GLenum t){
      auto* p=gl_.GetString(t);
      return p?std::string(reinterpret_cast<const char*>(p)):"unknown";
    };
    vendor=str(GL_VENDOR_);
    renderer=str(GL_RENDERER_);
    version=str(GL_VERSION_);
    is_es=version.find("OpenGL ES")!=std::string::npos;
    if(is_es&&gl_.GetShaderPrecisionFormat){
      GLint range[2]{},precision=0;
      gl_.GetShaderPrecisionFormat(GL_FRAGMENT_SHADER_,0x8df2,range,&precision); // GL_HIGH_FLOAT
      fragment_highp_=precision>0;
    }
    gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE_,&texture_limit_);
    if(texture_limit_<512)throw std::runtime_error("GPU texture limit is too small");
    const std::string varying=fragment_highp_?"highp":"mediump";
    const std::string prefix=is_es?"#version 100\nprecision "+varying+" float;\n#define VARY_PREC "+varying+"\n":"#version 120\n#define VARY_PREC\n";
    const std::string background=R"(
uniform sampler2D u_backdrop_texture;
uniform float u_backdrop_blend;
vec4 composite_backdrop(vec4 sample,vec2 uv){
  float y=uv.y*1080.0;
  float blend=u_backdrop_blend;
  if(blend>0.0)sample.rgb=mix(sample.rgb,texture2D(u_backdrop_texture,uv).rgb,blend);
  float shade=max(0.58*(1.0-clamp(y/430.0,0.0,1.0)),0.88*clamp((y-670.0)/410.0,0.0,1.0));
  sample.rgb=mix(sample.rgb,vec3(0.0,0.0,0.02),shade);
  sample.rgb=mix(sample.rgb,vec3(0.02,0.04,0.09),0.16);
  // Preserve the legacy SRC_ALPHA alpha-channel equation as well as RGB.
  // GL's blend factors apply to alpha too, so its opaque-base crossfade is
  // 1-t+t*t; the lighting passes each apply a*a+(1-a)*destination_alpha.
  float image_alpha=1.0-blend+blend*blend;
  float shaded_alpha=shade*shade+(1.0-shade)*image_alpha;
  sample.a=0.16*0.16+0.84*shaded_alpha;
  return sample;
}
)";
    std::string vertex=(is_es?"#version 100\nprecision highp float;\n#define VARY_PREC "+varying+"\n":prefix)+R"(
attribute vec2 a_position;
attribute vec2 a_uv;
varying VARY_PREC vec2 v_uv;
void main(){v_uv=a_uv;gl_Position=vec4(a_position.x/960.0-1.0,1.0-a_position.y/540.0,0.0,1.0);}
)";
    std::string fragment=prefix+background+R"(
varying VARY_PREC vec2 v_uv;
uniform sampler2D u_texture;
uniform vec4 u_color;
uniform vec4 u_color2;
uniform vec2 u_size;
uniform float u_radius;
uniform float u_border;
uniform float u_aa;
uniform int u_mode;
void main(){
  vec4 col=u_color;
  if(u_mode==1)col*=texture2D(u_texture,v_uv);
  if(u_mode==2)col=mix(u_color,u_color2,v_uv.y);
  if(u_mode==3)col.a*=texture2D(u_texture,v_uv).a;
  if(u_mode==4)col*=composite_backdrop(texture2D(u_texture,v_uv),v_uv);
  if(u_mode!=3&&u_mode!=4){
    vec2 p=abs((v_uv-0.5)*u_size)-(u_size*0.5-vec2(u_radius));
    float d=length(max(p,0.0))+min(max(p.x,p.y),0.0)-u_radius;
    float coverage=1.0-smoothstep(-u_aa,u_aa,d);
    if(u_border>0.0)coverage*=smoothstep(-u_border-u_aa,-u_border+u_aa,d);
    col.a*=coverage;
  }
  gl_FragColor=col;
}
)";
    if(batched_){
      GLint attributes=0,varyings=0;
      gl_.GetIntegerv(0x8869,&attributes); // GL_MAX_VERTEX_ATTRIBS, core GLES2.
      if(is_es)gl_.GetIntegerv(0x8dfc,&varyings); // GL_MAX_VARYING_VECTORS.
      if(attributes<5||(is_es&&varyings<4))throw std::runtime_error("Batch renderer needs five attributes and four varying vectors");
      batch_data_=std::make_unique<float[]>(BatchFloats);
      vertex=(is_es?"#version 100\nprecision highp float;\n#define VARY_PREC "+varying+"\n":prefix)+R"(
attribute vec4 a_vertex;
attribute vec4 a_color;
attribute vec4 a_color2;
attribute vec4 a_shape;
attribute vec2 a_params;
varying VARY_PREC vec4 v_uv_size;
varying VARY_PREC vec4 v_color;
varying VARY_PREC vec4 v_color2;
varying VARY_PREC vec4 v_params;
void main(){
  v_uv_size=vec4(a_vertex.zw,a_shape.xy);v_color=a_color;v_color2=a_color2;
  v_params=vec4(a_shape.zw,a_params);
  gl_Position=vec4(a_vertex.x/960.0-1.0,1.0-a_vertex.y/540.0,0.0,1.0);
}
)";
      fragment=prefix+background+R"(
varying VARY_PREC vec4 v_uv_size;
varying VARY_PREC vec4 v_color;
varying VARY_PREC vec4 v_color2;
varying VARY_PREC vec4 v_params;
uniform sampler2D u_texture;
void main(){
  vec2 uv=v_uv_size.xy;vec2 size=v_uv_size.zw;
  float radius=v_params.x,border=v_params.y,mode=v_params.z,aa=v_params.w;
  vec4 col=v_color;
  if(mode>0.5&&mode<1.5)col*=texture2D(u_texture,uv);
  if(mode>1.5&&mode<2.5)col=mix(v_color,v_color2,uv.y);
  if(mode>2.5&&mode<3.5)col.a*=texture2D(u_texture,uv).a;
  if(mode>3.5)col*=composite_backdrop(texture2D(u_texture,uv),uv);
  if(mode<2.5){
    vec2 p=abs((uv-0.5)*size)-(size*0.5-vec2(radius));
    float d=length(max(p,0.0))+min(max(p.x,p.y),0.0)-radius;
    float coverage=1.0-smoothstep(-aa,aa,d);
    if(border>0.0)coverage*=smoothstep(-border-aa,-border+aa,d);
    col.a*=coverage;
  }
  gl_FragColor=col;
}
)";
    }
    if(!batched_){
      batch_data_=std::make_unique<float[]>(24*512);
      immediate_commands_=std::make_unique<ImmediateCommand[]>(512);
    }
    auto vs=compile(GL_VERTEX_SHADER_,vertex),fs=compile(GL_FRAGMENT_SHADER_,fragment);
    program_=gl_.CreateProgram();
    gl_.AttachShader(program_,vs);
    gl_.AttachShader(program_,fs);
    gl_.BindAttribLocation(program_,0,batched_?"a_vertex":"a_position");
    gl_.BindAttribLocation(program_,1,batched_?"a_color":"a_uv");
    if(batched_){
      gl_.BindAttribLocation(program_,2,"a_color2");
      gl_.BindAttribLocation(program_,3,"a_shape");
      gl_.BindAttribLocation(program_,4,"a_params");
    }
    gl_.LinkProgram(program_);
    gl_.DeleteShader(fs);
    int linked=0;
    gl_.GetProgramiv(program_,GL_LINK_STATUS_,&linked);
    if(!linked){
      std::array<char,2048> log{};
      gl_.GetProgramInfoLog(program_,log.size(),nullptr,log.data());
      throw std::runtime_error(std::string("GL program failed: ")+log.data());
    }
    if(!batched_){
      const auto background_fs=compile(GL_FRAGMENT_SHADER_,prefix+background+R"(
varying VARY_PREC vec2 v_uv;
uniform sampler2D u_texture;
uniform vec4 u_color;
void main(){gl_FragColor=u_color*composite_backdrop(texture2D(u_texture,v_uv),v_uv);}
)");
      backdrop_program_=gl_.CreateProgram();
      gl_.AttachShader(backdrop_program_,vs);gl_.AttachShader(backdrop_program_,background_fs);
      gl_.BindAttribLocation(backdrop_program_,0,"a_position");gl_.BindAttribLocation(backdrop_program_,1,"a_uv");
      gl_.LinkProgram(backdrop_program_);gl_.DeleteShader(background_fs);
      gl_.GetProgramiv(backdrop_program_,GL_LINK_STATUS_,&linked);
      if(!linked)throw std::runtime_error("Dedicated backdrop program failed");
      gl_.UseProgram(backdrop_program_);
      gl_.Uniform1i(gl_.GetUniformLocation(backdrop_program_,"u_texture"),0);
      gl_.Uniform1i(gl_.GetUniformLocation(backdrop_program_,"u_backdrop_texture"),1);
      backdrop_color_=gl_.GetUniformLocation(backdrop_program_,"u_color");
      backdrop_mix_location_=gl_.GetUniformLocation(backdrop_program_,"u_backdrop_blend");
      const auto copy_fs=compile(GL_FRAGMENT_SHADER_,prefix+R"(
varying VARY_PREC vec2 v_uv;
uniform sampler2D u_texture;
void main(){gl_FragColor=texture2D(u_texture,v_uv);}
)");
      copy_program_=gl_.CreateProgram();gl_.AttachShader(copy_program_,vs);gl_.AttachShader(copy_program_,copy_fs);
      gl_.BindAttribLocation(copy_program_,0,"a_position");gl_.BindAttribLocation(copy_program_,1,"a_uv");
      gl_.LinkProgram(copy_program_);gl_.DeleteShader(copy_fs);
      gl_.GetProgramiv(copy_program_,GL_LINK_STATUS_,&linked);
      if(!linked)throw std::runtime_error("Cached Home copy program failed");
      gl_.UseProgram(copy_program_);gl_.Uniform1i(gl_.GetUniformLocation(copy_program_,"u_texture"),0);
    }
    gl_.DeleteShader(vs);
    gl_.UseProgram(program_);
    mode_=gl_.GetUniformLocation(program_,"u_mode");
    color_=gl_.GetUniformLocation(program_,"u_color");
    color2_=gl_.GetUniformLocation(program_,"u_color2");
    size_=gl_.GetUniformLocation(program_,"u_size");
    radius_=gl_.GetUniformLocation(program_,"u_radius");
    border_=gl_.GetUniformLocation(program_,"u_border");
    aa_=gl_.GetUniformLocation(program_,"u_aa");
    gl_.Uniform1i(gl_.GetUniformLocation(program_,"u_texture"),0);
    gl_.Uniform1i(gl_.GetUniformLocation(program_,"u_backdrop_texture"),1);
    backdrop_blend_=gl_.GetUniformLocation(program_,"u_backdrop_blend");
    gl_.GenBuffers(1,&vbo_);
    gl_.BindBuffer(GL_ARRAY_BUFFER_,vbo_);
    gl_.BufferData(GL_ARRAY_BUFFER_,static_cast<GLsizeiptr>(geometry_capacity_bytes()),nullptr,GL_DYNAMIC_DRAW_);
    const std::array<unsigned char,4> white{
      255,255,255,255
    };
    white_=upload(1,1,white.data());
    for(auto* name:{
      "welcome","eclipse","aether","ember","nova","tidal","cinema","library"
    }
    )for(auto* kind:{
      "hero","covers"
    }
    ){
      auto key=std::string(kind)+"/"+name;
      if(!sample_art&&std::string_view(name)!="library"&&std::string_view(name)!="welcome")continue;
      auto t=png(assets/kind/(std::string(name)+".png"));
      image_bytes_+=t.bytes;
      images_[key]=t;
    }
  }
  Renderer::~Renderer(){
    flush();
    for(auto& [key,t]:images_){
      (void)key;
      gl_.DeleteTextures(1,&t.id);
    }
    for(auto& [key,t]:texts_){
      (void)key;
      gl_.DeleteTextures(1,&t.tex.id);
    }
    if(white_.id)gl_.DeleteTextures(1,&white_.id);
    if(vbo_)gl_.DeleteBuffers(1,&vbo_);
    if(program_)gl_.DeleteProgram(program_);
    if(backdrop_program_)gl_.DeleteProgram(backdrop_program_);
    if(copy_program_)gl_.DeleteProgram(copy_program_);
    if(home_fbo_)gl_.DeleteFramebuffers(1,&home_fbo_);
    if(home_cache_.id)gl_.DeleteTextures(1,&home_cache_.id);
  }
  Texture Renderer::upload(int w,int h,const unsigned char* data){
    flush(); // Queued draws must consume their textures before uploads/eviction.
    Texture t;
    t.w=w;
    t.h=h;
    t.bytes=static_cast<size_t>(w)*h*4;
    gl_.GenTextures(1,&t.id);
    gl_.ActiveTexture(GL_TEXTURE0_);
    gl_.BindTexture(GL_TEXTURE_2D_,t.id);
    gl_.PixelStorei(GL_UNPACK_ALIGNMENT_,1);
    gl_.TexParameteri(GL_TEXTURE_2D_,GL_TEXTURE_MIN_FILTER_,GL_LINEAR_);
    gl_.TexParameteri(GL_TEXTURE_2D_,GL_TEXTURE_MAG_FILTER_,GL_LINEAR_);
    gl_.TexParameteri(GL_TEXTURE_2D_,GL_TEXTURE_WRAP_S_,GL_CLAMP_TO_EDGE_);
    gl_.TexParameteri(GL_TEXTURE_2D_,GL_TEXTURE_WRAP_T_,GL_CLAMP_TO_EDGE_);
    gl_.TexImage2D(GL_TEXTURE_2D_,0,GL_RGBA_,w,h,0,GL_RGBA_,GL_UNSIGNED_BYTE_,data);
    ++counters_.texture_uploads;
    counters_.texture_upload_bytes+=t.bytes;
    if(gl_.GetError()!=0){gl_.DeleteTextures(1,&t.id);throw std::runtime_error("GPU texture upload failed");}
    return t;
  }
  Texture Renderer::png(const std::filesystem::path& path){
    png_image image{};
    image.version=PNG_IMAGE_VERSION;
    if(!png_image_begin_read_from_file(&image,path.string().c_str()))throw std::runtime_error("Cannot decode artwork: "+path.filename().string());
    image.format=PNG_FORMAT_RGBA;
    if(image.width>8192||image.height>8192){
      png_image_free(&image);
      throw std::runtime_error("Artwork exceeds decode budget");
    }
    std::vector<unsigned char> data(PNG_IMAGE_SIZE(image));
    if(!png_image_finish_read(&image,nullptr,data.data(),0,nullptr)){
      png_image_free(&image);
      throw std::runtime_error("Artwork decode failed");
    }
    int w=static_cast<int>(image.width),h=static_cast<int>(image.height);
    png_image_free(&image);
    if(w<=texture_limit_&&h<=texture_limit_)return upload(w,h,data.data());
    float s=std::min(static_cast<float>(texture_limit_)/w,static_cast<float>(texture_limit_)/h);
    int nw=std::max(1,static_cast<int>(w*s)),nh=std::max(1,static_cast<int>(h*s));
    std::vector<unsigned char> small(static_cast<size_t>(nw)*nh*4);
    for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){
      auto from=(static_cast<size_t>(static_cast<int>(y/s))*w+static_cast<int>(x/s))*4;
      auto to=(static_cast<size_t>(y)*nw+x)*4;
      std::copy_n(data.data()+from,4,small.data()+to);
    }
    return upload(nw,nh,small.data());
  }
  Texture Renderer::prepared_png(std::string_view bytes,bool opaque){
    png_image image{};
    image.version=PNG_IMAGE_VERSION;
    if(!png_image_begin_read_from_memory(&image,bytes.data(),bytes.size()))throw std::runtime_error("Prepared PNG cannot be decoded");
    image.format=PNG_FORMAT_RGBA;
    // Panel-scale stock art (hero 3840x936 and similar) is admitted up to the
    // 4K content area; beyond that it can only be downscaled again, since the
    // design canvas stays 1920x1080. The upload path still honours the GPU
    // texture limit. The bundle manifest and the packaging tool enforce the
    // same bound before an image reaches this point.
    constexpr int PreparedMaxWidth=3840,PreparedMaxHeight=2160;
    if(!image.width||!image.height||image.width>PreparedMaxWidth||image.height>PreparedMaxHeight){png_image_free(&image);throw std::runtime_error("Prepared PNG exceeds decode budget");}
    std::vector<unsigned char> data(PNG_IMAGE_SIZE(image));
    if(!png_image_finish_read(&image,nullptr,data.data(),0,nullptr)){png_image_free(&image);throw std::runtime_error("Prepared PNG decode failed");}
    int w=static_cast<int>(image.width),h=static_cast<int>(image.height);
    png_image_free(&image);
    // The fused backdrop reproduces the legacy opaque-art lighting equation.
    // Preparation flattens hero alpha once; reject a hand-edited transparent hero.
    if(opaque)for(size_t i=3;i<data.size();i+=4)if(data[i]!=255)throw std::runtime_error("Prepared background must be opaque");
    if(w<=texture_limit_&&h<=texture_limit_)return upload(w,h,data.data());
    float scale=std::min(static_cast<float>(texture_limit_)/w,static_cast<float>(texture_limit_)/h);
    int nw=std::max(1,static_cast<int>(w*scale)),nh=std::max(1,static_cast<int>(h*scale));
    std::vector<unsigned char> small(static_cast<size_t>(nw)*nh*4);
    for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){
      auto from=(static_cast<size_t>(static_cast<int>(y/scale))*w+static_cast<int>(x/scale))*4;
      auto to=(static_cast<size_t>(y)*nw+x)*4;
      std::copy_n(data.data()+from,4,small.data()+to);
    }
    return upload(nw,nh,small.data());
  }
  void Renderer::preload_prepared(const PreparedData& data){
    if(frame_!=0)throw std::runtime_error("Prepared graphics must be uploaded before the first frame");
    if(data.loaded&&!data.graphics_validated)throw std::runtime_error("Metadata-only scans cannot upload graphics");
    std::map<std::string,Texture> uploaded;
    try{
      for(const auto& image:data.images){
        if(images_.contains(image.key)||uploaded.contains(image.key))throw std::runtime_error("Prepared texture key collision");
        uploaded.emplace(image.key,prepared_png(image.png,image.key.starts_with("hero/")));
      }
    }catch(...){for(auto& [key,t]:uploaded){(void)key;gl_.DeleteTextures(1,&t.id);}throw;}
    for(auto& [key,t]:uploaded){(void)key;image_bytes_+=t.bytes;}
    images_.merge(uploaded);
  }
  void Renderer::preload_theme(const Theme& theme){
    if(frame_!=0)throw std::runtime_error("Theme graphics must be uploaded before the first frame");
    if(!theme.loaded)return;
    constexpr size_t reserve=1600u*900u*4u,limit=24u*1024u*1024u;
    size_t planned=reserve,compressed=0;
    for(const auto& image:theme.images){
      if(image.width!=(image.background?1600:128)||image.height!=(image.background?900:128)||image.png.empty())throw std::runtime_error("Theme graphics were not prepared for startup");
      planned+=static_cast<size_t>(image.width)*static_cast<size_t>(image.height)*4;
      compressed+=image.png.size();
    }
    if(planned>limit||planned!=theme.rgba_bytes||compressed!=theme.compressed_bytes)throw std::runtime_error("Theme texture admission mismatch");
    std::map<std::string,Texture> uploaded;
    try{
      for(const auto& image:theme.images){
        const auto key="theme/"+image.sha256;
        if(images_.contains(key)||uploaded.contains(key))throw std::runtime_error("Theme texture key collision");
        auto [entry,inserted]=uploaded.try_emplace(key);
        (void)inserted;
        entry->second=prepared_png(image.png,image.background);
      }
    }catch(...){for(auto& [key,t]:uploaded){(void)key;if(t.id)gl_.DeleteTextures(1,&t.id);}throw;}
    for(auto& [key,t]:uploaded){(void)key;image_bytes_+=t.bytes;theme_bytes_+=t.bytes;}
    images_.merge(uploaded);
  }
  void Renderer::begin(const Viewport& v){
    flush();
    transform();
    view_=v;
    ++frame_;
    draws=0;
    frame_rasterizations_=0;
    gl_.Viewport(0,0,v.drawable_w,v.drawable_h);
    gl_.ClearColor(.008f,.012f,.024f,1);
    gl_.Clear(GL_COLOR_BUFFER_BIT_);
    gl_.Viewport(static_cast<int>(v.left),static_cast<int>(v.top),static_cast<int>(DesignWidth*v.scale),static_cast<int>(DesignHeight*v.scale));
    gl_.Disable(GL_DEPTH_TEST_);
    gl_.Enable(GL_BLEND_);
    gl_.BlendFunc(GL_SRC_ALPHA_,GL_ONE_MINUS_SRC_ALPHA_);
    gl_.UseProgram(program_);
    gl_.BindBuffer(GL_ARRAY_BUFFER_,vbo_);
    // Each primitive gets fresh space until the next orphan. Rewriting the
    // same 96 bytes forced the TV driver to synchronize with preceding draws.
    gl_.BufferData(GL_ARRAY_BUFFER_,static_cast<GLsizeiptr>(geometry_capacity_bytes()),nullptr,0x88e0); // GL_STREAM_DRAW
    upload_cursor_=0;
    ++counters_.buffer_orphans;
    gl_.EnableVertexAttribArray(0);
    gl_.EnableVertexAttribArray(1);
    if(batched_){
      for(GLuint i=2;i<5;++i)gl_.EnableVertexAttribArray(i);
    }else{
      for(GLuint i=2;i<5;++i)gl_.DisableVertexAttribArray(i);
      gl_.VertexAttribPointer(0,2,GL_FLOAT_,0,4*sizeof(float),nullptr);
      gl_.VertexAttribPointer(1,2,GL_FLOAT_,0,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));
    }
  }
  void Renderer::end(){flush();}
  void Renderer::flush(){
    if(batch_count_==0)return;
    if(!batched_){
      const auto upload_start=profiling_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
      // Upload the bounded chunk before issuing any draw that reads it. Mali
      // synchronizes updates to a buffer even when the byte ranges differ.
      gl_.BindBuffer(GL_ARRAY_BUFFER_,vbo_);
      gl_.BufferData(GL_ARRAY_BUFFER_,static_cast<GLsizeiptr>(geometry_capacity_bytes()),nullptr,0x88e0);
      gl_.BufferSubData(GL_ARRAY_BUFFER_,0,static_cast<GLsizeiptr>(batch_count_*sizeof(float)),batch_data_.get());
      if(profiling_)counters_.vertex_upload_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-upload_start).count();
      ++counters_.buffer_orphans;++counters_.vertex_upload_calls;
      counters_.vertex_upload_bytes+=batch_count_*sizeof(float);
      GLuint active_program=program_;
      for(size_t i=0;i<immediate_count_;++i){
        const auto& c=immediate_commands_[i];
        const auto uniform_start=profiling_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        const auto wanted=c.mode==5?copy_program_:(c.mode==4&&backdrop_program_?backdrop_program_:program_);
        if(active_program!=wanted){gl_.UseProgram(wanted);active_program=wanted;}
        if(c.mode==5){
          // The cached full-frame RGBA value is copied without another blend.
        }else if(wanted==backdrop_program_){
          gl_.Uniform4f(backdrop_color_,c.color.r,c.color.g,c.color.b,c.color.a);
          gl_.Uniform1f(backdrop_mix_location_,backdrop_mix_);
        }else{
          gl_.Uniform1i(mode_,c.mode);
          gl_.Uniform4f(color_,c.color.r,c.color.g,c.color.b,c.color.a);
          gl_.Uniform4f(color2_,c.color2.r,c.color2.g,c.color2.b,c.color2.a);
          gl_.Uniform2f(size_,c.width,c.height);
          gl_.Uniform1f(radius_,c.radius);gl_.Uniform1f(border_,c.border);gl_.Uniform1f(aa_,c.aa);
        }
        gl_.ActiveTexture(GL_TEXTURE0_);gl_.BindTexture(GL_TEXTURE_2D_,c.texture);
        const auto draw_start=profiling_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        if(profiling_)counters_.uniform_submit_ms+=std::chrono::duration<double,std::milli>(draw_start-uniform_start).count();
        gl_.DrawArrays(GL_TRIANGLES_,static_cast<GLint>(i*6),6);
        if(profiling_)counters_.draw_submit_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-draw_start).count();
        ++counters_.draws;
      }
      if(active_program!=program_)gl_.UseProgram(program_);
      batch_count_=0;immediate_count_=0;return;
    }
    gl_.BindBuffer(GL_ARRAY_BUFFER_,vbo_);
    if(upload_cursor_+batch_count_>BatchFloats){
      gl_.BufferData(GL_ARRAY_BUFFER_,static_cast<GLsizeiptr>(geometry_capacity_bytes()),nullptr,0x88e0);
      upload_cursor_=0;
      ++counters_.buffer_orphans;
    }
    const auto base=upload_cursor_*sizeof(float);
    gl_.ActiveTexture(GL_TEXTURE0_);
    gl_.BindTexture(GL_TEXTURE_2D_,batch_texture_);
    gl_.BufferSubData(GL_ARRAY_BUFFER_,static_cast<ptrdiff_t>(base),static_cast<GLsizeiptr>(batch_count_*sizeof(float)),batch_data_.get());
    for(GLuint i=0;i<5;++i){
      const size_t component=i<4?static_cast<size_t>(i)*4:16;
      gl_.VertexAttribPointer(i,i<4?4:2,GL_FLOAT_,0,static_cast<GLsizei>(BatchStride*sizeof(float)),reinterpret_cast<void*>(base+component*sizeof(float)));
    }
    gl_.DrawArrays(GL_TRIANGLES_,0,static_cast<GLsizei>(batch_count_/BatchStride));
    ++draws;++counters_.draws;++counters_.vertex_upload_calls;
    counters_.vertex_upload_bytes+=batch_count_*sizeof(float);
    upload_cursor_+=batch_count_;
    batch_count_=0;
  }
  void Renderer::submit(const std::array<float,24>& vertices,int mode,Color c,Color c2,
                        float width,float height,float radius,float border,float aa,GLuint texture){
    ++counters_.primitives;
    if(batched_){
      if(batch_count_&&(texture!=batch_texture_||batch_count_+6*BatchStride>BatchFloats))flush();
      batch_texture_=texture;
      const std::array<float,14> parameters{c.r,c.g,c.b,c.a,c2.r,c2.g,c2.b,c2.a,width,height,radius,border,static_cast<float>(mode),aa};
      for(size_t i=0;i<6;++i){
        std::copy_n(vertices.data()+i*4,4,batch_data_.get()+batch_count_);
        std::copy(parameters.begin(),parameters.end(),batch_data_.get()+batch_count_+4);
        batch_count_+=BatchStride;
      }
      return;
    }
    if(immediate_count_==512)flush();
    std::copy(vertices.begin(),vertices.end(),batch_data_.get()+batch_count_);
    batch_count_+=vertices.size();
    immediate_commands_[immediate_count_++]={mode,c,c2,width,height,radius,border,aa,texture};
    ++draws;
  }
  void Renderer::evict_texts(size_t incoming){
    flush();
    constexpr size_t budget=24*1024*1024;
    while(!texts_.empty()&&(texts_.size()>=384||text_bytes_>budget-incoming)){
      auto oldest=std::min_element(texts_.begin(),texts_.end(),[](const auto& a,const auto& b){
        return a.second.last_use<b.second.last_use;
      });
      text_bytes_-=oldest->second.tex.bytes;
      gl_.DeleteTextures(1,&oldest->second.tex.id);
      texts_.erase(oldest);
      ++counters_.text_evictions;
    }
  }
  bool Renderer::home_cache_fits()const{
    const int w=static_cast<int>(DesignWidth*view_.scale),h=static_cast<int>(DesignHeight*view_.scale);
    if(w<=0||h<=0||w>texture_limit_||h>texture_limit_)return false;
    return static_cast<size_t>(w)*static_cast<size_t>(h)*4<=HomeCacheBudgetBytes;
  }
  void Renderer::home_layer(uint64_t revision,const std::function<void()>& paint){
    const int w=static_cast<int>(DesignWidth*view_.scale),h=static_cast<int>(DesignHeight*view_.scale);
    if(batched_||!home_cache_fits()||!gl_.GenFramebuffers||!gl_.DeleteFramebuffers||!gl_.BindFramebuffer||!gl_.FramebufferTexture2D||!gl_.CheckFramebufferStatus){paint();return;}
    flush();
    // These driver queries are only needed when this call actually leaves the
    // current framebuffer. On a cache hit we composite straight back to the
    // caller's state, so avoid two synchronous round-trips per frame on Mali.
    GLint original_fbo=0,viewport[4]{};
    bool state_saved=false;
    auto save_state=[&]{
      if(state_saved)return;
      gl_.GetIntegerv(0x8ca6,&original_fbo);gl_.GetIntegerv(0x0ba2,viewport);
      state_saved=true;
    };
    if(home_cache_.w!=w||home_cache_.h!=h){
      home_cache_valid_=false;
      if(home_fbo_){gl_.DeleteFramebuffers(1,&home_fbo_);home_fbo_=0;}
      if(home_cache_.id)gl_.DeleteTextures(1,&home_cache_.id);
      home_cache_={};
      try{home_cache_=upload(w,h,nullptr);}catch(const std::exception&){paint();return;}
      // This texture is copied at exactly one texel per drawable pixel. Linear
      // filtering would soften high-contrast glyph edges after interpolation.
      gl_.TexParameteri(GL_TEXTURE_2D_,0x2801,0x2600);
      gl_.TexParameteri(GL_TEXTURE_2D_,0x2800,0x2600);
      save_state();
      gl_.GenFramebuffers(1,&home_fbo_);gl_.BindFramebuffer(0x8d40,home_fbo_);
      gl_.FramebufferTexture2D(0x8d40,0x8ce0,GL_TEXTURE_2D_,home_cache_.id,0);
      const bool complete=gl_.CheckFramebufferStatus(0x8d40)==0x8cd5;
      gl_.BindFramebuffer(0x8d40,static_cast<GLuint>(original_fbo));
      if(!complete){gl_.DeleteFramebuffers(1,&home_fbo_);home_fbo_=0;gl_.DeleteTextures(1,&home_cache_.id);home_cache_={};paint();return;}
    }
    if(!home_cache_valid_||home_cache_revision_!=revision){
      ++counters_.home_cache_repaints;
      save_state();
      gl_.BindFramebuffer(0x8d40,home_fbo_);gl_.Viewport(0,0,w,h);
      // Transparent content layer: straight-alpha fragments from paint() are
      // accumulated as premultiplied RGB with correct coverage, so the cached
      // layer can be composited over a live backdrop without covering it.
      gl_.ClearColor(0,0,0,0);gl_.Clear(GL_COLOR_BUFFER_BIT_);
      gl_.BlendFuncSeparate(GL_SRC_ALPHA_,GL_ONE_MINUS_SRC_ALPHA_,GL_ONE_,GL_ONE_MINUS_SRC_ALPHA_);
      try{paint();flush();}catch(...){gl_.BindFramebuffer(0x8d40,static_cast<GLuint>(original_fbo));gl_.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);gl_.BlendFunc(GL_SRC_ALPHA_,GL_ONE_MINUS_SRC_ALPHA_);home_cache_valid_=false;throw;}
      gl_.BindFramebuffer(0x8d40,static_cast<GLuint>(original_fbo));
      gl_.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);
      home_cache_revision_=revision;home_cache_valid_=true;
    }else ++counters_.home_cache_hits;
    // The cache is already premultiplied; blend its RGB exactly once and keep
    // blending enabled so the live backdrop shows through transparent pixels.
    gl_.BlendFuncSeparate(GL_ONE_,GL_ONE_MINUS_SRC_ALPHA_,GL_ONE_,GL_ONE_MINUS_SRC_ALPHA_);
    quad({0,0,DesignWidth,DesignHeight},5,{1,1,1,1},{},0,0,home_cache_.id,{0,1,1,0});
    flush();
    gl_.BlendFunc(GL_SRC_ALPHA_,GL_ONE_MINUS_SRC_ALPHA_);
  }
  void Renderer::release_caches(){
    flush();
    for(auto& [key,t]:texts_){
      (void)key;
      if(t.tex.id)gl_.DeleteTextures(1,&t.tex.id);
    }
    texts_.clear();
    metrics_.clear();
    ellipses_.clear();
    text_bytes_=0;
    // Detach/delete the framebuffer first: an FBO still bound to the texture
    // keeps the storage alive even after glDeleteTextures on some drivers.
    if(home_fbo_){gl_.DeleteFramebuffers(1,&home_fbo_);home_fbo_=0;}
    if(home_cache_.id)gl_.DeleteTextures(1,&home_cache_.id);
    home_cache_={};
    home_cache_valid_=false;
    home_cache_revision_=0;
  }
  float Renderer::density()const{
    return std::round(std::clamp(view_.scale,.5f,4.f)*8.f)/8.f;
  }
  Renderer::TextKey Renderer::text_key(const std::string& value,float size,uint32_t max_bits)const{
    const float d=density();
    const int px=std::max(8,static_cast<int>(std::lround(size*d)));
    return {px,static_cast<int>(d*8),value,max_bits};
  }
  void Renderer::quad(Rect r,int mode,Color c,Color c2,float radius,float border,GLuint tex,const std::array<float,4>& uv){
    if(r.w<=0||r.h<=0||(c.a<=0&&(mode!=2||c2.a<=0)))return;
    c.a*=opacity_;
    c2.a*=opacity_;
    if(c.a<=0&&(mode!=2||c2.a<=0))return;
    float x=r.x+offset_x_,y=r.y+offset_y_,x2=x+r.w,y2=y+r.h;
    std::array<float,24> vertices{
      x,y,uv[0],uv[1],x2,y,uv[2],uv[1],x2,y2,uv[2],uv[3],x,y,uv[0],uv[1],x2,y2,uv[2],uv[3],x,y2,uv[0],uv[3]
    };
    submit(vertices,mode,c,c2,r.w,r.h,radius,border,.9f/view_.scale,tex?tex:white_.id);
  }
  void Renderer::rect(Rect r,Color c,float radius,float stroke){
    quad(r,0,c,c,radius,stroke,white_.id);
  }
  void Renderer::gradient(Rect r,Color a,Color b,float radius){
    quad(r,2,a,b,radius,0,white_.id);
  }
  void Renderer::image(const std::string& key,Rect r,float alpha,float radius){
    image_mode(key,r,alpha,radius,1);
  }
  void Renderer::image_mode(const std::string& key,Rect r,float alpha,float radius,int mode){
    auto it=images_.find(key);
    if(it==images_.end())return;
    auto& t=it->second;
    float source=static_cast<float>(t.w)/t.h,dest=r.w/r.h;
    std::array<float,4> uv{
      0,0,1,1
    };
    if(source>dest){
      float u=dest/source;
      uv[0]=(1-u)*.5f;
      uv[2]=1-uv[0];
    }
    else{
      float v=source/dest;
      uv[1]=(1-v)*.5f;
      uv[3]=1-uv[1];
    }
    quad(r,mode,{
      1,1,1,alpha
    },
    {},
    radius,0,t.id,uv);
  }
  void Renderer::backdrop(const std::string& from,const std::string& to,float blend){
    constexpr Rect canvas{0,0,1920,1080};
    if(composite_background_){
      auto source=images_.find(from);if(source==images_.end())return;
      auto target=images_.find(to);if(target==images_.end())target=source;
      flush();
      backdrop_mix_=target==source?0:std::clamp(blend,0.f,1.f);
      gl_.Uniform1f(backdrop_blend_,backdrop_mix_);
      gl_.ActiveTexture(GL_TEXTURE0_+1);gl_.BindTexture(GL_TEXTURE_2D_,target->second.id);
      gl_.ActiveTexture(GL_TEXTURE0_);
      gl_.Disable(GL_BLEND_);
      image_mode(from,canvas,1,0,4);
      flush();
      gl_.Enable(GL_BLEND_);
    }else{
      image_mode(from,canvas,1,0,1);
      if(to!=from&&blend>0)image_mode(to,canvas,blend,0,1);
      gradient({0,0,1920,430},{0,0,.02f,.58f},{0,0,.02f,0});
      gradient({0,670,1920,410},{0,0,.02f,0},{0,0,.02f,.88f});
      rect(canvas,{.02f,.04f,.09f,.16f});
    }
  }
  static std::vector<unsigned long> decode_utf8(const std::string& s){
    std::vector<unsigned long> out;
    for(size_t i=0;i<s.size()&&out.size()<512;){
      auto b=static_cast<unsigned char>(s[i++]);
      if(b<128){
        out.push_back(b);
        continue;
      }
      int n=b>=0xf0?3:b>=0xe0?2:b>=0xc0?1:0;
      unsigned long c=b&((1<<(6-n))-1);
      bool valid=n>0&&i+static_cast<size_t>(n)<=s.size();
      for(int j=0;j<n&&valid;++j){
        auto v=static_cast<unsigned char>(s[i]);
        if((v&0xc0)!=0x80){
          valid=false;
          break;
        }
        c=(c<<6)|(v&63);
        ++i;
      }
      out.push_back(valid?c:static_cast<unsigned long>('?'));
    }
    return out;
  }
  TextTexture Renderer::rasterize(const std::string& value,float logical_size){
    float density=this->density();
    ++counters_.rasterizations;
    ++frame_rasterizations_;
    int px=std::max(8,static_cast<int>(std::lround(logical_size*density)));
    if(ft_.SetPixels(ft_.face,0,static_cast<FT_UInt>(px)))throw std::runtime_error("Font size unavailable");
    struct Glyph {
      int x,top,w,h;
      std::vector<unsigned char> pixels;
    };
    std::vector<Glyph> glyphs;
    int pen=0,asc=0,desc=0;
    unsigned previous=0;
    for(auto c:decode_utf8(value)){
      auto idx=ft_.CharIndex(ft_.face,c);
      if(!idx){
        c='?';
        idx=ft_.CharIndex(ft_.face,c);
      }
      if(previous&&idx){
        FT_Vector kern{};
        if(!ft_.Kerning(ft_.face,previous,idx,0,&kern))pen+=static_cast<int>(kern.x/64);
      }
      if(ft_.LoadChar(ft_.face,c,FT_LOAD_RENDER))continue;
      auto slot=ft_.face->glyph;
      auto& b=slot->bitmap;
      Glyph g{
        pen+slot->bitmap_left,slot->bitmap_top,static_cast<int>(b.width),static_cast<int>(b.rows),{}
      };
      g.pixels.resize(static_cast<size_t>(g.w)*g.h);
      for(int y=0;y<g.h;++y){
        int sy=b.pitch>=0?y:g.h-1-y;
        auto* row=b.buffer+static_cast<ptrdiff_t>(sy)*std::abs(b.pitch);
        for(int x=0;x<g.w;++x)g.pixels[static_cast<size_t>(y)*g.w+x]=b.pixel_mode==1?((row[x/8]&(0x80>>(x%8)))?255:0):row[x];
      }
      asc=std::max(asc,g.top);
      desc=std::max(desc,g.h-g.top);
      pen+=static_cast<int>(slot->advance.x/64);
      glyphs.push_back(std::move(g));
      previous=idx;
    }
    int min_x=0,max_x=pen;
    for(auto& g:glyphs){
      min_x=std::min(min_x,g.x);
      max_x=std::max(max_x,g.x+g.w);
    }
    int w=std::clamp(max_x-min_x+4,1,texture_limit_),h=std::clamp(asc+desc+4,1,texture_limit_);
    constexpr size_t budget=24*1024*1024;
    w=std::min(w,static_cast<int>(budget/(static_cast<size_t>(h)*4)));
    const size_t bytes=static_cast<size_t>(w)*h*4;
    raster_scratch_.assign(bytes,0);
    auto& rgba=raster_scratch_;
    evict_texts(bytes);
    for(auto& g:glyphs)for(int y=0;y<g.h;++y)for(int x=0;x<g.w;++x){
      int dx=g.x-min_x+x+2,dy=asc-g.top+y+2;
      if(dx<0||dx>=w||dy<0||dy>=h)continue;
      auto to=(static_cast<size_t>(dy)*w+dx)*4;
      rgba[to]=rgba[to+1]=rgba[to+2]=255;
      rgba[to+3]=std::max(rgba[to+3],g.pixels[static_cast<size_t>(y)*g.w+x]);
    }
    return {
      upload(w,h,rgba.data()),w/density,h/density,frame_
    };
  }
  float Renderer::width(const std::string& value,float size){
    auto key=text_key(value,size);
    auto it=metrics_.find(key);
    if(it!=metrics_.end()){
      ++counters_.metric_hits;
      it->second.last_use=frame_;
      return it->second.value;
    }
    ++counters_.metric_misses;
    const float d=density();
    int px=std::max(8,static_cast<int>(std::lround(size*d)));
    if(ft_.SetPixels(ft_.face,0,static_cast<FT_UInt>(px)))throw std::runtime_error("Font size unavailable");
    int pen=0,min_x=0,max_x=0;
    unsigned previous=0;
    for(auto c:decode_utf8(value)){
      auto idx=ft_.CharIndex(ft_.face,c);
      if(!idx){c='?';idx=ft_.CharIndex(ft_.face,c);}
      if(previous&&idx){
        FT_Vector kern{};
        if(!ft_.Kerning(ft_.face,previous,idx,0,&kern))pen+=static_cast<int>(kern.x/64);
      }
      if(ft_.LoadChar(ft_.face,c,0))continue;
      auto slot=ft_.face->glyph;
      const int left=pen+static_cast<int>(slot->metrics.horiBearingX/64);
      const int glyph_width=static_cast<int>((slot->metrics.width+63)/64);
      min_x=std::min(min_x,left);
      max_x=std::max(max_x,left+glyph_width);
      pen+=static_cast<int>(slot->advance.x/64);
      previous=idx;
    }
    max_x=std::max(max_x,pen);
    float result=std::clamp(max_x-min_x+4,1,texture_limit_)/d;
    if(value.size()<=4096){
      if(metrics_.size()>=1024){
        // std::map::begin() is the smallest key, not the least recently used.
        auto victim=std::min_element(metrics_.begin(),metrics_.end(),[](const auto& a,const auto& b){
          return a.second.last_use<b.second.last_use;
        });
        metrics_.erase(victim);
      }
      metrics_.emplace(std::move(key),MetricEntry{result,frame_});
    }
    return result;
  }
  float Renderer::text(const std::string& value,float x,float y,float size,Color color){
    auto key=text_key(value,size);
    if(value.size()>4096){
      ++counters_.text_misses;
      auto t=rasterize(value,size);
      quad({x,y,t.logical_w,t.logical_h},3,color,color,0,0,t.tex.id);
      flush();
      gl_.DeleteTextures(1,&t.tex.id);
      return t.logical_w;
    }
    auto it=texts_.find(key);
    if(it==texts_.end()){
      ++counters_.text_misses;
      auto t=rasterize(value,size);
      text_bytes_+=t.tex.bytes;
      it=texts_.emplace(std::move(key),std::move(t)).first;
    } else ++counters_.text_hits;
    auto& t=it->second;
    t.last_use=frame_;
    quad({x,y,t.logical_w,t.logical_h},3,color,color,0,0,t.tex.id);
    return t.logical_w;
  }
  std::string Renderer::ellipsis(const std::string& value,float size,float max){
    std::optional<TextKey> key;
    if(value.size()<=4096){
      key=text_key(value,size,std::bit_cast<uint32_t>(max));
      auto found=ellipses_.find(*key);
      if(found!=ellipses_.end()){
        ++counters_.ellipsis_hits;
        found->second.last_use=frame_;
        return found->second.value;
      }
    }
    ++counters_.ellipsis_misses;
    auto result=value;
    if(width(value,size)>max){
      while(!result.empty()&&width(result+"...",size)>max)pop_utf8(result);
      result+="...";
    }
    if(key){
      if(ellipses_.size()>=512){
        auto victim=std::min_element(ellipses_.begin(),ellipses_.end(),[](const auto& a,const auto& b){
          return a.second.last_use<b.second.last_use;
        });
        ellipses_.erase(victim);
      }
      ellipses_.emplace(std::move(*key),EllipsisEntry{result,frame_});
    }
    return result;
  }
  void Renderer::line(float x,float y,float x2,float y2,Color c,float thickness){
    x+=offset_x_;
    x2+=offset_x_;
    y+=offset_y_;
    y2+=offset_y_;
    c.a*=opacity_;
    if(c.a<=0)return;
    float dx=x2-x,dy=y2-y;
    float len=std::sqrt(dx*dx+dy*dy);
    if(len<.001f)return;
    float nx=-dy/len*thickness*.5f,ny=dx/len*thickness*.5f;
    std::array<float,24> v{
      x+nx,y+ny,0,0,x2+nx,y2+ny,1,0,x2-nx,y2-ny,1,1,x+nx,y+ny,0,0,x2-nx,y2-ny,1,1,x-nx,y-ny,0,1
    };
    submit(v,0,c,c,len,thickness,0,0,.5f/view_.scale,white_.id);
  }
  void Renderer::icon(const std::string& name,Rect r,Color c){
    float x=r.x,y=r.y,w=r.w,h=r.h;
    auto l=[&](float a,float b,float d,float e){
      line(x+a*w,y+b*h,x+d*w,y+e*h,c,std::max(1.5f,w*.05f));
    };
    auto box=[&](float a,float b,float d,float e,float rad=2.f){
      rect({
        x+a*w,y+b*h,d*w,e*h
      },
      c,rad,w*.045f);
    };
    if(name=="tv"||name=="input"){
      box(.08f,.16f,.84f,.57f);
      l(.5f,.73f,.5f,.9f);
      l(.3f,.9f,.7f,.9f);
      if(name=="input"){
        l(.3f,.44f,.7f,.44f);
        l(.58f,.32f,.7f,.44f);
        l(.58f,.56f,.7f,.44f);
      }
    }
    else if(name=="cpu"){
      box(.24f,.24f,.52f,.52f);
      box(.36f,.36f,.28f,.28f);
      for(int i=0;i<3;++i){
        float v=.32f+i*.18f;
        l(v,.08f,v,.24f);
        l(v,.76f,v,.92f);
        l(.08f,v,.24f,v);
        l(.76f,v,.92f,v);
      }
    }
    else if(name=="library"||name=="apps"){
      for(int a=0;a<2;++a)for(int b=0;b<2;++b)box(.1f+a*.45f,.1f+b*.45f,.31f,.31f);
    }
    else if(name=="search"){
      rect({
        x+w*.08f,y+h*.08f,w*.62f,h*.62f
      },
      c,w*.31f,w*.05f);
      l(.62f,.62f,.92f,.92f);
    }
    else if(name=="layers"){
      for(int i=0;i<3;++i){
        float yy=.15f+i*.24f;
        l(.1f,yy+.13f,.5f,yy-.07f);
        l(.5f,yy-.07f,.9f,yy+.13f);
        l(.9f,yy+.13f,.5f,yy+.33f);
        l(.5f,yy+.33f,.1f,yy+.13f);
      }
    }
    else if(name=="controls"){
      for(int i=0;i<3;++i){float yy=.22f+i*.28f;float knob=i==1?.66f:.34f;l(.1f,yy,.9f,yy);box(knob-.07f,yy-.1f,.14f,.2f,.03f);}
    }
    else if(name=="settings"){
      rect({
        x+w*.22f,y+h*.22f,w*.56f,h*.56f
      },
      c,w*.28f,w*.07f);
      rect({
        x+w*.4f,y+h*.4f,w*.2f,h*.2f
      },
      c,w*.1f,w*.045f);
      for(int i=0;i<8;++i){
        float a=i*3.14159265f/4;
        l(.5f+std::cos(a)*.27f,.5f+std::sin(a)*.27f,.5f+std::cos(a)*.43f,.5f+std::sin(a)*.43f);
      }
    }
    else if(name=="back"){
      l(.75f,.27f,.25f,.27f);
      l(.25f,.27f,.43f,.1f);
      l(.25f,.27f,.43f,.44f);
      l(.75f,.27f,.75f,.78f);
      l(.75f,.78f,.28f,.78f);
    }
    else if(name=="home"){
      l(.1f,.46f,.5f,.1f);
      l(.5f,.1f,.9f,.46f);
      l(.23f,.36f,.23f,.9f);
      l(.23f,.9f,.77f,.9f);
      l(.77f,.9f,.77f,.36f);
    }
    else if(name=="star"){
      for(int i=0;i<10;++i){
        float a=-3.14159265f/2+i*3.14159265f/5,b=a+3.14159265f/5;
        float r1=i%2?.19f:.46f,r2=i%2?.46f:.19f;
        l(.5f+std::cos(a)*r1,.5f+std::sin(a)*r1,.5f+std::cos(b)*r2,.5f+std::sin(b)*r2);
      }
    }
    else if(name=="refresh"){
      for(int i=0;i<14;++i){
        float a=.5f+i*.36f,b=a+.36f;
        l(.5f+std::cos(a)*.35f,.5f+std::sin(a)*.35f,.5f+std::cos(b)*.35f,.5f+std::sin(b)*.35f);
      }
      l(.85f,.25f,.85f,.55f);
      l(.85f,.55f,.6f,.45f);
    }
    else if(name=="network"){
      for(int row=0;row<3;++row){
        float rr=.2f+row*.18f;
        for(int i=0;i<8;++i){
          float a=3.9f+i*.2f,b=a+.2f;
          l(.5f+std::cos(a)*rr,.8f+std::sin(a)*rr,.5f+std::cos(b)*rr,.8f+std::sin(b)*rr);
        }
      }
      rect({
        x+.45f*w,y+.75f*h,.1f*w,.1f*h
      },
      c,w*.05f);
    }
    else if(name=="play"||name=="media"){
      l(.3f,.16f,.8f,.5f);
      l(.8f,.5f,.3f,.84f);
      l(.3f,.84f,.3f,.16f);
    }
    else if(name=="homebrew"){
      box(.13f,.42f,.74f,.46f);
      l(.5f,.08f,.5f,.63f);
      l(.3f,.43f,.5f,.63f);
      l(.7f,.43f,.5f,.63f);
    }
    else if(name=="display"){
      box(.06f,.12f,.88f,.62f);
      l(.5f,.74f,.5f,.9f);
      l(.3f,.9f,.7f,.9f);
      l(.23f,.56f,.4f,.37f);
      l(.4f,.37f,.54f,.5f);
      l(.54f,.5f,.8f,.26f);
    }
    else if(name=="game"||name=="retropie"){
      box(.05f,.28f,.9f,.48f,w*.17f);
      l(.22f,.41f,.22f,.64f);
      l(.1f,.525f,.34f,.525f);
      rect({
        x+w*.65f,y+h*.38f,w*.09f,h*.09f
      },
      c,w*.045f);
      rect({
        x+w*.79f,y+h*.55f,w*.09f,h*.09f
      },
      c,w*.045f);
    }
    else {
      rect({
        x+w*.1f,y+h*.1f,w*.8f,h*.8f
      },
      c,w*.4f,w*.04f);
      l(.32f,.5f,.46f,.64f);
      l(.46f,.64f,.72f,.34f);
    }
  }
  void Renderer::screenshot(const std::filesystem::path& output){
    flush();
    if(!output.parent_path().empty())std::filesystem::create_directories(output.parent_path());
    int w=view_.drawable_w,h=view_.drawable_h;
    std::vector<unsigned char> data(static_cast<size_t>(w)*h*4),flipped(data.size());
    gl_.Finish();
    gl_.PixelStorei(GL_PACK_ALIGNMENT_,1);
    gl_.ReadPixels(0,0,w,h,GL_RGBA_,GL_UNSIGNED_BYTE_,data.data());
    if(gl_.GetError()!=0)throw std::runtime_error("Framebuffer read failed");
    for(int y=0;y<h;++y)std::memcpy(flipped.data()+static_cast<size_t>(y)*w*4,data.data()+static_cast<size_t>(h-1-y)*w*4,static_cast<size_t>(w)*4);
    png_image image{};
    image.version=PNG_IMAGE_VERSION;
    image.width=w;
    image.height=h;
    image.format=PNG_FORMAT_RGBA;
    if(!png_image_write_to_file(&image,output.string().c_str(),0,flipped.data(),0,nullptr))throw std::runtime_error("Screenshot write failed");
  }
}
