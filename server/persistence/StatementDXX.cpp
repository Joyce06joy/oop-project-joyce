#include "StatementDXX.h"
#include <stdexcept>
StatementDXX::StatementDXX(sqlite3* db,const std::string& sql,const std::vector<JsonDXX>& params) {
    if(sqlite3_prepare_v2(db,sql.c_str(),-1,&stmt_,nullptr)!=SQLITE_OK) throw std::runtime_error("database prepare failed");
    int i=1;
    for(const auto& p:params) {
        int rc;
        if(p.isNull()) rc=sqlite3_bind_null(stmt_,i++);
        else if(p.isString()) { auto s=p.text(); rc=sqlite3_bind_text(stmt_,i++,s.data(),static_cast<int>(s.size()),SQLITE_TRANSIENT); }
        else rc=sqlite3_bind_int64(stmt_,i++,p.integer());
        if(rc!=SQLITE_OK) { sqlite3_finalize(stmt_); stmt_=nullptr; throw std::runtime_error("database bind failed"); }
    }
}
StatementDXX::~StatementDXX() { sqlite3_finalize(stmt_); }
std::vector<JsonDXX> StatementDXX::rows() {
    std::vector<JsonDXX> out; int rc;
    while((rc=sqlite3_step(stmt_))==SQLITE_ROW) {
        JsonDXX row({});
        for(int i=0;i<sqlite3_column_count(stmt_);++i) {
            JsonDXX v;
            if(sqlite3_column_type(stmt_,i)==SQLITE_INTEGER) v=JsonDXX(static_cast<std::int64_t>(sqlite3_column_int64(stmt_,i)));
            else if(sqlite3_column_type(stmt_,i)!=SQLITE_NULL) v=JsonDXX(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt_,i)),sqlite3_column_bytes(stmt_,i)));
            row.set(sqlite3_column_name(stmt_,i),v);
        }
        out.push_back(row);
    }
    if(rc!=SQLITE_DONE) throw std::runtime_error("database operation failed");
    return out;
}
