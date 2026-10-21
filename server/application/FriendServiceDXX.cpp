#include "FriendServiceDXX.h"
#include "BusinessErrorDXX.h"
#include "PasswordDXX.h"
#include <algorithm>
FriendServiceDXX::FriendServiceDXX(DatabaseDXX& db):db_(db),repository_(db) {}
JsonDXX FriendServiceDXX::requestRecord(const JsonDXX& d) {
    const auto id=SocialRepositoryDXX::stringField(d,"friend_request_id",64);
    auto rows=db_.query("SELECT * FROM friend_requests WHERE request_id=?",{id});
    if(rows.empty()) throw BusinessErrorDXX("FRIEND_REQUEST_NOT_FOUND","好友申请不存在"); return rows[0];
}
void FriendServiceDXX::authorize(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
    const auto d=r["data"];
    if(r["type"].text()=="friend.request.respond") {
        const auto record=requestRecord(d); auto target=repository_.byId(record["target_account_pk"].integer());
        if(target["user_id"].integer()!=user) throw BusinessErrorDXX("FRIEND_PERMISSION_DENIED","只有被申请方可以处理申请");
        repository_.owned(user,record["service_type"].text(),active);
    } else repository_.owned(user,SocialRepositoryDXX::stringField(d,"service_type",8),active);
}
JsonDXX FriendServiceDXX::read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
    const auto d=r["data"], actor=repository_.owned(user,SocialRepositoryDXX::stringField(d,"service_type",8),active);
    const auto id=actor["account_pk"]; const auto s=actor["service_type"].text(); const auto type=r["type"].text();
    if(type=="friend.search") {
        const auto query=SocialRepositoryDXX::stringField(d,"query",80);
        std::string escaped; for(char c:query) { if(c=='%'||c=='_'||c=='\\') escaped+='\\'; escaped+=c; }
        return repository_.page("SELECT "+SocialRepositoryDXX::Columns+" FROM accounts a JOIN users u ON u.user_id=a.user_id WHERE a.service_type=? AND a.status='ACTIVE' AND (a.display_id=? OR u.nickname LIKE ? ESCAPE '\\') ORDER BY a.display_id,a.account_pk",{s,query,"%"+escaped+"%"},d,id.integer());
    }
    if(type=="friend.list") return repository_.page("SELECT "+SocialRepositoryDXX::Columns+" FROM friend_edges e JOIN accounts a ON a.account_pk=e.friend JOIN users u ON u.user_id=a.user_id WHERE e.owner=? AND a.status='ACTIVE' ORDER BY a.display_id,a.account_pk",{id},d,id.integer());
    if(type=="friend.request.pending") {
        const std::string sql="SELECT r.request_id AS friend_request_id,r.service_type,s.display_id AS sender_display_id,su.nickname AS sender_nickname,t.display_id AS target_display_id,tu.nickname AS target_nickname,r.verification_message,r.created_at,CASE WHEN r.target_account_pk=? THEN 'INCOMING' ELSE 'OUTGOING' END AS direction FROM friend_requests r JOIN accounts s ON s.account_pk=r.sender_account_pk JOIN users su ON su.user_id=s.user_id JOIN accounts t ON t.account_pk=r.target_account_pk JOIN users tu ON tu.user_id=t.user_id WHERE r.service_type=? AND r.status='PENDING' AND (r.sender_account_pk=? OR r.target_account_pk=?) ORDER BY r.created_at,r.request_id";
        return repository_.page(sql,{id,s,id,id},d,id.integer(),false);
    }
    throw BusinessErrorDXX("PROTOCOL_ERROR","未知好友查询");
}
JsonDXX FriendServiceDXX::mutate(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
    const auto d=r["data"]; const auto type=r["type"].text();
    if(type=="friend.request.respond") {
        const auto record=requestRecord(d); auto receiver=repository_.byId(record["target_account_pk"].integer());
        if(receiver["user_id"].integer()!=user) throw BusinessErrorDXX("FRIEND_PERMISSION_DENIED","只有被申请方可以处理申请");
        const auto s=record["service_type"].text(); repository_.owned(user,s,active);
        const auto decision=SocialRepositoryDXX::stringField(d,"decision",6);
        if(decision!="ACCEPT"&&decision!="REJECT") throw BusinessErrorDXX("VALIDATION_ERROR","请选择接受或拒绝");
        if(record["status"].text()!="PENDING") throw BusinessErrorDXX("FRIEND_REQUEST_ALREADY_RESOLVED","申请已经处理");
        const std::string status=decision=="ACCEPT"?"ACCEPTED":"REJECTED"; const auto time=SocialRepositoryDXX::now();
        const auto from=record["sender_account_pk"].integer(),to=record["target_account_pk"].integer();
        if(decision=="ACCEPT") db_.query("INSERT INTO friendships(account_a,account_b,created_at) VALUES(?,?,?)",{std::min(from,to),std::max(from,to),time});
        db_.query("UPDATE friend_requests SET status=?,resolved_at=? WHERE request_id=? AND status='PENDING'",{status,time,record["request_id"]});
        auto sender=repository_.byId(from);
        JsonDXX result{{"friend_request_id",record["request_id"]},{"status",status}};
        JsonDXX event{{"service_type",s},{"friend_request_id",record["request_id"]},{"status",status}};
        std::vector<JsonDXX> notices;
        for(auto recipient:{sender["user_id"].integer(),user}) {
            notices.push_back(SocialRepositoryDXX::notice(recipient,"friend.request.resolved",s,event));
            if(decision=="ACCEPT") notices.push_back(SocialRepositoryDXX::notice(recipient,"friend.relationship.changed",s,JsonDXX{{"service_type",s},{"change","ADDED"}}));
        }
        return SocialRepositoryDXX::changed(result,notices);
    }
    const auto s=SocialRepositoryDXX::stringField(d,"service_type",8);
    auto actor=repository_.owned(user,s,active),target=repository_.target(s,SocialRepositoryDXX::stringField(d,"target_display_id",64));
    const auto from=actor["account_pk"].integer(),to=target["account_pk"].integer();
    if(from==to) throw BusinessErrorDXX("FRIEND_SELF_FORBIDDEN","不能对自己的账号执行该好友操作");
    if(type=="friend.request.send") {
        const auto message=SocialRepositoryDXX::stringField(d,"verification_message",240,true);
        if(repository_.friends(from,to)) throw BusinessErrorDXX("FRIEND_ALREADY_EXISTS","双方已是好友");
        if(repository_.pending(from,to)) throw BusinessErrorDXX("FRIEND_REQUEST_PENDING","该账号对已有待处理申请，请先处理对向申请");
        const auto id=PasswordDXX::randomId();
        db_.query("INSERT INTO friend_requests(request_id,service_type,sender_account_pk,target_account_pk,verification_message,status,created_at) VALUES(?,?,?,?,?,'PENDING',?)",{id,s,from,to,message,SocialRepositoryDXX::now()});
        JsonDXX result{{"friend_request_id",id},{"status","PENDING"}};
        auto noticeData=result; noticeData.set("service_type",s);
        return SocialRepositoryDXX::changed(result,{SocialRepositoryDXX::notice(user,"friend.request.new",s,noticeData),SocialRepositoryDXX::notice(target["user_id"].integer(),"friend.request.new",s,noticeData)});
    }
    if(!repository_.friends(from,to)) throw BusinessErrorDXX("FRIEND_NOT_FOUND","双方不是好友");
    if(type=="friend.remove") {
        db_.query("DELETE FROM friendships WHERE account_a=? AND account_b=?",{std::min(from,to),std::max(from,to)});
        JsonDXX data{{"service_type",s},{"change","REMOVED"}};
        return SocialRepositoryDXX::changed(JsonDXX{{"removed",true}},{SocialRepositoryDXX::notice(user,"friend.relationship.changed",s,data),SocialRepositoryDXX::notice(target["user_id"].integer(),"friend.relationship.changed",s,data)});
    }
    if(type=="friend.remark.set") {
        const auto remark=SocialRepositoryDXX::stringField(d,"remark",160,true);
        if(remark.empty()) db_.query("DELETE FROM friend_remarks WHERE owner_account_pk=? AND friend_account_pk=?",{from,to});
        else db_.query("INSERT INTO friend_remarks(owner_account_pk,friend_account_pk,remark,updated_at) VALUES(?,?,?,?) ON CONFLICT(owner_account_pk,friend_account_pk) DO UPDATE SET remark=excluded.remark,updated_at=excluded.updated_at",{from,to,remark,SocialRepositoryDXX::now()});
        return SocialRepositoryDXX::changed(JsonDXX{{"remark",remark}},{SocialRepositoryDXX::notice(user,"friend.relationship.changed",s,JsonDXX{{"service_type",s},{"change","REMARK"}})});
    }
    throw BusinessErrorDXX("PROTOCOL_ERROR","未知好友操作");
}
