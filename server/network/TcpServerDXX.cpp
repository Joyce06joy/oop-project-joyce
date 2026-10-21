#include "TcpServerDXX.h"
#include "ProtocolDXX.h"
#include "DatabaseDXX.h"
#include "RequestDispatcherDXX.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <condition_variable>
#include <deque>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <tuple>
namespace { volatile sig_atomic_t stopping=0; void stopSignal(int) { stopping=1; } }
TcpServerDXX::TcpServerDXX(std::string path,int port):path_(std::move(path)),port_(port) {}
int TcpServerDXX::run() {
    stopping=0; std::signal(SIGINT,stopSignal); std::signal(SIGTERM,stopSignal); std::signal(SIGPIPE,SIG_IGN);
    const int listener=socket(AF_INET,SOCK_STREAM,0);
    if(listener<0) { std::cerr<<"socket creation failed\n"; return 1; }
    int one=1; setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    sockaddr_in address{}; address.sin_family=AF_INET; address.sin_port=htons(static_cast<std::uint16_t>(port_)); inet_pton(AF_INET,"127.0.0.1",&address.sin_addr);
    if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof address)<0||listen(listener,32)<0||fcntl(listener,F_SETFL,O_NONBLOCK)<0) { close(listener); std::cerr<<"cannot listen on loopback port\n"; return 1; }
    // Network thread never touches SQLite. The worker owns DB and all sessions.
    std::mutex mutex; std::condition_variable ready; bool done=false;
    std::deque<std::pair<std::uint64_t,JsonDXX>> tasks,results;
    std::promise<void> initialized; auto startup=initialized.get_future();
    std::thread worker([&] {
        try {
            DatabaseDXX db(path_); RequestDispatcherDXX dispatcher(db); initialized.set_value();
            for(;;) {
                std::pair<std::uint64_t,JsonDXX> task;
                { std::unique_lock<std::mutex> lock(mutex); ready.wait(lock,[&] {return done||!tasks.empty();}); if(tasks.empty()&&done) break; task=std::move(tasks.front()); tasks.pop_front(); }
                if(task.second.isNull()) { dispatcher.disconnected(task.first); continue; }
                auto result=dispatcher.dispatch(task.first,task.second);
                auto events=dispatcher.takeEvents();
                { std::lock_guard<std::mutex> lock(mutex);
                  results.emplace_back(task.first,std::move(result));
                  for(auto& event:events) results.push_back(std::move(event));
                }
            }
        } catch(...) { try { initialized.set_exception(std::current_exception()); } catch(...) {} }
    });
    try { startup.get(); } catch(const std::exception& e) { std::cerr<<"startup: "<<e.what()<<std::endl; close(listener); worker.join(); return 1; }
    std::cout<<"IMServer listening 127.0.0.1:"<<port_<<std::endl;
    // Parallel maps avoid introducing an anonymous session class without files.
    std::map<int,std::uint64_t> ids; std::map<int,ProtocolDXX> decoders; std::map<int,std::string> output; std::map<std::uint64_t,int> pending;
    std::uint64_t serial=0;
    auto enqueue=[&](std::uint64_t id,const JsonDXX& request) { std::lock_guard<std::mutex> lock(mutex); tasks.emplace_back(id,request); ready.notify_one(); };
    auto disconnect=[&](int fd) { const auto id=ids.at(fd); close(fd); ids.erase(fd); decoders.erase(fd); output.erase(fd); pending.erase(id); enqueue(id,JsonDXX()); std::cout<<"disconnected "<<id<<std::endl; };
    while(!stopping) {
        { std::lock_guard<std::mutex> lock(mutex);
          while(!results.empty()) { auto result=std::move(results.front()); results.pop_front(); for(const auto& entry:ids) if(entry.second==result.first) { try { output[entry.first]+=ProtocolDXX::frame(result.second); }
                    catch(const std::exception&) { output[entry.first]+=ProtocolDXX::frame(ProtocolDXX::error(JsonDXX({}),"SERVER_ERROR","Response encoding failed")); }
                    if(result.second["kind"].text()=="response") --pending[result.first]; break; } }
        }
        std::vector<pollfd> polls{{listener,POLLIN,0}};
        for(const auto& entry:ids) polls.push_back({entry.first,static_cast<short>((pending[entry.second]<8?POLLIN:0)|(output[entry.first].empty()?0:POLLOUT)),0});
        if(poll(polls.data(),static_cast<nfds_t>(polls.size()),20)<0) { if(errno==EINTR) continue; break; }
        if(polls[0].revents&POLLIN) {
            int fd=accept(listener,nullptr,nullptr);
            if(fd>=0) { if(ids.size()>=64||fcntl(fd,F_SETFL,O_NONBLOCK)<0) close(fd); else { ids[fd]=++serial; decoders.emplace(fd,ProtocolDXX()); pending[serial]=0; std::cout<<"connected "<<serial<<std::endl; } }
        }
        for(std::size_t i=1;i<polls.size();++i) {
            const int fd=polls[i].fd; const auto events=polls[i].revents; bool lost=false;
            if(events&(POLLERR|POLLHUP|POLLNVAL)) lost=true;
            if(!lost&&(events&POLLIN)) {
                char bytes[8192]; const auto n=recv(fd,bytes,sizeof bytes,0);
                if(n==0) lost=true;
                else if(n<0) { if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR) lost=true; }
                else try {
                    for(const auto& request:decoders[fd].feed(bytes,static_cast<std::size_t>(n))) { if(pending[ids[fd]]>=8) { lost=true; break; } ++pending[ids[fd]]; enqueue(ids[fd],request); }
                } catch(...) { lost=true; std::cerr<<"protocol error: connection closed\n"; }
            }
            if(!lost&&(events&POLLOUT)) {
                auto& bytes=output[fd]; const auto n=send(fd,bytes.data(),bytes.size(),0);
                if(n>0) bytes.erase(0,static_cast<std::size_t>(n));
                else if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR) lost=true;
            }
            if(output[fd].size()>8*(ProtocolDXX::MaxPayload+4)) lost=true;
            if(lost) disconnect(fd);
        }
    }
    close(listener); while(!ids.empty()) disconnect(ids.begin()->first);
    { std::lock_guard<std::mutex> lock(mutex); done=true; } ready.notify_one(); worker.join();
    std::cout<<"IMServer stopped\n"; return 0;
}
