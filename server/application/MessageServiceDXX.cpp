#include "MessageServiceDXX.h"
#include "BusinessErrorDXX.h"
#include <algorithm>
#include <regex>
namespace {
bool onlyWhitespace(const std::string& text) {
    for(std::size_t i=0;i<text.size();) {
        auto byte=static_cast<unsigned char>(text[i++]);std::uint32_t cp=byte;int remaining=0;
        if(byte>=0xF0){cp=byte&7;remaining=3;}else if(byte>=0xE0){cp=byte&15;remaining=2;}else if(byte>=0xC0){cp=byte&31;remaining=1;}
        while(remaining--&&i<text.size())cp=(cp<<6)|(static_cast<unsigned char>(text[i++])&63);
        if(!(cp==0x20||(cp>=9&&cp<=13)||cp==0x85||cp==0xA0||cp==0x1680||(cp>=0x2000&&cp<=0x200A)||cp==0x2028||cp==0x2029||cp==0x202F||cp==0x205F||cp==0x3000))return false;
    }return true;
}
}

MessageServiceDXX::MessageServiceDXX(DatabaseDXX& db):repo_(db),social_(db) {}
std::int64_t MessageServiceDXX::id(const JsonDXX& d,const std::string& k,bool zero) {if(!d[k].isInteger()||d[k].integer()<(zero?0:1))throw BusinessErrorDXX("CHAT_CURSOR_INVALID","编号或游标无效");return d[k].integer();}
std::int64_t MessageServiceDXX::limit(const JsonDXX& d) {if(!d.has("limit"))return 50;auto n=id(d,"limit");if(n>50)throw BusinessErrorDXX("CHAT_CURSOR_INVALID","每页最多50条");return n;}
JsonDXX MessageServiceDXX::groupConversation(const JsonDXX& d) {
 auto g=id(d,"group_pk");std::vector<JsonDXX> rows;
 if(d.has("subgroup_pk"))rows=repo_.query("SELECT * FROM conversations WHERE kind='SUBGROUP' AND group_pk=? AND subgroup_pk=?",{g,id(d,"subgroup_pk")});
 else rows=repo_.query("SELECT * FROM conversations WHERE kind='GROUP' AND group_pk=?",{g});
 if(rows.empty())throw BusinessErrorDXX("CHAT_CONVERSATION_NOT_FOUND","会话不存在");return rows[0];
}
JsonDXX MessageServiceDXX::actor(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
 auto d=r["data"];std::string service=d["service_type"].text();auto t=r["type"].text();
 if(t=="chat.group.open"||t=="chat.group.send")service=groupConversation(d)["service_type"].text();
 if(t=="chat.history.get")service=repo_.conversation(id(d,"conversation_id"))["service_type"].text();
 if(t=="chat.message.ack") {auto rows=repo_.query("SELECT c.service_type FROM messages m JOIN conversations c ON c.conversation_pk=m.conversation_pk WHERE m.message_pk=?",{id(d,"message_id")});if(rows.empty())throw BusinessErrorDXX("CHAT_CONVERSATION_NOT_FOUND","消息不存在");service=rows[0]["service_type"].text();}
 if(service!="QQ"&&service!="WECHAT")throw BusinessErrorDXX("CHAT_SEND_NOT_ALLOWED","此服务不支持聊天");
 if(d.has("service_type")&&d["service_type"].text()!=service)throw BusinessErrorDXX("CHAT_SEND_NOT_ALLOWED","服务不匹配");
 return social_.owned(user,service,active);
}
void MessageServiceDXX::authorize(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
 for(const auto& k:{"sender_id","from_account","sender_account_pk"})if(r["data"].has(k))throw BusinessErrorDXX("VALIDATION_ERROR","发送者由登录会话确定");
 actor(user,active,r);
}
void MessageServiceDXX::access(const JsonDXX& c,std::int64_t a,bool sending) {
 if(c["kind"].text()=="SUBGROUP") {auto rows=repo_.query("SELECT policy_type FROM groups WHERE group_pk=?",{c["group_pk"]});if(rows.empty()||rows[0]["policy_type"].text()!="QQ")throw BusinessErrorDXX("CHAT_POLICY_DISABLED","当前策略停用子群聊天");}
 if(sending) {if(!repo_.canSend(c,a))throw BusinessErrorDXX(c["kind"].text()=="PRIVATE"?"CHAT_NOT_FRIENDS":"CHAT_NOT_MEMBER","无当前发送权限");return;}
 bool exists;
 if(c["kind"].text()=="PRIVATE")exists=!repo_.query("SELECT 1 FROM private_pairs WHERE conversation_pk=? AND ? IN(account_a,account_b)",{c["conversation_pk"],a}).empty();
 else exists=!repo_.query("SELECT 1 FROM conversation_members WHERE conversation_pk=? AND account_pk=?",{c["conversation_pk"],a}).empty();
 if(!exists)throw BusinessErrorDXX("CHAT_NOT_MEMBER","无历史访问权限");
}
JsonDXX MessageServiceDXX::openPrivate(const JsonDXX& a,const JsonDXX& d) {
 auto target=social_.target(a["service_type"].text(),SocialRepositoryDXX::stringField(d,"target_display_id",128));auto x=a["account_pk"].integer(),y=target["account_pk"].integer();
 if(!social_.friends(x,y))throw BusinessErrorDXX("CHAT_NOT_FRIENDS","请先成为好友");
 auto rows=repo_.query("SELECT c.* FROM private_pairs p JOIN conversations c ON c.conversation_pk=p.conversation_pk WHERE p.service_type=? AND p.account_a=? AND p.account_b=?",{a["service_type"],std::min(x,y),std::max(x,y)});
 if(!rows.empty())return rows[0];
 repo_.query("INSERT INTO conversations(service_type,kind,created_at) VALUES(?,'PRIVATE',?)",{a["service_type"],SocialRepositoryDXX::now()});auto pk=repo_.insertedId();
 repo_.query("INSERT INTO private_pairs(conversation_pk,service_type,account_a,account_b) VALUES(?,?,?,?)",{pk,a["service_type"],std::min(x,y),std::max(x,y)});return repo_.conversation(pk);
}
JsonDXX MessageServiceDXX::read(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
 auto a=actor(user,active,r);auto account=a["account_pk"].integer();auto d=r["data"];auto t=r["type"].text();
 if(t=="chat.group.open") {auto c=groupConversation(d);access(c,account,true);return repo_.summary(c,account);}
 if(t=="chat.conversation.list") {
  auto n=limit(d);std::int64_t offset=0;if(d.has("cursor")&&!d["cursor"].isNull()){auto s=d["cursor"].text();if(!d["cursor"].isString()||!std::regex_match(s,std::regex("[0-9]{1,7}")))throw BusinessErrorDXX("CHAT_CURSOR_INVALID","分页游标无效");offset=std::stoll(s);}
  std::string sql=" FROM conversations c WHERE c.service_type=? AND ((c.kind='PRIVATE' AND EXISTS(SELECT 1 FROM private_pairs p WHERE p.conversation_pk=c.conversation_pk AND ? IN(p.account_a,p.account_b))) OR EXISTS(SELECT 1 FROM conversation_members e WHERE e.conversation_pk=c.conversation_pk AND e.account_pk=?)) AND (c.kind<>'SUBGROUP' OR EXISTS(SELECT 1 FROM groups g WHERE g.group_pk=c.group_pk AND g.policy_type='QQ'))";
  auto total=repo_.query("SELECT COUNT(*) AS n"+sql,{a["service_type"],account,account})[0]["n"].integer();
  auto rows=repo_.query("SELECT c.*"+sql+" ORDER BY (SELECT COALESCE(MAX(m.message_pk),0) FROM messages m WHERE m.conversation_pk=c.conversation_pk AND "+MessageRepositoryDXX::visibility()+") DESC,c.conversation_pk DESC LIMIT ? OFFSET ?",{a["service_type"],account,account,account,account,n,offset});std::vector<JsonDXX> items;for(auto& c:rows)items.push_back(repo_.summary(c,account));auto end=offset+static_cast<std::int64_t>(items.size());return {{"items",JsonDXX::array(items)},{"total",total},{"next_cursor",end<total?JsonDXX(std::to_string(end)):JsonDXX()}};
 }
 auto n=limit(d);bool sync=t=="chat.sync",before=d.has("before_message_id"),after=d.has("after_message_id");if(before&&after)throw BusinessErrorDXX("CHAT_CURSOR_INVALID","游标互斥");
 std::string where="c.service_type=? AND "+MessageRepositoryDXX::visibility();std::vector<JsonDXX> p{a["service_type"],account,account};std::int64_t cursor=0;
 if(sync){cursor=id(d,"after_message_id",true);where+=" AND m.message_pk>? AND (c.kind<>'SUBGROUP' OR EXISTS(SELECT 1 FROM groups g WHERE g.group_pk=c.group_pk AND g.policy_type='QQ'))";p.emplace_back(cursor);}
 else {auto c=repo_.conversation(id(d,"conversation_id"));access(c,account,false);where+=" AND c.conversation_pk=?";p.push_back(c["conversation_pk"]);if(before||after){cursor=id(d,before?"before_message_id":"after_message_id",true);where+=before?" AND m.message_pk<?":" AND m.message_pk>?";p.emplace_back(cursor);}}
 const bool descending=!sync&&!after;p.emplace_back(n+1);auto rows=repo_.query("SELECT "+MessageRepositoryDXX::messageColumns()+" FROM messages m JOIN conversations c ON c.conversation_pk=m.conversation_pk JOIN accounts a ON a.account_pk=m.sender_account_pk WHERE "+where+" ORDER BY m.message_pk "+(descending?"DESC":"ASC")+" LIMIT ?",p);bool more=static_cast<std::int64_t>(rows.size())>n;if(more)rows.pop_back();
 // Count escaped JSON bytes, not UTF-8 content bytes. Keep the nearest slice
 // in SQL order, then reverse a before-page for chronological presentation.
 std::size_t responseBytes=0,kept=0;
 for(const auto& row:rows) {const auto bytes=repo_.message(row).dump().size()+1;if(responseBytes+bytes>56*1024){more=true;break;}responseBytes+=bytes;++kept;}
 rows.resize(kept);if(descending)std::reverse(rows.begin(),rows.end());std::vector<JsonDXX> items;for(auto& row:rows)items.push_back(repo_.message(row));
 if(sync)return {{"items",JsonDXX::array(items)},{"has_more",more},{"next_cursor",rows.empty()?cursor:rows.back()["message_pk"].integer()}};
 return {{"items",JsonDXX::array(items)},{"has_more",more},{"before_cursor",rows.empty()?JsonDXX():rows.front()["message_pk"]},{"after_cursor",rows.empty()?JsonDXX():rows.back()["message_pk"]}};
}
JsonDXX MessageServiceDXX::mutate(std::int64_t user,const std::vector<std::string>& active,const JsonDXX& r) {
 auto a=actor(user,active,r);auto account=a["account_pk"].integer();auto d=r["data"];auto t=r["type"].text();
 if(t=="chat.private.open"){auto c=openPrivate(a,d);return SocialRepositoryDXX::changed(repo_.summary(c,account),{});}
 if(t=="chat.message.ack") {
  auto mid=id(d,"message_id");auto rows=repo_.query("SELECT m.*,c.service_type,x.display_id AS recipient_display_id,s.user_id AS sender_user FROM message_receipts q JOIN messages m ON m.message_pk=q.message_pk JOIN conversations c ON c.conversation_pk=m.conversation_pk JOIN accounts x ON x.account_pk=q.recipient_account_pk JOIN accounts s ON s.account_pk=m.sender_account_pk WHERE q.message_pk=? AND q.recipient_account_pk=?",{mid,account});if(rows.empty())throw BusinessErrorDXX("CHAT_SEND_NOT_ALLOWED","不是此消息的接收账号");auto q=rows[0];auto changed=repo_.query("SELECT 1 FROM message_receipts WHERE message_pk=? AND recipient_account_pk=? AND delivered_at IS NULL",{mid,account});
  repo_.query("UPDATE message_receipts SET delivered_at=COALESCE(delivered_at,?) WHERE message_pk=? AND recipient_account_pk=?",{SocialRepositoryDXX::now(),mid,account});JsonDXX result{{"message_id",mid},{"status","delivered"}};std::vector<JsonDXX> notices;if(!changed.empty()){auto e=result;e.set("conversation_id",q["conversation_pk"]);e.set("service_type",q["service_type"]);e.set("recipient_display_id",q["recipient_display_id"]);auto counts=repo_.query("SELECT COUNT(*) AS recipient_count,COUNT(delivered_at) AS delivered_count FROM message_receipts WHERE message_pk=?",{mid})[0];e.set("recipient_count",counts["recipient_count"]);e.set("delivered_count",counts["delivered_count"]);notices.push_back(SocialRepositoryDXX::notice(q["sender_user"].integer(),"chat.message.delivered",q["service_type"].text(),e));}return SocialRepositoryDXX::changed(result,notices);
 }
 auto content=d["content"].text();if(!d["content"].isString()||content.empty()||content.find('\0')!=std::string::npos)throw BusinessErrorDXX("CHAT_INVALID_CONTENT","正文无效");if(content.size()>4096)throw BusinessErrorDXX("CHAT_MESSAGE_TOO_LARGE","正文最多4096字节");if(onlyWhitespace(content))throw BusinessErrorDXX("CHAT_INVALID_CONTENT","正文不能为空白");
 auto c=t=="chat.private.send"?openPrivate(a,d):groupConversation(d);access(c,account,true);
 repo_.query("INSERT INTO messages(conversation_pk,sender_account_pk,request_id,operation_type,content,created_at) VALUES(?,?,?,?,?,?)",{c["conversation_pk"],account,r["request_id"],t,content,SocialRepositoryDXX::now()});auto mid=repo_.insertedId();
 std::vector<JsonDXX> recipients;
 if(c["kind"].text()=="PRIVATE")recipients=repo_.query("SELECT a.account_pk,a.user_id FROM private_pairs p JOIN accounts a ON a.account_pk IN(p.account_a,p.account_b) WHERE p.conversation_pk=? AND a.status='ACTIVE'",{c["conversation_pk"]});
 else if(c["kind"].text()=="GROUP")recipients=repo_.query("SELECT a.account_pk,a.user_id FROM group_members m JOIN accounts a ON a.account_pk=m.account_pk WHERE m.group_pk=? AND a.status='ACTIVE'",{c["group_pk"]});
 else recipients=repo_.query("SELECT a.account_pk,a.user_id FROM subgroup_members m JOIN group_members p ON p.group_pk=m.parent_group_pk AND p.account_pk=m.account_pk JOIN accounts a ON a.account_pk=m.account_pk WHERE m.subgroup_pk=? AND a.status='ACTIVE'",{c["subgroup_pk"]});
 for(const auto& recipient:recipients)if(recipient["account_pk"].integer()!=account)repo_.query("INSERT INTO message_receipts(message_pk,recipient_account_pk) VALUES(?,?)",{mid,recipient["account_pk"]});
 auto row=repo_.query("SELECT "+MessageRepositoryDXX::messageColumns()+" FROM messages m JOIN conversations c ON c.conversation_pk=m.conversation_pk JOIN accounts a ON a.account_pk=m.sender_account_pk WHERE m.message_pk=?",{mid})[0];auto message=repo_.message(row);std::vector<JsonDXX> notices;
 for(auto& recipient:recipients){notices.push_back(SocialRepositoryDXX::notice(recipient["user_id"].integer(),"chat.message.new",c["service_type"].text(),message));}return SocialRepositoryDXX::changed(message,notices);
}
