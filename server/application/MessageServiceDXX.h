#pragma once
#include "MessageRepositoryDXX.h"
#include "SocialRepositoryDXX.h"
class MessageServiceDXX {
public:
 explicit MessageServiceDXX(DatabaseDXX& db);
 void authorize(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
 JsonDXX read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
 JsonDXX mutate(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
private:
 MessageRepositoryDXX repo_;
 SocialRepositoryDXX social_;
 JsonDXX actor(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
 JsonDXX groupConversation(const JsonDXX& data);
 void access(const JsonDXX& conversation,std::int64_t actor,bool sending);
 JsonDXX openPrivate(const JsonDXX& actor,const JsonDXX& data);
 static std::int64_t id(const JsonDXX& d,const std::string& key,bool zero=false);
 static std::int64_t limit(const JsonDXX& d);
};
