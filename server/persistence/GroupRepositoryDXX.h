#pragma once
#include "DatabaseDXX.h"
#include "SocialRepositoryDXX.h"
#include "GroupMemberDXX.h"
#include "SubgroupDXX.h"
class GroupRepositoryDXX {
public:
    explicit GroupRepositoryDXX(DatabaseDXX& db);
    JsonDXX group(std::int64_t pk);
    JsonDXX subgroup(std::int64_t pk);
    JsonDXX request(std::int64_t pk);
    std::vector<GroupMemberDXX> members(std::int64_t group);
    std::vector<SubgroupDXX> subgroups(std::int64_t group);
    JsonDXX target(const std::string& service,const std::string& display);
    std::vector<std::int64_t> memberUsers(std::int64_t group);
    std::int64_t accountUser(std::int64_t account);
    JsonDXX summary(const JsonDXX& row,std::int64_t actor);
    JsonDXX page(const std::string& sql,const std::vector<JsonDXX>& params,const JsonDXX& data);
private:
    DatabaseDXX& db_;
    SocialRepositoryDXX social_;
};
