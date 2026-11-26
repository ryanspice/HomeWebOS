#include "spice/core.hpp"
#include "spice/persistence.hpp"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace spice;
using namespace std::chrono_literals;

namespace {
  int passed=0,failed=0;
  void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
  void test(const char* name,const std::function<void()>& fn){
    try{fn();++passed;std::cout<<"PASS "<<name<<'\n';}
    catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';}
  }
  struct BlockedSink {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered=false,released=false,fail_first=false,fail_last=false;
    std::vector<std::string> writes;
    void write(const std::filesystem::path&,const std::string& value){
      std::unique_lock lock(mutex);
      writes.push_back(value);
      const bool first=writes.size()==1;
      if(first){entered=true;changed.notify_all();changed.wait(lock,[this]{return released;});}
      const bool fail=(first&&fail_first)||(!first&&fail_last);
      lock.unlock();
      if(fail)throw std::runtime_error(first?"first write failed":"last write failed");
    }
    bool wait_entered(){
      std::unique_lock lock(mutex);
      return changed.wait_for(lock,5s,[this]{return entered;});
    }
    void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
  };
  struct ReleaseOnExit {BlockedSink& sink;~ReleaseOnExit(){sink.release();}};
  struct TempDir {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
      ("spice-persistence-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TempDir(){std::filesystem::create_directories(path);}
    ~TempDir(){std::error_code error;std::filesystem::remove_all(path,error);}
  };
}

int main(){
  test("blocked sink coalesces pending snapshots in order and flushes last",[]{
    BlockedSink sink;
    SettingsWriter writer("fixture.json",[&](const auto& path,const auto& value){sink.write(path,value);});
    ReleaseOnExit guard{sink};
    std::string snapshot="first";const auto first=writer.submit(snapshot);snapshot="mutated";
    require(sink.wait_entered(),"sink did not start");
    const auto second=writer.submit("second"),third=writer.submit("third");
    require(writer.poll(first).state==SettingsWriter::State::Writing,"in-flight state missing");
    require(writer.poll(second).state==SettingsWriter::State::Superseded,"replacement not acknowledged");
    require(writer.poll(third).state==SettingsWriter::State::Pending,"pending reported as durable");
    sink.release();const auto last=writer.shutdown();
    require(last.sequence==third&&last.state==SettingsWriter::State::Succeeded,"final snapshot did not flush");
    require(writer.poll(first).state==SettingsWriter::State::Succeeded,"first completion missing");
    require(sink.writes==std::vector<std::string>({"first","third"}),"write order or snapshot changed");
    require(writer.shutdown().sequence==third,"shutdown is not idempotent");
  });
  test("oversized snapshot does not cancel a pending valid write",[]{
    BlockedSink sink;
    SettingsWriter writer("fixture.json",[&](const auto& path,const auto& value){sink.write(path,value);});
    ReleaseOnExit guard{sink};
    const auto first=writer.submit("first");require(sink.wait_entered(),"sink did not start");
    const auto valid=writer.submit("second");
    const auto bad=writer.submit(std::string(131073,'x'));
    require(writer.poll(bad).state==SettingsWriter::State::Failed,"oversized snapshot was not rejected");
    require(writer.poll(valid).state==SettingsWriter::State::Pending,"oversized snapshot dropped a valid pending write");
    sink.release();writer.shutdown();
    require(writer.poll(valid).state==SettingsWriter::State::Succeeded,"valid pending write did not flush");
    require(writer.poll(first).state==SettingsWriter::State::Succeeded,"first write missing");
  });
  test("failed old write cannot replace newer success",[]{
    BlockedSink sink;sink.fail_first=true;
    SettingsWriter writer("fixture.json",[&](const auto& path,const auto& value){sink.write(path,value);});
    ReleaseOnExit guard{sink};
    const auto first=writer.submit("first");require(sink.wait_entered(),"sink did not start");
    const auto second=writer.submit("second");sink.release();const auto last=writer.shutdown();
    const auto failure=writer.poll(first);
    require(failure.state==SettingsWriter::State::Failed&&failure.error=="first write failed","failure disappeared");
    require(last.sequence==second&&last.state==SettingsWriter::State::Succeeded,"stale failure replaced success");
  });
  test("final write failure remains visible after shutdown",[]{
    BlockedSink sink;sink.fail_last=true;
    SettingsWriter writer("fixture.json",[&](const auto& path,const auto& value){sink.write(path,value);});
    ReleaseOnExit guard{sink};
    writer.submit("first");require(sink.wait_entered(),"sink did not start");
    const auto last_sequence=writer.submit("last");sink.release();const auto last=writer.shutdown();
    require(last.sequence==last_sequence&&last.state==SettingsWriter::State::Failed&&last.error=="last write failed","final failure lost");
    bool rejected=false;try{writer.submit("after shutdown");}catch(const std::logic_error&){rejected=true;}
    require(rejected,"submit after shutdown accepted");
  });
  test("history is bounded and evicted result never overwrites latest",[]{
    BlockedSink sink;sink.fail_first=true;
    SettingsWriter writer("fixture.json",[&](const auto& path,const auto& value){sink.write(path,value);});
    ReleaseOnExit guard{sink};
    const auto first=writer.submit("first");require(sink.wait_entered(),"sink did not start");
    SettingsWriter::Sequence latest=0;
    for(std::size_t i=0;i<SettingsWriter::HistoryLimit;++i)latest=writer.submit(std::to_string(i));
    require(writer.poll(first).state==SettingsWriter::State::Unknown,"old history should be evicted");
    sink.release();const auto final=writer.shutdown();
    require(final.sequence==latest&&final.state==SettingsWriter::State::Succeeded,"evicted completion overwrote latest");
    require(writer.poll(first).state==SettingsWriter::State::Unknown,"evicted completion was resurrected");
    require(sink.writes.size()==2&&sink.writes.back()=="63","bounded queue did not keep latest");
  });
  test("default atomic sink preserves preference extras and destructor drains",[]{
    TempDir temp;const auto path=temp.path/"settings.json";
    auto original=Json::parse(R"({"theme":"ps5","extra_example":{"nested":[1,2,3]}})");
    auto prefs=Preferences::from_json(original);prefs.reduced_motion=true;
    const auto snapshot=prefs.json().dump(true)+"\n";
    {SettingsWriter writer(path);writer.submit(snapshot);}
    const auto written=Json::parse(read_file(path));
    require(written.get("reduced_motion").flag(),"preference not saved");
    require(written.get("extra_example").get("nested").size()==3,"unknown nested field lost");
    require(written.get("extra_example").get("nested").at(2).num()==3,"extra value changed");
  });
  test("invalid existing JSON is kept by the startup write gate",[]{
    TempDir temp;const auto path=temp.path/"settings.json";const std::string invalid="{invalid";
    atomic_write(path,invalid);
    bool writable=true;
    try{(void)Preferences::from_json(Json::parse(read_file(path)));}
    catch(const std::exception&){writable=false;}
    {SettingsWriter writer(path);if(writable)writer.submit("{}\n");const auto ack=writer.shutdown();
      require(ack.state==SettingsWriter::State::Unknown&&ack.sequence==0,"invalid file entered writer");}
    require(read_file(path)==invalid,"invalid settings overwritten");
  });
  test("default sink reports filesystem failure",[]{
    TempDir temp;const auto path=temp.path/"directory";std::filesystem::create_directory(path);
    SettingsWriter writer(path);const auto seq=writer.submit("{}\n");const auto ack=writer.shutdown();
    require(ack.sequence==seq&&ack.state==SettingsWriter::State::Failed&&!ack.error.empty(),"failed replace reported success");
    require(std::filesystem::is_directory(path),"failed replace damaged destination");
  });
  test("oversized latest snapshot fails without cancelling pending write",[]{
    BlockedSink sink;
    SettingsWriter writer("fixture.json",[&](const auto& path,const auto& value){sink.write(path,value);});
    ReleaseOnExit guard{sink};
    writer.submit("first");require(sink.wait_entered(),"sink did not start");
    const auto pending=writer.submit("older");
    const auto too_big=writer.submit(std::string(SettingsWriter::SnapshotLimit+1,'x'));
    require(writer.poll(pending).state==SettingsWriter::State::Pending,"valid pending write was cancelled by oversized input");
    require(writer.poll(too_big).state==SettingsWriter::State::Failed,"oversized save not rejected");
    sink.release();const auto final=writer.shutdown();
    require(final.sequence==too_big&&final.state==SettingsWriter::State::Failed,"latest failure was lost");
    require(sink.writes==std::vector<std::string>({"first","older"}),"valid pending data was not written or oversized data leaked");
  });
  std::cout<<passed<<" passed / "<<failed<<" failed\n";
  return failed?1:0;
}
