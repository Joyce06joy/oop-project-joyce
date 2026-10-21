#include "LoginWindowDXX.h"
#include "RegisterWindowDXX.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>

#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTest>
#include <QTimer>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void until(const std::function<bool()>& condition,const char* message,int ms=10000) { QElapsedTimer elapsed; elapsed.start(); bool ok=condition(); while(!ok&&elapsed.elapsed()<ms) { QTest::qWait(20); ok=condition(); } check(ok,message); }
template<class T> T* find(QWidget& w,const char* name) { auto* object=w.findChild<T*>(name); check(object!=nullptr,name); return object; }
void text(QWidget& w,const char* name,const QString& value) { find<QLineEdit>(w,name)->setText(value); }
void click(QWidget& w,const char* name) { auto* b=find<QPushButton>(w,name); check(b->isEnabled(),name); QTest::mouseClick(b,Qt::LeftButton); }
}
int main(int argc,char** argv) {
    QApplication app(argc,argv); QProcess server;
    try {
        check(argc==3,"requires server and client executables");
        QTemporaryDir temp; check(temp.isValid(),"temporary directory");
        QTcpServer probe; check(probe.listen(QHostAddress::LocalHost,0),"reserve test port"); const auto port=probe.serverPort(); probe.close();
        LoginWindowDXX offline(port); offline.show();
        until([&] { return find<QLabel>(offline,"connectionStatus")->text().contains(QStringLiteral("连接失败")); },"T10 offline status");
        std::cout<<"T10 PASS: offline Qt window shows error\n";
        server.start(QString::fromLocal8Bit(argv[1]),{"--db",temp.filePath("ui.sqlite3"),"--port",QString::number(port)});
        check(server.waitForStarted(),"start server");
        until([&] { return server.readAllStandardOutput().contains("listening"); },"server ready");
        click(offline,"reconnectButton"); until([&] {return find<QLabel>(offline,"connectionStatus")->text()==QStringLiteral("已连接");},"T10 retry connects");
        std::vector<std::unique_ptr<LoginWindowDXX>> clients;
        for(int i=0;i<3;++i) {
            auto w=std::make_unique<LoginWindowDXX>(port); w->show();
            until([&] {return find<QLabel>(*w,"connectionStatus")->text()==QStringLiteral("已连接");},"connect Qt client");
            click(*w,"openRegistration"); auto* r=w->findChild<RegisterWindowDXX*>(); check(r,"registration window");
            const auto name=QString("UiUser%1").arg(i); text(*r,"registerName",name); text(*r,"registerNickname",QStringLiteral("测试用户")+QString::number(i)); text(*r,"registerPassword","UiPassword2026!"); text(*r,"registerConfirm","mismatch");
            find<QCheckBox>(*r,"chooseQQ")->setChecked(true);
            if(i==1) {find<QCheckBox>(*r,"chooseWECHAT")->setChecked(true);find<QCheckBox>(*r,"chooseWEIBO")->setChecked(true);}
            if(i==2) {find<QCheckBox>(*r,"chooseQQ")->setChecked(false);find<QCheckBox>(*r,"chooseWECHAT")->setChecked(true);}
            click(*r,"registerSubmit"); check(find<QLabel>(*r,"registerMessage")->text().contains(QStringLiteral("不一致")),"confirm validation"); check(find<QLineEdit>(*r,"registerName")->text()==name,"registration preserves name");
            text(*r,"registerConfirm","UiPassword2026!"); click(*r,"registerSubmit"); check(!find<QPushButton>(*r,"registerSubmit")->isEnabled(),"disable duplicate submit");
            until([&] {return find<QLabel>(*r,"registerResult")->text().contains(QStringLiteral("用户 ID"));},"register real response");
            check(find<QLabel>(*r,"registerMessage")->text().contains(QStringLiteral("注册成功")),"registration success");
            r->hide(); text(*w,"loginName",name); text(*w,"loginPassword","wrong"); click(*w,"loginSubmit");
            until([&] {return find<QLabel>(*w,"loginMessage")->text().contains(QStringLiteral("登录名或密码错误"));},"T05 wrong password");
            check(find<QLineEdit>(*w,"loginName")->text()==name,"T05 preserve correct name");
            text(*w,"loginPassword","UiPassword2026!"); click(*w,"loginSubmit");
            until([&] {return w->findChild<QPushButton*>(i==2?"activateWECHAT":"activateQQ")!=nullptr;},"login services");
            click(*w,i==2?"activateWECHAT":"activateQQ"); until([&] {return find<QLabel>(*w,"serviceMessage")->text().contains(QStringLiteral("已激活"));},"activate actual service");
            clients.push_back(std::move(w));
        }
        int ticks=0; QTimer timer; timer.setInterval(10); QObject::connect(&timer,&QTimer::timeout,[&] {++ticks;}); timer.start(); QTest::qWait(500); check(ticks>20,"Qt GUI event loop responsive");
        for(auto& w:clients) check(find<QLabel>(*w,"serviceMessage")->text().contains(QStringLiteral("已激活")),"three logged in clients");
        std::cout<<"T01–T05 Qt form checks PASS; 3 simultaneous Qt windows PASS; event-loop heartbeat PASS\n";
        // Also start three independent, unmodified IMClient executables.
        std::vector<std::unique_ptr<QProcess>> standalone;
        const auto binary=QString::fromLocal8Bit(argv[2]);
        for(int i=0;i<3;++i) { auto p=std::make_unique<QProcess>(); p->start(binary,{"--port",QString::number(port)}); check(p->waitForStarted(),"independent IMClient started"); standalone.push_back(std::move(p)); }
        QTest::qWait(500); for(auto& p:standalone) check(p->state()==QProcess::Running,"IMClient remains running");
        std::cout<<"T08 process check PASS: three independent IMClient processes running (login exercised by Qt harness windows)\n";
        server.terminate(); check(server.waitForFinished(10000),"server graceful stop"); check(server.exitCode()==0,"server exit code");
        for(auto& w:clients) until([&] {return find<QLabel>(*w,"connectionStatus")->text().contains(QStringLiteral("丢失"));},"disconnect status");
        for(auto& p:standalone) { p->terminate(); check(p->waitForFinished(5000),"client exits"); }
        // A silent peer exercises real 15-second timeout without blocking Qt.
        QTcpServer silent; check(silent.listen(QHostAddress::LocalHost,0),"silent peer");
        LoginWindowDXX slow(silent.serverPort()); slow.show();
        until([&] {return find<QLabel>(slow,"connectionStatus")->text()==QStringLiteral("已连接");},"slow connect");
        click(slow,"openRegistration"); auto* r=slow.findChild<RegisterWindowDXX*>(); text(*r,"registerName","TimeoutUser"); text(*r,"registerNickname","Timeout"); text(*r,"registerPassword","TimeoutPass!"); text(*r,"registerConfirm","TimeoutPass!"); find<QCheckBox>(*r,"chooseQQ")->setChecked(true);
        click(*r,"registerSubmit"); until([&] { return silent.hasPendingConnections(); },"silent accepted");
        auto* peer=silent.nextPendingConnection(); until([&] {return peer->bytesAvailable()>0;},"first request sent"); const auto original=peer->readAll();
        ProtocolDXX fakeDecoder; const auto originalRequest=fakeDecoder.feed(original.data(),static_cast<std::size_t>(original.size())).at(0);
        const auto dbError=ProtocolDXX::frame(ProtocolDXX::error(originalRequest,"DATABASE_ERROR","temporary database read failure"));
        peer->write(dbError.data(),static_cast<qint64>(dbError.size()));
        until([&] {return find<QPushButton>(*r,"registerSubmit")->isEnabled();},"database error offers retry");
        check(!find<QLineEdit>(*r,"registerName")->isEnabled(),"database retry must preserve uncertain original payload");
        click(*r,"registerSubmit"); until([&] {return peer->bytesAvailable()>0;},"database error retry sent"); check(peer->readAll()==original,"database error retry keeps same ID");
        until([&] { return find<QPushButton>(*r,"registerSubmit")->isEnabled(); },"register timeout enables retry",18000);
        check(find<QLabel>(*r,"registerMessage")->text().contains(QStringLiteral("可能已保存")),"timeout is uncertain");
        click(*r,"registerSubmit"); until([&] {return peer->bytesAvailable()>0;},"retry sent"); check(peer->readAll()==original,"retry uses identical request id and payload"); check(ticks>1000,"GUI responsive during timeout");
        peer->close();
        LoginWindowDXX expired(silent.serverPort()); expired.show();
        until([&] {return find<QLabel>(expired,"connectionStatus")->text()==QStringLiteral("已连接");},"expired session connect");
        until([&] {return silent.hasPendingConnections();},"expired session accepted"); auto* expiryPeer=silent.nextPendingConnection();
        text(expired,"loginName","Expired"); text(expired,"loginPassword","ExpiredPass!"); click(expired,"loginSubmit");
        until([&] {return expiryPeer->bytesAvailable()>0;},"fake login received"); const auto loginBytes=expiryPeer->readAll(); ProtocolDXX expiryDecoder;
        const auto loginRequest=expiryDecoder.feed(loginBytes.data(),static_cast<std::size_t>(loginBytes.size())).at(0);
        const auto success=ProtocolDXX::frame(ProtocolDXX::response(loginRequest,JsonDXX{{"user_id",123},{"nickname","Expired"},{"session_id","temporary"},{"services",JsonDXX::array({JsonDXX{{"service_type","QQ"},{"display_id","10001"},{"active",false}}})}}));
        expiryPeer->write(success.data(),static_cast<qint64>(success.size()));
        until([&] {return expired.findChild<QPushButton*>("activateQQ")!=nullptr;},"fake login displayed"); click(expired,"logoutButton");
        until([&] {return expiryPeer->bytesAvailable()>0;},"logout received"); const auto logoutBytes=expiryPeer->readAll(); const auto logoutRequest=expiryDecoder.feed(logoutBytes.data(),static_cast<std::size_t>(logoutBytes.size())).at(0);
        const auto authError=ProtocolDXX::frame(ProtocolDXX::error(logoutRequest,"AUTH_REQUIRED","already logged out")); expiryPeer->write(authError.data(),static_cast<qint64>(authError.size()));
        until([&] {return find<QPushButton>(expired,"loginSubmit")->isVisible();},"AUTH_REQUIRED must restore login page",2000);
        std::cout<<"Disconnect/retry/uncertain registration timeout and identical replay PASS\nALL QT UI CHECKS PASSED\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; if(server.state()!=QProcess::NotRunning) {server.terminate();server.waitForFinished(10000);} return 1; }
}
