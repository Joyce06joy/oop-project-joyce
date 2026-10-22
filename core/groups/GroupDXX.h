#pragma once
#include "GroupMemberDXX.h"
#include "GroupPolicyDXX.h"
#include "SubgroupDXX.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GroupDXX {
public:
    GroupDXX(std::int64_t id, std::string service, std::int64_t owner,
             std::string policy, std::int64_t version);
    std::int64_t id() const;
    const std::string& service() const;
    std::int64_t ownerId() const;
    std::int64_t policyVersion() const;
    const GroupPolicyDXX& policy() const;
    void setActive(bool active);
    void setMembers(std::vector<GroupMemberDXX> members);
    void setSubgroups(std::vector<SubgroupDXX> subgroups);
    const std::vector<GroupMemberDXX>& members() const;
    const std::vector<SubgroupDXX>& subgroups() const;
    GroupContextDXX context(std::int64_t actor, std::int64_t target = 0,
                            std::int64_t creator = 0) const;
    void switchPolicy(std::string policy, std::int64_t version);
private:
    std::int64_t id_;
    std::string service_;
    std::int64_t ownerId_;
    std::int64_t policyVersion_;
    bool active_ = true;
    std::unique_ptr<GroupPolicyDXX> policy_;
    std::vector<GroupMemberDXX> members_;
    std::vector<SubgroupDXX> subgroups_;
};
