#include "ServiceSelectWindowDXX.h"
#include "WorkspaceDXX.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
ServiceSelectWindowDXX::ServiceSelectWindowDXX(TcpClientDXX& client,QWidget* parent):QWidget(parent),client_(client) {
    setObjectName("servicePage"); auto* root=new QVBoxLayout(this);
    profile_=new QLabel; profile_->setObjectName("accountProfile"); profile_->setTextFormat(Qt::PlainText); profile_->setWordWrap(true); root->addWidget(profile_);
    choices_=new QWidget; list_=new QVBoxLayout(choices_); root->addWidget(choices_);
    message_=new QLabel(QStringLiteral("请选择已开通的服务")); message_->setObjectName("serviceMessage"); message_->setWordWrap(true); root->addWidget(message_);
    workspace_=new WorkspaceDXX(client_); root->addWidget(workspace_);
    connect(workspace_,&WorkspaceDXX::authenticationRequired,this,&ServiceSelectWindowDXX::loggedOut);
    connect(workspace_,&WorkspaceDXX::servicesChanged,this,[this](const JsonDXX& services) {account_.set("services",services);showAccount(account_);});
    auto* logout=new QPushButton(QStringLiteral("退出登录")); logout->setObjectName("logoutButton"); root->addWidget(logout);
    connect(logout,&QPushButton::clicked,this,[this] { if(pendingId_.isEmpty()) pendingId_=client_.request("auth.logout",JsonDXX({})); });
    connect(&client_,&TcpClientDXX::responseReceived,this,[this](const JsonDXX& r) {
        if(QString::fromStdString(r["request_id"].text())!=pendingId_) return;
        pendingId_.clear(); choices_->setEnabled(true);
        if(!r["success"].boolean()) { if(r["error"]["code"].text()=="AUTH_REQUIRED") { emit loggedOut(); return; } message_->setText(QString::fromStdString(r["error"]["message"].text())); return; }
        if(r["type"].text()=="auth.logout") { emit loggedOut(); return; }
        account_.set("services",r["data"]["services"]); showAccount(account_);
        message_->setText(QStringLiteral("%1 已激活。请选择下方好友、申请、推荐或账号页面。").arg(selected_));
    });
    connect(&client_,&TcpClientDXX::requestTimedOut,this,[this](const QString& id,const QString&) { if(id==pendingId_) { pendingId_.clear(); choices_->setEnabled(true); message_->setText(QStringLiteral("响应超时，请重试。")); } });
}
void ServiceSelectWindowDXX::showAccount(const JsonDXX& account) {
    account_=account; pendingId_.clear(); choices_->setEnabled(true);
    profile_->setText(QStringLiteral("%1\n用户 ID：%2").arg(QString::fromStdString(account["nickname"].text()),QString::number(account["user_id"].integer())));
    while(auto* item=list_->takeAt(0)) { delete item->widget(); delete item; }
    for(const auto& s:account["services"].items()) {
        const auto service=QString::fromStdString(s["service_type"].text());
        auto* button=new QPushButton(QStringLiteral("%1 · %2%3").arg(service,QString::fromStdString(s["display_id"].text()),s["active"].boolean()?QStringLiteral(" · 已激活"):QString()));
        button->setObjectName("activate"+service); list_->addWidget(button);
        connect(button,&QPushButton::clicked,this,[this,service] { if(!pendingId_.isEmpty()) return; selected_=service; pendingId_=client_.request("account.service.activate",JsonDXX{{"service_type",service.toStdString()}}); if(!pendingId_.isEmpty()) choices_->setEnabled(false); });
    }
    message_->setText(QStringLiteral("请选择已开通的服务"));
    workspace_->showAccount(account_);
}
