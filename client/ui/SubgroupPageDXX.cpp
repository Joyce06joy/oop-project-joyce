#include "SubgroupPageDXX.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QRegularExpression>
SubgroupPageDXX::SubgroupPageDXX(TcpClientDXX &c) : RequestPanelDXX(c) {
  setObjectName("subgroupsPage");
  auto *l = new QVBoxLayout(body);
  enableBodyScrolling();
  summary_ = new QLabel;
  summary_->setObjectName("subgroupSummary");
  summary_->setWordWrap(true);
  l->addWidget(summary_);
  name_ = new QLineEdit;
  name_->setObjectName("subgroupName");
  name_->setPlaceholderText(QStringLiteral("临时子群名称"));
  l->addWidget(name_);
  targets_ = new QLineEdit;
  targets_->setObjectName("subgroupTargets");
  targets_->setPlaceholderText(QStringLiteral("父群成员展示 ID，多个用空格或逗号分隔"));
  l->addWidget(targets_);
  auto *row = new QHBoxLayout;
  l->addLayout(row);
  auto button = [this,row](const char *id, const QString &text, auto action) {
    auto *b = new QPushButton(text); b->setObjectName(id); row->addWidget(b);
    connect(b, &QPushButton::clicked, this, action); return b;
  };
  createButton_ = button("subgroupCreate", QStringLiteral("创建子群"), [this] {
    if (!enabled_ || !create_) return;
    std::vector<JsonDXX> ids;
    for (const auto &id : targets_->text().split(QRegularExpression("[,，\\s]+"), Qt::SkipEmptyParts))
      ids.emplace_back(id.toStdString());
    mutate("group.subgroup.create", {{"group_pk", group_}, {"name", name_->text().toStdString()},
      {"member_display_ids", JsonDXX::array(ids)}}, [this](const JsonDXX &) { refresh(); });
  });
  button("subgroupChat", QStringLiteral("进入子群聊天"), [this] { if(enabled_&&subgroup_)emit chatRequested(group_,subgroup_); });
  button("subgroupRefresh", QStringLiteral("刷新"), [this] { refresh(); });
  button("subgroupNext", QStringLiteral("下一页子群"), [this] { if (!next_.isEmpty()) { cursor_ = next_; load(); } });
  groups_ = new QListWidget; groups_->setObjectName("subgroupResults"); groups_->setMinimumHeight(100); l->addWidget(groups_);
  members_ = new QListWidget; members_->setObjectName("subgroupMembers"); members_->setMinimumHeight(100); l->addWidget(members_);
  row = new QHBoxLayout; l->addLayout(row);
  auto memberButton = [this,row](const char *id, const QString &text, auto action) {
    auto *b = new QPushButton(text); b->setObjectName(id); row->addWidget(b);
    connect(b, &QPushButton::clicked, this, action); return b;
  };
  add_ = memberButton("subgroupAdd", QStringLiteral("添加输入成员"), [this] { updateMembers(true); });
  remove_ = memberButton("subgroupRemove", QStringLiteral("移除输入成员"), [this] { updateMembers(false); });
  memberButton("subgroupMemberNext", QStringLiteral("下一页成员"), [this] {
    if (!memberNext_.isEmpty()) { memberCursor_ = memberNext_; loadMembers(); }
  });
  connect(groups_, &QListWidget::currentRowChanged, this, [this] {
    auto s = selected(); subgroup_ = s["subgroup_pk"].integer();
    add_->setEnabled(enabled_ && s["can_manage"].boolean());
    remove_->setEnabled(enabled_ && s["can_manage"].boolean());
    memberCursor_.clear(); members_->clear(); loadMembers();
  });
  clearGroup();
}
JsonDXX SubgroupPageDXX::selected() const {
  auto r = groups_->currentRow();
  return r < 0 || r >= int(items_.size()) ? JsonDXX() : items_[r];
}
void SubgroupPageDXX::clearGroup() {
  group_ = subgroup_ = 0; enabled_ = create_ = false;
  items_.clear(); groups_->clear(); members_->clear();
  createButton_->setEnabled(false); add_->setEnabled(false); remove_->setEnabled(false);
  summary_->setText(QStringLiteral("请选择已加入的 QQ 规则群。微信规则下子群暂停。"));
}
void SubgroupPageDXX::setGroup(qint64 id, bool enabled, bool create) {
  bool changedGroup = group_ != id;
  group_ = id; enabled_ = enabled; create_ = create;
  createButton_->setEnabled(enabled && create);
  if (!enabled) {
    items_.clear(); groups_->clear(); members_->clear(); subgroup_ = 0;
    add_->setEnabled(false); remove_->setEnabled(false);
    summary_->setText(QStringLiteral("当前规则不提供子群，原有数据保留；切回 QQ 后恢复。"));
    return;
  }
  if (changedGroup) { subgroup_ = 0; cursor_.clear(); memberCursor_.clear(); }
  refresh();
}
void SubgroupPageDXX::refresh() { cursor_.clear(); load(); }
void SubgroupPageDXX::load() {
  if (!group_ || !enabled_) return;
  auto id = group_;
  read("group.subgroup.list", {{"group_pk", id}, {"limit",20}, {"cursor",cursor_.isEmpty()?"0":cursor_.toStdString()}},
    [this,id](const JsonDXX &d) {
      if (group_ != id || !enabled_) return;
      if (d["disabled"].boolean()) { setGroup(id,false,false); return; }
      items_ = d["items"].items(); next_ = QString::fromStdString(d["next_cursor"].text());
      auto selectedId = subgroup_; groups_->clear(); int selectedRow = -1;
      for (int i=0; i<int(items_.size()); ++i) {
        const auto &s=items_[i];
        groups_->addItem(QStringLiteral("%1 · %2 人").arg(QString::fromStdString(s["name"].text())).arg(s["member_count"].integer()));
        if (s["subgroup_pk"].integer()==selectedId) selectedRow=i;
      }
      summary_->setText(items_.empty()?QStringLiteral("暂无临时子群。群主或获授权成员可从父群成员创建子群。"):QStringLiteral("共 %1 个临时子群").arg(d["total"].integer()));
      if (selectedRow>=0) groups_->setCurrentRow(selectedRow);
    });
}
void SubgroupPageDXX::loadMembers() {
  if (!enabled_ || !subgroup_) return;
  const auto group=group_, sub=subgroup_;
  read("group.subgroup.list", {{"group_pk", group}, {"subgroup_pk",sub}, {"limit",20},
       {"cursor",memberCursor_.isEmpty()?"0":memberCursor_.toStdString()}}, [this,group,sub](const JsonDXX &d) {
    if (!enabled_ || group_!=group || subgroup_!=sub) return;
    if (d["disabled"].boolean()) { setGroup(group,false,false); return; }
    members_->clear();
    for (const auto &m:d["items"].items())
      members_->addItem(QString::fromStdString(m["display_id"].text()+" · "+m["nickname"].text()));
    memberNext_=QString::fromStdString(d["next_cursor"].text());
  });
}
void SubgroupPageDXX::updateMembers(bool add) {
  auto s=selected(); if (!enabled_ || !subgroup_ || !s["can_manage"].boolean()) return;
  std::vector<JsonDXX> ids;
  for (const auto &id:targets_->text().split(QRegularExpression("[,，\\s]+"), Qt::SkipEmptyParts)) ids.emplace_back(id.toStdString());
  JsonDXX data{{"subgroup_pk",subgroup_}};
  data.set(add?"add_display_ids":"remove_display_ids",JsonDXX::array(ids));
  mutate("group.subgroup.members.update",data,[this](const JsonDXX &) { refresh(); });
}
