#include "WorkspaceDXX.h"
#include "AccountPageDXX.h"
#include "GroupPageDXX.h"
#include "FriendPageDXX.h"
#include "FriendRequestPageDXX.h"
#include "RecommendationPageDXX.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
WorkspaceDXX::WorkspaceDXX(TcpClientDXX &c) : RequestPanelDXX(c) {
  setObjectName("workspace");
  refreshTimer_ = new QTimer(this);
  refreshTimer_->setSingleShot(true);
  refreshTimer_->setInterval(100);
  connect(refreshTimer_, &QTimer::timeout, this, [this] {
    if (accountEvent_) {
      accountEvent_ = false;
      reloadServices();
    }
    if (active()) {
      friends_->refresh();
      requests_->refresh();
      if (service_ != "WEIBO") groups_->setService(service_);
      if (tabs_->currentWidget() == recommendations_)
        recommendations_->refresh();
    }
    accounts_->refresh();
  });
  auto *root = new QHBoxLayout(body);
  auto *sidebar = new QVBoxLayout;
  root->addLayout(sidebar);
  for (const auto *s : {"QQ", "WECHAT", "WEIBO"}) {
    auto *b = new QPushButton(QString::fromLatin1(s));
    b->setObjectName(QString("space") + s);
    sidebar->addWidget(b);
    connect(b, &QPushButton::clicked, this, [this, s] { selectService(s); });
  }
  sidebar->addStretch();
  auto *content = new QVBoxLayout;
  root->addLayout(content, 1);
  gate_ = new QLabel;
  gate_->setObjectName("serviceGate");
  gate_->setWordWrap(true);
  content->addWidget(gate_);
  tabs_ = new QTabWidget;
  tabs_->setObjectName("workspaceTabs");
  content->addWidget(tabs_);
  friends_ = new FriendPageDXX(c);
  requests_ = new FriendRequestPageDXX(c);
  recommendations_ = new RecommendationPageDXX(c);
  accounts_ = new AccountPageDXX(c);
  tabs_->addTab(friends_, QStringLiteral("好友"));
  tabs_->addTab(requests_, QStringLiteral("申请"));
  tabs_->addTab(recommendations_, QStringLiteral("推荐"));
  tabs_->addTab(accounts_, QStringLiteral("账号"));
  groups_ = new GroupPageDXX(c);
  tabs_->addTab(groups_, QStringLiteral("群组"));
  connect(tabs_, &QTabWidget::currentChanged, this,
          [this] { refreshCurrent(); });
  connect(recommendations_, &RecommendationPageDXX::settingsRequested, this,
          [this] { tabs_->setCurrentWidget(accounts_); });
  connect(accounts_, &AccountPageDXX::servicesChanged, this,
          [this](const JsonDXX &s) {
            services_ = s;
            emit servicesChanged(s);
            refreshCurrent();
          });
  for (auto *p : {static_cast<RequestPanelDXX *>(friends_),
                  static_cast<RequestPanelDXX *>(requests_),
                  static_cast<RequestPanelDXX *>(recommendations_),
                  static_cast<RequestPanelDXX *>(accounts_),
                  static_cast<RequestPanelDXX *>(groups_)}) {
    connect(p, &RequestPanelDXX::authenticationRequired, this,
            &RequestPanelDXX::authenticationRequired);
    connect(p, &RequestPanelDXX::changed, this, [this] { scheduleRefresh(); });
  }
  connect(&c, &TcpClientDXX::eventReceived, this, [this](const JsonDXX &e) {
    if (user_.isEmpty())
      return;
    status->setText(QStringLiteral("收到更新：%1")
                        .arg(QString::fromStdString(e["type"].text())));
    if (e["type"].text().find("account.") == 0) {
      scheduleRefresh(true);
    } else {
      const auto s = e["data"]["service_type"].text();
      if (s.empty() || s == service_.toStdString())
        scheduleRefresh();
    }
  });
}
bool WorkspaceDXX::active() const {
  for (const auto &s : services_.items())
    if (s["service_type"].text() == service_.toStdString())
      return s["active"].boolean();
  return false;
}
void WorkspaceDXX::showAccount(const JsonDXX &a) {
  auto user = QString::number(a["user_id"].integer());
  if (user_ != user) {
    user_ = user;
    service_ = "QQ";
    for (auto *p : {static_cast<RequestPanelDXX *>(this),
                    static_cast<RequestPanelDXX *>(friends_),
                    static_cast<RequestPanelDXX *>(requests_),
                    static_cast<RequestPanelDXX *>(recommendations_),
                    static_cast<RequestPanelDXX *>(accounts_)})
      p->resetForUser(user);
    groups_->resetGroupUser(user);
    recommendations_->setService(service_);
    for (auto *field : findChildren<QLineEdit *>())
      field->clear();
    for (auto *list : findChildren<QListWidget *>())
      list->clear();
    accounts_->findChild<QLabel *>("bindingSummary")->clear();
  }
  services_ = a["services"];
  accounts_->showServices(services_);
  refreshCurrent();
}
void WorkspaceDXX::selectService(const QString &s) {
  if (s != service_ &&
      (friends_->mutationPending() || requests_->mutationPending() ||
       recommendations_->mutationPending() || groups_->pending())) {
    status->setText(QStringLiteral("请先确认或重试当前操作，再切换服务。"));
    return;
  }
  if (service_ != s) {
    service_ = s;
    recommendations_->setService(s);
    if (active() && s != "WEIBO") groups_->setService(s);
    if (active()) {
      friends_->setService(s);
      requests_->setService(s);
    }
  }
  refreshCurrent();
}
void WorkspaceDXX::refreshCurrent() {
  bool enabled = active();
  tabs_->setTabVisible(4, service_ != "WEIBO");
  groups_->setEnabled(enabled && service_ != "WEIBO");
  friends_->setEnabled(enabled);
  requests_->setEnabled(enabled);
  recommendations_->setEnabled(enabled);
  gate_->setText(enabled ? QStringLiteral("%1 · 已激活").arg(service_)
                         : QStringLiteral("%1 "
                                          "未开通或尚未激活。请在账号页开通，再"
                                          "点击上方对应服务确认激活。")
                               .arg(service_));
  if (user_.isEmpty())
    return;
  if (tabs_->currentWidget() == accounts_)
    accounts_->refresh();
  else if (enabled) {
    if (tabs_->currentWidget() == friends_)
      friends_->setService(service_);
    else if (tabs_->currentWidget() == requests_)
      requests_->setService(service_);
    else if (tabs_->currentWidget() == groups_)
      groups_->setService(service_);
    else
      recommendations_->refresh();
  }
}
void WorkspaceDXX::reloadServices() {
  read("account.services.list", JsonDXX({}), [this](const JsonDXX &d) {
    services_ = d["services"];
    accounts_->showServices(services_);
    emit servicesChanged(services_);
    refreshCurrent();
  });
}

void WorkspaceDXX::scheduleRefresh(bool accounts) {
  accountEvent_ = accountEvent_ || accounts;
  if (!refreshTimer_->isActive())
    refreshTimer_->start();
}
