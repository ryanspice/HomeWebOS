#pragma once
#include "spice/core.hpp"
namespace spice {
  // Immutable, bounded preparation output. All disk reads and texture preparation
  // happen before interactive frames; this module has no service/network client.
  struct PreparedImage {
    std::string key,png;
    int width=0,height=0;
  };
  struct PreparedData {
    bool loaded=false,graphics_validated=false;
    std::string generation,captured_at,device_captured_at,source_kind,source_label,inventory_scope,error;
    std::vector<AppEntry> apps;
    std::vector<PreparedImage> images;
    Json device=Json::object();
    size_t rgba_bytes=0,compressed_bytes=0,image_count=0;
    Json report()const;
  };
  std::string sha256(std::string_view bytes);
  PreparedData load_prepared(const std::filesystem::path& root,bool read_graphics=true);
}
