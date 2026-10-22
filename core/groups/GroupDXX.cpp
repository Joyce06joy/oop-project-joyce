#include "GroupDXX.h"
#include "GroupPolicyFactoryDXX.h"
#include <utility>
GroupDXX::GroupDXX(std::int64_t id, std::string service, std::int64_t owner,
                   std::string policy, std::int64_t version)
    : id_(id), service_(std::move(service)), ownerId_(owner), policyVersion_(version),
      policy_(GroupPolicyFactoryDXX::create(policy)) {}
std::int64_t GroupDXX::id() const { return id_; }
const std::string& GroupDXX::service() const { return service_; }
std::int64_t GroupDXX::ownerId() const { return ownerId_; }
std::int64_t GroupDXX::policyVersion() const { return policyVersion_; }
const GroupPolicyDXX& GroupDXX::policy() const { return *policy_; }
void GroupDXX::setActive(bool active) { active_ = active; }
void GroupDXX::setMembers(std::vector<GroupMemberDXX> members) { members_ = std::move(members); }
void GroupDXX::setSubgroups(std::vector<SubgroupDXX> subgroups) { subgroups_ = std::move(subgroups); }
const std::vector<GroupMemberDXX>& GroupDXX::members() const { return members_; }
const std::vector<SubgroupDXX>& GroupDXX::subgroups() const { return subgroups_; }
GroupContextDXX GroupDXX::context(std::int64_t actor, std::int64_t target, std::int64_t creator) const {
    GroupContextDXX result;
    result.active = active_;
    for (const auto& member : members_) {
        if (member.accountId() == actor) {
            result.member = true;
            result.role = member.storedRole();
            result.owner = actor == ownerId_;
            result.creator = actor == creator;
        }
        if (member.accountId() == target) {
            result.targetMember = true;
            result.targetRole = member.storedRole();
        }
    }
    return result;
}
void GroupDXX::switchPolicy(std::string policy, std::int64_t version) {
    auto replacement = GroupPolicyFactoryDXX::create(policy);
    policy_ = std::move(replacement);
    policyVersion_ = version;
}
