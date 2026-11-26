#pragma once

#include <algorithm>
#include <chrono>

namespace spice {
  // Presentation time advances at most 50 ms per update. The elapsed value is
  // also exposed separately so callers do not mistake the clamp for wall time.
  class FrameClock {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    struct Tick {
      double elapsed_seconds;
      double animation_seconds;
    };

    explicit FrameClock(TimePoint now):last_(now){}

    Tick advance(TimePoint now){
      now=std::max(now,last_);
      const double elapsed=std::chrono::duration<double>(now-last_).count();
      last_=now;
      return {elapsed,std::min(elapsed,.05)};
    }

    // An input batch is one logical instant. Existing motion is advanced before
    // dispatch; its processing time is excluded from every newly started track.
    void event_batch_end(TimePoint now){last_=std::max(now,last_);}
    void resume(TimePoint now){last_=std::max(now,last_);}

  private:
    TimePoint last_;
  };
}
