#pragma once
#include "GroupContextDXX.h"
#include <string>

class GroupPolicyDXX {
public:
    virtual ~GroupPolicyDXX();
    virtual std::string type() const = 0;
    virtual bool canRequestJoin(const GroupContextDXX& context) const = 0;
    virtual bool canInvite(const GroupContextDXX& context) const = 0;
    virtual bool canApproveJoin(const GroupContextDXX& context) const = 0;
    virtual bool canKick(const GroupContextDXX& context) const = 0;
    virtual bool canManageAnnouncement(const GroupContextDXX& context) const = 0;
    virtual bool canCreateSubgroup(const GroupContextDXX& context) const = 0;
    virtual bool canManageAdmins(const GroupContextDXX& context) const = 0;
    virtual bool canManageSubgroup(const GroupContextDXX& context) const = 0;
};
