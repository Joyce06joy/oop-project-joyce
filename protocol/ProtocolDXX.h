#pragma once
#include "JsonDXX.h"
#include <string>
#include <vector>
class ProtocolDXX {
public:
    static constexpr std::size_t MaxPayload=65536;
    static std::string frame(const JsonDXX& value);
    std::vector<JsonDXX> feed(const char* data,std::size_t size);
    static JsonDXX response(const JsonDXX& request,const JsonDXX& data);
    static JsonDXX error(const JsonDXX& request,const std::string& code,const std::string& message);
private:
    std::string buffer_;
};
