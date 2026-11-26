#pragma once
#include "spice/core.hpp"
#include "spice/runtime.hpp"
#include <compare>
#include <cstddef>
#include <map>
#include <memory>
#include <functional>
namespace spice {
  struct PreparedData;
  struct Theme;
#ifdef _WIN32
#define SPICE_GL_CALL __stdcall
#else
#define SPICE_GL_CALL
#endif
  using GLenum=unsigned;
  using GLuint=unsigned;
  using GLint=int;
  using GLsizei=int;
  using GLboolean=unsigned char;
  using GLfloat=float;
  using GLchar=char;
  using GLsizeiptr=ptrdiff_t;
  struct Gl {
    const unsigned char*(SPICE_GL_CALL *GetString)(GLenum);
    void(SPICE_GL_CALL *GetIntegerv)(GLenum,GLint*);
    void(SPICE_GL_CALL *GetShaderPrecisionFormat)(GLenum,GLenum,GLint*,GLint*)=nullptr;
    GLenum(SPICE_GL_CALL *GetError)();
    void(SPICE_GL_CALL *Viewport)(GLint,GLint,GLsizei,GLsizei);
    void(SPICE_GL_CALL *ClearColor)(GLfloat,GLfloat,GLfloat,GLfloat);
    void(SPICE_GL_CALL *Clear)(GLenum);
    void(SPICE_GL_CALL *Enable)(GLenum);
    void(SPICE_GL_CALL *Disable)(GLenum);
    void(SPICE_GL_CALL *BlendFunc)(GLenum,GLenum);
    void(SPICE_GL_CALL *BlendFuncSeparate)(GLenum,GLenum,GLenum,GLenum);
    GLuint(SPICE_GL_CALL *CreateShader)(GLenum);
    void(SPICE_GL_CALL *ShaderSource)(GLuint,GLsizei,const GLchar* const*,const GLint*);
    void(SPICE_GL_CALL *CompileShader)(GLuint);
    void(SPICE_GL_CALL *GetShaderiv)(GLuint,GLenum,GLint*);
    void(SPICE_GL_CALL *GetShaderInfoLog)(GLuint,GLsizei,GLsizei*,GLchar*);
    void(SPICE_GL_CALL *DeleteShader)(GLuint);
    GLuint(SPICE_GL_CALL *CreateProgram)();
    void(SPICE_GL_CALL *AttachShader)(GLuint,GLuint);
    void(SPICE_GL_CALL *BindAttribLocation)(GLuint,GLuint,const GLchar*);
    void(SPICE_GL_CALL *LinkProgram)(GLuint);
    void(SPICE_GL_CALL *GetProgramiv)(GLuint,GLenum,GLint*);
    void(SPICE_GL_CALL *GetProgramInfoLog)(GLuint,GLsizei,GLsizei*,GLchar*);
    void(SPICE_GL_CALL *UseProgram)(GLuint);
    void(SPICE_GL_CALL *DeleteProgram)(GLuint);
    GLint(SPICE_GL_CALL *GetUniformLocation)(GLuint,const GLchar*);
    void(SPICE_GL_CALL *Uniform1i)(GLint,GLint);
    void(SPICE_GL_CALL *Uniform1f)(GLint,GLfloat);
    void(SPICE_GL_CALL *Uniform2f)(GLint,GLfloat,GLfloat);
    void(SPICE_GL_CALL *Uniform4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
    void(SPICE_GL_CALL *GenBuffers)(GLsizei,GLuint*);
    void(SPICE_GL_CALL *BindBuffer)(GLenum,GLuint);
    void(SPICE_GL_CALL *BufferData)(GLenum,GLsizeiptr,const void*,GLenum);
    void(SPICE_GL_CALL *BufferSubData)(GLenum,ptrdiff_t,GLsizeiptr,const void*);
    void(SPICE_GL_CALL *DeleteBuffers)(GLsizei,const GLuint*);
    void(SPICE_GL_CALL *EnableVertexAttribArray)(GLuint);
    void(SPICE_GL_CALL *DisableVertexAttribArray)(GLuint);
    void(SPICE_GL_CALL *VertexAttribPointer)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);
    void(SPICE_GL_CALL *DrawArrays)(GLenum,GLint,GLsizei);
    void(SPICE_GL_CALL *GenTextures)(GLsizei,GLuint*);
    void(SPICE_GL_CALL *BindTexture)(GLenum,GLuint);
    void(SPICE_GL_CALL *TexParameteri)(GLenum,GLenum,GLint);
    void(SPICE_GL_CALL *TexImage2D)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
    void(SPICE_GL_CALL *DeleteTextures)(GLsizei,const GLuint*);
    void(SPICE_GL_CALL *ActiveTexture)(GLenum);
    void(SPICE_GL_CALL *PixelStorei)(GLenum,GLint);
    void(SPICE_GL_CALL *ReadPixels)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
    void(SPICE_GL_CALL *Finish)();
    void(SPICE_GL_CALL *GenFramebuffers)(GLsizei,GLuint*)=nullptr;
    void(SPICE_GL_CALL *DeleteFramebuffers)(GLsizei,const GLuint*)=nullptr;
    void(SPICE_GL_CALL *BindFramebuffer)(GLenum,GLuint)=nullptr;
    void(SPICE_GL_CALL *FramebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint)=nullptr;
    GLenum(SPICE_GL_CALL *CheckFramebufferStatus)(GLenum)=nullptr;
    explicit Gl(Sdl& sdl);
  };
  struct Color {
    float r=1,g=1,b=1,a=1;
    Color opacity(float x)const{
      return {
        r,g,b,a*x
      };
    }
    static Color hex(const std::string& s);
  };
  struct Texture {
    GLuint id=0;
    int w=0,h=0;
    size_t bytes=0;
  };
  struct TextTexture {
    Texture tex;
    float logical_w=0,logical_h=0;
    uint64_t last_use=0;
  };
  struct RenderCounters {
    uint64_t home_cache_hits=0,home_cache_repaints=0;
    double uniform_submit_ms=0,vertex_upload_ms=0,draw_submit_ms=0;
    uint64_t draws=0, vertex_upload_bytes=0, texture_uploads=0, texture_upload_bytes=0;
    uint64_t primitives=0,vertex_upload_calls=0,buffer_orphans=0;
    uint64_t rasterizations=0, text_hits=0, text_misses=0, text_evictions=0;
    uint64_t metric_hits=0, metric_misses=0;
    uint64_t ellipsis_hits=0,ellipsis_misses=0;
  };
  class Renderer {
    Gl gl_;
    FreeType ft_;
    GLuint program_=0,vbo_=0;
    GLuint backdrop_program_=0,copy_program_=0,home_fbo_=0;
    Texture home_cache_;
    uint64_t home_cache_revision_=0;
    bool home_cache_valid_=false;
    GLint backdrop_color_=0,backdrop_mix_location_=0;
    float backdrop_mix_=0;
    Texture white_;
    GLint mode_=0,color_=0,color2_=0,size_=0,radius_=0,border_=0,aa_=0;
    GLint backdrop_blend_=0;
    struct TextKey {
      int px=0,density=0;
      std::string value;
      uint32_t max_bits=0;
      auto operator<=>(const TextKey&)const=default;
    };
    struct MetricEntry{float value;uint64_t last_use;};
    struct EllipsisEntry{std::string value;uint64_t last_use;};
    std::map<std::string,Texture> images_;
    std::map<TextKey,TextTexture> texts_;
    std::map<TextKey,MetricEntry> metrics_;
    std::map<TextKey,EllipsisEntry> ellipses_;
    std::vector<unsigned char> raster_scratch_;
    RenderCounters counters_;
    uint64_t frame_=0,frame_rasterizations_=0;
    size_t text_bytes_=0,image_bytes_=0,theme_bytes_=0;
    int texture_limit_=2048;
    Viewport view_;
    float offset_x_=0,offset_y_=0,opacity_=1;
    static constexpr size_t BatchStride=18,BatchFloats=512*6*BatchStride;
    struct ImmediateCommand {int mode;Color color,color2;float width,height,radius,border,aa;GLuint texture;};
    std::unique_ptr<ImmediateCommand[]> immediate_commands_;
    size_t immediate_count_=0;
    bool batched_=false,profiling_=false;
    bool composite_background_=true,fragment_highp_=false;
    std::unique_ptr<float[]> batch_data_;
    size_t batch_count_=0,upload_cursor_=0;
    GLuint batch_texture_=0;
    GLuint compile(GLenum type,const std::string& src);
    Texture upload(int w,int h,const unsigned char* bytes);
    Texture png(const std::filesystem::path& path);
    Texture prepared_png(std::string_view bytes,bool opaque);
    TextTexture rasterize(const std::string& text,float size);
    float density()const;
    TextKey text_key(const std::string& value,float size,uint32_t max_bits=0)const;
    void evict_texts(size_t incoming);
    void flush();
    void submit(const std::array<float,24>& vertices,int mode,Color color,Color color2,
                float width,float height,float radius,float border,float aa,GLuint texture);
    void image_mode(const std::string& name,Rect r,float alpha,float radius,int mode);
    void quad(Rect rect,int mode,Color color,Color color2,float radius,float border,GLuint texture,const std::array<float,4>& uv={
      0,0,1,1
    }
    );
    public:
    std::string vendor,renderer,version;
    bool is_es=false;
    void profiling(bool enabled){profiling_=enabled;}
    bool profiling()const{return profiling_;}
    void home_layer(uint64_t revision,const std::function<void()>& paint);
    // One full-drawable RGBA layer. A 3840x2160 content area is 31.64 MiB, so
    // this budget covers every 16:9 panel up to 2160 lines; above that the
    // cache is skipped and the home composition is painted directly.
    static constexpr size_t HomeCacheBudgetBytes=32u*1024u*1024u;
    // True when the drawable captured by the last begin() fits the Home cache
    // budget and the GPU texture limit (it says nothing about FBO support).
    bool home_cache_fits()const;
    // Quantized text density currently derived from the drawable scale; expose
    // it so a resolution override can be validated from the report.
    float text_density()const{
      return density();
    }
    // Drop derived GPU caches only (text atlas entries and the Home layer).
    // Prepared artwork stays resident so the render thread never re-reads files.
    void release_caches();
    size_t home_cache_bytes()const{return home_cache_.bytes;}
    int draws=0;
    Renderer(Sdl& sdl,const std::string& font,const std::filesystem::path& assets,bool batched=false,bool composite_background=true,bool sample_art=true);
    ~Renderer();
    void preload_prepared(const PreparedData& data);
    void preload_theme(const Theme& theme);
    bool has_image(const std::string& key)const{return images_.contains(key);}
    size_t theme_bytes()const{return theme_bytes_;}
    void transform(float x=0,float y=0,float opacity=1){
      offset_x_=x;
      offset_y_=y;
      opacity_=opacity;
    }
    void begin(const Viewport& v);
    void end();
    void rect(Rect r,Color c,float radius=0,float stroke=0);
    void gradient(Rect r,Color top,Color bottom,float radius=0);
    void image(const std::string& name,Rect r,float alpha=1,float radius=0);
    void backdrop(const std::string& from,const std::string& to,float blend);
    float text(const std::string& value,float x,float y,float size,Color color={
      1,1,1,1
    }
    );
    float width(const std::string& value,float size);
    std::string ellipsis(const std::string& value,float size,float max);
    void line(float x1,float y1,float x2,float y2,Color c,float thickness=2);
    void icon(const std::string& name,Rect r,Color c={
      1,1,1,1
    }
    );
    void screenshot(const std::filesystem::path& output);
    const RenderCounters& counters()const{return counters_;}
    uint64_t text_rasterizations_this_frame()const{return frame_rasterizations_;}
    bool batched()const{return batched_;}
    bool composite_background()const{return composite_background_;}
    bool fragment_highp()const{return fragment_highp_;}
    size_t geometry_capacity_bytes()const{return batched_?BatchFloats*sizeof(float):24*512*sizeof(float);}
    size_t raster_scratch_bytes()const{return raster_scratch_.capacity();}
    size_t metric_cache_size()const{return metrics_.size();}
    size_t ellipsis_cache_size()const{return ellipses_.size();}
    size_t texture_bytes()const{
      return text_bytes_+image_bytes_+home_cache_.bytes;
    }
    size_t text_cache_size()const{
      return texts_.size();
    }
    const Viewport& viewport()const{
      return view_;
    }
  };
}
