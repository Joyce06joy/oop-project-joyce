#include "GroupRepositoryDXX.h"
#include "BusinessErrorDXX.h"
GroupRepositoryDXX::GroupRepositoryDXX(DatabaseDXX& db):db_(db),social_(db) {}
JsonDXX GroupRepositoryDXX::group(std::int64_t pk) {
    auto rows=db_.query("SELECT * FROM groups WHERE group_pk=?",{pk});
    if(rows.empty()) throw BusinessErrorDXX("GROUP_NOT_FOUND","群不存在");
    return rows[0];
}
JsonDXX GroupRepositoryDXX::subgroup(std::int64_t pk) {
    auto rows=db_.query("SELECT * FROM subgroups WHERE subgroup_pk=?",{pk});
    if(rows.empty()) throw BusinessErrorDXX("GROUP_INVALID_TARGET","子群不存在");
    return rows[0];
}
JsonDXX GroupRepositoryDXX::request(std::int64_t pk) {
    auto rows=db_.query("SELECT * FROM group_requests WHERE request_pk=?",{pk});
    if(rows.empty()) throw BusinessErrorDXX("GROUP_INVALID_TARGET","群申请或邀请不存在");
    return rows[0];
}
std::vector<GroupMemberDXX> GroupRepositoryDXX::members(std::int64_t pk) {
    std::vector<GroupMemberDXX> members;
    for(const auto& r:db_.query("SELECT account_pk,stored_role,joined_at FROM group_members WHERE group_pk=? ORDER BY account_pk",{pk}))
        members.emplace_back(r["account_pk"].integer(),r["stored_role"].text(),r["joined_at"].text());
    return members;
}
std::vector<SubgroupDXX> GroupRepositoryDXX::subgroups(std::int64_t pk) {
    std::vector<SubgroupDXX> result;
    for(const auto& r:db_.query("SELECT * FROM subgroups WHERE parent_group_pk=? AND status='ACTIVE' ORDER BY subgroup_pk",{pk})) {
        std::vector<std::int64_t> members;
        for(const auto& m:db_.query("SELECT account_pk FROM subgroup_members WHERE subgroup_pk=? ORDER BY account_pk",{r["subgroup_pk"]})) members.push_back(m["account_pk"].integer());
        result.emplace_back(r["subgroup_pk"].integer(),pk,r["creator_account_pk"].integer(),r["name"].text(),members);
    }
    return result;
}
JsonDXX GroupRepositoryDXX::target(const std::string& service,const std::string& display) {
    try { return social_.target(service,display); }
    catch(const BusinessErrorDXX&) { throw BusinessErrorDXX("GROUP_INVALID_TARGET","目标必须是本群原属服务的有效账号"); }
}
std::vector<std::int64_t> GroupRepositoryDXX::memberUsers(std::int64_t group) {
    std::vector<std::int64_t> users;
    for(const auto& r:db_.query("SELECT a.user_id FROM group_members m JOIN accounts a ON a.account_pk=m.account_pk WHERE m.group_pk=?",{group})) users.push_back(r["user_id"].integer());
    return users;
}
std::int64_t GroupRepositoryDXX::accountUser(std::int64_t account) { return social_.byId(account)["user_id"].integer(); }
JsonDXX GroupRepositoryDXX::summary(const JsonDXX& r,std::int64_t actor) {
    auto roles=db_.query("SELECT stored_role FROM group_members WHERE group_pk=? AND account_pk=?",{r["group_pk"],actor});
    const auto role=roles.empty()?std::string():roles[0]["stored_role"].text();
    std::string effective=role;
    if(role=="ADMIN"&&r["policy_type"].text()=="WECHAT") effective="MEMBER"; // presentation only; authorization uses virtual policy
    return {{"group_pk",r["group_pk"]},{"service_type",r["service_type"]},{"group_number",r["group_number"]},{"name",r["name"]},
        {"policy_type",r["policy_type"]},{"policy_version",r["policy_version"]},{"status",r["status"]},
        {"member_count",db_.query("SELECT COUNT(*) AS n FROM group_members WHERE group_pk=?",{r["group_pk"]})[0]["n"]},
        {"is_member",!roles.empty()},{"stored_role",role},{"effective_role",effective}};
}
JsonDXX GroupRepositoryDXX::page(const std::string& sql,const std::vector<JsonDXX>& params,const JsonDXX& data) { return social_.page(sql,params,data,0,false); }
