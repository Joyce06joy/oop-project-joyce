#include "ProtocolDXX.h"
#include <stdexcept>
std::string ProtocolDXX::frame(const JsonDXX& value) {
    auto raw=value.dump(); if(raw.empty()||raw.size()>MaxPayload) throw std::runtime_error("invalid frame length");
    const auto n=static_cast<std::uint32_t>(raw.size());
    std::string out; for(int i=3;i>=0;--i) out.push_back(static_cast<char>((n>>(i*8))&255)); return out+raw;
}
std::vector<JsonDXX> ProtocolDXX::feed(const char* data,std::size_t size) {
    buffer_.append(data,size); std::vector<JsonDXX> out;
    while(buffer_.size()>=4) {
        std::uint32_t n=0; for(int i=0;i<4;++i) n=(n<<8)|static_cast<unsigned char>(buffer_[i]);
        if(n==0||n>MaxPayload) throw std::runtime_error("invalid frame length");
        if(buffer_.size()<n+4) break;
        out.push_back(JsonDXX::parse(buffer_.substr(4,n))); buffer_.erase(0,n+4);
    }
    if(buffer_.size()>MaxPayload+4) throw std::runtime_error("receive buffer limit");
    return out;
}
JsonDXX ProtocolDXX::response(const JsonDXX& r,const JsonDXX& d) { return {{"kind","response"},{"type",r["type"]},{"request_id",r["request_id"]},{"success",true},{"data",d},{"error",nullptr}}; }
JsonDXX ProtocolDXX::error(const JsonDXX& r,const std::string& code,const std::string& message) {
    // Untrusted envelope fields must not amplify a legal request into an
    // oversized response. Malformed correlation fields are returned as null.
    auto bounded=[&](const std::string& key) { const auto v=r[key]; return v.isString()&&v.text().size()<=64?v:JsonDXX(); };
    return {{"kind","response"},{"type",bounded("type")},{"request_id",bounded("request_id")},{"success",false},{"data",nullptr},{"error",JsonDXX{{"code",code},{"message",message}}}};
}
