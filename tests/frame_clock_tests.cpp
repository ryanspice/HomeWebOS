#include "spice/core.hpp"
#include "spice/frame_clock.hpp"

#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace spice;
using namespace std::chrono_literals;

namespace {
  int passed=0,failed=0;
  void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
  bool near(double a,double b,double epsilon=0.00001){return std::abs(a-b)<epsilon;}
  void test(const char* name,const std::function<void()>& fn){
    try{fn();++passed;std::cout<<"PASS "<<name<<'\n';}
    catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
  }
  FrameClock::TimePoint ms(int value){return FrameClock::TimePoint{}+std::chrono::milliseconds(value);}
}

int main(){
  test("baseline applies idle time to a new track",[]{
    FocusMotion old;old.snap({0,0,10,10});old.retarget({100,0,10,10});
    old.update(.049,true,false);
    require(near(old.progress(),.049/FocusMotion::Duration),"baseline age differs");
    require(old.value().x>70,"late-idle baseline should already be most of the way");
  });
  test("early and late input start at their event boundary",[]{
    for(int when:{1,49}){
      FrameClock clock(ms(0));FocusMotion motion;motion.snap({0,0,10,10});
      motion.update(clock.advance(ms(when)).animation_seconds,true,false);
      motion.retarget({100,0,10,10});clock.event_batch_end(ms(when));
      motion.update(clock.advance(ms(when)).animation_seconds,true,false);
      require(motion.value().x==0&&motion.progress()==0,"new track inherited pre-input time");
      motion.update(clock.advance(ms(when+5)).animation_seconds,true,false);
      require(near(motion.progress(),.005/FocusMotion::Duration),"post-input time not applied");
    }
  });
  test("existing motion advances before repeated retargets",[]{
    FrameClock clock(ms(0));FocusMotion motion;motion.snap({0,0,10,10});
    motion.retarget({100,0,10,10});
    motion.update(clock.advance(ms(30)).animation_seconds,true,false);
    const auto first=motion.value();require(first.x>0,"old track did not advance");
    motion.retarget({200,0,10,10});clock.event_batch_end(ms(30));
    require(near(motion.value().x,first.x)&&motion.progress()==0,"retarget jumped");
    motion.update(clock.advance(ms(55)).animation_seconds,true,false);
    const auto second=motion.value();require(second.x>first.x,"new track did not advance");
    motion.retarget({-50,0,10,10});clock.event_batch_end(ms(55));
    require(near(motion.value().x,second.x)&&motion.progress()==0,"second retarget jumped");
    for(int t:{105,155,195})motion.update(clock.advance(ms(t)).animation_seconds,true,false);
    require(motion.settled()&&motion.value().x==-50,"retarget did not settle exactly");
  });
  test("active stall retains the existing 50 ms clamp",[]{
    FrameClock clock(ms(0));const auto tick=clock.advance(ms(300));
    require(near(tick.elapsed_seconds,.3)&&near(tick.animation_seconds,.05),"wall and clamped time confused");
    FocusMotion motion;motion.snap({0,0,10,10});motion.retarget({100,0,10,10});
    motion.update(tick.animation_seconds,true,false);
    require(near(motion.progress(),.05/FocusMotion::Duration),"stall did not use bounded policy");
  });
  test("batch processing is excluded and clock never reverses",[]{
    FrameClock clock(ms(0));require(near(clock.advance(ms(40)).animation_seconds,.04),"initial interval");
    clock.event_batch_end(ms(60));
    require(near(clock.advance(ms(50)).animation_seconds,0),"clock reversed");
    require(near(clock.advance(ms(65)).animation_seconds,.005),"batch processing leaked");
  });
  test("lifecycle freezes and resume excludes hidden time",[]{
    FrameClock clock(ms(0));FocusMotion motion;motion.snap({0,0,10,10});motion.retarget({100,0,10,10});
    motion.update(clock.advance(ms(20)).animation_seconds,true,false);const auto before=motion.value();
    motion.update(clock.advance(ms(10000)).animation_seconds,false,false);clock.resume(ms(10000));
    motion.update(clock.advance(ms(10000)).animation_seconds,true,false);
    require(near(motion.value().x,before.x),"hidden time advanced focus");
    motion.update(clock.advance(ms(10010)).animation_seconds,true,false);
    require(motion.value().x>before.x,"resume did not advance");
  });
  test("zero-time update resolves reduced motion",[]{
    FocusMotion motion;motion.snap({0,0,10,10});motion.retarget({100,0,10,10});
    require(motion.update(0,true,true)&&motion.settled()&&motion.value().x==100,"reduced motion did not snap");
  });
  test("fixed-step captures still count the input frame",[]{
    float first=0;
    for(int hz:{60,120,240}){
      FocusMotion motion;motion.snap({0,0,10,10});motion.retarget({100,0,10,10});
      for(int i=0;i<hz/10;++i)motion.update(1.0/hz,true,false);
      if(hz==60)first=motion.value().x;
      require(near(motion.value().x,first,.0002),"fixed-step equivalence changed");
    }
  });
  test("known objection: pre-input service update can beat preview cancellation",[]{
    Preferences prefs;prefs.handoff=true;prefs.dwell=1;
    prefs.profiles["com.webos.app.hdmi1"]="native_handoff";
    PreviewState near_threshold;near_threshold.focus("com.webos.app.hdmi1");
    for(int i=0;i<19;++i)
      require(!near_threshold.tick(.05,i*.05,true,false,prefs),"fixture triggered early");
    auto input_first=near_threshold;input_first.suppress();
    require(!input_first.tick(.05,1,true,false,prefs),"Back did not cancel handoff");
    auto update_first=near_threshold;
    const auto triggered=update_first.tick(.05,1,true,false,prefs);
    update_first.suppress();
    require(triggered&&*triggered=="com.webos.app.hdmi1"&&update_first.pending(),
            "expected ordering regression was not reproduced");
  });
  std::cout<<passed<<" passed / "<<failed<<" failed\n";
  return failed?1:0;
}
