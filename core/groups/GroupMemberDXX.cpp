#include "GroupMemberDXX.h"
#include <utility>
GroupMemberDXX::GroupMemberDXX(std::int64_t account, std::string role, std::string joinedAt)
    : accountId_(account), storedRole_(std::move(role)), joinedAt_(std::move(joinedAt)) {}
std::int64_t GroupMemberDXX::accountId() const { return accountId_; }
const std::string& GroupMemberDXX::storedRole() const { return storedRole_; }
const std::string& GroupMemberDXX::joinedAt() const { return joinedAt_; }
