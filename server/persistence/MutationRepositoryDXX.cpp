#include "MutationRepositoryDXX.h"
#include "BusinessErrorDXX.h"
#include "PasswordDXX.h"
#include "SocialRepositoryDXX.h"
MutationRepositoryDXX::MutationRepositoryDXX(DatabaseDXX& db):db_(db) {}
std::pair<JsonDXX,bool> MutationRepositoryDXX::execute(std::int64_t user,const JsonDXX& r,const std::function<JsonDXX()>& operation) {
    const auto fingerprint=PasswordDXX::fingerprint(JsonDXX{{"user_id",user},{"type",r["type"]},{"data",r["data"]}}.canonicalDump());
    db_.exec("BEGIN IMMEDIATE");
    try {
        const auto saved=db_.query("SELECT request_fingerprint,result_json FROM mutation_requests WHERE request_id=?",{r["request_id"]});
        if(!saved.empty()) {
            if(saved[0]["request_fingerprint"].text()!=fingerprint) throw BusinessErrorDXX("REQUEST_ID_CONFLICT","请求编号已用于其他操作或内容");
            auto result=JsonDXX::parse(saved[0]["result_json"].text()); db_.exec("COMMIT"); return {result,true};
        }
        auto result=operation();
        db_.query("INSERT INTO mutation_requests(request_id,user_id,request_type,request_fingerprint,result_json,created_at) VALUES(?,?,?,?,?,?)",{r["request_id"],user,r["type"],fingerprint,result.dump(),SocialRepositoryDXX::now()});
        db_.exec("COMMIT"); return {result,false};
    } catch(...) { try { db_.exec("ROLLBACK"); } catch(...) {} throw; }
}
