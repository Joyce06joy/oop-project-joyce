#include "ChatPageDXX.h"
#include "MessageModelDXX.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QTest>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QListView>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <iostream>
#include <memory>
#include <functional>
#include <stdexcept>
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void until(const std::function<bool()>&f,const char*m,int ms=12000){QElapsedTimer t;t.start();while(!f()&&t.elapsed()<ms)QTest::qWait(10);check(f(),m);}
JsonDXX rpc(TcpClientDXX&c,const QString&t,const JsonDXX&d){QString id;JsonDXX out;bool done=false;auto con=QObject::connect(&c,&TcpClientDXX::responseReceived,[&](const JsonDXX&r){if(QString::fromStdString(r["request_id"].text())==id){out=r;done=true;}});id=c.request(t,d);until([&]{return done;},"RPC timeout");QObject::disconnect(con);if(!out["success"].boolean())throw std::runtime_error(t.toStdString()+out["error"].dump());return out["data"];}
JsonDXX account(TcpClientDXX&c,const char*name){rpc(c,"auth.register",{{"login_name",name},{"nickname",name},{"password","Mvp4Password!"},{"services",JsonDXX::array({"QQ","WECHAT","WEIBO"})}});return rpc(c,"auth.login",{{"login_name",name},{"password","Mvp4Password!"}});}
std::string display(const JsonDXX&a,const char*s){for(auto i:a["services"].items())if(i["service_type"].text()==s)return i["display_id"].text();throw std::runtime_error("display missing");}
void activate(TcpClientDXX&c,const char*s){rpc(c,"account.service.activate",{{"service_type",s}});}
void friends(TcpClientDXX&a,TcpClientDXX&b,const JsonDXX&bob,const char*s){rpc(a,"friend.request.send",{{"service_type",s},{"target_display_id",display(bob,s)},{"verification_message","smoke"}});auto list=rpc(b,"friend.request.pending",{{"service_type",s}});auto item=list["items"].items().at(0);rpc(b,"friend.request.respond",{{"friend_request_id",item["friend_request_id"]},{"decision","ACCEPT"}});}
}
int main(int argc,char**argv){QApplication app(argc,argv);QProcess server;std::vector<std::unique_ptr<QProcess>> processes;
 try{
 check(argc>=3,"requires server and client paths");QTemporaryDir tmp;check(tmp.isValid(),"temporary database");QTcpServer probe;check(probe.listen(QHostAddress::LocalHost,0),"port");auto port=probe.serverPort();probe.close();auto db=tmp.filePath("mvp4-ui.sqlite3");
 auto start=[&]{server.start(QString::fromLocal8Bit(argv[1]),{"--db",db,"--port",QString::number(port)});check(server.waitForStarted(),"start server");QByteArray log;until([&]{log+=server.readAllStandardOutput();return log.contains("listening");},"listening");};start();
 TcpClientDXX a(port),b(port),c(port);for(auto x:{&a,&b,&c})x->connectServer();until([&]{return a.connected()&&b.connected()&&c.connected();},"setup TCP");auto alice=account(a,"M4UiAlice"),bob=account(b,"M4UiBob"),carol=account(c,"M4UiCarol");for(auto s:{"QQ","WECHAT"}){for(auto x:{&a,&b,&c})activate(*x,s);friends(a,b,bob,s);}
 auto group=rpc(a,"group.create",{{"service_type","QQ"},{"name","M4 Native Process Group"}})["group_pk"];
 for(auto x:{&b,&c}){rpc(*x,"group.join.apply",{{"group_pk",group}});auto list=rpc(a,"group.request.list",{{"service_type","QQ"}});auto item=list["items"].items().at(0);rpc(a,"group.request.respond",{{"request_pk",item["request_pk"]},{"decision","ACCEPT"}});}
 auto sub=rpc(a,"group.subgroup.create",{{"group_pk",group},{"name","Native Subgroup"},{"member_display_ids",JsonDXX::array({display(bob,"QQ")})}})["subgroup_pk"];

 auto round=[&](const QString&label,const QString&service,const QStringList&route,bool carolReceives){processes.clear();QStringList logs;for(int i=0;i<3;++i){auto p=std::make_unique<QProcess>();p->setProcessChannelMode(QProcess::MergedChannels);QStringList args{"--port",QString::number(port),"--chat-smoke","--smoke-login",QStringList{"M4UiAlice","M4UiBob","M4UiCarol"}[i],"--smoke-password","Mvp4Password!","--smoke-service",service,"--smoke-duration","5000"};if(i==0){args+=route;args<<"--smoke-text"<<label;}else if(i==1||carolReceives)args<<"--smoke-expect"<<label;if(argc>=4)args<<"--smoke-screenshot"<<QDir(QString::fromLocal8Bit(argv[3])).filePath(label+QString::number(i)+".png");p->start(QString::fromLocal8Bit(argv[2]),args);check(p->waitForStarted(),"independent IMClient start");processes.push_back(std::move(p));}check(processes[0]->processId()!=processes[1]->processId()&&processes[1]->processId()!=processes[2]->processId(),"three independent PIDs");until([&]{return processes[0]->state()==QProcess::NotRunning&&processes[1]->state()==QProcess::NotRunning&&processes[2]->state()==QProcess::NotRunning;},"native smoke complete",16000);for(int i=0;i<3;++i){auto log=QString::fromUtf8(processes[i]->readAll());std::cout<<log.toStdString();check(processes[i]->exitCode()==0,"native process failed");check(log.contains("SMOKE READY"),"independent authenticated connection");if(i==1||i==2&&carolReceives)check(log.count("content= "+label)==1,"one real event row per receiver");if(i==2&&!carolReceives)check(!log.contains("content= "+label),"nonrecipient receives nothing");}std::cout<<label.toStdString()<<" PASS three independent Qt processes / TCP / real widget receive\n";};
 if(argc>=4)QDir().mkpath(QString::fromLocal8Bit(argv[3]));
 round("M4-QQ-private-中文😀","QQ",{"--smoke-target",QString::fromStdString(display(bob,"QQ"))},false);
 round("M4-WECHAT-private","WECHAT",{"--smoke-target",QString::fromStdString(display(bob,"WECHAT"))},false);
 round("M4-QQ-group","QQ",{"--smoke-group",QString::number(group.integer())},true);
 round("M4-QQ-subgroup","QQ",{"--smoke-group",QString::number(group.integer()),"--smoke-subgroup",QString::number(sub.integer())},false);
 ChatPageDXX page(a);page.resize(950,700);page.show();page.setAccount(QString::number(alice["user_id"].integer()),"QQ",QString::fromStdString(display(alice,"QQ")),true);qint64 conversation=0;QObject::connect(&page,&ChatPageDXX::conversationOpened,[&](qint64 id){conversation=id;});page.openPrivate(QString::fromStdString(display(bob,"QQ")));until([&]{return conversation>0;},"open actual native page");
 for(int i=0;i<65;++i)rpc(b,"chat.private.send",{{"service_type","QQ"},{"target_display_id",display(alice,"QQ")},{"content",std::string("page-")+std::to_string(i)}});
 page.openPrivate(QString::fromStdString(display(bob,"QQ")));until([&]{bool last=false;for(const auto &message:page.currentModel()->messages())if(message["content"].text()=="page-64")last=true;return page.currentModel()->rowCount()>=65&&last;},"all65 real events observed before dedup check");auto count=page.currentModel()->rowCount();page.findChild<QPushButton*>("chatOlder")->click();QTest::qWait(200);check(page.currentModel()->rowCount()==count,"history event dedup");
 auto *input=page.findChild<QTextEdit*>("chatInput");input->setPlainText("retained draft");page.openGroup(group.integer());until([&]{return conversation!=rpc(a,"chat.private.open",{{"service_type","QQ"},{"target_display_id",display(bob,"QQ")}})["conversation_id"].integer();},"switch group");page.openPrivate(QString::fromStdString(display(bob,"QQ")));until([&]{return input->toPlainText()=="retained draft";},"per conversation draft");

 // Bob has no chat page yet: these 55 incoming messages have no automatic ACK.
 for(int i=0;i<55;++i)rpc(a,"chat.private.send",{{"service_type","QQ"},{"target_display_id",display(bob,"QQ")},{"content",std::string("offline-ack-")+std::to_string(i)}});
 ChatPageDXX freshBob(b);freshBob.setAccount(QString::number(bob["user_id"].integer()),"QQ",QString::fromStdString(display(bob,"QQ")),true);freshBob.openPrivate(QString::fromStdString(display(alice,"QQ")));
 until([&]{return freshBob.currentModel()&&freshBob.currentModel()->rowCount()==50;},"fresh Bob recent page bounded50");
 until([&]{check(b.connected(),"50-message history ACK burst disconnected Bob");auto history=rpc(a,"chat.history.get",{{"conversation_id",conversation},{"limit",50}});int delivered=0;for(auto m:history["items"].items())if(m["content"].text().find("offline-ack-")==0&&m["delivered_count"].integer()==1)++delivered;return delivered==50;},"all50 actual ACK receipts persisted",20000);
 std::cout<<"M4 native recent50 ACK queue PASS bounded page, connection remains live,50 persisted delivery receipts\n";

 auto *recent=page.findChild<QListWidget*>("chatConversations");
 auto latestPrivate=rpc(a,"chat.private.send",{{"service_type","QQ"},{"target_display_id",display(bob,"QQ")},{"content","recent-private-first"}});
 until([&]{for(auto m:page.currentModel()->messages())if(m["message_id"].integer()==latestPrivate["message_id"].integer())return true;return false;},"private live event rendered before recent ordering check");
 check(recent->count()>=2&&recent->item(0)->data(Qt::UserRole).toLongLong()==conversation,"latest private conversation must be first");
 auto latestGroup=rpc(a,"chat.group.send",{{"group_pk",group},{"content","recent-group-second"}});
 until([&]{return recent->item(0)->data(Qt::UserRole).toLongLong()==latestGroup["conversation_id"].integer();},"new group message must move group above private");
 auto newestPrivate=rpc(a,"chat.private.send",{{"service_type","QQ"},{"target_display_id",display(bob,"QQ")},{"content","recent-private-third"}});
 until([&]{for(auto m:page.currentModel()->messages())if(m["message_id"].integer()==newestPrivate["message_id"].integer())return true;return false;},"second private event received");
 check(recent->item(0)->data(Qt::UserRole).toLongLong()==conversation,"new private message must move private above group");
 std::cout<<"M4 native recent conversation ordering PASS real private/group/private messages move latest conversation to row0\n";
 // Test-only fault injection changes only a fresh temporary server database.
 auto fault=[&](bool enable){QProcess sql;sql.start("/usr/bin/python3",{"-c","import sqlite3,sys;c=sqlite3.connect(sys.argv[1]);c.execute(sys.argv[2]);c.commit()",db,enable?"CREATE TRIGGER fail_m4_ui BEFORE INSERT ON messages BEGIN SELECT RAISE(ABORT,'native ui test fault'); END":"DROP TRIGGER fail_m4_ui"});check(sql.waitForFinished()&&sql.exitCode()==0,"fault injection");};fault(true);page.sendText("must fail without stored");until([&]{for(auto m:page.currentModel()->messages())if(m["content"].text()=="must fail without stored")return m["status"].text()=="failed";return false;},"SQL failure must show failed");check(input->toPlainText()=="must fail without stored","failed input recovered");fault(false);

 // Transparent fault proxy still forwards the existing TCP protocol to the real server.
 // Drop only the first COMMIT-backed send response and its sender echo, then close.
 QTcpServer proxy;check(proxy.listen(QHostAddress::LocalHost,0),"response loss proxy");bool dropResponse=true;
 QObject::connect(&proxy,&QTcpServer::newConnection,&proxy,[&]{while(proxy.hasPendingConnections()){
  auto *down=proxy.nextPendingConnection();auto *up=new QTcpSocket(&proxy);auto decoder=std::make_shared<ProtocolDXX>();auto waiting=std::make_shared<QByteArray>();
  QObject::connect(down,&QTcpSocket::readyRead,&proxy,[down,up,waiting]{*waiting+=down->readAll();if(up->state()==QAbstractSocket::ConnectedState){up->write(*waiting);waiting->clear();}});
  QObject::connect(up,&QTcpSocket::connected,&proxy,[up,waiting]{up->write(*waiting);waiting->clear();});
  QObject::connect(down,&QTcpSocket::disconnected,up,&QTcpSocket::abort);
  QObject::connect(up,&QTcpSocket::readyRead,&proxy,[&,up,down,decoder]{auto bytes=up->readAll();for(auto response:decoder->feed(bytes.data(),bytes.size())){
   if(response["type"].text()=="chat.message.new"&&response["data"]["content"].text()=="response-loss-one")continue;
   if(dropResponse&&response["kind"].text()=="response"&&response["type"].text()=="chat.private.send"&&response["success"].boolean()){dropResponse=false;down->abort();continue;}
   auto frame=ProtocolDXX::frame(response);down->write(frame.data(),frame.size());
  }});up->connectToHost(QHostAddress::LocalHost,port);
 }});
 TcpClientDXX lossy(proxy.serverPort());lossy.connectServer();until([&]{return lossy.connected();},"fault proxy TCP");rpc(lossy,"auth.login",{{"login_name","M4UiAlice"},{"password","Mvp4Password!"}});activate(lossy,"QQ");
 ChatPageDXX lossPage(lossy);lossPage.setAccount(QString::number(alice["user_id"].integer()),"QQ",QString::fromStdString(display(alice,"QQ")),true);lossPage.show();lossPage.openPrivate(QString::fromStdString(display(bob,"QQ")));until([&]{return lossPage.currentModel()!=nullptr;},"fault conversation");bool lossRestored=false;QString originalUuid;QObject::connect(&lossy,&TcpClientDXX::sessionRestored,[&](const JsonDXX&account){lossRestored=true;lossPage.setAccount(QString::number(account["user_id"].integer()),"QQ",QString::fromStdString(display(alice,"QQ")),true);auto *retryView=lossPage.findChild<QListView*>("chatMessages");for(int row=0;row<lossPage.currentModel()->rowCount();++row)if(lossPage.currentModel()->index(row).data(Qt::UserRole).toString()==originalUuid)retryView->setCurrentIndex(lossPage.currentModel()->index(row));lossPage.findChild<QPushButton*>("chatRetry")->click();});
 lossPage.sendText("response-loss-one");for(auto m:lossPage.currentModel()->messages())if(m["content"].text()=="response-loss-one")originalUuid=QString::fromStdString(m["request_id"].text());check(!originalUuid.isEmpty(),"pending UUID retained");
 until([&]{for(auto m:lossPage.currentModel()->messages())if(m["content"].text()=="response-loss-one")return m["status"].text()=="unknown";return false;},"lost committed response yields unknown");until([&]{return lossRestored;},"fault proxy automatic authentication restore");
 auto *view=lossPage.findChild<QListView*>("chatMessages");for(int row=0;row<lossPage.currentModel()->rowCount();++row)if(lossPage.currentModel()->index(row).data(Qt::UserRole).toString()==originalUuid)view->setCurrentIndex(lossPage.currentModel()->index(row));lossPage.findChild<QPushButton*>("chatRetry")->click();
 until([&]{int found=0;bool stored=false;for(auto m:lossPage.currentModel()->messages())if(m["content"].text()=="response-loss-one"){++found;stored=m["status"].text()=="stored"&&QString::fromStdString(m["request_id"].text())==originalUuid;}return found==1&&stored;},"same UUID retry resolves exactly one stored row");
 std::cout<<"M4 native unknown/identical UUID retry PASS real TCP response-loss proxy after SQLite COMMIT\n";
 int ticks=0;QTimer heartbeat;heartbeat.setInterval(5);QObject::connect(&heartbeat,&QTimer::timeout,[&]{++ticks;});heartbeat.start();bool restored=false;QObject::connect(&a,&TcpClientDXX::sessionRestored,[&](const JsonDXX&account){restored=true;page.setAccount(QString::number(account["user_id"].integer()),"QQ",QString::fromStdString(display(alice,"QQ")),true);});server.terminate();check(server.waitForFinished(),"server stop");QTest::qWait(30);start();check(!a.connected(),"sender offline before recovery producer");
 QProcess producer;producer.start("/usr/bin/python3",{"-c",R"PY(import socket,struct,json,sys,uuid
s=socket.create_connection(('127.0.0.1',int(sys.argv[1])))
def rpc(t,d):
 rid=str(uuid.uuid4());raw=json.dumps(dict(kind='request',type=t,request_id=rid,data=d)).encode();s.sendall(struct.pack('!I',len(raw))+raw)
 while True:
  def read(n):
   data=b''
   while len(data)<n:data+=s.recv(n-len(data))
   return data
  r=json.loads(read(struct.unpack('!I',read(4))[0]))
  if r.get('request_id')==rid:
   assert r['success'],r
   return r['data']
rpc('auth.login',dict(login_name='M4UiBob',password='Mvp4Password!'))
rpc('account.service.activate',dict(service_type='QQ'))
rpc('chat.private.send',dict(service_type='QQ',target_display_id=sys.argv[2],content='offline-recovery-real-message'))
s.close()
)PY",QString::number(port),QString::fromStdString(display(alice,"QQ"))});check(producer.waitForFinished()&&producer.exitCode()==0,"real offline producer TCP");
 until([&]{return restored;},"bounded async reconnect reauthentication",20000);until([&]{int count=0;for(auto message:page.currentModel()->messages())if(message["content"].text()=="offline-recovery-real-message")++count;return count==1;},"offline message restored via sync exactly once");check(ticks>5,"GUI heartbeat during reconnect");
 std::cout<<"M4 native UI PASS draft/history dedup/SQL failure accurate state/async reconnect heartbeat\n";
 server.terminate();server.waitForFinished();return 0;
 }catch(const std::exception&e){for(auto &p:processes){p->kill();p->waitForFinished();std::cerr<<p->readAll().toStdString();}server.terminate();server.waitForFinished();std::cerr<<"M4 native FAIL: "<<e.what()<<"\n"<<server.readAllStandardError().toStdString();return 1;}}
