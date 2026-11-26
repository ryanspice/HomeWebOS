#pragma once
#include <cstdint>
namespace spice {
  // Fresh window-directed input can recover a stale focus/hidden notification.
  // Ignore input queued before that notification. The native caller separately
  // disables background controller events before treating navigation as input.
  class WindowActivity {
    bool visible_=true,active_=true;
    uint32_t blurred_at_=0;
  public:
    void blur(uint32_t timestamp){active_=false;blurred_at_=timestamp;}
    void hide(uint32_t timestamp){visible_=false;blur(timestamp);}
    void show(){visible_=true;active_=true;}
    bool active()const{return active_;}
    bool recover_input(bool webos,uint32_t timestamp){
      if(!webos||active_||static_cast<int32_t>(timestamp-blurred_at_)<=0)return false;
      visible_=true;active_=true;return true;
    }
    bool recover_keyboard_focus(bool webos,bool owns_focus){
      if(!webos||active_||!visible_||!owns_focus)return false;
      active_=true;return true;
    }
  };
}
