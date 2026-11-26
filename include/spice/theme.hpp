#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace spice {
  // All I/O and PNG validation happens in load_theme, before interactive frames.
  // Buffers are immutable by convention after preload; lookups perform no I/O.
  struct ThemeImage {
    std::string sha256, png;
    int width=0, height=0;
    bool background=false;
  };
  struct ThemeAppOverride { std::string icon, hero; };
  struct Theme {
    bool loaded=false;
    std::string name, version, provenance, accent, background_policy, generation, error;
    std::string background;
    std::map<std::string,std::string> icons;
    std::map<std::string,ThemeAppOverride> apps;
    std::vector<ThemeImage> images;
    // Includes a 1600x900 RGBA crossfade reserve; excludes prepared/text textures
    // and transient PNG decoder scratch, which are accounted independently.
    size_t rgba_bytes=0, compressed_bytes=0;
    const ThemeImage* image(const std::string& digest)const;
    const ThemeImage* global_background()const;
    const ThemeImage* semantic_icon(const std::string& slot)const;
    const ThemeImage* app_icon(const std::string& id)const;
    const ThemeImage* app_hero(const std::string& id)const;
    // Explicit hero wins; otherwise global follows all/missing policy. A null
    // result means the caller retains its prepared-art / built-in fallback.
    const ThemeImage* background_for(const std::string& id,bool has_prepared_hero)const;
  };
  // root is the app-owned themes directory. No pointer means implicit default.
  // Implicit failures return defaults with error; explicit failures throw an
  // actionable exception. A selected built-in default succeeds in either mode.
  Theme load_theme(const std::filesystem::path& root,bool explicit_selection=false);
}
