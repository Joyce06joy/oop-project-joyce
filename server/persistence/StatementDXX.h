#pragma once
#include "JsonDXX.h"
#include <sqlite3.h>
#include <vector>
class StatementDXX {
public:
    StatementDXX(sqlite3* db,const std::string& sql,const std::vector<JsonDXX>& params);
    ~StatementDXX();
    StatementDXX(const StatementDXX&)=delete;
    StatementDXX& operator=(const StatementDXX&)=delete;
    std::vector<JsonDXX> rows();
private:
    sqlite3_stmt* stmt_=nullptr;
};
