#include "core/groups/GroupDXX.h"
#include "core/groups/GroupPolicyFactoryDXX.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <type_traits>

static int checks = 0;
static void require(bool result, const char* message) {
    ++checks;
    if (!result) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
using Permissions = std::array<bool, 8>;
static Permissions permissions(const GroupPolicyDXX& policy, const GroupContextDXX& c) {
    return {policy.canRequestJoin(c), policy.canInvite(c), policy.canApproveJoin(c),
            policy.canKick(c), policy.canManageAnnouncement(c), policy.canCreateSubgroup(c),
            policy.canManageAdmins(c), policy.canManageSubgroup(c)};
}
static void policyTables() {
    auto qq = GroupPolicyFactoryDXX::create("QQ");
    auto wx = GroupPolicyFactoryDXX::create("WECHAT");
    GroupContextDXX outsider;
    require(permissions(*qq, outsider) == Permissions{true,false,false,false,false,false,false,false}, "QQ outsider applies only");
    require(permissions(*wx, outsider) == Permissions{}, "WeChat outsider cannot directly join");
    GroupContextDXX member;
    member.member = true; member.role = "MEMBER";
    member.targetMember = true; member.targetRole = "MEMBER";
    require(permissions(*qq, member) == Permissions{}, "QQ member has no management/invitation rights");
    require(permissions(*wx, member) == Permissions{false,true,false,false,false,false,false,false}, "WeChat member can invite only");
    auto admin = member; admin.role = "ADMIN";
    require(permissions(*qq, admin) == Permissions{false,false,true,true,true,false,false,false}, "QQ admin manages ordinary members, approval, announcements");
    require(permissions(*wx, admin) == Permissions{false,true,false,false,false,false,false,false}, "stored admin has no WeChat management rights");
    auto owner = member; owner.owner = true; owner.role = "OWNER";
    require(permissions(*qq, owner) == Permissions{false,false,true,true,true,true,true,true}, "QQ owner management table");
    require(permissions(*wx, owner) == Permissions{false,true,false,true,true,false,false,false}, "WeChat owner management table");
    for (const char* role : {"OWNER", "ADMIN", "MEMBER"}) {
        admin.targetRole = role; owner.targetRole = role;
        require(qq->canKick(admin) == (std::string(role) == "MEMBER"), "QQ admin cannot kick another admin or owner");
        require(qq->canKick(owner) == (std::string(role) != "OWNER"), "QQ owner cannot kick owner");
        require(wx->canKick(owner) == (std::string(role) != "OWNER"), "WeChat owner cannot kick owner");
    }
    owner.targetMember = false;
    require(!qq->canKick(owner) && !wx->canKick(owner), "kick requires real target member");
    auto creator = member; creator.creator = true;
    require(qq->canManageSubgroup(creator), "current parent member creator can manage subgroup");
    creator.member = false;
    require(!qq->canManageSubgroup(creator), "former parent member creator cannot manage subgroup");
    for (auto context : {outsider, member, admin, owner, creator}) {
        context.active = false;
        require(permissions(*qq, context) == Permissions{}, "disbanded QQ denies every action");
        require(permissions(*wx, context) == Permissions{}, "disbanded WeChat denies every action");
    }
    bool invalid = false;
    try { auto unsupported = GroupPolicyFactoryDXX::create("WEIBO"); } catch (const std::invalid_argument&) { invalid = true; }
    require(invalid, "unknown policy is rejected");
}
static void runtimeSwitchAndRestore() {
    GroupDXX group(17, "QQ", 101, "QQ", 3);
    group.setMembers({{101,"OWNER","time1"},{102,"ADMIN","time2"},{103,"MEMBER","time3"}});
    group.setSubgroups({{21,17,101,"discussion",{101,103}}});
    const GroupDXX* same = &group;
    const GroupPolicyDXX& qq = group.policy();
    require(qq.canApproveJoin(group.context(102)), "base reference dispatches QQ approval");
    require(qq.canKick(group.context(102,103)), "base reference dispatches QQ admin kick");
    require(group.context(101,103,101).owner && group.context(101,103,101).creator, "context derives owner and creator");
    require(!group.context(999,999,999).creator, "outsider does not gain creator authority");
    group.switchPolicy("WECHAT",4);
    const GroupPolicyDXX& wx = group.policy();
    require(&group == same && wx.type() == "WECHAT", "same Group changes runtime strategy");
    require(!wx.canApproveJoin(group.context(102)) && !wx.canKick(group.context(102,103)), "same stored admin loses privileges under WeChat");
    require(wx.canInvite(group.context(103)), "same member gains WeChat invitation behavior");
    require(!wx.canCreateSubgroup(group.context(101)) && !wx.canManageSubgroup(group.context(101,0,101)), "existing subgroup disabled by WeChat policy");
    require(group.members().size()==3 && group.members()[1].accountId()==102 && group.members()[1].storedRole()=="ADMIN" && group.members()[1].joinedAt()=="time2", "member identity role and join metadata survive");
    require(group.subgroups().size()==1 && group.subgroups()[0].id()==21 && group.subgroups()[0].parentId()==17 && group.subgroups()[0].creatorId()==101 && group.subgroups()[0].name()=="discussion" && group.subgroups()[0].members()==std::vector<std::int64_t>{101,103}, "subgroup metadata and members survive");
    GroupDXX reloaded(group.id(),group.service(),group.ownerId(),wx.type(),group.policyVersion());
    reloaded.setMembers(group.members()); reloaded.setSubgroups(group.subgroups());
    require(!reloaded.policy().canApproveJoin(reloaded.context(102)) && reloaded.policy().canInvite(reloaded.context(103)), "factory rebuild restores persisted WeChat behavior");
    group.switchPolicy("QQ",5);
    require(group.policy().canApproveJoin(group.context(102)) && group.policy().canManageSubgroup(group.context(101,0,101)), "QQ restore revives stored admin and subgroup");
    require(group.service()=="QQ" && group.ownerId()==101 && group.policyVersion()==5, "service and owner stable while committed version changes");
    bool invalid = false;
    try { group.switchPolicy("INVALID",6); } catch (const std::invalid_argument&) { invalid = true; }
    require(invalid && group.policy().type()=="QQ" && group.policyVersion()==5, "failed strategy creation keeps previous policy and version");
    GroupDXX nativeWechat(18,"WECHAT",101,"QQ",1);
    nativeWechat.setMembers(group.members());
    require(nativeWechat.service()=="WECHAT" && nativeWechat.policy().canCreateSubgroup(nativeWechat.context(101)), "QQ rules apply independently of original service");
    group.setActive(false);
    require(!group.context(101).active && !group.policy().canManageAnnouncement(group.context(101)), "inactive Group reaches policy authorization");
}
int main() {
    static_assert(std::has_virtual_destructor<GroupPolicyDXX>::value, "polymorphic destruction required");
    policyTables(); runtimeSwitchAndRestore();
    std::cout << "PASS group policy: " << checks << " checks (pure C++17, runtime dispatch, restoration)\n";
}
