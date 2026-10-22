#include "RequestPanelDXX.h"
#include "GroupPolicyDialogDXX.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QTextEdit>
#include "ServiceSelectWindowDXX.h"
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <functional>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok, const char *m) {
  if (!ok)
    throw std::runtime_error(m);
}
void until(const std::function<bool()> &f, const char *m, int ms = 10000) {
  QElapsedTimer t;
  t.start();
  while (!f() && t.elapsed() < ms)
    QTest::qWait(20);
  check(f(), m);
}
template <class T> T *widget(QWidget &w, const char *n) {
  auto *p = w.findChild<T *>(n);
  check(p, n);
  return p;
}
void click(QWidget &w, const char *n) {
  auto *p = widget<QPushButton>(w, n);
  until([&] { return p->isEnabled(); }, n);
  QTest::mouseClick(p, Qt::LeftButton);
}
void confirm(QWidget &w, const char *n) {
  QTimer::singleShot(50, [] {
    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
    if (box)
      QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton);
  });
  click(w, n);
}
JsonDXX rpc(TcpClientDXX &c, const QString &type, const JsonDXX &data) {
  QString id;
  JsonDXX result;
  bool done = false;
  auto connection = QObject::connect(
      &c, &TcpClientDXX::responseReceived, &c, [&](const JsonDXX &r) {
        if (QString::fromStdString(r["request_id"].text()) == id) {
          result = r;
          done = true;
        }
      });
  id = c.request(type, data);
  try {
    check(!id.isEmpty(), "send rpc");
    until([&] { return done; }, "rpc response");
  } catch (...) {
    QObject::disconnect(connection);
    throw;
  }
  QObject::disconnect(connection);
  if (!result["success"].boolean())
    throw std::runtime_error(type.toStdString() + ": " +
                             result["error"].dump());
  return result["data"];
}
JsonDXX registerLogin(TcpClientDXX &c, const char *name,
                      const JsonDXX &services) {
  rpc(c, "auth.register",
      {{"login_name", name},
       {"nickname", name},
       {"password", "TestMvp2Password!"},
       {"birthday", JsonDXX()},
       {"location", JsonDXX()},
       {"services", services}});
  return rpc(c, "auth.login",
             {{"login_name", name}, {"password", "TestMvp2Password!"}});
}
std::string display(const JsonDXX &a, const char *service) {
  for (const auto &s : a["services"].items())
    if (s["service_type"].text() == service)
      return s["display_id"].text();
  throw std::runtime_error("display id missing");
}
void tab(QWidget &w, int index) {
  widget<QTabWidget>(w, "workspaceTabs")->setCurrentIndex(index);
}
void activate(QWidget &w, const char *service) {
  const auto n = std::string("activate") + service;
  click(w, n.c_str());
  until(
      [&] {
        return widget<QPushButton>(w, n.c_str())
                   ->text()
                   .contains(QStringLiteral("已激活")) &&
               widget<QPushButton>(w, n.c_str())->isEnabled();
      },
      "service activation");
}
} // namespace
namespace {
void groupTab(QWidget &w, int page) {
  tab(w,4);
  widget<QTabWidget>(w,"groupTabs")->setCurrentIndex(page);
}
void selectGroup(QWidget &w,const std::string &name) {
  groupTab(w,0);
  widget<QLineEdit>(w,"groupQuery")->setText(QString::fromStdString(name));
  click(w,"groupSearch");
  auto *list=widget<QListWidget>(w,"groupResults");
  until([&]{return list->count()==1 && list->item(0)->text().contains(QString::fromStdString(name));},"group search result");
  list->setCurrentRow(0);
  groupTab(w,1);
  until([&]{return widget<QLabel>(w,"groupSummary")->text().contains(QString::fromStdString(name));},"group detail");
}
void chooseMember(QWidget &w,const std::string &id) {
  auto *list=widget<QListWidget>(w,"groupMembers");
  until([&]{for(int i=0;i<list->count();++i)if(list->item(i)->text().startsWith(QString::fromStdString(id)+" ·")){list->setCurrentRow(i);return true;}return false;},"select member");
}
void switchPolicy(QWidget &w) {
  QTimer timer; timer.setInterval(20);
  QObject::connect(&timer,&QTimer::timeout,[&]{
    auto *modal=QApplication::activeModalWidget();
    if(auto *box=qobject_cast<QMessageBox*>(modal)) QTest::mouseClick(box->button(QMessageBox::Yes),Qt::LeftButton);
    else if(auto *dialog=qobject_cast<GroupPolicyDialogDXX*>(modal)) {
      auto *buttons=dialog->findChild<QDialogButtonBox*>("groupPolicyButtons");
      QTimer::singleShot(30, [] {
        if (auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
          QTest::mouseClick(box->button(QMessageBox::Yes),Qt::LeftButton);
      });
      QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);
    }
  });
  timer.start();click(w,"groupPolicySwitch");
}
}
int main(int argc,char **argv) {
 QApplication app(argc,argv);QProcess server;
 try {
  check(argc>=2,"requires IMServer");QTemporaryDir tmp;check(tmp.isValid(),"temp database");
  QTcpServer probe;check(probe.listen(QHostAddress::LocalHost,0),"reserve port");auto port=probe.serverPort();probe.close();
  server.start(QString::fromLocal8Bit(argv[1]),{"--db",tmp.filePath("mvp3-ui.sqlite3"),"--port",QString::number(port)});
  check(server.waitForStarted(),"server started");QByteArray output;
  until([&]{output+=server.readAllStandardOutput();return output.contains("listening");},"server ready");
  TcpClientDXX a(port),b(port),c(port);for(auto *client:{&a,&b,&c})client->connectServer();
  until([&]{return a.connected()&&b.connected()&&c.connected();},"clients connected");
  auto all=JsonDXX::array({"QQ","WECHAT","WEIBO"});
  auto alice=registerLogin(a,"UiAliceM3",all),bob=registerLogin(b,"UiBobM3",all),carol=registerLogin(c,"UiCarolM3",all);
  ServiceSelectWindowDXX aw(a),bw(b),cw(c);aw.showAccount(alice);bw.showAccount(bob);cw.showAccount(carol);
  for(auto *w:{&aw,&bw,&cw}){w->resize(1000,950);w->show();}
  for(const auto &s:rpc(a,"account.services.list",JsonDXX({}))["services"].items())check(!s["active"].boolean(),"no implicit activation");
  int pushes=0;QObject::connect(&a,&TcpClientDXX::eventReceived,[&](const JsonDXX &e){if(e["type"].text().find("group.")==0)++pushes;});
  for(auto *w:{&aw,&bw,&cw})activate(*w,"QQ");
  groupTab(aw,0);widget<QLineEdit>(aw,"groupName")->setText("UiM3QQ");click(aw,"groupCreate");
  auto groups=rpc(a,"group.list",{{"service_type","QQ"}});
  until([&]{groups=rpc(a,"group.list",{{"service_type","QQ"}});return groups["total"].integer()==1;},"created persisted group");
  auto gid=groups["items"].items().at(0)["group_pk"];
  selectGroup(aw,"UiM3QQ");selectGroup(bw,"UiM3QQ");
  click(bw,"groupApply");
  groupTab(aw,2);auto *requests=widget<QListWidget>(aw,"groupRequests");
  until([&]{return requests->count()==1;},"application push");requests->setCurrentRow(0);click(aw,"groupRequestAccept");
  until([&]{return rpc(a,"group.detail",{{"group_pk",gid}})["group"]["member_count"].integer()==2;},"approved membership");
  groupTab(aw,1);chooseMember(aw,display(bob,"QQ"));click(aw,"groupAdminSet");
  until([&]{auto d=rpc(b,"group.detail",{{"group_pk",gid}});return d["permissions"]["can_review"].boolean();},"admin permissions");
  selectGroup(cw,"UiM3QQ");click(cw,"groupApply");
  groupTab(bw,2);auto *br=widget<QListWidget>(bw,"groupRequests");
  until([&]{return br->count()==1;},"admin application push");br->setCurrentRow(0);click(bw,"groupRequestAccept");
  until([&]{return rpc(a,"group.detail",{{"group_pk",gid}})["group"]["member_count"].integer()==3;},"admin approves");
  groupTab(bw,1);until([&]{return widget<QPushButton>(bw,"groupAnnouncementSave")->isEnabled();},"admin announcement allowed");
  widget<QTextEdit>(bw,"groupAnnouncement")->setPlainText("UiM3 announcement");click(bw,"groupAnnouncementSave");
  until([&]{return widget<QLabel>(aw,"groupSummary")->text().contains("UiM3 announcement");},"announcement pushed");
  groupTab(aw,3);widget<QLineEdit>(aw,"subgroupName")->setText("UiM3Sub");widget<QLineEdit>(aw,"subgroupTargets")->setText(QString::fromStdString(display(bob,"QQ")));
  until([&]{return widget<QPushButton>(aw,"subgroupCreate")->isEnabled();},"subgroup permission");click(aw,"subgroupCreate");
  auto *subs=widget<QListWidget>(aw,"subgroupResults");until([&]{return subs->count()==1;},"subgroup created");subs->setCurrentRow(0);
  until([&]{return widget<QListWidget>(aw,"subgroupMembers")->count()==2;},"independent subgroup membership");
  if(argc>=3) {
    QDir().mkpath(QString::fromLocal8Bit(argv[2]));
    for(const auto &entry:std::vector<std::pair<QWidget*,QString>>{{&aw,"alice-qq"},{&bw,"bob-qq"},{&cw,"carol-qq"}})
      check(entry.first->grab().save(QString::fromLocal8Bit(argv[2])+"/"+entry.second+".png"),"save native Qt evidence");
  }
  groupTab(aw,1);switchPolicy(aw);
  until([&]{return widget<QLabel>(aw,"groupSummary")->text().contains("WECHAT 规则");},"policy switch response");
  until([&]{return !widget<QPushButton>(bw,"groupAnnouncementSave")->isEnabled();},"admin dynamically disabled by push");
  check(widget<QLabel>(aw,"groupSummary")->text().contains("3 人"),"member count preserved");
  if(argc>=3)check(aw.grab().save(QString::fromLocal8Bit(argv[2])+"/alice-wechat-policy.png"),"save switched policy evidence");
  groupTab(aw,3);check(subs->count()==0,"subgroup hidden under WeChat policy");
  groupTab(aw,1);switchPolicy(aw);
  until([&]{return widget<QLabel>(aw,"groupSummary")->text().contains("QQ 规则");},"QQ restored");
  until([&]{return widget<QPushButton>(bw,"groupAnnouncementSave")->isEnabled();},"admin restored by push");
  groupTab(aw,3);until([&]{return subs->count()==1;},"subgroup restored");subs->setCurrentRow(0);
  until([&]{return widget<QListWidget>(aw,"subgroupMembers")->count()==2;},"subgroup members restored");
  widget<QLineEdit>(aw,"subgroupTargets")->setText(QString::fromStdString(display(carol,"QQ")));click(aw,"subgroupAdd");
  until([&]{return widget<QListWidget>(aw,"subgroupMembers")->count()==3;},"subgroup add");click(aw,"subgroupRemove");
  until([&]{return widget<QListWidget>(aw,"subgroupMembers")->count()==2;},"subgroup remove");
  groupTab(aw,1);chooseMember(aw,display(bob,"QQ"));click(aw,"groupAdminRevoke");
  until([&]{return !widget<QPushButton>(bw,"groupAnnouncementSave")->isEnabled();},"revoke admin");
  chooseMember(aw,display(carol,"QQ"));confirm(aw,"groupKick");
  until([&]{return rpc(a,"group.detail",{{"group_pk",gid}})["group"]["member_count"].integer()==2;},"kick persisted");
  widget<QLineEdit>(aw,"groupRename")->setText("UiM3Renamed");click(aw,"groupRenameSave");
  until([&]{return widget<QLabel>(aw,"groupSummary")->text().contains("UiM3Renamed");},"rename persisted");
  groupTab(bw,1);until([&]{return widget<QPushButton>(bw,"groupLeave")->isEnabled();},"leave permission");confirm(bw,"groupLeave");
  until([&]{return rpc(a,"group.detail",{{"group_pk",gid}})["group"]["member_count"].integer()==1;},"leave persisted");
  confirm(aw,"groupDisband");until([&]{return widget<QListWidget>(aw,"groupResults")->count()==0;},"disband list refreshed");
  for(auto *w:{&aw,&bw,&cw}){activate(*w,"WECHAT");click(*w,"spaceWECHAT");}
  groupTab(aw,0);widget<QLineEdit>(aw,"groupQuery")->clear();widget<QLineEdit>(aw,"groupName")->setText("UiM3WeChat");click(aw,"groupCreate");
  selectGroup(aw,"UiM3WeChat");widget<QLineEdit>(aw,"groupTarget")->setText(QString::fromStdString(display(bob,"WECHAT")));click(aw,"groupInvite");
  groupTab(bw,2);until([&]{return br->count()==1;},"invitation push to target");br->setCurrentRow(0);click(bw,"groupRequestAccept");
  selectGroup(bw,"UiM3WeChat");until([&]{return widget<QPushButton>(bw,"groupInvite")->isEnabled();},"ordinary WeChat member invite enabled");
  check(!widget<QPushButton>(bw,"groupAnnouncementSave")->isEnabled(),"ordinary member no management");
  widget<QLineEdit>(bw,"groupTarget")->setText(QString::fromStdString(display(carol,"WECHAT")));click(bw,"groupInvite");
  groupTab(cw,2);auto *cr=widget<QListWidget>(cw,"groupRequests");until([&]{return cr->count()==1;},"ordinary member invitation");cr->setCurrentRow(0);click(cw,"groupRequestReject");
  until([&]{return cr->count()==0;},"invite target rejects");
  groupTab(aw,0);widget<QLineEdit>(aw,"groupName")->setText(QString(120,'x'));click(aw,"groupCreate");
  until([&]{return widget<QWidget>(aw,"groupsPage")->findChild<QWidget*>("groupTabs")->isEnabled();},"failure unlocks form");
  check(widget<QLineEdit>(aw,"groupName")->text()==QString(120,'x'),"failed input preserved");
  // Inject a real SQL failure, then prove Qt preserves the original operation/UUID.
  auto ddl=[&](const QString &statement){
    QProcess process;
    process.start("python3",{"-c","import sqlite3,sys; c=sqlite3.connect(sys.argv[1]); c.execute(sys.argv[2]); c.commit()",tmp.filePath("mvp3-ui.sqlite3"),statement});
    check(process.waitForFinished(10000)&&process.exitCode()==0,"inject SQLite failure");
  };
  ddl("CREATE TRIGGER ui_failure BEFORE INSERT ON groups BEGIN SELECT RAISE(ABORT,'test failure'); END");
  QString failedId,retriedId;
  auto responseWatch=QObject::connect(&a,&TcpClientDXX::responseReceived,[&](const JsonDXX &r){
    if(r["type"].text()=="group.create") {
      if(!r["success"].boolean()&&r["error"]["code"].text()=="DATABASE_ERROR")failedId=QString::fromStdString(r["request_id"].text());
      if(r["success"].boolean()&&!failedId.isEmpty())retriedId=QString::fromStdString(r["request_id"].text());
    }
  });
  widget<QLineEdit>(aw,"groupName")->setText("UiM3Retry");click(aw,"groupCreate");
  auto *groupPanel=widget<QWidget>(aw,"groupsPage");
  auto *retry=groupPanel->findChild<QPushButton*>("retryMutation",Qt::FindDirectChildrenOnly);
  check(retry,"group retry button");
  until([&]{return !failedId.isEmpty()&&retry->isVisible();},"database error enables original retry");
  check(widget<QLineEdit>(aw,"groupName")->text()=="UiM3Retry","DB failure retains input");
  check(rpc(a,"group.list",{{"service_type","WECHAT"},{"query","UiM3Retry"}})["total"].integer()==0,"DB failure creates no false group");
  ddl("DROP TRIGGER ui_failure");QTest::mouseClick(retry,Qt::LeftButton);
  until([&]{return !retriedId.isEmpty();},"original operation retried");
  check(failedId==retriedId,"same UUID retry after rollback");
  check(rpc(a,"group.list",{{"service_type","WECHAT"},{"query","UiM3Retry"}})["total"].integer()==1,"retry creates exactly one group");
  QObject::disconnect(responseWatch);
  click(aw,"spaceWEIBO");check(!widget<QTabWidget>(aw,"workspaceTabs")->isTabVisible(4),"Weibo group entry hidden");
  check(widget<QTabWidget>(aw,"workspaceTabs")->tabText(3)==QStringLiteral("账号"),"MVP2 tab index unchanged");
  check(pushes>5,"real group socket pushes observed");
  std::cout<<"PASS MVP3 real Qt TCP: explicit activation, QQ create/search/apply/approve/admin/announcement/rename/kick/leave/disband, subgroup members, policy confirmation and restored roles/data, WeChat invitation self-decision, failure input preservation, SQL rollback with same UUID retry, pushes, legacy tabs\n";
  server.terminate();check(server.waitForFinished(10000),"server exit");return 0;
 } catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';if(server.state()!=QProcess::NotRunning){server.terminate();server.waitForFinished(10000);}return 1;}
}
