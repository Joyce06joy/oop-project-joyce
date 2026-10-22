#include "WeChatGroupPolicyDXX.h"

std::string WeChatGroupPolicyDXX::type() const { return "WECHAT"; }
bool WeChatGroupPolicyDXX::canRequestJoin(const GroupContextDXX&) const { return false; }
bool WeChatGroupPolicyDXX::canInvite(const GroupContextDXX& context) const {
    return context.active && context.member;
}
bool WeChatGroupPolicyDXX::canApproveJoin(const GroupContextDXX&) const { return false; }
bool WeChatGroupPolicyDXX::canKick(const GroupContextDXX& context) const {
    return context.active && context.member && context.owner && context.targetMember && context.targetRole != "OWNER";
}
bool WeChatGroupPolicyDXX::canManageAnnouncement(const GroupContextDXX& context) const {
    return context.active && context.member && context.owner;
}
bool WeChatGroupPolicyDXX::canCreateSubgroup(const GroupContextDXX&) const { return false; }
bool WeChatGroupPolicyDXX::canManageAdmins(const GroupContextDXX&) const { return false; }
bool WeChatGroupPolicyDXX::canManageSubgroup(const GroupContextDXX&) const { return false; }
