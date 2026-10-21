#pragma once
#include "DatabaseDXX.h"
#include <functional>
class MutationRepositoryDXX {
public:
    explicit MutationRepositoryDXX(DatabaseDXX& db);
    // second is true for a replay. Notifications are delivered only after a new commit.
    std::pair<JsonDXX,bool> execute(std::int64_t user,const JsonDXX& request,const std::function<JsonDXX()>& operation);
private:
    DatabaseDXX& db_;
};
