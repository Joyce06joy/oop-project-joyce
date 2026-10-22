#pragma once
#include "DatabaseDXX.h"
class MessageRepositoryDXX {
public:
 explicit MessageRepositoryDXX(DatabaseDXX& db);
 std::vector<JsonDXX> query(const std::string& sql,const std::vector<JsonDXX>& params={});
 std::int64_t insertedId() const;
 JsonDXX conversation(std::int64_t id);
 JsonDXX message(const JsonDXX& row);
 JsonDXX summary(const JsonDXX& conversation,std::int64_t actor);
 bool canSend(const JsonDXX& conversation,std::int64_t actor);
 static std::string visibility();
 static std::string messageColumns();
private:
 DatabaseDXX& db_;
};
