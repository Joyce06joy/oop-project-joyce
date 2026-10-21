#include "RecommendationServiceDXX.h"
#include "BusinessErrorDXX.h"
RecommendationServiceDXX::RecommendationServiceDXX(DatabaseDXX& db):repository_(db) {}
JsonDXX RecommendationServiceDXX::read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
    const auto d=r["data"]; const auto type=r["type"].text();
    if(type=="friend.mutual.list") {
        const auto s=SocialRepositoryDXX::stringField(d,"service_type",8); const auto actor=repository_.owned(user,s,active);
        const auto other=repository_.target(s,SocialRepositoryDXX::stringField(d,"target_display_id",64));
        return repository_.page("SELECT "+SocialRepositoryDXX::Columns+" FROM friend_edges e JOIN friend_edges t ON e.friend=t.friend JOIN accounts a ON a.account_pk=e.friend JOIN users u ON u.user_id=a.user_id WHERE e.owner=? AND t.owner=? AND a.status='ACTIVE' ORDER BY a.display_id,a.account_pk",{actor["account_pk"],other["account_pk"]},d,actor["account_pk"].integer());
    }
    if(type=="friend.recommend.same") {
        const auto s=SocialRepositoryDXX::stringField(d,"service_type",8); const auto actor=repository_.owned(user,s,active); const auto pk=actor["account_pk"];
        const std::string sql="SELECT "+SocialRepositoryDXX::Columns+",COUNT(DISTINCT first.friend) AS mutual_count,'共同好友' AS reason FROM friend_edges first JOIN friend_edges second ON second.owner=first.friend JOIN accounts a ON a.account_pk=second.friend JOIN users u ON u.user_id=a.user_id WHERE first.owner=? AND a.account_pk<>? AND a.status='ACTIVE' AND NOT EXISTS (SELECT 1 FROM friend_edges own WHERE own.owner=? AND own.friend=a.account_pk) AND NOT EXISTS (SELECT 1 FROM friend_requests p WHERE p.status='PENDING' AND ((p.sender_account_pk=? AND p.target_account_pk=a.account_pk) OR (p.target_account_pk=? AND p.sender_account_pk=a.account_pk))) GROUP BY a.account_pk ORDER BY mutual_count DESC,a.display_id,a.account_pk";
        return repository_.page(sql,{pk,pk,pk,pk,pk},d,pk.integer());
    }
    const auto source=SocialRepositoryDXX::stringField(d,"source_service",8),target=SocialRepositoryDXX::stringField(d,"target_service",8);
    if(source==target) throw BusinessErrorDXX("VALIDATION_ERROR","请选择不同来源和目标服务");
    const auto from=repository_.owned(user,source,active),to=repository_.owned(user,target,active);
    const bool needsBinding=source=="WECHAT"||target=="WECHAT";
    if(needsBinding&&!repository_.bound(user)) throw BusinessErrorDXX("BINDING_REQUIRED","请先绑定自己的QQ与微信账号");
    const bool common=type=="friend.cross.common";
    std::string sql="SELECT "+SocialRepositoryDXX::Columns+",? AS reason FROM friend_edges e JOIN accounts source ON source.account_pk=e.friend JOIN accounts a ON a.user_id=source.user_id JOIN users u ON u.user_id=a.user_id WHERE e.owner=? AND source.status='ACTIVE' AND a.service_type=? AND a.status='ACTIVE' AND a.account_pk<>?";
    std::vector<JsonDXX> params{"你的 "+source+" 好友",from["account_pk"],target,to["account_pk"]};
    if(needsBinding) sql+=" AND EXISTS (SELECT 1 FROM account_bindings b JOIN accounts q ON q.account_pk=b.qq_account_pk JOIN accounts w ON w.account_pk=b.wechat_account_pk WHERE q.user_id=a.user_id AND w.user_id=a.user_id AND q.status='ACTIVE' AND w.status='ACTIVE')";
    else sql+=" AND a.display_id=source.display_id";
    sql+=common?" AND EXISTS ":" AND NOT EXISTS ";
    sql+="(SELECT 1 FROM friend_edges own WHERE own.owner=? AND own.friend=a.account_pk)"; params.push_back(to["account_pk"]);
    if(!common) {
        sql+=" AND NOT EXISTS (SELECT 1 FROM friend_requests p WHERE p.status='PENDING' AND ((p.sender_account_pk=? AND p.target_account_pk=a.account_pk) OR (p.target_account_pk=? AND p.sender_account_pk=a.account_pk)))";
        params.push_back(to["account_pk"]); params.push_back(to["account_pk"]);
    }
    sql+=" ORDER BY a.display_id,a.account_pk";
    return repository_.page(sql,params,d,to["account_pk"].integer());
}
