#include "SocialRepositoryDXX.h"
#include "BusinessErrorDXX.h"
#include <algorithm>
#include <ctime>
#include <regex>
const std::string SocialRepositoryDXX::Columns="a.account_pk,a.user_id,a.service_type,a.display_id,u.nickname,u.birthday,u.location,u.registered_at";
SocialRepositoryDXX::SocialRepositoryDXX(DatabaseDXX& db):db_(db) {}
bool SocialRepositoryDXX::validService(const std::string& s) { return s=="QQ"||s=="WECHAT"||s=="WEIBO"; }
std::string SocialRepositoryDXX::now() { auto t=std::time(nullptr); std::tm utc{}; gmtime_r(&t,&utc); char value[32]; std::strftime(value,sizeof value,"%Y-%m-%dT%H:%M:%SZ",&utc); return value; }
std::string SocialRepositoryDXX::stringField(const JsonDXX& d,const std::string& key,std::size_t max,bool optional) {
    auto value=d[key]; if(optional&&value.isNull()) return {};
    const auto s=value.text();
    if(!value.isString()||s.size()>max||s.find('\0')!=std::string::npos||(!optional&&s.empty())) throw BusinessErrorDXX("VALIDATION_ERROR","字段无效："+key);
    return s;
}
JsonDXX SocialRepositoryDXX::changed(const JsonDXX& result,const std::vector<JsonDXX>& notices) { return {{"result",result},{"notifications",JsonDXX::array(notices)}}; }
JsonDXX SocialRepositoryDXX::notice(std::int64_t user,const std::string& type,const std::string& service,const JsonDXX& data) { return {{"user_id",user},{"type",type},{"service_type",service},{"data",data}}; }
JsonDXX SocialRepositoryDXX::owned(std::int64_t user,const std::string& s,const std::vector<std::string>& active,bool requireActive) {
    if(!validService(s)) throw BusinessErrorDXX("VALIDATION_ERROR","未知服务");
    auto rows=db_.query("SELECT "+Columns+" FROM accounts a JOIN users u ON u.user_id=a.user_id WHERE a.user_id=? AND a.service_type=? AND a.status='ACTIVE'",{user,s});
    if(rows.empty()) throw BusinessErrorDXX("SERVICE_NOT_OPEN","请先开通该服务");
    if(requireActive&&std::find(active.begin(),active.end(),s)==active.end()) throw BusinessErrorDXX("SERVICE_NOT_ACTIVE","请先确认激活该服务");
    return rows[0];
}
JsonDXX SocialRepositoryDXX::target(const std::string& s,const std::string& display) {
    auto rows=db_.query("SELECT "+Columns+" FROM accounts a JOIN users u ON u.user_id=a.user_id WHERE a.service_type=? AND a.display_id=? AND a.status='ACTIVE'",{s,display});
    if(rows.empty()) throw BusinessErrorDXX("ACCOUNT_NOT_FOUND","未找到该服务账号");
    return rows[0];
}
JsonDXX SocialRepositoryDXX::byId(std::int64_t pk) {
    auto rows=db_.query("SELECT "+Columns+" FROM accounts a JOIN users u ON u.user_id=a.user_id WHERE a.account_pk=? AND a.status='ACTIVE'",{pk});
    if(rows.empty()) throw BusinessErrorDXX("ACCOUNT_NOT_FOUND","账号不可用"); return rows[0];
}
bool SocialRepositoryDXX::friends(std::int64_t a,std::int64_t b) { return !db_.query("SELECT 1 FROM friendships WHERE account_a=? AND account_b=?",{std::min(a,b),std::max(a,b)}).empty(); }
bool SocialRepositoryDXX::pending(std::int64_t a,std::int64_t b) { return !db_.query("SELECT 1 FROM friend_requests WHERE status='PENDING' AND ((sender_account_pk=? AND target_account_pk=?) OR (sender_account_pk=? AND target_account_pk=?))",{a,b,b,a}).empty(); }
bool SocialRepositoryDXX::bound(std::int64_t user) { return !db_.query("SELECT 1 FROM account_bindings b JOIN accounts q ON q.account_pk=b.qq_account_pk JOIN accounts w ON w.account_pk=b.wechat_account_pk WHERE q.user_id=? AND w.user_id=? AND q.status='ACTIVE' AND w.status='ACTIVE'",{user,user}).empty(); }
JsonDXX SocialRepositoryDXX::profile(const JsonDXX& a,std::int64_t actor) {
    const auto pk=a["account_pk"].integer(); std::string relationship="NONE",remark;
    if(pk==actor) relationship="SELF";
    else if(friends(actor,pk)) {
        relationship="FRIEND"; auto r=db_.query("SELECT remark FROM friend_remarks WHERE owner_account_pk=? AND friend_account_pk=?",{actor,pk}); if(!r.empty()) remark=r[0]["remark"].text();
    } else {
        auto requests=db_.query("SELECT sender_account_pk FROM friend_requests WHERE status='PENDING' AND ((sender_account_pk=? AND target_account_pk=?) OR (sender_account_pk=? AND target_account_pk=?))",{actor,pk,pk,actor});
        if(!requests.empty()) relationship=requests[0]["sender_account_pk"].integer()==actor?"OUTGOING_PENDING":"INCOMING_PENDING";
    }
    JsonDXX result{{"service_type",a["service_type"]},{"display_id",a["display_id"]},{"nickname",a["nickname"]},{"birthday",a["birthday"]},{"location",a["location"]},{"registered_at",a["registered_at"]},{"remark",remark},{"relationship",relationship}};
    if(a.has("mutual_count")) result.set("mutual_count",a["mutual_count"]);
    if(a.has("reason")) result.set("reason",a["reason"]);
    return result;
}
JsonDXX SocialRepositoryDXX::page(const std::string& sql,const std::vector<JsonDXX>& params,const JsonDXX& d,std::int64_t actor,bool profiles) {
    std::int64_t limit=20,offset=0;
    if(d.has("limit")) { if(!d["limit"].isInteger()) throw BusinessErrorDXX("VALIDATION_ERROR","limit须为整数"); limit=d["limit"].integer(); }
    if(limit<1||limit>20) throw BusinessErrorDXX("VALIDATION_ERROR","每页数量须为1–20");
    if(d.has("cursor")&&!d["cursor"].isNull()) {
        auto s=d["cursor"].text(); if(!d["cursor"].isString()||!std::regex_match(s,std::regex("[0-9]{1,7}"))) throw BusinessErrorDXX("VALIDATION_ERROR","分页游标无效");
        offset=std::stoll(s); if(offset>1000000) throw BusinessErrorDXX("VALIDATION_ERROR","分页游标过大");
    }
    const auto total=db_.query("SELECT COUNT(*) AS total FROM ("+sql+")",params).at(0)["total"].integer();
    auto paging=params; paging.emplace_back(limit); paging.emplace_back(offset);
    auto rows=db_.query(sql+" LIMIT ? OFFSET ?",paging); std::vector<JsonDXX> result;
    for(const auto& row:rows) result.push_back(profiles?profile(row,actor):row);
    const auto end=offset+static_cast<std::int64_t>(result.size());
    return {{"items",JsonDXX::array(result)},{"total",total},{"next_cursor",end<total&&!result.empty()?JsonDXX(std::to_string(end)):JsonDXX()}};
}
