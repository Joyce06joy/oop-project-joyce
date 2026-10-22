#include "RecommendationPageDXX.h"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
RecommendationPageDXX::RecommendationPageDXX(TcpClientDXX &c)
    : RequestPanelDXX(c) {
  setObjectName("recommendationsPage");
  auto *l = new QVBoxLayout(body);
  enableBodyScrolling();
  l->addWidget(
      new QLabel(QStringLiteral("来源和目标服务须手动激活；涉及微信，请先绑定 "
                                "QQ 与微信账号。推荐仅发送申请。")));
  source_ = new QComboBox;
  source_->setObjectName("recommendSource");
  target_ = new QComboBox;
  target_->setObjectName("recommendTarget");
  for (auto *box : {source_, target_}) {
    box->addItems({"QQ", "WECHAT", "WEIBO"});
    l->addWidget(box);
  }
  target_->setCurrentIndex(1);
  mode_ = new QComboBox;
  mode_->setObjectName("recommendMode");
  mode_->addItems({QStringLiteral("同平台推荐"), QStringLiteral("共同好友"),
                   QStringLiteral("跨平台迁移推荐"),
                   QStringLiteral("跨平台好友交集")});
  l->addWidget(mode_);
  mutualTarget_ = new QLineEdit;
  mutualTarget_->setObjectName("mutualTarget");
  mutualTarget_->setPlaceholderText(
      QStringLiteral("共同好友查询：输入同平台展示 ID"));
  l->addWidget(mutualTarget_);
  list_ = new QListWidget;
  list_->setObjectName("recommendResults"); list_->setMinimumHeight(150); list_->setWordWrap(true);
  l->addWidget(list_);
  auto add = [&](const char *n, const QString &t, auto f) {
    auto *b = new QPushButton(t);
    b->setObjectName(n);
    l->addWidget(b);
    connect(b, &QPushButton::clicked, this, f);
  };
  add("recommendRefresh", QStringLiteral("查询 / 刷新"), [this] { refresh(); });
  add("recommendNext", QStringLiteral("下一页"), [this] {
    if (!next_.isEmpty()) {
      cursor_ = next_;
      load();
    }
  });
  add("recommendSend", QStringLiteral("向所选账号发送申请"), [this] {
    int r = list_->currentRow();
    if (r < 0 || r >= int(items_.size()))
      return;
    auto i = items_[r];
    mutate("friend.request.send",
           {{"service_type", i["service_type"]},
            {"target_display_id", i["display_id"]},
            {"verification_message", ""}},
           [this](const JsonDXX &) { refresh(); });
  });
  add("recommendSettings", QStringLiteral("前往账号绑定设置"),
      [this] { emit settingsRequested(); });
  auto *services=new QHBoxLayout;
  for (auto *box:{source_,target_,mode_}) { l->removeWidget(box); services->addWidget(box); }
  l->insertLayout(1,services);
  auto *actions=new QHBoxLayout;
  for (const auto *name:{"recommendRefresh","recommendNext","recommendSend","recommendSettings"}) {
    auto *b=findChild<QPushButton*>(name); l->removeWidget(b); actions->addWidget(b);
  }
  l->addLayout(actions);
  for (auto *label:body->findChildren<QLabel*>()) label->setWordWrap(true);
}
void RecommendationPageDXX::setService(const QString &s) {
  source_->setCurrentText(s);
  status->setText(QStringLiteral("选择来源与目标服务，然后点击查询。"));
  list_->clear();
  items_.clear();
}
void RecommendationPageDXX::refresh() {
  cursor_.clear();
  load();
}
void RecommendationPageDXX::load() {
  JsonDXX d{{"limit", 20},
            {"cursor",
             (cursor_.isEmpty() ? std::string("0") : cursor_.toStdString())}};
  QString type;
  int m = mode_->currentIndex();
  if (m < 2) {
    d.set("service_type", source_->currentText().toStdString());
    type = m == 0 ? "friend.recommend.same" : "friend.mutual.list";
    if (m == 1)
      d.set("target_display_id", mutualTarget_->text().toStdString());
  } else {
    d.set("source_service", source_->currentText().toStdString());
    d.set("target_service", target_->currentText().toStdString());
    type = m == 2 ? "friend.recommend.cross" : "friend.cross.common";
  }
  read(type, d, [this](const JsonDXX &data) {
    items_ = data["items"].items();
    next_ = QString::fromStdString(data["next_cursor"].text());
    list_->clear();
    for (const auto &i : items_)
      list_->addItem(
          QString::fromStdString(
              i["service_type"].text() + " · " + i["display_id"].text() +
              " · " + i["nickname"].text() + " · " + i["reason"].text()) +
          QStringLiteral(" · 共同好友 %1").arg(i["mutual_count"].integer()));
    status->setText(items_.empty()?QStringLiteral("共 0 项。暂无推荐，可调整来源、目标服务或查询模式；跨服务推荐需先完成绑定。"):QStringLiteral("共 %1 项").arg(data["total"].integer()));
  });
}
