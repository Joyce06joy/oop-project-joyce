#include "AccountPageDXX.h"
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
AccountPageDXX::AccountPageDXX(TcpClientDXX &c) : RequestPanelDXX(c) {
  setObjectName("accountPage");
  auto *l = new QVBoxLayout(body);
  services_ = new QLabel;
  services_->setObjectName("servicesSummary");
  services_->setTextFormat(Qt::PlainText);
  l->addWidget(services_);
  binding_ = new QLabel;
  binding_->setObjectName("bindingSummary");
  l->addWidget(binding_);
  choice_ = new QComboBox;
  choice_->setObjectName("openServiceChoice");
  choice_->addItems({"QQ", "WECHAT", "WEIBO"});
  l->addWidget(choice_);
  auto add = [&](const char *n, const QString &t, auto f) {
    auto *b = new QPushButton(t);
    b->setObjectName(n);
    l->addWidget(b);
    connect(b, &QPushButton::clicked, this, f);
  };
  add("openService", QStringLiteral("开通所选服务…"), [this] {
    if (QMessageBox::question(this, QStringLiteral("确认开通"),
                              QStringLiteral("开通 %1？开通后还需手动激活。")
                                  .arg(choice_->currentText())) !=
        QMessageBox::Yes)
      return;
    mutate("account.service.open",
           {{"service_type", choice_->currentText().toStdString()}},
           [this](const JsonDXX &) {
             read("account.services.list", JsonDXX({}),
                  [this](const JsonDXX &d) {
                    showServices(d["services"]);
                    emit servicesChanged(d["services"]);
                  });
           });
  });
  add("bindAccounts", QStringLiteral("绑定我的 QQ 与微信…"), [this] {
    if (QMessageBox::question(
            this, QStringLiteral("确认绑定"),
            QStringLiteral(
                "绑定本注册主体的 QQ 和微信账号，启用跨服务推荐映射？")) !=
        QMessageBox::Yes)
      return;
    mutate("account.binding.create", JsonDXX({}),
           [this](const JsonDXX &) { refresh(); });
  });
  add("unbindAccounts", QStringLiteral("解除 QQ 与微信绑定…"), [this] {
    if (QMessageBox::question(
            this, QStringLiteral("确认解绑"),
            QStringLiteral("解除后跨服务推荐映射将停止；好友和账号将保留。")) !=
        QMessageBox::Yes)
      return;
    mutate("account.binding.remove", JsonDXX({}),
           [this](const JsonDXX &) { refresh(); });
  });
  add("accountRefresh", QStringLiteral("刷新绑定"), [this] { refresh(); });
}
void AccountPageDXX::showServices(const JsonDXX &items) {
  QString t;
  for (const auto &i : items.items())
    t += QString::fromStdString(i["service_type"].text() + " · " +
                                i["display_id"].text()) +
         (i["active"].boolean() ? QStringLiteral(" · 已激活\n")
                                : QStringLiteral(" · 未激活\n"));
  services_->setText(t);
}
void AccountPageDXX::refresh() {
  read("account.binding.get", JsonDXX({}), [this](const JsonDXX &d) {
    binding_->setText(
        d["bound"].boolean()
            ? QStringLiteral("已绑定：QQ %1 ↔ 微信 %2")
                  .arg(QString::fromStdString(d["qq_display_id"].text()),
                       QString::fromStdString(d["wechat_display_id"].text()))
            : QStringLiteral("尚未绑定 QQ 与微信"));
  });
}
