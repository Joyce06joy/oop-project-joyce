#pragma once
#include "SocialRepositoryDXX.h"
class FriendServiceDXX {
public:
    explicit FriendServiceDXX(DatabaseDXX& db);
    void authorize(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
    JsonDXX read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
    JsonDXX mutate(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
private:
    DatabaseDXX& db_;
    SocialRepositoryDXX repository_;
    JsonDXX requestRecord(const JsonDXX& data);
};
