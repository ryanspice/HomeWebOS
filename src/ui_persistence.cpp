#include "spice/ui.hpp"
namespace spice {
  void Deck::observe_save(const SettingsWriter::Ack& ack){
    if(!save_sequence_||ack.sequence!=save_sequence_)return;
    if(ack.sequence==save_ack_.sequence&&ack.state==save_ack_.state&&ack.error==save_ack_.error)return;
    save_ack_=ack;
    dirty_=true;
    if(ack.state==SettingsWriter::State::Failed)note("Settings save failed: "+ack.error);
  }
  std::string Deck::settings_state()const{
    if(!config_writable_)return "blocked-invalid-original";
    if(!save_error_.empty())return "failed";
    if(!save_sequence_)return "unchanged";
    switch(save_ack_.state){
      case SettingsWriter::State::Pending:return "pending";
      case SettingsWriter::State::Writing:return "writing";
      case SettingsWriter::State::Succeeded:return "saved";
      case SettingsWriter::State::Failed:return "failed";
      case SettingsWriter::State::Superseded:return "superseded";
      case SettingsWriter::State::Unknown:return "unknown";
    }
    return "unknown";
  }
  bool Deck::finish_settings(){
    const auto final=settings_writer_.shutdown();
    observe_save(final);
    settings_finalized_=true;
    return save_error_.empty()&&(!save_sequence_||save_ack_.state==SettingsWriter::State::Succeeded);
  }
  Json Deck::persistence_report()const{
    auto out=Json::object();
    out.set("state",Json::string(settings_state()));
    out.set("sequence",Json::number(static_cast<double>(save_sequence_)));
    out.set("ack_sequence",Json::number(static_cast<double>(save_ack_.sequence)));
    out.set("error",Json::string(save_error_.empty()?save_ack_.error:save_error_));
    out.set("shutdown_drained",Json::boolean(settings_finalized_));
    out.set("write_thread",Json::string("ordered-background-worker"));
    out.set("interactive_file_io",Json::boolean(false));
    return out;
  }
}
