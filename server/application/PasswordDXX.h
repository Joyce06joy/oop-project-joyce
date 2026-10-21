#pragma once
#include <string>
class PasswordDXX {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password,const std::string& encoded);
    static std::string fingerprint(const std::string& normalized);
    static std::string randomId();
};
