#include "ProtocolDXX.h"
#include <iostream>
#include <stdexcept>
namespace { void check(bool ok) { if(!ok) throw std::runtime_error("protocol assertion failed"); } }
int main() {
    try {
        JsonDXX request{{"kind","request"},{"type","system.ping"},{"request_id","0123456789abcdef"},{"data",JsonDXX{{"中文","测试"}}}};
        const auto ordered=JsonDXX::parse(R"({"service_type":"QQ","target_display_id":"10002","remark":"同学"})");
        const auto reordered=JsonDXX::parse(R"({"remark":"同学","target_display_id":"10002","service_type":"QQ"})");
        check(ordered.canonicalDump()==reordered.canonicalDump());
        check(JsonDXX(20).isInteger()&&!JsonDXX(true).isInteger()&&!JsonDXX("20").isInteger());
        auto copied=request; copied.set("type","auth.logout"); check(request["type"].text()=="system.ping");
        auto child=request["data"]; child.set("中文","changed"); check(request["data"]["中文"].text()=="测试");
        const auto frame=ProtocolDXX::frame(request); ProtocolDXX decoder; int count=0;
        for(char c:frame) { auto messages=decoder.feed(&c,1); count+=static_cast<int>(messages.size()); if(!messages.empty()) check(messages[0].dump()==request.dump()); }
        check(count==1); auto joined=frame+frame; check(decoder.feed(joined.data(),joined.size()).size()==2);
        const std::vector<std::string> invalid={std::string(4,'\0'),std::string("\0\1\0\1",4),std::string("\0\0\0\2\xff\xfe",6),std::string("\0\0\0\1{",5)};
        for(const auto& bytes:invalid) { bool threw=false; try { ProtocolDXX bad; bad.feed(bytes.data(),bytes.size()); } catch(...) { threw=true; } check(threw); }
        auto huge=request; huge.set("type",std::string(65400,'x'));
        check(ProtocolDXX::frame(ProtocolDXX::error(huge,"PROTOCOL_ERROR","unknown type")).size()<1024);
        auto response=ProtocolDXX::response(request,JsonDXX{{"ok",true}}); check(response["success"].boolean()&&response["error"].isNull());
        check(ProtocolDXX::error(request,"PROTOCOL_ERROR","bad")["error"]["code"].text()=="PROTOCOL_ERROR");
        std::cout<<"Protocol: fragmented/coalesced frames, invalid length/UTF8/JSON, envelopes PASS\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
