#pragma once
#include "spice/designs.hpp"
#include "spice/render.hpp"
#include "spice/services.hpp"
#include "spice/prepared.hpp"
#include "spice/inventory.hpp"
#include "spice/theme.hpp"
#include "spice/persistence.hpp"
namespace spice {
  // One bounded procedural painter per catalog scene family. Draws only with the
  // native primitive renderer: no images, decoders, downloads or per-design textures.
  void draw_design_scene(Renderer& renderer,const std::string& scene,const Rect& bounds,const Design& design);
  class Deck {
    Renderer& renderer_;
    Services& services_;
    std::filesystem::path config_,assets_;
    bool webos_=false;
    bool config_writable_=true;
    Preferences prefs_;
    SettingsWriter settings_writer_;
    SettingsWriter::Sequence save_sequence_=0;
    SettingsWriter::Ack save_ack_{};
    std::string save_error_;
    bool settings_finalized_=false;
    const Theme* theme_=nullptr;
    std::vector<AppEntry> apps_;
    std::vector<Node> nodes_;
    int focus_=0,home_focus_=6;
    std::string view_="home",media_="Games",selected_="sample.welcome",query_,category_="picture";
    float panel_progress_=1;
    FocusMotion focus_motion_;
    ArtworkMotion artwork_motion_;
    AmbientMotion ambience_;
    double last_ambient_frame_=-1;
    bool dirty_=true,quit_=false,active_=true;
    bool motion_redraw_=false;
    uint64_t home_content_revision_=1;
    std::string home_clock_;
    double next_clock_=0;
    double time_=0,next_cpu_=0,toast_until_=0,visible_since_=0;
    int page_=0,settings_page_=0;
    std::string toast_,last_key_="None",backend_notice_,confirmation_,return_view_;
    Processor processor_;
    std::deque<double> cpu_history_;
    Json device_,inputs_,retro_,settings_;
    Json prepared_report_=Json::object();
    bool device_snapshot_=false;
    InventoryReview inventory_review_;
    bool scan_pending_=false,review_only_=false;
    bool library_keyboard_=false;
    std::string library_search_focus_;
    struct ViewContext {
      std::string view,focus,query,selected,category;
      int page=0,settings_page=0;
      bool review_only=false;
    };
    std::vector<ViewContext> navigation_;
    std::string scan_message_,scan_source_,last_inventory_notice_,selected_title_,identity_key_;
    std::vector<std::string> selected_identity_;
    uint64_t interaction_epoch_=0,scan_epoch_=0,dwell_epoch_=0;
    PreviewState preview_;
    std::deque<std::string> events_;
    FrameSummary frames_;
    int swap_=0;
    Viewport viewport_;
    DesignCatalog designs_;
    bool designs_loaded_=false;
    std::string design_category_="All",design_selected_;
    std::string design_return_view_="home",design_return_focus_;
    int design_page_=0;
    bool design_inspector_=false,design_keyboard_=false;
    static constexpr char design_keys_[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    void ensure_designs();
    void rebuild_designs(std::string preferred="");
    void rebuild();
    void focus_changed();
    void sync_library_selection();
    void rebuild_library(const std::string& preferred="");
    void restore_previous();
    void activate(std::string id);
    void open(const std::string& view);
    void launch(const std::string& app);
    void note(const std::string& message);
    void save();
    void observe_save(const SettingsWriter::Ack& ack);
    std::string settings_state()const;
    Json persistence_report()const;
    AppEntry entry(const std::string& node)const;
    std::vector<AppEntry> visible_apps()const;
    void draw_home();
    void draw_home_backdrop();
    void draw_home_content();
    void draw_home_hero();
    void draw_home_refresh();
    void draw_ambience();
    // Right-column home widgets share the tile/ambient motion layer but are
    // painted after renderer_.home_layer so the cached home content stays put.
    std::array<Rect,3> widget_rects()const;
    float widget_breath(int index)const;
    Rect widget_pose(const Rect& base,int index)const;
    void draw_home_widgets();
    void draw_panel();
    void draw_control();
    void draw_library_search();
    void draw_designs();
    void draw_focus(Rect rect,float opacity=1);
    Presentation presentation()const;
    void button(const Node& node,const std::string& label,const std::string& icon="",bool on=false,const std::string& slot="");
    void label(const std::string& text,float x,float y,float size=21,Color c={
      .72f,.77f,.86f,1
    }
    );
    Color accent()const;
    void glyph(const std::string& slot,const std::string& fallback,Rect rect,Color color);
    std::string app_icon_key(const AppEntry& app)const;
    std::string app_artwork_key(const AppEntry& app)const;
    public:
    Deck(Renderer&,Services&,std::filesystem::path config,std::filesystem::path assets,bool webos,const PreparedData* prepared=nullptr,const Theme* theme=nullptr);
    void action(const std::string& name);
    void pointer(Point p,bool click);
    void text_input(const std::string& text);
    void backspace();
    bool advance_presentation(double dt);
    bool update(double dt,bool advance_tracks=true);
    bool finish_settings();
    // webOS low-memory notification: release rebuildable GPU caches and force
    // the next Home frame to repaint them from source.
    void release_caches();
    void draw(const Viewport&,const FrameSummary&,int swap);
    void active(bool value);
    bool quit()const{
      return quit_;
    }
    int idle_wait_ms()const;
    bool accepts_text()const{
      return view_=="library"||(view_=="designs"&&!design_inspector_);
    }
    void key_diagnostic(const std::string& value){
      last_key_=value;
    }
    std::string remote_action(const std::string& scancode)const;
    const std::string& view()const{
      return view_;
    }
    Json report()const;
    Json motion_report()const;
  };
}
