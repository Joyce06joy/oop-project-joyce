#include "GroupServiceDXX.h"
#include "BusinessErrorDXX.h"
#include <algorithm>
#include <limits>
#include <set>
GroupServiceDXX::GroupServiceDXX(DatabaseDXX& db):db_(db),social_(db),repository_(db) {}
std::int64_t GroupServiceDXX::positiveId(const JsonDXX& d,const std::string& field) {
    if(!d[field].isInteger()||d[field].integer()<=0) throw BusinessErrorDXX("VALIDATION_ERROR","无效标识："+field);
    return d[field].integer();
}
void GroupServiceDXX::activeGroup(const JsonDXX& row) { if(row["status"].text()!="ACTIVE") throw BusinessErrorDXX("GROUP_DISBANDED","该群已解散"); }
void GroupServiceDXX::member(const GroupContextDXX& context) { if(!context.member) throw BusinessErrorDXX("GROUP_NOT_MEMBER","只有当前群成员可执行此操作"); }
GroupDXX& GroupServiceDXX::load(const JsonDXX& row) {
    const auto id=row["group_pk"].integer(); auto it=groups_.find(id);
    if(it==groups_.end()) it=groups_.emplace(id,std::make_unique<GroupDXX>(id,row["service_type"].text(),row["owner_account_pk"].integer(),row["policy_type"].text(),row["policy_version"].integer())).first;
    auto& g=*it->second;
    if(g.policy().type()!=row["policy_type"].text()||g.policyVersion()!=row["policy_version"].integer()) g.switchPolicy(row["policy_type"].text(),row["policy_version"].integer());
    g.setActive(row["status"].text()=="ACTIVE");
    g.setMembers(repository_.members(id)); g.setSubgroups(repository_.subgroups(id));
    return g;
}
JsonDXX GroupServiceDXX::resolve(const JsonDXX& request) {
    const auto type=request["type"].text(); const auto d=request["data"];
    if(type=="group.request.respond") return repository_.group(repository_.request(positiveId(d,"request_pk"))["group_pk"].integer());
    if(type=="group.subgroup.members.update") return repository_.group(repository_.subgroup(positiveId(d,"subgroup_pk"))["parent_group_pk"].integer());
    return repository_.group(positiveId(d,"group_pk"));
}
void GroupServiceDXX::authorize(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
    const auto type=r["type"].text();
    if(type=="group.create"||type=="group.list"||type=="group.request.list") {
        const auto s=SocialRepositoryDXX::stringField(r["data"],"service_type",8);
        if(s!="QQ"&&s!="WECHAT") throw BusinessErrorDXX("GROUP_POLICY_DISABLED","微博不支持群组");
        social_.owned(user,s,active); return;
    }
    auto row=resolve(r);
    if(r["data"].has("service_type")&&r["data"]["service_type"].text()!=row["service_type"].text()) throw BusinessErrorDXX("GROUP_INVALID_TARGET","群原属服务不匹配");
    social_.owned(user,row["service_type"].text(),active);
}
std::vector<JsonDXX> GroupServiceDXX::notices(const JsonDXX& row,const std::string& type,const std::string& change,const std::vector<std::int64_t>& users,const JsonDXX& extra) {
    auto data=JsonDXX{{"group_pk",row["group_pk"]},{"service_type",row["service_type"]},{"change",change}};
    for(const auto* key:{"request_pk","status","policy_type","policy_version","subgroup_pk"}) if(extra.has(key)) data.set(key,extra[key]);
    std::set<std::int64_t> unique(users.begin(),users.end()); std::vector<JsonDXX> result;
    for(auto user:unique) result.push_back(SocialRepositoryDXX::notice(user,type,row["service_type"].text(),data));
    return result;
}
JsonDXX GroupServiceDXX::changed(const JsonDXX& row,const JsonDXX& result,const std::vector<JsonDXX>& notifications) {
    auto wrapped=SocialRepositoryDXX::changed(result,notifications); wrapped.set("group_pk",row["group_pk"]); return wrapped;
}
std::vector<std::string> GroupServiceDXX::displays(const JsonDXX& d,const std::string& field) {
    if(d[field].isNull()) return {};
    if(!d[field].isArray()||d[field].items().size()>100) throw BusinessErrorDXX("VALIDATION_ERROR","成员列表最多100项");
    std::set<std::string> result;
    for(const auto& item:d[field].items()) result.insert(SocialRepositoryDXX::stringField(JsonDXX{{"id",item}},"id",64));
    return {result.begin(),result.end()};
}
void GroupServiceDXX::committed(const JsonDXX& result) {
    // A cached Group object changes virtual strategy only after SQL commit.
    const auto update=result["policy_update"];
    if(!update.isNull()) { auto it=groups_.find(result["group_pk"].integer()); if(it!=groups_.end()) it->second->switchPolicy(update["policy_type"].text(),update["policy_version"].integer()); }
    if(result["result"]["disbanded"].boolean()) { auto it=groups_.find(result["group_pk"].integer()); if(it!=groups_.end()) it->second->setActive(false); }
}
JsonDXX GroupServiceDXX::read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
    const auto type=r["type"].text(); const auto d=r["data"];
    if(type=="group.list") {
        const auto s=SocialRepositoryDXX::stringField(d,"service_type",8); const auto actor=social_.owned(user,s,active)["account_pk"].integer();
        std::string sql="SELECT g.* FROM groups g WHERE g.service_type=? AND g.status='ACTIVE'"; std::vector<JsonDXX> params{s};
        if(d["mine_only"].boolean()) { sql+=" AND EXISTS(SELECT 1 FROM group_members m WHERE m.group_pk=g.group_pk AND m.account_pk=?)"; params.emplace_back(actor); }
        if(d.has("query")&&!d["query"].isNull()) {
            const auto query=SocialRepositoryDXX::stringField(d,"query",80,true); std::string escaped;
            for(char c:query) { if(c=='%'||c=='_'||c=='\\') escaped+='\\'; escaped+=c; }
            sql+=" AND (g.group_number=? OR g.name LIKE ? ESCAPE '\\')"; params.emplace_back(query); params.emplace_back("%"+escaped+"%");
        }
        auto result=repository_.page(sql+" ORDER BY g.group_number,g.group_pk",params,d); std::vector<JsonDXX> items;
        for(const auto& row:result["items"].items()) items.push_back(repository_.summary(row,actor)); result.set("items",JsonDXX::array(items)); return result;
    }
    if(type=="group.request.list") {
        const auto s=SocialRepositoryDXX::stringField(d,"service_type",8); const auto actor=social_.owned(user,s,active)["account_pk"].integer();
        std::vector<std::int64_t> review;
        for(const auto& row:db_.query("SELECT g.* FROM groups g JOIN group_members m ON m.group_pk=g.group_pk WHERE m.account_pk=? AND g.service_type=? AND g.status='ACTIVE'",{actor,s})) {
            auto& group=load(row); if(group.policy().canApproveJoin(group.context(actor))) review.push_back(group.id());
        }
        std::string sql="SELECT r.request_pk,r.group_pk,g.group_number,g.name AS group_name,r.kind,r.status,i.display_id AS initiator_display_id,t.display_id AS target_display_id,r.created_at,r.initiator_account_pk,r.target_account_pk FROM group_requests r JOIN groups g ON g.group_pk=r.group_pk JOIN accounts i ON i.account_pk=r.initiator_account_pk JOIN accounts t ON t.account_pk=r.target_account_pk WHERE r.status='PENDING' AND g.status='ACTIVE' AND g.service_type=? AND (r.initiator_account_pk=? OR r.target_account_pk=?";
        std::vector<JsonDXX> params{s,actor,actor};
        if(!review.empty()) { sql+=" OR (r.kind='APPLY' AND r.group_pk IN ("; for(std::size_t n=0;n<review.size();++n) {if(n)sql+=",";sql+="?";params.emplace_back(review[n]);}sql+="))"; } sql+=")";
        if(d.has("group_pk")) { const auto row=repository_.group(positiveId(d,"group_pk")); activeGroup(row); if(row["service_type"].text()!=s) throw BusinessErrorDXX("GROUP_INVALID_TARGET","群原属服务不匹配"); sql+=" AND r.group_pk=?";params.push_back(d["group_pk"]); }
        auto page=repository_.page(sql+" ORDER BY r.created_at,r.request_pk",params,d); std::vector<JsonDXX> items;
        for(const auto& row:page["items"].items()) {
            auto& group=load(repository_.group(row["group_pk"].integer()));
            const bool incoming=row["kind"].text()=="INVITE"&&row["target_account_pk"].integer()==actor;
            const bool reviewing=row["kind"].text()=="APPLY"&&group.policy().canApproveJoin(group.context(actor));
            const bool canRespond=reviewing||(incoming&&group.policy().canInvite(group.context(group.ownerId())));
            items.emplace_back(JsonDXX{{"request_pk",row["request_pk"]},{"group_pk",row["group_pk"]},{"group_number",row["group_number"]},{"group_name",row["group_name"]},{"kind",row["kind"]},{"status",row["status"]},{"initiator_display_id",row["initiator_display_id"]},{"target_display_id",row["target_display_id"]},{"created_at",row["created_at"]},{"direction",reviewing?"REVIEW":(incoming?"INCOMING":"OUTGOING")},{"can_respond",canRespond}});
        }
        page.set("items",JsonDXX::array(items)); return page;
    }
    auto row=resolve(r); activeGroup(row); auto& group=load(row);
    const auto actor=social_.owned(user,group.service(),active)["account_pk"].integer(); const auto context=group.context(actor);
    if(type=="group.detail") {
        auto publicGroup=repository_.summary(row,actor); publicGroup.set("announcement",row["announcement"]);
        publicGroup.set("owner_display_id",social_.byId(group.ownerId())["display_id"]);
        JsonDXX permissions{{"can_apply",group.policy().canRequestJoin(context)},{"can_invite",group.policy().canInvite(context)},
            {"can_review",group.policy().canApproveJoin(context)},{"can_manage_announcement",group.policy().canManageAnnouncement(context)},
            {"can_rename",context.owner},{"can_set_admin",group.policy().canManageAdmins(context)},{"can_create_subgroup",group.policy().canCreateSubgroup(context)},
            {"can_switch_policy",context.owner},{"can_disband",context.owner},{"can_leave",context.member&&!context.owner}};
        JsonDXX memberPage{{"items",JsonDXX::array()},{"total",publicGroup["member_count"]},{"next_cursor",nullptr}};
        if(context.member) {
            memberPage=repository_.page("SELECT a.display_id,u.nickname,m.stored_role,a.account_pk FROM group_members m JOIN accounts a ON a.account_pk=m.account_pk JOIN users u ON u.user_id=a.user_id WHERE m.group_pk=? ORDER BY a.display_id,a.account_pk",{group.id()},d);
            std::vector<JsonDXX> members;
            for(const auto& m:memberPage["items"].items()) {
                auto ctx=group.context(actor,m["account_pk"].integer()); const auto role=m["stored_role"].text();
                members.emplace_back(JsonDXX{{"display_id",m["display_id"]},{"nickname",m["nickname"]},{"stored_role",role},
                    {"effective_role",role=="ADMIN"&&group.policy().type()=="WECHAT"?"MEMBER":role},
                    {"can_kick",group.policy().canKick(ctx)},{"can_set_admin",group.policy().canManageAdmins(context)&&m["account_pk"].integer()!=group.ownerId()}});
            }
            memberPage.set("items",JsonDXX::array(members));
        }
        auto mine=db_.query("SELECT request_pk,kind,status FROM group_requests WHERE group_pk=? AND target_account_pk=? ORDER BY request_pk DESC LIMIT 1",{group.id(),actor});
        return {{"group",publicGroup},{"permissions",permissions},{"members",memberPage["items"]},{"total",memberPage["total"]},{"next_cursor",memberPage["next_cursor"]},{"my_request",mine.empty()?JsonDXX():mine[0]}};
    }
    if(type=="group.subgroup.list") {
        member(context);
        if(!group.policy().canCreateSubgroup(group.context(group.ownerId()))) return {{"items",JsonDXX::array()},{"total",0},{"next_cursor",nullptr},{"disabled",true}};
        JsonDXX result;
        if(d.has("subgroup_pk")) {
            auto sub=repository_.subgroup(positiveId(d,"subgroup_pk"));
            if(sub["parent_group_pk"].integer()!=group.id()||sub["status"].text()!="ACTIVE") throw BusinessErrorDXX("GROUP_INVALID_TARGET","子群不属于当前有效主群");
            result=repository_.page("SELECT a.display_id,u.nickname FROM subgroup_members m JOIN accounts a ON a.account_pk=m.account_pk JOIN users u ON u.user_id=a.user_id WHERE m.subgroup_pk=? ORDER BY a.display_id,a.account_pk",{sub["subgroup_pk"]},d);
            result.set("subgroup",JsonDXX{{"subgroup_pk",sub["subgroup_pk"]},{"parent_group_pk",sub["parent_group_pk"]},{"name",sub["name"]},{"can_manage",group.policy().canManageSubgroup(group.context(actor,0,sub["creator_account_pk"].integer()))}});
        } else {
            result=repository_.page("SELECT s.subgroup_pk,s.parent_group_pk,s.name,a.display_id AS creator_display_id,s.creator_account_pk,(SELECT COUNT(*) FROM subgroup_members m WHERE m.subgroup_pk=s.subgroup_pk) AS member_count FROM subgroups s JOIN accounts a ON a.account_pk=s.creator_account_pk WHERE s.parent_group_pk=? AND s.status='ACTIVE' ORDER BY s.subgroup_pk",{group.id()},d);
            std::vector<JsonDXX> items;
            for(const auto& sub:result["items"].items()) items.emplace_back(JsonDXX{{"subgroup_pk",sub["subgroup_pk"]},{"parent_group_pk",sub["parent_group_pk"]},{"name",sub["name"]},{"creator_display_id",sub["creator_display_id"]},{"member_count",sub["member_count"]},{"can_manage",group.policy().canManageSubgroup(group.context(actor,0,sub["creator_account_pk"].integer()))}});
            result.set("items",JsonDXX::array(items));
        }
        result.set("disabled",false); return result;
    }
    throw BusinessErrorDXX("PROTOCOL_ERROR","未知群读取请求");
}
JsonDXX GroupServiceDXX::mutate(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
    const auto type=r["type"].text(); const auto d=r["data"]; const auto now=SocialRepositoryDXX::now();
    if(type=="group.create") {
        const auto service=SocialRepositoryDXX::stringField(d,"service_type",8);
        if(service!="QQ"&&service!="WECHAT") throw BusinessErrorDXX("GROUP_POLICY_DISABLED","微博不支持群组");
        const auto actor=social_.owned(user,service,active)["account_pk"].integer();
        const auto name=SocialRepositoryDXX::stringField(d,"name",96);
        if(name.find_first_not_of(" \t\r\n")==std::string::npos) throw BusinessErrorDXX("VALIDATION_ERROR","群名称不能全为空白");
        const auto number=db_.query("SELECT next_value FROM group_counters WHERE service_type=?",{service}).at(0)["next_value"].integer();
        if(number>=std::numeric_limits<std::int64_t>::max()) throw std::runtime_error("group numbers exhausted");
        db_.query("UPDATE group_counters SET next_value=next_value+1 WHERE service_type=?",{service});
        db_.query("INSERT INTO groups(service_type,group_number,name,owner_account_pk,policy_type,policy_version,announcement,status,created_at) VALUES(?,?,?,?,?,0,'','ACTIVE',?)",{service,std::to_string(number),name,actor,service,now});
        const auto id=db_.insertedId();
        db_.query("INSERT INTO group_members(group_pk,account_pk,stored_role,joined_at) VALUES(?,?,'OWNER',?)",{id,actor,now});
        const auto row=repository_.group(id);
        return changed(row,repository_.summary(row,actor),notices(row,"group.members.changed","CREATED",{user}));
    }
    auto row=resolve(r); activeGroup(row); auto& group=load(row);
    const auto actor=social_.owned(user,group.service(),active)["account_pk"].integer(); const auto context=group.context(actor);
    if(d.has("expected_policy_version")) {
        if(!d["expected_policy_version"].isInteger()||d["expected_policy_version"].integer()<0) throw BusinessErrorDXX("VALIDATION_ERROR","规则版本必须是非负整数");
        if(d["expected_policy_version"].integer()!=group.policyVersion()) throw BusinessErrorDXX("GROUP_VERSION_CONFLICT","规则已变化，请刷新后确认");
    }
    const auto before=repository_.memberUsers(group.id());
    if(type=="group.join.apply"||type=="group.invite") {
        std::int64_t target=actor;
        if(type=="group.join.apply") {
            if(context.member) throw BusinessErrorDXX("GROUP_ALREADY_MEMBER","已经是群成员");
            if(!group.policy().canRequestJoin(context)) throw BusinessErrorDXX("GROUP_POLICY_DISABLED","当前规则不支持申请入群，请等待成员邀请");
        } else {
            member(context);
            if(!group.policy().canInvite(context)) throw BusinessErrorDXX("GROUP_POLICY_DISABLED","当前规则不支持邀请加入，请使用申请审核");
            target=repository_.target(group.service(),SocialRepositoryDXX::stringField(d,"target_display_id",64))["account_pk"].integer();
            if(group.context(target).member) throw BusinessErrorDXX("GROUP_ALREADY_MEMBER","目标已经是群成员");
        }
        if(!db_.query("SELECT request_pk FROM group_requests WHERE group_pk=? AND target_account_pk=? AND status='PENDING'",{group.id(),target}).empty()) throw BusinessErrorDXX("GROUP_REQUEST_PENDING","已有待处理入群申请或邀请");
        const std::string kind=type=="group.join.apply"?"APPLY":"INVITE";
        db_.query("INSERT INTO group_requests(group_pk,kind,initiator_account_pk,target_account_pk,status,created_at) VALUES(?,?,?,?,'PENDING',?)",{group.id(),kind,actor,target,now});
        JsonDXX result{{"request_pk",db_.insertedId()},{"status","PENDING"}};
        std::vector<std::int64_t> recipients{user,repository_.accountUser(target)};
        if(kind=="APPLY") for(const auto& m:group.members()) if(group.policy().canApproveJoin(group.context(m.accountId()))) recipients.push_back(repository_.accountUser(m.accountId()));
        return changed(row,result,notices(row,"group.request.new",kind,recipients,result));
    }
    if(type=="group.request.respond") {
        const auto request=repository_.request(positiveId(d,"request_pk")); const auto kind=request["kind"].text();
        if(kind=="APPLY") {
            member(context);
            if(!group.policy().canApproveJoin(context)) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","当前身份无权审批申请");
        } else {
            if(request["target_account_pk"].integer()!=actor) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","只有受邀者本人能确认邀请");
        }
        if(request["status"].text()!="PENDING") throw BusinessErrorDXX("GROUP_REQUEST_ALREADY_RESOLVED","该申请已处理或因规则切换取消");
        if(kind=="INVITE"&&!group.policy().canInvite(group.context(group.ownerId()))) throw BusinessErrorDXX("GROUP_POLICY_DISABLED","当前规则不支持确认该邀请");
        const auto decision=SocialRepositoryDXX::stringField(d,"decision",6);
        if(decision!="ACCEPT"&&decision!="REJECT") throw BusinessErrorDXX("VALIDATION_ERROR","请选择接受或拒绝");
        const auto target=request["target_account_pk"].integer();
        if(decision=="ACCEPT") {
            if(group.context(target).member) throw BusinessErrorDXX("GROUP_ALREADY_MEMBER","目标已经是群成员");
            if(kind=="INVITE"&&!group.context(request["initiator_account_pk"].integer()).member) throw BusinessErrorDXX("GROUP_INVALID_TARGET","邀请人已退出，请先拒绝旧邀请后由当前成员重新邀请");
            db_.query("INSERT INTO group_members(group_pk,account_pk,stored_role,joined_at) VALUES(?,?,'MEMBER',?)",{group.id(),target,now});
        }
        const std::string status=decision=="ACCEPT"?"ACCEPTED":"REJECTED";
        db_.query("UPDATE group_requests SET status=?,processed_at=? WHERE request_pk=?",{status,now,request["request_pk"]});
        auto recipients=before; recipients.push_back(repository_.accountUser(target)); recipients.push_back(repository_.accountUser(request["initiator_account_pk"].integer()));
        JsonDXX result{{"request_pk",request["request_pk"]},{"status",status}};
        return changed(row,result,notices(row,"group.members.changed","REQUEST_"+status,recipients,result));
    }
    member(context);
    if(type=="group.policy.switch") {
        if(!context.owner) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","只有群主可以切换规则");
        if(!d.has("expected_policy_version")) throw BusinessErrorDXX("VALIDATION_ERROR","切换必须携带预期规则版本");
        const auto policy=SocialRepositoryDXX::stringField(d,"target_policy",8);
        if(policy!="QQ"&&policy!="WECHAT") throw BusinessErrorDXX("VALIDATION_ERROR","未知群管理规则");
        JsonDXX result{{"group_pk",group.id()},{"policy_type",group.policy().type()},{"policy_version",group.policyVersion()},{"cancelled_requests",0}};
        if(policy==group.policy().type()) return changed(row,result,{});
        if(group.policyVersion()>=std::numeric_limits<std::int64_t>::max()) throw std::runtime_error("policy version exhausted");
        auto recipients=before; auto pending=db_.query("SELECT initiator_account_pk,target_account_pk FROM group_requests WHERE group_pk=? AND status='PENDING'",{group.id()});
        for(const auto& request:pending) { recipients.push_back(repository_.accountUser(request["initiator_account_pk"].integer())); recipients.push_back(repository_.accountUser(request["target_account_pk"].integer())); }
        const auto version=group.policyVersion()+1;
        db_.query("UPDATE groups SET policy_type=?,policy_version=? WHERE group_pk=? AND policy_version=?",{policy,version,group.id(),group.policyVersion()});
        db_.query("UPDATE group_requests SET status='CANCELLED_POLICY_CHANGED',processed_at=? WHERE group_pk=? AND status='PENDING'",{now,group.id()});
        result.set("policy_type",policy);result.set("policy_version",version);result.set("cancelled_requests",static_cast<std::int64_t>(pending.size()));
        auto wrapped=changed(row,result,notices(row,"group.policy.changed","SWITCHED",recipients,result));
        wrapped.set("policy_update",JsonDXX{{"policy_type",policy},{"policy_version",version}}); return wrapped;
    }
    if(type=="group.admin.set") {
        if(!group.policy().canManageAdmins(group.context(group.ownerId()))) throw BusinessErrorDXX("GROUP_POLICY_DISABLED","当前规则不支持管理员权限");
        if(!group.policy().canManageAdmins(context)) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","只有群主可以设置管理员");
        if(d["enabled"].dump()!="true"&&d["enabled"].dump()!="false") throw BusinessErrorDXX("VALIDATION_ERROR","enabled必须是布尔值");
        const auto target=repository_.target(group.service(),SocialRepositoryDXX::stringField(d,"target_display_id",64))["account_pk"].integer();
        if(target==group.ownerId()||!group.context(target).member) throw BusinessErrorDXX("GROUP_INVALID_TARGET","管理员必须为本群非群主成员");
        db_.query("UPDATE group_members SET stored_role=? WHERE group_pk=? AND account_pk=?",{d["enabled"].boolean()?"ADMIN":"MEMBER",group.id(),target});
        return changed(row,JsonDXX{{"updated",true}},notices(row,"group.members.changed","ROLE",before));
    }
    if(type=="group.member.kick"||type=="group.member.leave") {
        std::int64_t target=actor;
        if(type=="group.member.leave") { if(context.owner) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","群主不能直接退出，请先解散群"); }
        else {
            target=repository_.target(group.service(),SocialRepositoryDXX::stringField(d,"target_display_id",64))["account_pk"].integer();
            if(!group.context(target).member) throw BusinessErrorDXX("GROUP_INVALID_TARGET","目标不是群成员");
            if(!group.policy().canKick(group.context(actor,target))) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","无权踢出该成员");
        }
        // Composite FK ON DELETE CASCADE removes all valid child memberships.
        db_.query("DELETE FROM group_members WHERE group_pk=? AND account_pk=?",{group.id(),target});
        return changed(row,type=="group.member.leave"?JsonDXX{{"left",true}}:JsonDXX{{"removed",true}},notices(row,"group.members.changed",type=="group.member.leave"?"LEFT":"KICKED",before));
    }
    if(type=="group.announcement.update"||type=="group.name.update") {
        if(type=="group.announcement.update") {
            if(!group.policy().canManageAnnouncement(context)) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","当前身份无权维护公告");
            db_.query("UPDATE groups SET announcement=? WHERE group_pk=?",{SocialRepositoryDXX::stringField(d,"announcement",1000,true),group.id()});
        } else {
            if(!context.owner) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","只有群主可以改群名称");
            const auto name=SocialRepositoryDXX::stringField(d,"name",96);
            if(name.find_first_not_of(" \t\r\n")==std::string::npos) throw BusinessErrorDXX("VALIDATION_ERROR","群名称不能全为空白");
            db_.query("UPDATE groups SET name=? WHERE group_pk=?",{name,group.id()});
        }
        return changed(row,JsonDXX{{"updated",true}},notices(row,"group.members.changed","PROFILE",before));
    }
    if(type=="group.disband") {
        if(!context.owner) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","只有群主可以解散群");
        // Pending outsiders also need to invalidate their request views on disband.
        auto recipients=before;
        for(const auto& pending:db_.query("SELECT initiator_account_pk,target_account_pk FROM group_requests WHERE group_pk=? AND status='PENDING'",{group.id()})) {
            recipients.push_back(repository_.accountUser(pending["initiator_account_pk"].integer()));
            recipients.push_back(repository_.accountUser(pending["target_account_pk"].integer()));
        }
        db_.query("UPDATE subgroups SET status='DISBANDED' WHERE parent_group_pk=? AND status='ACTIVE'",{group.id()});
        db_.query("UPDATE groups SET status='DISBANDED' WHERE group_pk=?",{group.id()});
        return changed(row,JsonDXX{{"disbanded",true}},notices(row,"group.disbanded","DISBANDED",recipients));
    }
    if(type=="group.subgroup.create"||type=="group.subgroup.members.update") {
        if(!group.policy().canCreateSubgroup(group.context(group.ownerId()))) throw BusinessErrorDXX("GROUP_POLICY_DISABLED","当前规则停用子群，数据仍保留");
        std::int64_t subId; JsonDXX result;
        if(type=="group.subgroup.create") {
            if(!group.policy().canCreateSubgroup(context)) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","只有群主可以创建临时子群");
            const auto name=SocialRepositoryDXX::stringField(d,"name",96);
            if(name.find_first_not_of(" \t\r\n")==std::string::npos) throw BusinessErrorDXX("VALIDATION_ERROR","子群名称不能全为空白");
            std::set<std::int64_t> initial{actor};
            for(const auto& display:displays(d,"member_display_ids")) { const auto target=repository_.target(group.service(),display)["account_pk"].integer(); if(!group.context(target).member) throw BusinessErrorDXX("GROUP_INVALID_TARGET","子群成员必须是当前主群成员"); initial.insert(target); }
            db_.query("INSERT INTO subgroups(parent_group_pk,name,creator_account_pk,status,created_at) VALUES(?,?,?,'ACTIVE',?)",{group.id(),name,actor,now}); subId=db_.insertedId();
            for(auto target:initial) db_.query("INSERT INTO subgroup_members(subgroup_pk,parent_group_pk,account_pk,joined_at) VALUES(?,?,?,?)",{subId,group.id(),target,now});
            result=JsonDXX{{"subgroup_pk",subId},{"parent_group_pk",group.id()},{"name",name}};
        } else {
            const auto sub=repository_.subgroup(positiveId(d,"subgroup_pk")); subId=sub["subgroup_pk"].integer();
            if(sub["status"].text()!="ACTIVE") throw BusinessErrorDXX("GROUP_INVALID_TARGET","子群已失效");
            if(!group.policy().canManageSubgroup(group.context(actor,0,sub["creator_account_pk"].integer()))) throw BusinessErrorDXX("GROUP_PERMISSION_DENIED","只有仍在主群的群主或创建者可管理子群");
            const auto adds=displays(d,"add_display_ids"),removes=displays(d,"remove_display_ids");
            auto validateTargets=[&](const std::vector<std::string>& values) {
                std::vector<std::int64_t> ids;
                for(const auto& display:values) {
                    const auto target=repository_.target(group.service(),display)["account_pk"].integer();
                    if(!group.context(target).member) throw BusinessErrorDXX("GROUP_INVALID_TARGET","子群成员必须是当前主群成员");
                    ids.push_back(target);
                }
                return ids;
            };
            const auto addIds=validateTargets(adds),removeIds=validateTargets(removes);
            for(auto target:addIds) if(std::find(removeIds.begin(),removeIds.end(),target)!=removeIds.end()) throw BusinessErrorDXX("VALIDATION_ERROR","同一成员不能同时加入和移除");
            for(auto target:removeIds) db_.query("DELETE FROM subgroup_members WHERE subgroup_pk=? AND account_pk=?",{subId,target});
            for(auto target:addIds) db_.query("INSERT OR IGNORE INTO subgroup_members(subgroup_pk,parent_group_pk,account_pk,joined_at) VALUES(?,?,?,?)",{subId,group.id(),target,now});
            result=JsonDXX{{"updated",true}};
        }
        return changed(row,result,notices(row,"group.subgroup.changed","UPDATED",before,JsonDXX{{"subgroup_pk",subId}}));
    }
    throw BusinessErrorDXX("PROTOCOL_ERROR","未知群变更请求");
}
