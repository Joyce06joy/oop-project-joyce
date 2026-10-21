#include "RegisterWindowDXX.h"
#include <QApplication>
#include <QClipboard>
#include <QDateEdit>
#include <QFormLayout>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QUuid>
#include <QRegularExpression>
#include <QHBoxLayout>

RegisterWindowDXX::RegisterWindowDXX(TcpClientDXX& client,QWidget* parent):QWidget(parent,Qt::Window),client_(client) {
    setObjectName("registerWindow"); setWindowTitle(QStringLiteral("创建统一账号 · IMPlatform")); resize(480,620);
    auto* root=new QVBoxLayout(this); auto* title=new QLabel(QStringLiteral("创建统一账号")); title->setStyleSheet("font-size: 24px; font-weight: 600;"); root->addWidget(title);
    fields_=new QWidget; auto* form=new QFormLayout(fields_);
    auto field=[&](const char* object,const QString& label,bool secret=false) { auto* line=new QLineEdit; line->setObjectName(object); if(secret) line->setEchoMode(QLineEdit::Password); form->addRow(label,line); return line; };
    name_=field("registerName",QStringLiteral("登录名")); name_->setPlaceholderText(QStringLiteral("3–32 位英文、数字或下划线"));
    nickname_=field("registerNickname",QStringLiteral("昵称")); password_=field("registerPassword",QStringLiteral("密码"),true); password_->setPlaceholderText(QStringLiteral("8–128 字节")); confirm_=field("registerConfirm",QStringLiteral("确认密码"),true);
    hasBirthday_=new QCheckBox(QStringLiteral("填写生日（可选）")); hasBirthday_->setObjectName("hasBirthday"); form->addRow(hasBirthday_);
    birthday_=new QDateEdit(QDate(2000,1,1)); birthday_->setObjectName("registerBirthday"); birthday_->setCalendarPopup(true); birthday_->setDisplayFormat("yyyy-MM-dd"); birthday_->setDateRange(QDate(1900,1,1),QDate::currentDate()); birthday_->setEnabled(false); connect(hasBirthday_,&QCheckBox::toggled,birthday_,&QDateEdit::setEnabled); form->addRow(QStringLiteral("生日"),birthday_);
    location_=field("registerLocation",QStringLiteral("所在地（可选）"));
    auto* serviceBox=new QWidget; auto* services=new QHBoxLayout(serviceBox); qq_=new QCheckBox("QQ"); qq_->setObjectName("chooseQQ"); wechat_=new QCheckBox(QStringLiteral("微信")); wechat_->setObjectName("chooseWECHAT"); weibo_=new QCheckBox(QStringLiteral("微博")); weibo_->setObjectName("chooseWEIBO"); services->addWidget(qq_); services->addWidget(wechat_); services->addWidget(weibo_); form->addRow(QStringLiteral("开通服务"),serviceBox); root->addWidget(fields_);
    message_=new QLabel; message_->setObjectName("registerMessage"); message_->setWordWrap(true); message_->setStyleSheet("color: #a63d26;"); root->addWidget(message_);
    submit_=new QPushButton(QStringLiteral("注册")); submit_->setObjectName("registerSubmit"); root->addWidget(submit_); connect(submit_,&QPushButton::clicked,this,&RegisterWindowDXX::submit);
    result_=new QLabel; result_->setObjectName("registerResult"); result_->setTextFormat(Qt::PlainText); result_->setTextInteractionFlags(Qt::TextSelectableByMouse); result_->setWordWrap(true); root->addWidget(result_);
    copy_=new QPushButton(QStringLiteral("复制新账号 ID")); copy_->setEnabled(false); root->addWidget(copy_); connect(copy_,&QPushButton::clicked,this,[this] { QApplication::clipboard()->setText(result_->text()); });
    auto* close=new QPushButton(QStringLiteral("返回登录")); root->addWidget(close); connect(close,&QPushButton::clicked,this,&QWidget::hide);
    connect(&client_,&TcpClientDXX::responseReceived,this,[this](const JsonDXX& r) {
        if(QString::fromStdString(r["request_id"].text())!=requestId_) return;
        if(!r["success"].boolean()) {
            const auto code=r["error"]["code"].text();
            if(code=="DATABASE_ERROR"||code=="SERVER_ERROR") { uncertain(); return; }
            message_->setText(QString::fromStdString(r["error"]["message"].text())); requestId_.clear(); fields_->setEnabled(true); submit_->setEnabled(true); submit_->setText(QStringLiteral("注册")); return;
        }
        completed_=true; payload_=JsonDXX(); message_->setText(QStringLiteral("注册成功，数据已保存。请返回登录。"));
        const auto d=r["data"]; QString text=QStringLiteral("用户 ID：%1\n注册时间：%2").arg(QString::number(d["user_id"].integer()),QString::fromStdString(d["registered_at"].text()));
        for(const auto& a:d["accounts"].items()) text+=QStringLiteral("\n%1：%2").arg(QString::fromStdString(a["service_type"].text()),QString::fromStdString(a["display_id"].text()));
        result_->setText(text); submit_->setEnabled(false); copy_->setEnabled(true); password_->clear(); confirm_->clear();
    });
    connect(&client_,&TcpClientDXX::requestTimedOut,this,[this](const QString& id,const QString&) { if(id==requestId_) uncertain(); });
    connect(&client_,&TcpClientDXX::statusChanged,this,[this](const QString&,bool online) { if(!online&&!requestId_.isEmpty()&&!completed_) uncertain(); });
    connect(&client_,&TcpClientDXX::problem,this,[this](const QString& text) { if(isVisible()) message_->setText(text); });
}
void RegisterWindowDXX::uncertain() { message_->setText(QStringLiteral("尚未收到结果，注册可能已保存。恢复连接后点击重试，安全查询原结果。输入暂时保留锁定。")); submit_->setEnabled(true); submit_->setText(QStringLiteral("重试原注册请求")); }
void RegisterWindowDXX::submit() {
    if(completed_) return;
    if(requestId_.isEmpty()) {
        const auto name=name_->text();
        if(!QRegularExpression(QStringLiteral("^[A-Za-z0-9_]{3,32}$")).match(name).hasMatch()) { message_->setText(QStringLiteral("登录名须为 3–32 位英文、数字或下划线")); name_->setFocus(); return; }
        if(nickname_->text().trimmed().isEmpty()||nickname_->text().toUtf8().size()>96) { message_->setText(QStringLiteral("请填写昵称（最多 96 字节）")); nickname_->setFocus(); return; }
        if(password_->text().toUtf8().size()<8||password_->text().toUtf8().size()>128) { message_->setText(QStringLiteral("密码须为 8–128 字节")); password_->setFocus(); return; }
        if(password_->text()!=confirm_->text()) { message_->setText(QStringLiteral("两次密码不一致，请检查确认密码")); confirm_->setFocus(); return; }
        if(location_->text().toUtf8().size()>160) { message_->setText(QStringLiteral("所在地最多 160 字节")); location_->setFocus(); return; }
        std::vector<JsonDXX> services; if(qq_->isChecked()) services.emplace_back("QQ"); if(wechat_->isChecked()) services.emplace_back("WECHAT"); if(weibo_->isChecked()) services.emplace_back("WEIBO");
        if(services.empty()) { message_->setText(QStringLiteral("请至少选择一种服务")); return; }
        payload_=JsonDXX{{"login_name",name.toStdString()},{"nickname",nickname_->text().toStdString()},{"password",password_->text().toStdString()},{"birthday",hasBirthday_->isChecked()?JsonDXX(birthday_->date().toString(Qt::ISODate).toStdString()):JsonDXX()},{"location",location_->text().isEmpty()?JsonDXX():JsonDXX(location_->text().toStdString())},{"services",JsonDXX::array(services)}};
        if(!client_.connected()) { message_->setText(QStringLiteral("请先在登录窗口重试连接服务器")); return; }
        requestId_=QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if(client_.request("auth.register",payload_,requestId_).isEmpty()) return;
    fields_->setEnabled(false); submit_->setEnabled(false); submit_->setText(QStringLiteral("注册中…")); message_->setText(QStringLiteral("正在保存，请稍候…"));
}
