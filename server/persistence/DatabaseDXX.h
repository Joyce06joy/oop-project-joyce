#pragma once
#include "JsonDXX.h"
#include <sqlite3.h>
#include <vector>
class DatabaseDXX {
public:
    explicit DatabaseDXX(const std::string& path);
    ~DatabaseDXX();
    DatabaseDXX(const DatabaseDXX&)=delete;
    DatabaseDXX& operator=(const DatabaseDXX&)=delete;
    void exec(const std::string& sql);
    std::vector<JsonDXX> query(const std::string& sql,const std::vector<JsonDXX>& params={});
    std::int64_t insertedId() const;
private:
    sqlite3* db_=nullptr;
    int lock_=-1;
    void backupBeforeMigration(const std::string& path,int version);
};
