#include "LoginWindowDXX.h"
#include "ThemeDXX.h"
#include "ChatPageDXX.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QDebug>
int main(int argc,char** argv) {
 QApplication app(argc,argv);app.setApplicationName("IMClient");ThemeDXX::apply();
 QCommandLineParser parser;parser.addHelpOption();parser.addOption({"port","本机服务器端口","port","8888"});
 parser.addOption({"chat-smoke","Run asynchronous native chat smoke driver"});
 for(const auto *key:{"smoke-login","smoke-password","smoke-target","smoke-text","smoke-expect","smoke-group","smoke-subgroup","smoke-screenshot"})parser.addOption({key,key,"value"});
 parser.addOption({"smoke-service","Service","service","QQ"});parser.addOption({"smoke-duration","Smoke timeout ms","ms","12000"});parser.process(app);
 bool ok=false;auto port=parser.value("port").toInt(&ok);if(!ok||port<1||port>65535)return 2;
 LoginWindowDXX window(static_cast<quint16>(port));window.show();
 if(!parser.isSet("chat-smoke"))return app.exec();
 int stage=0;bool received=false,sent=false;auto *poll=new QTimer(&window);poll->setInterval(40);auto *chat=window.findChild<ChatPageDXX*>();
 QObject::connect(chat,&ChatPageDXX::messageMerged,&window,[&](qint64 id,const QString &text){qInfo().noquote()<<"SMOKE RECEIVE pid="<<QCoreApplication::applicationPid()<<"message_id="<<id<<"content="<<text;if(text==parser.value("smoke-expect"))received=true;});
 QObject::connect(chat,&ChatPageDXX::conversationOpened,&window,[&](qint64 id){qInfo()<<"SMOKE OPEN pid="<<QCoreApplication::applicationPid()<<"conversation="<<id;if(!sent&&parser.isSet("smoke-text")){sent=true;QTimer::singleShot(1800,&window,[&]{chat->sendText(parser.value("smoke-text"));qInfo()<<"SMOKE SEND pid="<<QCoreApplication::applicationPid();});}});
 QObject::connect(poll,&QTimer::timeout,&window,[&]{
  if(stage==0&&window.findChild<QLabel*>("connectionStatus")->text()==QStringLiteral("已连接")){window.findChild<QLineEdit*>("loginName")->setText(parser.value("smoke-login"));window.findChild<QLineEdit*>("loginPassword")->setText(parser.value("smoke-password"));window.findChild<QPushButton*>("loginSubmit")->click();stage=1;}
  if(stage==1){auto *activate=window.findChild<QPushButton*>("activate"+parser.value("smoke-service"));if(activate&&activate->isEnabled()){activate->click();stage=2;}}
  if(stage==2){auto *activate=window.findChild<QPushButton*>("activate"+parser.value("smoke-service"));if(activate&&activate->text().contains(QStringLiteral("已激活"))&&activate->isEnabled()){auto *space=window.findChild<QPushButton*>("space"+parser.value("smoke-service"));space->click();window.findChild<QTabWidget*>("workspaceTabs")->setCurrentIndex(5);qInfo()<<"SMOKE READY pid="<<QCoreApplication::applicationPid()<<"login="<<parser.value("smoke-login")<<"service="<<parser.value("smoke-service");stage=3;if(parser.isSet("smoke-target"))chat->openPrivate(parser.value("smoke-target"));else if(parser.isSet("smoke-group"))chat->openGroup(parser.value("smoke-group").toLongLong(),parser.value("smoke-subgroup").toLongLong());}}
 });poll->start();
 QTimer::singleShot(parser.value("smoke-duration").toInt(),&window,[&]{if(parser.isSet("smoke-screenshot"))window.grab().save(parser.value("smoke-screenshot"));bool passed=stage==3&&(!parser.isSet("smoke-expect")||received);qInfo()<<"SMOKE RESULT pid="<<QCoreApplication::applicationPid()<<"pass="<<passed;app.exit(passed?0:3);});
 return app.exec();
}
