#pragma once
#include "GroupRepositoryDXX.h"
#include "GroupDXX.h"
#include <map>
#include <memory>
class GroupServiceDXX {
public:
    explicit GroupServiceDXX(DatabaseDXX& db);
    void authorize(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
    JsonDXX read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
    JsonDXX mutate(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& request);
    void committed(const JsonDXX& result);
    static std::int64_t positiveId(const JsonDXX& data,const std::string& field);
private:
    DatabaseDXX& db_;
    SocialRepositoryDXX social_;
    GroupRepositoryDXX repository_;
    std::map<std::int64_t,std::unique_ptr<GroupDXX>> groups_;
    GroupDXX& load(const JsonDXX& row);
    JsonDXX resolve(const JsonDXX& request);
    void activeGroup(const JsonDXX& row);
    void member(const GroupContextDXX& context);
    std::vector<JsonDXX> notices(const JsonDXX& row,const std::string& type,const std::string& change,const std::vector<std::int64_t>& users,const JsonDXX& extra=JsonDXX({}));
    JsonDXX changed(const JsonDXX& row,const JsonDXX& result,const std::vector<JsonDXX>& notifications);
    std::vector<std::string> displays(const JsonDXX& data,const std::string& field);
};
