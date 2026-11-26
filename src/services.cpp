#include "spice/services.hpp"
#include "spice/prepared.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
#ifdef SPICE_HAVE_CURL
#include <curl/curl.h>
#endif
namespace spice {
  Access luna_access(const std::string& uri){
    if(uri=="luna://com.webos.applicationManager/launch"||uri=="luna://com.webos.applicationManager/getAppLoadStatus"||uri=="luna://com.webos.applicationManager/dev/listApps"||uri=="luna://com.webos.service.tv.systemproperty/getSystemInfo"||uri=="luna://com.palm.connectionmanager/getStatus"||uri=="luna://com.webos.settingsservice/getSystemSettings")return Access::Public;
    if(uri=="luna://com.webos.applicationManager/listLaunchPoints"||uri=="luna://com.webos.service.eim/getAllInputStatus")return Access::PrivateRead;
    return Access::Denied;
  }
  CommandResult bounded_process(const std::string& exe,const std::vector<std::string>& args,int timeout_ms,size_t limit){
    if(exe.empty()||exe.find('\0')!=std::string::npos||timeout_ms<1||timeout_ms>30000||limit<1||limit>1048576)throw std::runtime_error("Invalid process limits");
    for(const auto& a:args)if(a.find('\0')!=std::string::npos)throw std::runtime_error("Invalid process argument");
    CommandResult result;
#ifdef _WIN32
    (void)exe;
    (void)args;
    (void)timeout_ms;
    (void)limit;
    throw std::runtime_error("Native Luna transport is available only on webOS/Linux");
#else
    int pipes[2];
    if(pipe(pipes)!=0)throw std::runtime_error("Cannot create Luna pipe");
    fcntl(pipes[0],F_SETFD,FD_CLOEXEC);
    fcntl(pipes[1],F_SETFD,FD_CLOEXEC);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,pipes[1],STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions,pipes[0]);
    posix_spawn_file_actions_addclose(&actions,pipes[1]);
    posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr,0);
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(exe.c_str()));
    for(auto& a:args)argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid=-1;
    int err=posix_spawnp(&pid,exe.c_str(),&actions,&attr,argv.data(),environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    close(pipes[1]);
    if(err!=0){
      close(pipes[0]);
      throw std::runtime_error("Native service executable unavailable or blocked");
    }
    fcntl(pipes[0],F_SETFL,fcntl(pipes[0],F_GETFL)|O_NONBLOCK);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout_ms);
    bool exited=false,eof=false;
    int status=0;
    std::array<char,4096> buffer{};
    while(!exited||!eof){
      auto now=std::chrono::steady_clock::now();
      if(now>=deadline){
        result.timed_out=true;
        kill(-pid,SIGKILL);
        break;
      }
      if(result.too_large){
        kill(-pid,SIGKILL);
        break;
      }
      pollfd fd{
        pipes[0],POLLIN|POLLHUP,0
      };
      int wait=std::min(20,static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline-now).count()));
      int polled=poll(&fd,1,std::max(1,wait));
      if(polled<0&&errno!=EINTR){
        kill(-pid,SIGKILL);
        break;
      }
      if(polled>0){
        for(;;){
          ssize_t n=read(pipes[0],buffer.data(),buffer.size());
          if(n==0){
            eof=true;
            break;
          }
          if(n<0){
            if(errno!=EAGAIN&&errno!=EINTR)eof=true;
            break;
          }
          size_t amount=static_cast<size_t>(n);
          if(result.output.size()+amount>limit){
            result.too_large=true;
            break;
          }
          result.output.append(buffer.data(),amount);
        }
      }
      if(!exited){
        pid_t w=waitpid(pid,&status,WNOHANG);
        if(w==pid)exited=true;
        else if(w<0&&errno!=EINTR){
          exited=true;
          status=127<<8;
        }
      }
    }
    close(pipes[0]);
    if(!exited){
      while(waitpid(pid,&status,0)<0&&errno==EINTR){}
    }
    result.exit_code=WIFEXITED(status)?WEXITSTATUS(status):-1;
    return result;
#endif
  }
  Services::Services(bool webos,std::filesystem::path prepared_root):webos_(webos),prepared_root_(std::move(prepared_root)){
#ifdef SPICE_HAVE_CURL
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw std::runtime_error("HTTP runtime initialization failed");
#endif
    auto* p=std::getenv("SPICE_CPP_PRIVATE_LUNA");
    private_reads_=webos_&&p&&std::string(p)=="1";
  }
  Services::~Services(){
    {
      std::lock_guard lock(mutex_);
      stopping_=true;
      work_.clear();
    }
    cv_.notify_all();
    if(worker_.joinable())worker_.join();
#ifdef SPICE_HAVE_CURL
    curl_global_cleanup();
#endif
  }
  bool Services::post(Request r){
    std::lock_guard lock(mutex_);
    if(webos_&&r.kind==RequestKind::Processor)return false;
    if(stopping_||work_.size()>=8)return false;
    for(auto& q:work_)if(q.kind==r.kind&&q.id==r.id)return false;
    work_.push_back(std::move(r));
    ++posted_[static_cast<size_t>(work_.back().kind)];
    if(!worker_.joinable()){
      try{worker_=std::thread(&Services::run,this);}
      catch(const std::system_error&){--posted_[static_cast<size_t>(work_.back().kind)];work_.pop_back();return false;}
    }
    cv_.notify_one();
    return true;
  }
  std::vector<Response> Services::drain(){
    std::lock_guard lock(mutex_);
    std::vector<Response> out;
    while(!done_.empty()){
      out.push_back(std::move(done_.front()));
      done_.pop_front();
    }
    return out;
  }
  Json Services::request_counts(){
    std::lock_guard lock(mutex_);
    auto out=Json::object();
    const std::array<const char*,8> names{"device","apps","inputs","settings","processor_local","retropie","launch","probe"};
    for(size_t i=0;i<names.size();++i)out.set(names[i],Json::number(static_cast<double>(posted_[i])));
    return out;
  }
  bool Services::worker_started(){std::lock_guard lock(mutex_);return worker_.joinable();}
  void Services::run(){
    for(;;){
      Request request;
      {
        std::unique_lock lock(mutex_);
        cv_.wait(lock,[&]{
          return stopping_||!work_.empty();
        }
        );
        if(stopping_)return;
        request=std::move(work_.front());
        work_.pop_front();
      }
      Response r{
        request.kind,false,request.id,"",Json(),{}
      };
      try{
        r=execute(request);
      }
      catch(const std::exception& e){
        r.message=clean_text(e.what(),140);
      }
      catch(...){
        r.message="Service request failed";
      }
      std::lock_guard lock(mutex_);
      if(done_.size()>=16)done_.pop_front();
      done_.push_back(std::move(r));
    }
  }
  Json Services::luna(const std::string& uri,const Json& params){
    if(!webos_)throw std::runtime_error("Desktop mode: no TV request was sent");
    auto access=luna_access(uri);
    if(access==Access::Denied)throw std::runtime_error("Luna operation not allowlisted");
    if(access==Access::PrivateRead&&!private_reads_)throw std::runtime_error("Private TV read disabled; configured shortcuts remain usable");
    auto body=params.dump();
    if(body.size()>4096)throw std::runtime_error("Luna parameters too large");
    auto command=access==Access::Public?"luna-send-pub":"luna-send";
    auto r=bounded_process(command,{
      "-n","1","-f",uri,body
    }
    );
    if(r.timed_out)throw std::runtime_error("Luna request timed out (4 seconds)");
    if(r.too_large)throw std::runtime_error("Luna response exceeded 128 KiB");
    if(r.exit_code!=0)throw std::runtime_error("Luna denied or failed the request");
    auto pos=r.output.find('{');
    if(pos==std::string::npos)throw std::runtime_error("Luna reply has no JSON object");
    auto j=Json::parse(r.output.substr(pos));
    if(!j.get("returnValue").flag())throw std::runtime_error("TV returned an unavailable or permission-denied result");
    return j;
  }
#ifdef SPICE_HAVE_CURL
  struct HttpBody {
    std::string data;
  };
  static size_t receive_http(char* p,size_t size,size_t count,void* userdata){
    if(size&&count>SIZE_MAX/size)return 0;
    size_t bytes=size*count;
    auto& b=*static_cast<HttpBody*>(userdata);
    if(bytes>65536-b.data.size())return 0;
    b.data.append(p,bytes);
    return bytes;
  }
  static Json retro_read(const Preferences& prefs){
    if(!private_ipv4(prefs.retropie_host))throw std::runtime_error("Set a private LAN IPv4 for RetroPie in settings.json");
    auto* env=std::getenv("SPICE_RETROPIE_TOKEN");
    std::string token=env?env:"";
    if(!safe_token(token))throw std::runtime_error("Set a 24-256 character session SPICE_RETROPIE_TOKEN");
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(),curl_easy_cleanup);
    if(!curl)throw std::runtime_error("Cannot initialize HTTP client");
    auto url="http://"+prefs.retropie_host+":"+std::to_string(prefs.retropie_port)+"/v1/status";
    curl_slist* raw=nullptr;
    raw=curl_slist_append(raw,("Authorization: Bearer "+token).c_str());
    raw=curl_slist_append(raw,"Accept: application/json");
    std::unique_ptr<curl_slist,decltype(&curl_slist_free_all)> headers(raw,curl_slist_free_all);
    HttpBody body;
    curl_easy_setopt(curl.get(),CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_HTTPHEADER,headers.get());
    curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT_MS,1500L);
    curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT_MS,4000L);
    curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(curl.get(),CURLOPT_PROXY,"");
    curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,receive_http);
    curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&body);
    auto code=curl_easy_perform(curl.get());
    long status=0;
    curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status);
    if(code!=CURLE_OK||status!=200)throw std::runtime_error("RetroPie unavailable, rejected authentication, or exceeded response limit");
    auto j=Json::parse(body.data,65536);
    if(j.get("api_version").num()!=1)throw std::runtime_error("Unsupported RetroPie companion version");
    return j;
  }
#endif
  Response Services::execute(const Request& r){
    Response out{
      r.kind,true,r.id,"",Json::object(),{}
    };
    switch(r.kind){
      case RequestKind::Launch:{
        if(!safe_app_id(r.id))throw std::runtime_error("Invalid app ID");
        auto now=std::chrono::steady_clock::now();
        if(r.id==last_launch_&&now-last_launch_time_<std::chrono::milliseconds(700))throw std::runtime_error("Duplicate launch suppressed");
        last_launch_=r.id;
        last_launch_time_=now;
        auto p=Json::object();
        p.set("id",Json::string(r.id));
        out.data=luna("luna://com.webos.applicationManager/launch",p);
        out.message="Native launch acknowledged";
        break;
      }
      case RequestKind::Apps:{
        if(!webos_){
          if(prepared_root_.empty())throw std::runtime_error("No prepared scan source. Prepare TV data first; desktop sent no TV request.");
          auto snapshot=load_prepared(prepared_root_,false);
          auto apps=Json::array();
          for(const auto& app:snapshot.apps){
            auto row=Json::object();row.set("id",Json::string(app.id));row.set("title",Json::string(app.title));
            row.set("description",Json::string(app.description));row.set("vendor",Json::string(app.vendor));row.set("version",Json::string(app.version));
            row.set("inventory_confirmed",Json::boolean(app.installed));row.set("observed_at",Json::string(app.observed_at));apps.add(row);
          }
          out.data.set("apps",apps);out.data.set("source_kind",Json::string("prepared"));out.data.set("inventory_scope",Json::string(snapshot.inventory_scope));
          out.data.set("generation",Json::string(snapshot.generation));out.data.set("source_label",Json::string("Local prepared snapshot"));
          out.message="Prepared metadata scanned; graphics stay loaded from startup";
        }else{
          if(private_reads_){
            try{out.data=luna("luna://com.webos.applicationManager/listLaunchPoints",Json::object());out.data.set("inventory_scope",Json::string("private-full"));}
            catch(...){out.data=Json();}
          }
          if(out.data.empty()||!out.data.get("launchPoints").is(json_type_array)){
            out.data=luna("luna://com.webos.applicationManager/dev/listApps",Json::object());out.data.set("inventory_scope",Json::string("developer"));
          }
          out.data.set("source_kind",Json::string("live"));out.data.set("source_label",Json::string("Explicit TV inventory"));out.message="TV app inventory scanned";
          if(!out.data.get("apps").is(json_type_array)&&!out.data.get("launchPoints").is(json_type_array))throw std::runtime_error("TV inventory response did not contain an app list");
        }
        break;
      }
      case RequestKind::Probe:{
        if(!safe_app_id(r.id))throw std::runtime_error("Invalid app ID");
        auto p=Json::object();
        p.set("id",Json::string(r.id));
        out.data=luna("luna://com.webos.applicationManager/getAppLoadStatus",p);
        out.message="App availability queried";
        break;
      }
      case RequestKind::Inputs:out.data=luna("luna://com.webos.service.eim/getAllInputStatus",Json::object());
      out.message="Input status refreshed";
      break;
      case RequestKind::Device:{
        auto p=Json::object();
        auto keys=Json::array();
        for(auto* key:{
          "modelName","firmwareVersion","sdkVersion","UHD"
        }
        )keys.add(Json::string(key));
        p.set("keys",keys);
        out.data=luna("luna://com.webos.service.tv.systemproperty/getSystemInfo",p);
        try{
          out.data.set("network",luna("luna://com.palm.connectionmanager/getStatus",Json::object()));
        }
        catch(...){
          out.data.set("network_status",Json::string("Unavailable / permission dependent"));
        }
        out.message="TV information queried";
        break;
      }
      case RequestKind::Settings:{
        // One bounded category per request; failures cannot stall the render thread.
        const std::map<std::string,std::vector<std::string>> groups{
          {
            "picture",{
              "pictureMode","brightness","contrast","sharpness","energySaving"
            }
          },
          {
            "sound",{
              "soundOutput","soundMode","autoVolume","avSync"
            }
          },
          {
            "general",{
              "language","timeZone","country","automaticUpdate"
            }
          },
          {
            "accessibility",{
              "highContrast","audioGuidance","subtitle","closedCaption"
            }
          },
          {
            "privacy",{
              "acrAllowed","lmt","doNotSellMyPersonalInformation"
            }
          },
          {
            "network",{
              "wifiState","wiredState","networkStatus"
            }
          }
        };
        auto group=groups.find(r.id);
        if(group==groups.end())throw std::runtime_error("Settings category not allowlisted");
        auto keys=Json::array();
        for(auto& key:group->second)keys.add(Json::string(key));
        for(auto& key:r.prefs.custom_settings){
          auto dot=key.find('.');
          if(safe_setting(key)&&key.substr(0,dot)==r.id)keys.add(Json::string(key.substr(dot+1)));
        }
        auto p=Json::object();
        p.set("category",Json::string(r.id));
        p.set("keys",keys);
        out.data=luna("luna://com.webos.settingsservice/getSystemSettings",p);
        out.message="Settings read only; nothing changed on TV";
        break;
      }
      case RequestKind::Processor:{
#ifdef _WIN32
        MEMORYSTATUSEX m{};
        m.dwLength=sizeof(m);
        if(!GlobalMemoryStatusEx(&m))throw std::runtime_error("Host memory counters unavailable");
        out.processor.available=true;
        out.processor.total_kib=m.ullTotalPhys/1024;
        out.processor.available_kib=m.ullAvailPhys/1024;
        out.processor.memory=m.dwMemoryLoad;
        FILETIME idle{},
        kernel{},
        user{};
        if(GetSystemTimes(&idle,&kernel,&user)){
          auto ticks=[](FILETIME t){
            return (static_cast<uint64_t>(t.dwHighDateTime)<<32)|t.dwLowDateTime;
          };
          out.processor.total_ticks=ticks(kernel)+ticks(user);
          out.processor.idle_ticks=ticks(idle);
          if(previous_.available&&out.processor.total_ticks>previous_.total_ticks)out.processor.cpu=100.*(1.-static_cast<double>(out.processor.idle_ticks-previous_.idle_ticks)/(out.processor.total_ticks-previous_.total_ticks));
        }
#else
        out.processor=parse_processor(read_file("/proc/stat",131072),read_file("/proc/meminfo",65536),read_file("/proc/loadavg",2048),previous_.available?&previous_:nullptr);
#endif
        previous_=out.processor;
        out.message=webos_?"Read-only TV procfs counters":"Read-only DESKTOP host counters (not TV)";
        break;
      }
      case RequestKind::RetroPie:
#ifdef SPICE_HAVE_CURL
      out.data=retro_read(r.prefs);
      out.message="RetroPie companion status received";
      break;
#else
      throw std::runtime_error("This binary was built without libcurl; RetroPie status unavailable");
#endif
    }
    return out;
  }
}
