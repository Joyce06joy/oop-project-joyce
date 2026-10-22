#pragma once
#include <cstdint>
#include <string>

class GroupMemberDXX {
public:
    GroupMemberDXX(std::int64_t account, std::string role, std::string joinedAt);
    std::int64_t accountId() const;
    const std::string& storedRole() const;
    const std::string& joinedAt() const;
private:
    std::int64_t accountId_;
    std::string storedRole_;
    std::string joinedAt_;
};
