#pragma once
#include "DatabaseDXX.h"
class AuthServiceDXX {
public:
    explicit AuthServiceDXX(DatabaseDXX& db);
    JsonDXX registerUser(const JsonDXX& request);
    JsonDXX login(const JsonDXX& request);
    JsonDXX services(std::int64_t userId,const std::vector<std::string>& active={});
private:
    DatabaseDXX& db_;
    std::string dummyHash_;
    JsonDXX registrationResult(std::int64_t userId);
};
