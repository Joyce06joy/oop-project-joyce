#pragma once
#include "AuthServiceDXX.h"
#include "AccountServiceDXX.h"
#include "FriendServiceDXX.h"
#include "RecommendationServiceDXX.h"
#include "MutationRepositoryDXX.h"
#include <map>
class RequestDispatcherDXX {
public:
    explicit RequestDispatcherDXX(DatabaseDXX& db);
    JsonDXX dispatch(std::uint64_t connection,const JsonDXX& request);
    void disconnected(std::uint64_t connection);
    std::vector<std::pair<std::uint64_t,JsonDXX>> takeEvents();
private:
    AuthServiceDXX auth_;
    AccountServiceDXX accounts_;
    FriendServiceDXX friends_;
    RecommendationServiceDXX recommendations_;
    MutationRepositoryDXX mutations_;
    std::vector<std::pair<std::uint64_t,JsonDXX>> events_;
    void publish(const JsonDXX& notifications);
    std::map<std::uint64_t,JsonDXX> sessions_;
};
