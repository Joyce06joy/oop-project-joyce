#include "DatabaseDXX.h"
#include "StatementDXX.h"
#include "SchemaDXX.h"
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdexcept>
DatabaseDXX::DatabaseDXX(const std::string& path) {
    // One server owns this database, even if another port is selected.
    lock_=open((path+".lock").c_str(),O_CREAT|O_RDWR,0600);
    if(lock_<0||flock(lock_,LOCK_EX|LOCK_NB)!=0) { if(lock_>=0) close(lock_); throw std::runtime_error("database unavailable or already owned"); }
    try {
        if(sqlite3_open_v2(path.c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_NOMUTEX,nullptr)!=SQLITE_OK) throw std::runtime_error("database open failed");
        sqlite3_busy_timeout(db_,1000);
        exec("PRAGMA foreign_keys=ON");
        const auto version=query("PRAGMA user_version")[0]["user_version"].integer();
        if(version==0) {
            if(!query("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'").empty()) throw std::runtime_error("refusing unknown nonempty database");
            exec(SchemaDXX);
        } else if(version!=1) throw std::runtime_error("unsupported database schema version");
        exec("PRAGMA journal_mode=WAL");
        if(query("PRAGMA foreign_keys")[0]["foreign_keys"].integer()!=1) throw std::runtime_error("foreign keys unavailable");
        query("SELECT user_id,password_hash FROM users LIMIT 0");
    } catch(...) { sqlite3_close(db_); db_=nullptr; close(lock_); throw; }
}
DatabaseDXX::~DatabaseDXX() { sqlite3_close(db_); if(lock_>=0) close(lock_); }
void DatabaseDXX::exec(const std::string& sql) { if(sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,nullptr)!=SQLITE_OK) throw std::runtime_error("database operation failed"); }
std::vector<JsonDXX> DatabaseDXX::query(const std::string& sql,const std::vector<JsonDXX>& params) { StatementDXX statement(db_,sql,params); return statement.rows(); }
std::int64_t DatabaseDXX::insertedId() const { return sqlite3_last_insert_rowid(db_); }
