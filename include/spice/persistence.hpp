#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace spice {
  // One destination per writer. A snapshot must be fully serialized by the
  // caller; neither submit nor poll accesses the filesystem.
  class SettingsWriter {
  public:
    using Sequence = std::uint64_t;
    enum class State { Unknown, Pending, Writing, Succeeded, Failed, Superseded };
    struct Ack {
      Sequence sequence=0;
      State state=State::Unknown;
      std::string error;
    };
    using Sink=std::function<void(const std::filesystem::path&,const std::string&)>;
    static constexpr std::size_t HistoryLimit=64;
    // Match read_file's settings limit so an accepted write remains reloadable.
    static constexpr std::size_t SnapshotLimit=131072;

    explicit SettingsWriter(std::filesystem::path path, Sink sink={});
    ~SettingsWriter();
    SettingsWriter(const SettingsWriter&)=delete;
    SettingsWriter& operator=(const SettingsWriter&)=delete;

    // Replaces the one waiting snapshot, marking it Superseded. The write
    // already in progress finishes before the newest snapshot is attempted.
    // Oversized snapshots receive a Failed acknowledgment and are not written.
    Sequence submit(std::string json_snapshot);
    Ack poll(Sequence sequence)const;
    Ack latest()const;

    // UI owner calls this on orderly exit. It waits for the in-flight and last
    // pending snapshot, returns the latest status, and is safe to call again.
    // Submit/shutdown belong to one owner thread; poll can run concurrently.
    Ack shutdown();

  private:
    struct Job { Sequence sequence; std::string snapshot; };
    void run();
    void set_status(Sequence sequence,State state,std::string error={});
    Ack status_locked(Sequence sequence)const;

    const std::filesystem::path path_;
    Sink sink_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::array<Ack,HistoryLimit> history_{};
    std::optional<Job> pending_;
    Sequence latest_=0;
    bool stopping_=false;
    std::thread worker_;
  };
}
