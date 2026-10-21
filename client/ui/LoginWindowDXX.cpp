#include "LoginWindowDXX.h"
#include "RegisterWindowDXX.h"
#include "ServiceSelectWindowDXX.h"
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
LoginWindowDXX::LoginWindowDXX(quint16 port,QWidget* parent):QMainWindow(parent),client_(port,this) {
    setObjectName("loginWindow"); setWindowTitle(QStringLiteral("IMPlatform · MVP 2")); resize(900,860);
    auto* center=new QWidget; setCentralWidget(center); auto* root=new QVBoxLayout(center); root->setContentsMargins(28,24,28,24); root->setSpacing(14);
    auto* title=new QLabel(QStringLiteral("IMPlatform")); title->setStyleSheet("font-size: 28px; font-weight: 600;"); root->addWidget(title);
    status_=new QLabel(QStringLiteral("连接中…")); status_->setObjectName("connectionStatus"); status_->setWordWrap(true); root->addWidget(status_);
    auto* reconnect=new QPushButton(QStringLiteral("重试连接")); reconnect->setObjectName("reconnectButton"); root->addWidget(reconnect); connect(reconnect,&QPushButton::clicked,&client_,&TcpClientDXX::connectServer);
    pages_=new QStackedWidget; root->addWidget(pages_); auto* formPage=new QWidget; auto* form=new QFormLayout(formPage);
    name_=new QLineEdit; name_->setObjectName("loginName"); form->addRow(QStringLiteral("登录名"),name_);
    password_=new QLineEdit; password_->setObjectName("loginPassword"); password_->setEchoMode(QLineEdit::Password); form->addRow(QStringLiteral("密码"),password_);
    message_=new QLabel; message_->setObjectName("loginMessage"); message_->setWordWrap(true); form->addRow(message_);
    login_=new QPushButton(QStringLiteral("登录")); login_->setObjectName("loginSubmit"); form->addRow(login_);
    auto* registration=new QPushButton(QStringLiteral("创建统一账号")); registration->setObjectName("openRegistration"); form->addRow(registration); pages_->addWidget(formPage);
    registration_=new RegisterWindowDXX(client_,this); services_=new ServiceSelectWindowDXX(client_); pages_->addWidget(services_);
    connect(registration,&QPushButton::clicked,this,[this] { registration_->show(); registration_->raise(); registration_->activateWindow(); });
    connect(login_,&QPushButton::clicked,this,[this] {
        if(!pendingId_.isEmpty()) return;
        if(name_->text().isEmpty()||password_->text().isEmpty()) { message_->setText(QStringLiteral("请填写登录名与密码")); return; }
        pendingId_=client_.request("auth.login",JsonDXX{{"login_name",name_->text().toStdString()},{"password",password_->text().toStdString()}});
        if(!pendingId_.isEmpty()) { login_->setEnabled(false); message_->setText(QStringLiteral("登录中…")); }
    });
    connect(password_,&QLineEdit::returnPressed,login_,&QPushButton::click);
    connect(&client_,&TcpClientDXX::statusChanged,this,[this](const QString& status,bool online) { status_->setText(status); if(!online) { pendingId_.clear(); login_->setEnabled(true); pages_->setCurrentIndex(0); } });
    connect(&client_,&TcpClientDXX::problem,message_,&QLabel::setText);
    connect(&client_,&TcpClientDXX::requestTimedOut,this,[this](const QString& id,const QString&) { if(id==pendingId_) { pendingId_.clear(); login_->setEnabled(true); message_->setText(QStringLiteral("登录超时，请重试。")); } });
    connect(&client_,&TcpClientDXX::responseReceived,this,[this](const JsonDXX& r) {
        if(QString::fromStdString(r["request_id"].text())!=pendingId_) return;
        pendingId_.clear(); login_->setEnabled(true);
        if(!r["success"].boolean()) { message_->setText(QString::fromStdString(r["error"]["message"].text())); return; }
        services_->showAccount(r["data"]); pages_->setCurrentIndex(1); message_->clear(); password_->clear();
    });
    connect(services_,&ServiceSelectWindowDXX::loggedOut,this,[this] { pages_->setCurrentIndex(0); password_->clear(); });
    client_.connectServer();
}
