#include "RequestPanelDXX.h"
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
  check(p->isEnabled(), n);
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
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QProcess server;
  try {
    check(argc >= 2, "requires IMServer executable");
    QTemporaryDir tmp;
    check(tmp.isValid(), "temporary database");
    QTcpServer portProbe;
    check(portProbe.listen(QHostAddress::LocalHost, 0), "reserve port");
    auto port = portProbe.serverPort();
    portProbe.close();
    server.start(QString::fromLocal8Bit(argv[1]),
                 {"--db", tmp.filePath("mvp2-ui.sqlite3"), "--port",
                  QString::number(port)});
    check(server.waitForStarted(), "server start");
    QByteArray output;
    until(
        [&] {
          output += server.readAllStandardOutput();
          return output.contains("listening");
        },
        "server ready");
    TcpClientDXX a(port), b(port), c(port);
    for (auto *client : {&a, &b, &c})
      client->connectServer();
    until([&] { return a.connected() && b.connected() && c.connected(); },
          "clients connected");
    const auto all = JsonDXX::array({"QQ", "WECHAT", "WEIBO"});
    const auto alice = registerLogin(a, "UiAliceM2", all),
               bob = registerLogin(b, "UiBobM2", all),
               carol =
                   registerLogin(c, "UiCarolM2", JsonDXX::array({"WECHAT"}));
    ServiceSelectWindowDXX aw(a), bw(b), cw(c);
    aw.showAccount(alice);
    bw.showAccount(bob);
    cw.showAccount(carol);
    aw.show();
    bw.show();
    cw.show();
    for (const auto &s :
         rpc(a, "account.services.list", JsonDXX({}))["services"].items())
      check(!s["active"].boolean(), "workspace never silently activates");
    int pushes = 0;
    QObject::connect(&b, &TcpClientDXX::eventReceived, &b,
                     [&](const JsonDXX &) { ++pushes; });
    activate(aw, "QQ");
    activate(bw, "QQ");
    widget<QLineEdit>(aw, "friendQuery")
        ->setText(QString::fromStdString(display(bob, "QQ")));
    click(aw, "friendSearch");
    auto *ar = widget<QListWidget>(aw, "friendResults");
    until(
        [&] {
          return ar->count() == 1 && ar->item(0)->text().contains("UiBobM2");
        },
        "friend search real socket");
    ar->setCurrentRow(0);
    widget<QLineEdit>(aw, "verificationMessage")
        ->setText("UI request preserved");
    click(aw, "sendFriendRequest");
    auto *br = widget<QListWidget>(bw, "requestResults");
    until([&] { return pushes > 0 && br->count() == 1; },
          "new application push refresh");
    tab(bw, 1);
    until([&] { return br->count() == 1; }, "pending inbox");
    br->setCurrentRow(0);
    click(bw, "acceptRequest");
    until(
        [&] { return widget<QListWidget>(bw, "friendResults")->count() == 1; },
        "accept refreshes friends");
    click(aw, "friendRefresh");
    until(
        [&] {
          return ar->count() == 1 && ar->item(0)->text().contains("FRIEND");
        },
        "sender relationship pushed");
    ar->setCurrentRow(0);
    widget<QLineEdit>(aw, "friendRemark")->setText("Bob private remark");
    click(aw, "saveFriendRemark");
    until(
        [&] {
          return ar->count() == 1 &&
                 ar->item(0)->text().contains("Bob private remark");
        },
        "save remark confirmed");
    check(rpc(b, "friend.list", {{"service_type", "QQ"}})["items"]
              .items()
              .at(0)["remark"]
              .text()
              .empty(),
          "remark one direction");
    for (auto *w : {&aw, &bw}) {
      activate(*w, "WECHAT");
      tab(*w, 3);
      confirm(*w, "bindAccounts");
      until(
          [&] {
            return widget<QLabel>(*w, "bindingSummary")
                ->text()
                .startsWith(QStringLiteral("已绑定"));
          },
          "bind accounts UI");
    }
    tab(aw, 2);
    widget<QComboBox>(aw, "recommendMode")->setCurrentIndex(2);
    widget<QComboBox>(aw, "recommendSource")->setCurrentText("QQ");
    widget<QComboBox>(aw, "recommendTarget")->setCurrentText("WECHAT");
    click(aw, "recommendRefresh");
    auto *rec = widget<QListWidget>(aw, "recommendResults");
    until([&] { return rec->count() == 1; }, "cross recommendation visible");
    check(rec->item(0)->text().contains(
              QString::fromStdString(display(bob, "WECHAT"))),
          "recommend target identity");
    rec->setCurrentRow(0);
    click(aw, "recommendSend");
    click(bw, "spaceWECHAT");
    tab(bw, 1);
    until([&] { return br->count() == 1; }, "target-platform pending");
    br->setCurrentRow(0);
    click(bw, "rejectRequest");
    until([&] { return br->count() == 0; }, "reject confirmed");
    // Same-platform mutual query and cross-service intersection remain distinct
    // modes.
    widget<QComboBox>(aw, "recommendMode")->setCurrentIndex(1);
    widget<QLineEdit>(aw, "mutualTarget")
        ->setText(QString::fromStdString(display(bob, "QQ")));
    click(aw, "recommendRefresh");
    until(
        [&] {
          return widget<QLabel>(*widget<QWidget>(aw, "recommendationsPage"),
                                "pageStatus")
              ->text()
              .contains(QStringLiteral("共 0"));
        },
        "mutual empty intersection");
    widget<QComboBox>(aw, "recommendMode")->setCurrentIndex(3);
    click(aw, "recommendRefresh");
    until(
        [&] {
          return rec->count() == 0 &&
                 widget<QLabel>(*widget<QWidget>(aw, "recommendationsPage"),
                                "pageStatus")
                     ->text()
                     .contains(QStringLiteral("共 0"));
        },
        "cross common excludes unaccepted");
    tab(aw, 0);
    click(aw, "friendRefresh");
    until([&] { return ar->count() == 1; }, "QQ friend before deletion");
    ar->setCurrentRow(0);
    confirm(aw, "removeFriend");
    until([&] { return ar->count() == 0; }, "confirmed delete removes friend");
    click(bw, "spaceQQ");
    tab(bw, 0);
    until(
        [&] { return widget<QListWidget>(bw, "friendResults")->count() == 0; },
        "relationship delete push");
    tab(aw, 3);
    confirm(aw, "unbindAccounts");
    until(
        [&] {
          return widget<QLabel>(aw, "bindingSummary")
              ->text()
              .startsWith(QStringLiteral("尚未绑定"));
        },
        "unbind UI");
    tab(cw, 3);
    widget<QComboBox>(cw, "openServiceChoice")->setCurrentText("QQ");
    confirm(cw, "openService");
    until([&] { return cw.findChild<QPushButton *>("activateQQ") != nullptr; },
          "open QQ exposes activation control");
    for (const auto &s :
         rpc(c, "account.services.list", JsonDXX({}))["services"].items())
      if (s["service_type"].text() == "QQ")
        check(!s["active"].boolean(), "new service remains inactive");
    // Search pagination uses stable server cursors and keeps the query text.
    for (int i = 0; i < 21; ++i) {
      std::string n = "UiSeedM2_" + std::to_string(i);
      rpc(c, "auth.register",
          {{"login_name", n},
           {"nickname", "PageM2"},
           {"password", "TestMvp2Password!"},
           {"birthday", JsonDXX()},
           {"location", JsonDXX()},
           {"services", JsonDXX::array({"QQ"})}});
    }
    tab(aw, 0);
    widget<QLineEdit>(aw, "friendQuery")->setText("PageM2");
    click(aw, "friendSearch");
    until([&] { return ar->count() == 20; }, "20 search results page one");
    click(aw, "friendNext");
    until([&] { return ar->count() == 1; }, "search page two");
    check(widget<QLineEdit>(aw, "friendQuery")->text() == "PageM2",
          "query preserved during paging");
    std::cout << "PASS real-socket Qt: explicit activation, search/pagination, "
                 "send/accept/reject, pushes, remark, delete confirmation, "
                 "binding/unbinding/open, recommendations/mutual/common\n";
    // A controlled silent TCP peer verifies retry identity after a genuine
    // timeout.
    QTcpServer silent;
    check(silent.listen(QHostAddress::LocalHost, 0), "silent listen");
    TcpClientDXX retryClient(silent.serverPort());
    retryClient.connectServer();
    until(
        [&] {
          return retryClient.connected() && silent.hasPendingConnections();
        },
        "silent connected");
    auto *peer = silent.nextPendingConnection();
    RequestPanelDXX panel(retryClient);
    panel.show();
    int ticks = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&] { ++ticks; });
    timer.start();
    panel.mutate("friend.remark.set",
                 {{"service_type", "QQ"},
                  {"target_display_id", "123"},
                  {"remark", "preserve"}},
                 [](const JsonDXX &) {});
    until([&] { return peer->bytesAvailable() > 0; },
          "original mutation bytes");
    const auto original = peer->readAll();
    until(
        [&] {
          return widget<QPushButton>(panel, "retryMutation")->isVisible();
        },
        "timeout retry visible", 18000);
    click(panel, "retryMutation");
    until([&] { return peer->bytesAvailable() > 0; }, "timeout replay bytes");
    check(peer->readAll() == original,
          "identical request ID and payload on timeout");
    check(ticks > 1000, "event loop remains responsive");
    int events = 0;
    QObject::connect(&retryClient, &TcpClientDXX::eventReceived,
                     [&](const JsonDXX &) { ++events; });
    const auto event =
        ProtocolDXX::frame({{"kind", "event"},
                            {"type", "friend.relationship.changed"},
                            {"event_id", "duplicate-event"},
                            {"data", JsonDXX({})}});
    peer->write(event.data(), event.size());
    peer->write(event.data(), event.size());
    until([&] { return events == 1; }, "deduplicated event");
    QTest::qWait(80);
    check(events == 1, "duplicate suppressed");
    peer->close();
    until([&] { return !retryClient.connected(); }, "disconnected");
    retryClient.connectServer();
    until(
        [&] {
          return retryClient.connected() && silent.hasPendingConnections();
        },
        "reconnected");
    auto *peer2 = silent.nextPendingConnection();
    click(panel, "retryMutation");
    until([&] { return peer2->bytesAvailable() > 0; },
          "disconnect replay bytes");
    check(peer2->readAll() == original, "identical mutation after disconnect");
    std::cout << "PASS genuine 15-second timeout and disconnect identical "
                 "replay; push dedup; UI heartbeat\n";
    server.terminate();
    check(server.waitForFinished(10000), "server exits");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    if (server.state() != QProcess::NotRunning) {
      server.terminate();
      server.waitForFinished(10000);
    }
    return 1;
  }
}
