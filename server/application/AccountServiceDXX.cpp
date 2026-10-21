#include "AccountServiceDXX.h"
#include "BusinessErrorDXX.h"
#include <limits>
AccountServiceDXX::AccountServiceDXX(DatabaseDXX& db):db_(db),repository_(db) {}
JsonDXX AccountServiceDXX::binding(std::int64_t user) {
    auto find=[&](const std::string& s) { auto rows=db_.query("SELECT display_id FROM accounts WHERE user_id=? AND service_type=? AND status='ACTIVE'",{user,s}); return rows.empty()?JsonDXX():rows[0]["display_id"]; };
    return {{"bound",repository_.bound(user)},{"qq_display_id",find("QQ")},{"wechat_display_id",find("WECHAT")}};
}
JsonDXX AccountServiceDXX::open(std::int64_t user,const JsonDXX& d) {
    const auto s=SocialRepositoryDXX::stringField(d,"service_type",8);
    if(!SocialRepositoryDXX::validService(s)) throw BusinessErrorDXX("VALIDATION_ERROR","未知服务");
    auto existing=db_.query("SELECT service_type,display_id,status FROM accounts WHERE user_id=? AND service_type=?",{user,s});
    if(!existing.empty()) {
        if(existing[0]["status"].text()!="ACTIVE") throw BusinessErrorDXX("SERVICE_NOT_OPEN","已关闭账号不支持本轮恢复");
        return SocialRepositoryDXX::changed(JsonDXX{{"account",JsonDXX{{"service_type",s},{"display_id",existing[0]["display_id"]}}}},{});
    }
    auto allocate=[&](const std::string& space) {
        auto n=db_.query("SELECT next_value FROM id_counters WHERE namespace=?",{space}).at(0)["next_value"].integer();
        if(n>=std::numeric_limits<std::int64_t>::max()) throw std::runtime_error("ID space exhausted");
        db_.query("UPDATE id_counters SET next_value=next_value+1 WHERE namespace=?",{space}); return std::to_string(n);
    };
    std::string display;
    if(s=="WECHAT") display="WX"+allocate("WECHAT");
    else {
        display=db_.query("SELECT shared_social_id FROM users WHERE user_id=?",{user}).at(0)["shared_social_id"].text();
        if(display.empty()) { display=allocate("SOCIAL_SHARED"); db_.query("UPDATE users SET shared_social_id=? WHERE user_id=?",{display,user}); }
    }
    db_.query("INSERT INTO accounts(user_id,service_type,display_id,status,opened_at) VALUES(?,?,?,'ACTIVE',?)",{user,s,display,SocialRepositoryDXX::now()});
    JsonDXX eventData{{"service_type",s},{"display_id",display}};
    return SocialRepositoryDXX::changed(JsonDXX{{"account",eventData}},{SocialRepositoryDXX::notice(user,"account.service.opened","",eventData)});
}
JsonDXX AccountServiceDXX::bind(std::int64_t user,const JsonDXX& d,bool create) {
    // No caller-supplied ownership identifiers are accepted, even if they look valid.
    if(d.dump()!="{}") throw BusinessErrorDXX("FRIEND_PERMISSION_DENIED","绑定仅使用当前登录主体的QQ和微信账号");
    const auto qq=repository_.owned(user,"QQ",{},false),wechat=repository_.owned(user,"WECHAT",{},false);
    const auto q=qq["account_pk"],w=wechat["account_pk"];
    const bool bound=repository_.bound(user);
    if(create&&!bound) {
        if(!db_.query("SELECT binding_id FROM account_bindings WHERE qq_account_pk=? OR wechat_account_pk=?",{q,w}).empty()) throw BusinessErrorDXX("BINDING_CONFLICT","账号已有其他绑定");
        db_.query("INSERT INTO account_bindings(qq_account_pk,wechat_account_pk,created_at) VALUES(?,?,?)",{q,w,SocialRepositoryDXX::now()});
    } else if(!create&&bound) db_.query("DELETE FROM account_bindings WHERE qq_account_pk=? AND wechat_account_pk=?",{q,w});
    auto result=binding(user);
    return SocialRepositoryDXX::changed(result,bound==create?std::vector<JsonDXX>{}:std::vector<JsonDXX>{SocialRepositoryDXX::notice(user,"account.binding.changed","",result)});
}
