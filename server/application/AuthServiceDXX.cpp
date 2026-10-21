#include "AuthServiceDXX.h"
#include "PasswordDXX.h"
#include "ProtocolDXX.h"
#include <algorithm>
#include <ctime>
#include <regex>
#include <set>
#include <stdexcept>
namespace {
std::string now() { auto t=std::time(nullptr); std::tm utc{}; gmtime_r(&t,&utc); char out[32]; std::strftime(out,sizeof out,"%Y-%m-%dT%H:%M:%SZ",&utc); return out; }
std::string lower(std::string s) { for(char& c:s) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a'); return s; }
bool bounded(const JsonDXX& j,std::size_t min,std::size_t max) { if(!j.isString()) return false; const auto s=j.text(); return s.size()>=min&&s.size()<=max&&s.find('\0')==std::string::npos; }
bool birthday(const JsonDXX& j) {
    if(j.isNull()) return true;
    const auto s=j.text(); if(!j.isString()||!std::regex_match(s,std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2}"))) return false;
    const int y=std::stoi(s.substr(0,4)),m=std::stoi(s.substr(5,2)),d=std::stoi(s.substr(8,2));
    if(y<1900||m<1||m>12||d<1||s>now().substr(0,10)) return false;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return d<=days[m-1]+(m==2&&(y%400==0||(y%4==0&&y%100!=0)));
}
}
AuthServiceDXX::AuthServiceDXX(DatabaseDXX& db):db_(db),dummyHash_(PasswordDXX::hash("unused-dummy-password")) {}
JsonDXX AuthServiceDXX::registrationResult(std::int64_t id) {
    auto user=db_.query("SELECT user_id,registered_at FROM users WHERE user_id=?",{id}).at(0);
    user.set("accounts",JsonDXX::array(db_.query("SELECT service_type,display_id FROM accounts WHERE user_id=? AND status='ACTIVE' ORDER BY service_type",{id})));
    return user;
}
JsonDXX AuthServiceDXX::registerUser(const JsonDXX& r) {
    const auto d=r["data"]; const auto name=lower(d["login_name"].text());
    auto invalid=[&](const std::string& msg) { return ProtocolDXX::error(r,"VALIDATION_ERROR",msg); };
    if(!bounded(d["login_name"],3,32)||!std::regex_match(name,std::regex("[a-z0-9_]{3,32}"))) return invalid("登录名须为 3–32 位英文字母、数字或下划线");
    if(!bounded(d["nickname"],1,96)||d["nickname"].text().find_first_not_of(" \t\r\n")==std::string::npos) return invalid("昵称须为 1–96 字节且不能全为空白");
    if(!bounded(d["password"],8,128)) return invalid("密码须为 8–128 字节");
    if(!birthday(d["birthday"])) return invalid("生日须为有效的 1900 年至今日期");
    if(!d["location"].isNull()&&!bounded(d["location"],0,160)) return invalid("所在地最多 160 字节");
    if(!d["services"].isArray()) return invalid("请选择至少一种服务");
    std::set<std::string> selected;
    for(const auto& v:d["services"].items()) { const auto s=v.text(); if(!v.isString()||(s!="QQ"&&s!="WECHAT"&&s!="WEIBO")) return invalid("未知服务"); selected.insert(s); }
    if(selected.empty()||d["services"].items().size()>3) return invalid("请选择 1–3 种服务，不要重复选择");
    std::vector<JsonDXX> serviceList; for(const auto& s:selected) serviceList.emplace_back(s);
    JsonDXX canonical{{"login_name",name},{"nickname",d["nickname"]},{"birthday",d["birthday"]},{"location",d["location"]},{"services",JsonDXX::array(serviceList)}};
    // Passwords never enter the idempotency audit. Include the salted KDF record
    // and separately verify the submitted password when replaying a committed ID.
    auto previous=db_.query("SELECT r.request_fingerprint,r.user_id,u.password_hash FROM registration_requests r JOIN users u ON u.user_id=r.user_id WHERE r.request_id=?",{r["request_id"]});
    if(!previous.empty()) {
        canonical.set("credential",previous[0]["password_hash"]);
        if(!PasswordDXX::verify(d["password"].text(),previous[0]["password_hash"].text())||PasswordDXX::fingerprint(canonical.dump())!=previous[0]["request_fingerprint"].text()) return ProtocolDXX::error(r,"REQUEST_ID_CONFLICT","请求编号已用于其他注册内容");
        return ProtocolDXX::response(r,registrationResult(previous[0]["user_id"].integer()));
    }
    if(!db_.query("SELECT user_id FROM users WHERE login_name=?",{name}).empty()) return ProtocolDXX::error(r,"LOGIN_NAME_TAKEN","登录名已被使用");
    const auto hash=PasswordDXX::hash(d["password"].text()); canonical.set("credential",hash);
    const auto time=now();
    db_.exec("BEGIN IMMEDIATE");
    try {
        auto allocate=[&](const std::string& space) { const auto n=db_.query("SELECT next_value FROM id_counters WHERE namespace=?",{space}).at(0)["next_value"].integer(); if(n>=INT64_MAX) throw std::runtime_error("ID space exhausted"); db_.query("UPDATE id_counters SET next_value=next_value+1 WHERE namespace=?",{space}); return std::to_string(n); };
        JsonDXX shared;
        if(selected.count("QQ")||selected.count("WEIBO")) shared=allocate("SOCIAL_SHARED");
        db_.query("INSERT INTO users(login_name,password_hash,nickname,birthday,location,shared_social_id,registered_at) VALUES(?,?,?,?,?,?,?)",{name,hash,d["nickname"],d["birthday"],d["location"],shared,time});
        const auto user=db_.insertedId();
        for(const auto& s:selected) { const auto display=s=="WECHAT"?"WX"+allocate("WECHAT"):shared.text(); db_.query("INSERT INTO accounts(user_id,service_type,display_id,status,opened_at) VALUES(?,?,?,'ACTIVE',?)",{user,s,display,time}); }
        db_.query("INSERT INTO registration_requests(request_id,request_fingerprint,user_id,created_at) VALUES(?,?,?,?)",{r["request_id"],PasswordDXX::fingerprint(canonical.dump()),user,time});
        auto result=registrationResult(user);
        db_.exec("COMMIT");
        return ProtocolDXX::response(r,result);
    } catch(...) { try { db_.exec("ROLLBACK"); } catch(...) {} throw; }
}
JsonDXX AuthServiceDXX::services(std::int64_t id,const std::vector<std::string>& active) {
    auto rows=db_.query("SELECT service_type,display_id FROM accounts WHERE user_id=? AND status='ACTIVE' ORDER BY service_type",{id});
    for(auto& row:rows) row.set("active",std::find(active.begin(),active.end(),row["service_type"].text())!=active.end());
    return JsonDXX::array(rows);
}
JsonDXX AuthServiceDXX::login(const JsonDXX& r) {
    const auto d=r["data"];
    if(!bounded(d["login_name"],3,32)||!bounded(d["password"],1,128)) return ProtocolDXX::error(r,"AUTH_INVALID_CREDENTIALS","登录名或密码错误");
    auto users=db_.query("SELECT user_id,nickname,password_hash FROM users WHERE login_name=?",{lower(d["login_name"].text())});
    const bool valid=PasswordDXX::verify(d["password"].text(),users.empty()?dummyHash_:users[0]["password_hash"].text());
    if(users.empty()||!valid) return ProtocolDXX::error(r,"AUTH_INVALID_CREDENTIALS","登录名或密码错误");
    auto result=users[0]; result=JsonDXX{{"user_id",result["user_id"]},{"nickname",result["nickname"]},{"services",services(result["user_id"].integer())},{"session_id",PasswordDXX::randomId()}};
    return ProtocolDXX::response(r,result);
}
