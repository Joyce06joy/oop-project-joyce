#pragma once
#include "AuthServiceDXX.h"
#include <map>
class RequestDispatcherDXX {
public:
    explicit RequestDispatcherDXX(DatabaseDXX& db);
    JsonDXX dispatch(std::uint64_t connection,const JsonDXX& request);
    void disconnected(std::uint64_t connection);
private:
    AuthServiceDXX auth_;
    std::map<std::uint64_t,JsonDXX> sessions_;
};
