#pragma once
#include "GroupPolicyDXX.h"

class WeChatGroupPolicyDXX : public GroupPolicyDXX {
public:
    std::string type() const override;
    bool canRequestJoin(const GroupContextDXX& context) const override;
    bool canInvite(const GroupContextDXX& context) const override;
    bool canApproveJoin(const GroupContextDXX& context) const override;
    bool canKick(const GroupContextDXX& context) const override;
    bool canManageAnnouncement(const GroupContextDXX& context) const override;
    bool canCreateSubgroup(const GroupContextDXX& context) const override;
    bool canManageAdmins(const GroupContextDXX& context) const override;
    bool canManageSubgroup(const GroupContextDXX& context) const override;
};
