#include "MessageRepositoryDXX.h"
#include "BusinessErrorDXX.h"
MessageRepositoryDXX::MessageRepositoryDXX(DatabaseDXX& db):db_(db) {}
std::vector<JsonDXX> MessageRepositoryDXX::query(const std::string& s,const std::vector<JsonDXX>& p) {return db_.query(s,p);}
std::int64_t MessageRepositoryDXX::insertedId() const {return db_.insertedId();}
JsonDXX MessageRepositoryDXX::conversation(std::int64_t id) {auto r=query("SELECT * FROM conversations WHERE conversation_pk=?",{id}); if(r.empty()) throw BusinessErrorDXX("CHAT_CONVERSATION_NOT_FOUND","会话不存在"); return r[0];}
std::string MessageRepositoryDXX::messageColumns() {return "m.*,c.service_type,a.display_id AS sender_display_id,(SELECT COUNT(*) FROM message_receipts q WHERE q.message_pk=m.message_pk) AS recipient_count,(SELECT COUNT(*) FROM message_receipts q WHERE q.message_pk=m.message_pk AND q.delivered_at IS NOT NULL) AS delivered_count";}
std::string MessageRepositoryDXX::visibility() {return "((c.kind='PRIVATE' AND EXISTS (SELECT 1 FROM private_pairs p WHERE p.conversation_pk=c.conversation_pk AND ? IN (p.account_a,p.account_b))) OR (c.kind<>'PRIVATE' AND EXISTS (SELECT 1 FROM conversation_members e WHERE e.conversation_pk=c.conversation_pk AND e.account_pk=? AND m.message_pk>e.joined_after_message_id AND (e.left_at_message_id IS NULL OR m.message_pk<=e.left_at_message_id))))";}
JsonDXX MessageRepositoryDXX::message(const JsonDXX& r) {return {{"message_id",r["message_pk"]},{"conversation_id",r["conversation_pk"]},{"service_type",r["service_type"]},{"sender_display_id",r["sender_display_id"]},{"content",r["content"]},{"content_type","TEXT"},{"created_at",r["created_at"]},{"status","stored"},{"request_id",r["request_id"]},{"recipient_count",r["recipient_count"]},{"delivered_count",r["delivered_count"]}};}
bool MessageRepositoryDXX::canSend(const JsonDXX& c,std::int64_t actor) {
 if(c["status"].text()!="ACTIVE") return false;
 if(c["kind"].text()=="PRIVATE") return !query("SELECT 1 FROM private_pairs p JOIN friendships f ON f.account_a=p.account_a AND f.account_b=p.account_b JOIN accounts a ON a.account_pk=p.account_a JOIN accounts b ON b.account_pk=p.account_b WHERE p.conversation_pk=? AND ? IN (p.account_a,p.account_b) AND a.status='ACTIVE' AND b.status='ACTIVE'",{c["conversation_pk"],actor}).empty();
 auto g=query("SELECT 1 FROM groups g JOIN group_members m ON m.group_pk=g.group_pk WHERE g.group_pk=? AND g.status='ACTIVE' AND m.account_pk=?",{c["group_pk"],actor}); if(g.empty()) return false;
 if(c["kind"].text()=="SUBGROUP") return !query("SELECT 1 FROM subgroups s JOIN subgroup_members m ON m.subgroup_pk=s.subgroup_pk JOIN groups g ON g.group_pk=s.parent_group_pk WHERE s.subgroup_pk=? AND m.account_pk=? AND s.status='ACTIVE' AND g.policy_type='QQ' AND g.service_type='QQ'",{c["subgroup_pk"],actor}).empty();
 return true;
}
JsonDXX MessageRepositoryDXX::summary(const JsonDXX& c,std::int64_t actor) {
 std::string title;
 if(c["kind"].text()=="PRIVATE") {auto r=query("SELECT u.nickname,a.display_id FROM private_pairs p JOIN accounts a ON a.account_pk=CASE WHEN p.account_a=? THEN p.account_b ELSE p.account_a END JOIN users u ON u.user_id=a.user_id WHERE p.conversation_pk=?",{actor,c["conversation_pk"]}); if(!r.empty()) title=r[0]["nickname"].text();}
 else {auto r=query(c["kind"].text()=="GROUP"?"SELECT name FROM groups WHERE group_pk=?":"SELECT name FROM subgroups WHERE subgroup_pk=?",{c["kind"].text()=="GROUP"?c["group_pk"]:c["subgroup_pk"]});if(!r.empty()) title=r[0]["name"].text();}
 auto latest=query("SELECT COALESCE(MAX(m.message_pk),0) AS id FROM messages m JOIN conversations c ON c.conversation_pk=m.conversation_pk WHERE c.conversation_pk=? AND "+visibility(),{c["conversation_pk"],actor,actor})[0]["id"];
 JsonDXX r{{"conversation_id",c["conversation_pk"]},{"service_type",c["service_type"]},{"kind",c["kind"]},{"title",title},{"latest_message_id",latest},{"can_send",canSend(c,actor)}};
 if(c["kind"].text()=="PRIVATE") {auto peer=query("SELECT a.display_id FROM private_pairs p JOIN accounts a ON a.account_pk=CASE WHEN p.account_a=? THEN p.account_b ELSE p.account_a END WHERE p.conversation_pk=?",{actor,c["conversation_pk"]});if(!peer.empty())r.set("target_display_id",peer[0]["display_id"]);}
 if(!c["group_pk"].isNull())r.set("group_pk",c["group_pk"]);if(!c["subgroup_pk"].isNull())r.set("subgroup_pk",c["subgroup_pk"]);return r;
}
