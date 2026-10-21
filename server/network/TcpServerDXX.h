#pragma once
#include <string>
class TcpServerDXX {
public:
    TcpServerDXX(std::string path,int port);
    int run();
private:
    std::string path_;
    int port_;
};
