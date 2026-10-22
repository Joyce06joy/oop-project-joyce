#include "RequestDispatcherDXX.h"
#include "ProtocolDXX.h"
#include "BusinessErrorDXX.h"
#include "PasswordDXX.h"
#include <algorithm>
#include <regex>
#include <iostream>
RequestDispatcherDXX::RequestDispatcherDXX(DatabaseDXX& db):auth_(db),accounts_(db),friends_(db),recommendations_(db),mutations_(db),groups_(db),messages_(db) {}
void RequestDispatcherDXX::disconnected(std::uint64_t connection) { sessions_.erase(connection); }
JsonDXX RequestDispatcherDXX::dispatch(std::uint64_t connection,const JsonDXX& r) {
    auto error=[&](const std::string& c,const std::string& m) { return ProtocolDXX::error(r,c,m); };
    if(!r.isObject()||r["kind"].text()!="request"||!r["type"].isString()||!r["data"].isObject()||!r["request_id"].isString()||!std::regex_match(r["request_id"].text(),std::regex("[A-Za-z0-9_-]{16,64}"))) return error("PROTOCOL_ERROR","无效的请求格式或请求编号");
    const auto type=r["type"].text();
    const std::vector<std::string> allowed={"auth.register","auth.login","auth.logout","account.services.list","account.service.activate","system.ping",
        "account.service.open","account.binding.get","account.binding.create","account.binding.remove",
        "friend.search","friend.list","friend.request.send","friend.request.pending","friend.request.respond",
        "friend.remove","friend.remark.set","friend.mutual.list","friend.recommend.same","friend.recommend.cross","friend.cross.common",
        "group.create","group.list","group.detail","group.join.apply","group.invite","group.request.list","group.request.respond",
        "group.member.kick","group.member.leave","group.admin.set","group.announcement.update","group.name.update","group.disband",
        "group.subgroup.create","group.subgroup.list","group.subgroup.members.update","group.policy.switch",
        "chat.private.open","chat.private.send","chat.group.open","chat.group.send","chat.conversation.list","chat.history.get","chat.sync","chat.message.ack"};
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
        const auto user=session["user_id"].integer();
        if(type.rfind("chat.",0)==0) {
            messages_.authorize(user,active,r);
            if(type=="chat.private.open"||type=="chat.private.send"||type=="chat.group.send"||type=="chat.message.ack") {
                auto result=mutations_.execute(user,r,[&] {return messages_.mutate(user,active,r);});
                if(!result.second) publish(result.first["notifications"]);
                return ProtocolDXX::response(r,result.first["result"]);
            }
            return ProtocolDXX::response(r,messages_.read(user,active,r));
        }
        if(type.rfind("group.",0)==0) {
            groups_.authorize(user,active,r);
            if(type=="group.list"||type=="group.detail"||type=="group.request.list"||type=="group.subgroup.list") return ProtocolDXX::response(r,groups_.read(user,active,r));
            auto result=mutations_.execute(user,r,[&] { return groups_.mutate(user,active,r); });
            if(!result.second) { groups_.committed(result.first); publish(result.first["notifications"]); }
            return ProtocolDXX::response(r,result.first["result"]);
        }
        if(type=="account.binding.get") return ProtocolDXX::response(r,accounts_.binding(user));
        if(type=="friend.mutual.list"||type=="friend.recommend.same"||type=="friend.recommend.cross"||type=="friend.cross.common") return ProtocolDXX::response(r,recommendations_.read(user,active,r));
        if(type=="friend.search"||type=="friend.list"||type=="friend.request.pending") return ProtocolDXX::response(r,friends_.read(user,active,r));
        const bool accountMutation=type=="account.service.open"||type=="account.binding.create"||type=="account.binding.remove";
        const bool friendMutation=type=="friend.request.send"||type=="friend.request.respond"||type=="friend.remove"||type=="friend.remark.set";
        if(accountMutation||friendMutation) {
            // Authorize the current connection on every attempt, including replays.
            if(friendMutation) friends_.authorize(user,active,r);
            auto result=mutations_.execute(user,r,[&] {
                if(type=="account.service.open") {
                    auto opened=accounts_.open(user,r["data"]);
                    auto data=opened["result"]; data.set("services",auth_.services(user,active));
                    opened.set("result",data); return opened;
                }
                if(type=="account.binding.create"||type=="account.binding.remove") return accounts_.bind(user,r["data"],type=="account.binding.create");
                return friends_.mutate(user,active,r);
            });
            if(!result.second) publish(result.first["notifications"]);
            return ProtocolDXX::response(r,result.first["result"]);
        }
        if(type=="account.service.activate") {
            const auto requested=r["data"]["service_type"].text(); bool owns=false;
            for(const auto& s:auth_.services(session["user_id"].integer()).items()) if(s["service_type"].text()==requested) owns=true;
            if(!owns) return error("SERVICE_NOT_OPEN","未开通该服务");
            if(std::find(active.begin(),active.end(),requested)==active.end()) active.push_back(requested);
        }
        auto services=auth_.services(session["user_id"].integer(),active); session.set("services",services);
        return ProtocolDXX::response(r,JsonDXX{{"services",services}});
    } catch(const BusinessErrorDXX& e) {
        return error(e.code(),e.what());
    } catch(const std::exception&) {
        // Do not log exception bodies: SQLite messages may contain user data.
        std::cerr<<"database request failed"<<std::endl;
        return error("DATABASE_ERROR","数据库操作未成功，请稍后重试；注册请保留原请求编号");
    }
}

std::vector<std::pair<std::uint64_t,JsonDXX>> RequestDispatcherDXX::takeEvents() { auto result=std::move(events_); events_.clear(); return result; }
void RequestDispatcherDXX::publish(const JsonDXX& notifications) {
    // Only committed facts reach this method. All sessions live on this worker.
    for(const auto& n:notifications.items()) {
        JsonDXX event{{"kind","event"},{"event_id",PasswordDXX::randomId()},{"type",n["type"]},{"data",n["data"]}};
        for(const auto& entry:sessions_) {
            if(entry.second["user_id"].integer()!=n["user_id"].integer()) continue;
            bool visible=n["service_type"].text().empty();
            for(const auto& s:entry.second["services"].items()) if(s["service_type"].text()==n["service_type"].text()&&s["active"].boolean()) visible=true;
            if(visible) events_.emplace_back(entry.first,event);
        }
    }
}
