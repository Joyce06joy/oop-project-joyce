#pragma once
#include "SocialRepositoryDXX.h"
class AccountServiceDXX {
public:
    explicit AccountServiceDXX(DatabaseDXX& db);
    JsonDXX binding(std::int64_t user);
    JsonDXX open(std::int64_t user,const JsonDXX& data);
    JsonDXX bind(std::int64_t user,const JsonDXX& data,bool create);
private:
    DatabaseDXX& db_;
    SocialRepositoryDXX repository_;
};
