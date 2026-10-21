#pragma once
#include "DatabaseDXX.h"
#include <string>
#include <vector>
class SocialRepositoryDXX {
public:
    explicit SocialRepositoryDXX(DatabaseDXX& db);
    static bool validService(const std::string& service);
    static std::string now();
    static std::string stringField(const JsonDXX& data,const std::string& key,std::size_t max,bool optional=false);
    static JsonDXX changed(const JsonDXX& result,const std::vector<JsonDXX>& notices);
    static JsonDXX notice(std::int64_t user,const std::string& type,const std::string& service,const JsonDXX& data);
    JsonDXX owned(std::int64_t user,const std::string& service,const std::vector<std::string>& active,bool requireActive=true);
    JsonDXX target(const std::string& service,const std::string& display);
    JsonDXX byId(std::int64_t pk);
    bool friends(std::int64_t a,std::int64_t b);
    bool pending(std::int64_t a,std::int64_t b);
    bool bound(std::int64_t user);
    JsonDXX profile(const JsonDXX& account,std::int64_t actor);
    JsonDXX page(const std::string& sql,const std::vector<JsonDXX>& params,const JsonDXX& data,std::int64_t actor,bool profiles=true);
    static const std::string Columns;
private:
    DatabaseDXX& db_;
};
