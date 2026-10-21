#include "TcpServerDXX.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    try {
        const char* home=std::getenv("HOME");
        std::filesystem::path folder=home?std::filesystem::path(home)/"Library/Application Support/IMPlatformMVP1":std::filesystem::path("data");
        std::string path=(folder/"implatform.sqlite3").string(); int port=8888;
        for(int i=1;i<argc;++i) {
            const std::string flag=argv[i];
            if(flag=="--db"&&i+1<argc) path=argv[++i];
            else if(flag=="--port"&&i+1<argc) { const std::string s=argv[++i]; std::size_t used=0; port=std::stoi(s,&used); if(used!=s.size()||port<1||port>65535) throw std::runtime_error("invalid port"); }
            else { std::cerr<<"Usage: IMServer [--db PATH] [--port PORT] (loopback only)\n"; return 2; }
        }
        const auto parent=std::filesystem::absolute(path).parent_path(); std::filesystem::create_directories(parent);
        TcpServerDXX server(path,port); return server.run();
    } catch(const std::exception& e) { std::cerr<<"IMServer: "<<e.what()<<'\n'; return 1; }
}
