#include "FriendPageDXX.h"
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
FriendPageDXX::FriendPageDXX(TcpClientDXX &c) : RequestPanelDXX(c) {
  setObjectName("friendsPage");
  auto *l = new QVBoxLayout(body);
  enableBodyScrolling();
  query_ = new QLineEdit;
  query_->setObjectName("friendQuery");
  query_->setPlaceholderText(QStringLiteral("展示 ID 或昵称"));
  l->addWidget(query_);
  auto button = [&](const char *name, const QString &text, auto action) {
    auto *b = new QPushButton(text);
    b->setObjectName(name);
    l->addWidget(b);
    connect(b, &QPushButton::clicked, this, action);
  };
  button("friendSearch", QStringLiteral("搜索"), [this] {
    searching_ = true;
    refresh();
  });
  button("friendRefresh", QStringLiteral("我的好友 / 刷新"), [this] {
    searching_ = false;
    refresh();
  });
  results_ = new QListWidget;
  results_->setObjectName("friendResults"); results_->setMinimumHeight(150); results_->setWordWrap(true);
  l->addWidget(results_);
  message_ = new QLineEdit;
  message_->setObjectName("verificationMessage");
  message_->setPlaceholderText(
      QStringLiteral("验证消息（最多 240 UTF-8 字节）"));
  l->addWidget(message_);
  button("sendFriendRequest", QStringLiteral("发送申请"), [this] {
    auto s = selected();
    if (s.isNull())
      return;
    if (message_->text().toUtf8().size() > 240) {
      status->setText(QStringLiteral("验证消息太长"));
      return;
    }
    mutate("friend.request.send",
           {{"service_type", service_.toStdString()},
            {"target_display_id", s["display_id"]},
            {"verification_message", message_->text().toStdString()}},
           [this](const JsonDXX &) {
             status->setText("PENDING · 申请已发送");
             load();
           });
  });
  remark_ = new QLineEdit;
  remark_->setObjectName("friendRemark");
  remark_->setPlaceholderText(QStringLiteral("我的备注，留空清除"));
  l->addWidget(remark_);
  button("saveFriendRemark", QStringLiteral("保存备注"), [this] {
    auto s = selected();
    if (s.isNull())
      return;
    if (remark_->text().toUtf8().size() > 160) {
      status->setText(QStringLiteral("备注太长"));
      return;
    }
    mutate("friend.remark.set",
           {{"service_type", service_.toStdString()},
            {"target_display_id", s["display_id"]},
            {"remark", remark_->text().toStdString()}},
           [this](const JsonDXX &) { load(); });
  });
  button("friendChat", QStringLiteral("发消息"), [this] { auto s=selected(); if(!s.isNull()&&service_!="WEIBO") emit chatRequested(QString::fromStdString(s["display_id"].text())); });
  button("friendProfile", QStringLiteral("查看资料"), [this] {
    auto s = selected();
    if (s.isNull())
      return;
    QString t;
    const std::vector<std::pair<const char *, QString>> fields = {
        {"service_type", QStringLiteral("服务")},
        {"display_id", QStringLiteral("展示号码")},
        {"nickname", QStringLiteral("昵称")},
        {"remark", QStringLiteral("备注")},
        {"birthday", QStringLiteral("生日")},
        {"location", QStringLiteral("所在地")},
        {"registered_at", QStringLiteral("注册时间")},
        {"relationship", QStringLiteral("关系")}};
    for (const auto &field : fields)
      t += field.second + QStringLiteral("：") +
           QString::fromStdString(s[field.first].text()) + "\n";
    QMessageBox::information(this, QStringLiteral("好友资料"), t);
  });
  button("removeFriend", QStringLiteral("删除好友…"), [this] {
    auto s = selected();
    if (s.isNull())
      return;
    if (QMessageBox::question(
            this, QStringLiteral("确认删除好友"),
            QStringLiteral(
                "双方将解除 %1 好友关系：%2。其他平台关系不受影响。是否继续？")
                .arg(service_,
                     QString::fromStdString(s["display_id"].text()))) !=
        QMessageBox::Yes)
      return;
    mutate("friend.remove",
           {{"service_type", service_.toStdString()},
            {"target_display_id", s["display_id"]}},
           [this](const JsonDXX &) { load(); });
  });
  button("friendNext", QStringLiteral("下一页"), [this] {
    if (!next_.isEmpty()) {
      cursor_ = next_;
      load();
    }
  });
  // Reuse the existing controls in compact, task-oriented rows.
  while (auto *item=l->takeAt(0)) delete item;
  auto row = [l](std::initializer_list<QWidget*> widgets) {
    auto *r=new QHBoxLayout; r->setSpacing(8); l->addLayout(r);
    for (auto *w:widgets) r->addWidget(w);
  };
  row({query_,findChild<QPushButton*>("friendSearch"),findChild<QPushButton*>("friendRefresh")});
  l->addWidget(results_,1);
  row({message_,findChild<QPushButton*>("sendFriendRequest")});
  row({remark_,findChild<QPushButton*>("saveFriendRemark")});
  row({findChild<QPushButton*>("friendChat"),findChild<QPushButton*>("friendProfile"),findChild<QPushButton*>("removeFriend"),findChild<QPushButton*>("friendNext")});
  findChild<QPushButton*>("removeFriend")->setProperty("role","danger");
  connect(results_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0 && row < int(items_.size()))
      remark_->setText(QString::fromStdString(items_[row]["remark"].text()));
  });
}
JsonDXX FriendPageDXX::selected() const {
  int r = results_->currentRow();
  return r < 0 || r >= int(items_.size()) ? JsonDXX() : items_[r];
}
void FriendPageDXX::setService(const QString &s) {
  if (mutationPending())
    return;
  service_ = s;
  findChild<QPushButton*>("friendChat")->setVisible(s!="WEIBO");
  items_.clear();
  results_->clear();
  refresh();
}
void FriendPageDXX::refresh() {
  cursor_.clear();
  load();
}
void FriendPageDXX::load() {
  JsonDXX data{{"service_type", service_.toStdString()},
               {"limit", 20},
               {"cursor", (cursor_.isEmpty() ? std::string("0")
                                             : cursor_.toStdString())}};
  if (searching_)
    data.set("query", query_->text().toStdString());
  read(searching_ ? "friend.search" : "friend.list", data,
       [this](const JsonDXX &d) {
         items_ = d["items"].items();
         next_ = QString::fromStdString(d["next_cursor"].text());
         results_->clear();
         for (const auto &i : items_)
           results_->addItem(QString::fromStdString(
               i["service_type"].text() + " · " + i["display_id"].text() +
               " · " + i["nickname"].text() + " · " + i["remark"].text() +
               " · " + i["relationship"].text()));
         status->setText(QStringLiteral("共 %1 项%2")
                             .arg(d["total"].integer())
                             .arg(items_.empty() ? (searching_ ? QStringLiteral(" · 未找到账号，请尝试完整展示 ID 或其他昵称") : QStringLiteral(" · 暂无好友，搜索账号后发送好友申请"))
                                                 : QString()));
       });
}

void FriendPageDXX::resetView() {
  searching_ = false; cursor_.clear(); next_.clear(); items_.clear(); results_->clear();
}
