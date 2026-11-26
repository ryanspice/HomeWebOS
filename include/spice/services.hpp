#pragma once
#include "spice/core.hpp"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
namespace spice {
  enum class RequestKind {
    Device,Apps,Inputs,Settings,Processor,RetroPie,Launch,Probe
  };
  struct Request {
    RequestKind kind;
    std::string id;
    Preferences prefs;
  };
  struct Response {
    RequestKind kind;
    bool ok=false;
    std::string id,message;
    Json data;
    Processor processor;
  };
  enum class Access {
    Denied,Public,PrivateRead
  };
  Access luna_access(const std::string& uri);
  struct CommandResult {
    int exit_code=-1;
    bool timed_out=false,too_large=false;
    std::string output;
  };
  CommandResult bounded_process(const std::string& executable,const std::vector<std::string>& args,int timeout_ms=4000,size_t output_limit=131072);
  class Services {
    bool webos_=false,private_reads_=false,stopping_=false;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Request> work_;
    std::deque<Response> done_;
    std::array<uint64_t,8> posted_{};
    std::filesystem::path prepared_root_;
    std::thread worker_;
    Processor previous_;
    std::string last_launch_;
    std::chrono::steady_clock::time_point last_launch_time_{};
    void run();
    Response execute(const Request& r);
    Json luna(const std::string& uri,const Json& params);
    public:
    explicit Services(bool webos,std::filesystem::path prepared_root={});
    ~Services();
    Services(const Services&)=delete;
    Services&operator=(const Services&)=delete;
    bool post(Request r);
    std::vector<Response> drain();
    Json request_counts();
    bool worker_started();
    bool private_reads()const{
      return private_reads_;
    }
  };
}
