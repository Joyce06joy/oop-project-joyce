#include "QQGroupPolicyDXX.h"

std::string QQGroupPolicyDXX::type() const { return "QQ"; }

bool QQGroupPolicyDXX::canRequestJoin(const GroupContextDXX& context) const {
    return context.active && !context.member;
}
bool QQGroupPolicyDXX::canInvite(const GroupContextDXX&) const { return false; }
bool QQGroupPolicyDXX::canApproveJoin(const GroupContextDXX& context) const {
    return context.active && context.member && (context.owner || context.role == "ADMIN");
}
bool QQGroupPolicyDXX::canKick(const GroupContextDXX& context) const {
    if (!context.active || !context.member || !context.targetMember || context.targetRole == "OWNER") return false;
    return context.owner || (context.role == "ADMIN" && context.targetRole == "MEMBER");
}
bool QQGroupPolicyDXX::canManageAnnouncement(const GroupContextDXX& context) const {
    return context.active && context.member && (context.owner || context.role == "ADMIN");
}
bool QQGroupPolicyDXX::canCreateSubgroup(const GroupContextDXX& context) const {
    return context.active && context.member && context.owner;
}
bool QQGroupPolicyDXX::canManageAdmins(const GroupContextDXX& context) const {
    return context.active && context.member && context.owner;
}
bool QQGroupPolicyDXX::canManageSubgroup(const GroupContextDXX& context) const {
    return context.active && context.member && (context.owner || context.creator);
}
