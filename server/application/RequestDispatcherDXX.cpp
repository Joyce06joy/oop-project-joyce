#include "RequestDispatcherDXX.h"
#include "ProtocolDXX.h"
#include <algorithm>
#include <regex>
#include <iostream>
RequestDispatcherDXX::RequestDispatcherDXX(DatabaseDXX& db):auth_(db) {}
void RequestDispatcherDXX::disconnected(std::uint64_t connection) { sessions_.erase(connection); }
JsonDXX RequestDispatcherDXX::dispatch(std::uint64_t connection,const JsonDXX& r) {
    auto error=[&](const std::string& c,const std::string& m) { return ProtocolDXX::error(r,c,m); };
    if(!r.isObject()||r["kind"].text()!="request"||!r["type"].isString()||!r["data"].isObject()||!r["request_id"].isString()||!std::regex_match(r["request_id"].text(),std::regex("[A-Za-z0-9_-]{16,64}"))) return error("PROTOCOL_ERROR","无效的请求格式或请求编号");
    const auto type=r["type"].text();
    const std::vector<std::string> allowed={"auth.register","auth.login","auth.logout","account.services.list","account.service.activate","system.ping"};
    if(std::find(allowed.begin(),allowed.end(),type)==allowed.end()) return error("PROTOCOL_ERROR","未知请求类型");
    std::cout<<"request "<<type<<" connection "<<connection<<std::endl;
    try {
        if(type=="system.ping") return ProtocolDXX::response(r,JsonDXX{{"protocol_version",1}});
        if(type=="auth.register") return auth_.registerUser(r);
        if(type=="auth.login") {
            auto result=auth_.login(r);
            if(result["success"].boolean()) sessions_[connection]=result["data"];
            return result;
        }
        auto it=sessions_.find(connection);
        if(it==sessions_.end()) return error("AUTH_REQUIRED","请先登录");
        if(type=="auth.logout") { sessions_.erase(it); return ProtocolDXX::response(r,JsonDXX{{"logged_out",true}}); }
        auto& session=it->second;
        std::vector<std::string> active;
        for(const auto& s:session["services"].items()) if(s["active"].boolean()) active.push_back(s["service_type"].text());
        if(type=="account.service.activate") {
            const auto requested=r["data"]["service_type"].text(); bool owns=false;
            for(const auto& s:auth_.services(session["user_id"].integer()).items()) if(s["service_type"].text()==requested) owns=true;
            if(!owns) return error("SERVICE_NOT_OPEN","未开通该服务");
            if(std::find(active.begin(),active.end(),requested)==active.end()) active.push_back(requested);
        }
        auto services=auth_.services(session["user_id"].integer(),active); session.set("services",services);
        return ProtocolDXX::response(r,JsonDXX{{"services",services}});
    } catch(const std::exception&) {
        // Do not log exception bodies: SQLite messages may contain user data.
        std::cerr<<"database request failed"<<std::endl;
        return error("DATABASE_ERROR","数据库操作未成功，请稍后重试；注册请保留原请求编号");
    }
}
