#pragma once
#include "SocialRepositoryDXX.h"
class RecommendationServiceDXX {
public:
    explicit RecommendationServiceDXX(DatabaseDXX& db);
    JsonDXX read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
private:
    SocialRepositoryDXX repository_;
};
