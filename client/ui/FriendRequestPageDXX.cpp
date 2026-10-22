#include "FriendRequestPageDXX.h"
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
FriendRequestPageDXX::FriendRequestPageDXX(TcpClientDXX &c)
    : RequestPanelDXX(c) {
  setObjectName("requestsPage");
  auto *l = new QVBoxLayout(body);
  enableBodyScrolling();
  list_ = new QListWidget;
  list_->setObjectName("requestResults"); list_->setMinimumHeight(180); list_->setWordWrap(true);
  l->addWidget(list_);
  auto add = [&](const char *n, const QString &t, auto f) {
    auto *b = new QPushButton(t);
    b->setObjectName(n);
    l->addWidget(b);
    connect(b, &QPushButton::clicked, this, f);
    return b;
  };
  add("requestsRefresh", QStringLiteral("刷新收发申请"), [this] { refresh(); });
  auto *accept = add("acceptRequest", QStringLiteral("接受收到的申请"),
                     [this] { respond(true); });
  auto *reject = add("rejectRequest", QStringLiteral("拒绝收到的申请"),
                     [this] { respond(false); });
  accept->setEnabled(false);
  reject->setEnabled(false);
  connect(list_, &QListWidget::currentRowChanged, this,
          [this, accept, reject](int row) {
            const bool incoming = row >= 0 && row < int(items_.size()) &&
                                  items_[row]["direction"].text() == "INCOMING";
            accept->setEnabled(incoming);
            reject->setEnabled(incoming);
          });
  add("requestsNext", QStringLiteral("下一页"), [this] {
    if (!next_.isEmpty()) {
      cursor_ = next_;
      load();
    }
  });
  auto *actions=new QHBoxLayout;
  for (const auto *name:{"requestsRefresh","acceptRequest","rejectRequest","requestsNext"}) {
    auto *b=findChild<QPushButton*>(name); l->removeWidget(b); actions->addWidget(b);
  }
  l->addLayout(actions);
}
void FriendRequestPageDXX::setService(const QString &s) {
  if (mutationPending())
    return;
  service_ = s;
  items_.clear();
  list_->clear();
  refresh();
}
void FriendRequestPageDXX::refresh() {
  cursor_.clear();
  load();
}
void FriendRequestPageDXX::load() {
  read(
      "friend.request.pending",
      {{"service_type", service_.toStdString()},
       {"limit", 20},
       {"cursor",
        (cursor_.isEmpty() ? std::string("0") : cursor_.toStdString())}},
      [this](const JsonDXX &d) {
        items_ = d["items"].items();
        next_ = QString::fromStdString(d["next_cursor"].text());
        list_->clear();
        for (const auto &i : items_)
          list_->addItem(QString::fromStdString(
              i["direction"].text() + " · " + i["sender_display_id"].text() +
              " " + i["sender_nickname"].text() + " → " +
              i["target_display_id"].text() + " · " +
              i["verification_message"].text() + " · PENDING"));
        status->setText(
            (items_.empty()?QStringLiteral("暂无待处理申请。搜索好友发送申请，或稍后刷新收到的邀请。"):QStringLiteral("待处理收发申请共 %1 项").arg(d["total"].integer())));
      });
}
void FriendRequestPageDXX::respond(bool accept) {
  int r = list_->currentRow();
  if (r < 0 || r >= int(items_.size()))
    return;
  const auto i = items_[r];
  if (i["direction"].text() != "INCOMING") {
    status->setText(QStringLiteral("只能处理收到的申请"));
    return;
  }
  mutate("friend.request.respond",
         {{"friend_request_id", i["friend_request_id"]},
          {"decision", accept ? "ACCEPT" : "REJECT"}},
         [this](const JsonDXX &) { refresh(); });
}
