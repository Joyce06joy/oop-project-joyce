#include "DatabaseDXX.h"
#include "StatementDXX.h"
#include "SchemaDXX.h"
#include "MigrationDXX.h"
#include <iostream>
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
        } else if(version!=1&&version!=2) throw std::runtime_error("unsupported database schema version");
        if(query("PRAGMA quick_check").at(0)["quick_check"].text()!="ok"||!query("PRAGMA foreign_key_check").empty()) throw std::runtime_error("database integrity check failed");
        if(version<2) {
            if(version==1) backupBeforeMigration(path);
            try { exec(MigrationV2DXX); }
            catch(...) { try { exec("ROLLBACK"); } catch(...) {} throw; }
        }
        if(!query("PRAGMA foreign_key_check").empty()) throw std::runtime_error("migration foreign key check failed");
        exec("PRAGMA journal_mode=WAL");
        if(query("PRAGMA foreign_keys")[0]["foreign_keys"].integer()!=1) throw std::runtime_error("foreign keys unavailable");
        query("SELECT user_id,password_hash FROM users LIMIT 0");
    } catch(...) { sqlite3_close(db_); db_=nullptr; close(lock_); throw; }
}
DatabaseDXX::~DatabaseDXX() { sqlite3_close(db_); if(lock_>=0) close(lock_); }
void DatabaseDXX::exec(const std::string& sql) { if(sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,nullptr)!=SQLITE_OK) throw std::runtime_error("database operation failed"); }
std::vector<JsonDXX> DatabaseDXX::query(const std::string& sql,const std::vector<JsonDXX>& params) { StatementDXX statement(db_,sql,params); return statement.rows(); }
std::int64_t DatabaseDXX::insertedId() const { return sqlite3_last_insert_rowid(db_); }

void DatabaseDXX::backupBeforeMigration(const std::string& path) {
    // SQLite backup includes committed WAL contents. Never overwrite a prior backup.
    std::string backupPath=path+".v1-backup.sqlite3";
    int fd=-1;
    for(int suffix=0;suffix<1000;++suffix) {
        backupPath=path+".v1-backup"+(suffix?"-"+std::to_string(suffix):std::string())+".sqlite3";
        fd=open(backupPath.c_str(),O_CREAT|O_EXCL|O_RDWR,0600);
        if(fd>=0) break;
        if(errno!=EEXIST) throw std::runtime_error("cannot create migration backup");
    }
    if(fd<0) throw std::runtime_error("cannot reserve migration backup");
    close(fd); sqlite3* copy=nullptr;
    try {
        if(sqlite3_open_v2(backupPath.c_str(),&copy,SQLITE_OPEN_READWRITE|SQLITE_OPEN_NOMUTEX,nullptr)!=SQLITE_OK) throw std::runtime_error("cannot open migration backup");
        auto* backup=sqlite3_backup_init(copy,"main",db_,"main");
        if(!backup) throw std::runtime_error("cannot initialize migration backup");
        const auto rc=sqlite3_backup_step(backup,-1); const auto finish=sqlite3_backup_finish(backup);
        if(rc!=SQLITE_DONE||finish!=SQLITE_OK) throw std::runtime_error("migration backup failed");
        { StatementDXX check(copy,"PRAGMA quick_check",{});
          if(check.rows().at(0)["quick_check"].text()!="ok") throw std::runtime_error("migration backup integrity failed"); }

        sqlite3_close(copy); copy=nullptr;
        std::cout<<"schema v1 backup verified; migrating to v2"<<std::endl;
    } catch(...) { sqlite3_close(copy); throw; }
}
