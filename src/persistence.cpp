#include "spice/persistence.hpp"
#include "spice/core.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace spice {
  SettingsWriter::SettingsWriter(std::filesystem::path path,Sink sink)
    :path_(std::move(path)),sink_(std::move(sink)){
    if(!sink_)sink_=[](const auto& destination,const auto& contents){atomic_write(destination,contents);};
    worker_=std::thread(&SettingsWriter::run,this);
  }

  SettingsWriter::~SettingsWriter(){
    {
      std::lock_guard lock(mutex_);
      stopping_=true;
    }
    ready_.notify_one();
    if(worker_.joinable())worker_.join();
  }

  void SettingsWriter::set_status(Sequence sequence,State state,std::string error){
    // Bound both the number and the size of retained acknowledgments.
    if(error.size()>512)error.resize(512);
    history_[sequence%HistoryLimit]={sequence,state,std::move(error)};
  }

  SettingsWriter::Ack SettingsWriter::status_locked(Sequence sequence)const{
    if(sequence==0)return {};
    const auto& ack=history_[sequence%HistoryLimit];
    return ack.sequence==sequence?ack:Ack{sequence,State::Unknown,{}};
  }

  SettingsWriter::Sequence SettingsWriter::submit(std::string snapshot){
    std::lock_guard lock(mutex_);
    if(stopping_)throw std::logic_error("Settings writer is shut down");
    if(latest_==std::numeric_limits<Sequence>::max())throw std::overflow_error("Settings sequence exhausted");
    const Sequence sequence=++latest_;
    if(snapshot.size()>SnapshotLimit){
      // Reject before touching pending_: an oversized snapshot must not cancel
      // an earlier, valid write that is still queued for the worker.
      set_status(sequence,State::Failed,"Settings snapshot exceeds the 131072-byte load limit");
      return sequence;
    }
    if(pending_){set_status(pending_->sequence,State::Superseded);pending_.reset();}
    set_status(sequence,State::Pending);
    pending_=Job{sequence,std::move(snapshot)};
    ready_.notify_one();
    return sequence;
  }

  SettingsWriter::Ack SettingsWriter::poll(Sequence sequence)const{
    std::lock_guard lock(mutex_);
    return status_locked(sequence);
  }

  SettingsWriter::Ack SettingsWriter::latest()const{
    std::lock_guard lock(mutex_);
    return status_locked(latest_);
  }

  SettingsWriter::Ack SettingsWriter::shutdown(){
    {
      std::lock_guard lock(mutex_);
      stopping_=true;
    }
    ready_.notify_one();
    if(worker_.joinable())worker_.join();
    return latest();
  }

  void SettingsWriter::run(){
    for(;;){
      std::optional<Job> job;
      {
        std::unique_lock lock(mutex_);
        ready_.wait(lock,[this]{return stopping_||pending_.has_value();});
        if(!pending_&&stopping_)return;
        job=std::move(pending_);
        pending_.reset();
        set_status(job->sequence,State::Writing);
      }
      State result=State::Succeeded;
      std::string error;
      try{sink_(path_,job->snapshot);}
      catch(const std::exception& e){result=State::Failed;error=e.what();}
      catch(...){result=State::Failed;error="Unknown settings write error";}
      {
        std::lock_guard lock(mutex_);
        // A slow old write must never evict or replace a newer ring entry.
        if(status_locked(job->sequence).sequence==job->sequence&&
           status_locked(job->sequence).state!=State::Unknown)
          set_status(job->sequence,result,std::move(error));
      }
    }
  }
}
