#include "GroupPageDXX.h"
#include "GroupPolicyDialogDXX.h"
#include "SubgroupPageDXX.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
GroupPageDXX::GroupPageDXX(TcpClientDXX &c) : RequestPanelDXX(c) {
  setObjectName("groupsPage");
  auto *root=new QVBoxLayout(body);
  auto *tabs=new QTabWidget; tabs->setObjectName("groupTabs"); root->addWidget(tabs);
  auto *listPage=new QWidget; auto *list=new QVBoxLayout(listPage);
  auto field=[](const char *id,const QString &placeholder,QVBoxLayout *layout) {
    auto *e=new QLineEdit; e->setObjectName(id); e->setPlaceholderText(placeholder); layout->addWidget(e); return e;
  };
  auto button=[this](QLayout *layout,const char *id,const QString &text,auto action) {
    auto *b=new QPushButton(text); b->setObjectName(id); layout->addWidget(b);
    connect(b,&QPushButton::clicked,this,action); return b;
  };
  query_=field("groupQuery",QStringLiteral("群号或群名称"),list);
  mine_=new QCheckBox(QStringLiteral("仅我加入的群")); mine_->setObjectName("groupMineOnly"); list->addWidget(mine_);
  auto *row=new QHBoxLayout; list->addLayout(row);
  button(row,"groupSearch",QStringLiteral("查询 / 刷新"),[this] { refresh(); });
  button(row,"groupNext",QStringLiteral("下一页"),[this] { if (!next_.isEmpty()) {cursor_=next_;load();} });
  name_=field("groupName",QStringLiteral("新群名称（最多96 UTF-8字节）"),list);
  button(list,"groupCreate",QStringLiteral("创建群"),[this] {
    if (subgroups_->mutationPending()) return;
    mutate("group.create",{{"service_type",service_.toStdString()},{"name",name_->text().toStdString()}},
      [this](const JsonDXX &d) {group_=d["group_pk"].integer();memberCursor_.clear();refresh();});
  });
  groups_=new QListWidget; groups_->setObjectName("groupResults"); list->addWidget(groups_);
  tabs->addTab(listPage,QStringLiteral("群列表"));
  auto *scroll=new QScrollArea; scroll->setWidgetResizable(true);
  auto *detailPage=new QWidget; auto *detailLayout=new QVBoxLayout(detailPage); scroll->setWidget(detailPage);
  summary_=new QLabel; summary_->setObjectName("groupSummary"); summary_->setWordWrap(true); summary_->setTextFormat(Qt::PlainText); detailLayout->addWidget(summary_);
  row=new QHBoxLayout; detailLayout->addLayout(row);
  apply_=button(row,"groupApply",QStringLiteral("申请加入"),[this] { mutation("group.join.apply",{}); });
  button(row,"groupDetailRefresh",QStringLiteral("刷新详情"),[this] { loadDetail(); });
  target_=field("groupTarget",QStringLiteral("受邀者展示 ID（由对方确认）"),detailLayout);
  invite_=button(detailLayout,"groupInvite",QStringLiteral("发送邀请"),[this] {
    mutation("group.invite",{{"target_display_id",target_->text().toStdString()}});
  });
  rename_=field("groupRename",QStringLiteral("新的群名称"),detailLayout);
  renameButton_=button(detailLayout,"groupRenameSave",QStringLiteral("修改群名"),[this] {
    mutation("group.name.update",{{"name",rename_->text().toStdString()}});
  });
  announcement_=new QTextEdit; announcement_->setObjectName("groupAnnouncement");
  announcement_->setPlaceholderText(QStringLiteral("群公告（最多1000 UTF-8字节，可留空）"));
  announcement_->setMaximumHeight(100); detailLayout->addWidget(announcement_);
  save_=button(detailLayout,"groupAnnouncementSave",QStringLiteral("保存公告"),[this] {
    mutation("group.announcement.update",{{"announcement",announcement_->toPlainText().toStdString()}});
  });
  members_=new QListWidget; members_->setObjectName("groupMembers"); members_->setMinimumHeight(100); detailLayout->addWidget(members_);
  row=new QHBoxLayout; detailLayout->addLayout(row);
  button(row,"groupMemberNext",QStringLiteral("下一页成员"),[this] {
    if(!memberNext_.isEmpty()){memberCursor_=memberNext_;loadDetail();}
  });
  admin_=button(row,"groupAdminSet",QStringLiteral("设管理员"),[this] {
    auto m=selectedMember(); if(!m.isNull()) mutation("group.admin.set",{{"target_display_id",m["display_id"]},{"enabled",true}});
  });
  revoke_=button(row,"groupAdminRevoke",QStringLiteral("撤管理员"),[this] {
    auto m=selectedMember(); if(!m.isNull()) mutation("group.admin.set",{{"target_display_id",m["display_id"]},{"enabled",false}});
  });
  kick_=button(row,"groupKick",QStringLiteral("踢出所选成员…"),[this] {
    auto m=selectedMember(); if(m.isNull()) return;
    if(QMessageBox::question(this,QStringLiteral("确认踢人"),QStringLiteral("将 %1 移出群与其子群？").arg(QString::fromStdString(m["display_id"].text())))==QMessageBox::Yes)
      mutation("group.member.kick",{{"target_display_id",m["display_id"]}});
  });
  row=new QHBoxLayout; detailLayout->addLayout(row);
  policy_=button(row,"groupPolicySwitch",QStringLiteral("切换群管理模式…"),[this] {
    if(!group_) return;
    GroupPolicyDialogDXX dialog(QString::fromStdString(detail_["policy_type"].text()),detail_["member_count"].integer(),this);
    if(dialog.exec()==QDialog::Accepted)
      mutation("group.policy.switch",{{"target_policy",dialog.targetPolicy().toStdString()}});
  });
  leave_=button(row,"groupLeave",QStringLiteral("退出群…"),[this] {
    if(QMessageBox::question(this,QStringLiteral("确认退出"),QStringLiteral("退出本群，并移除本群内的子群成员关系？"))==QMessageBox::Yes)
      mutation("group.member.leave",{});
  });
  disband_=button(row,"groupDisband",QStringLiteral("解散群…"),[this] {
    if(QMessageBox::question(this,QStringLiteral("确认解散"),QStringLiteral("解散本群及全部子群？此操作不可恢复。"))==QMessageBox::Yes)
      mutation("group.disband",{});
  });
  tabs->addTab(scroll,QStringLiteral("详情与成员"));
  auto *requestPage=new QWidget; auto *requestLayout=new QVBoxLayout(requestPage);
  auto *help=new QLabel(QStringLiteral("QQ申请由群主/管理员审批；微信邀请仅受邀者本人可接受或拒绝。")); help->setWordWrap(true); requestLayout->addWidget(help);
  requests_=new QListWidget; requests_->setObjectName("groupRequests"); requestLayout->addWidget(requests_);
  row=new QHBoxLayout; requestLayout->addLayout(row);
  button(row,"groupRequestRefresh",QStringLiteral("刷新待决请求"),[this]{requestCursor_.clear();loadRequests();});
  button(row,"groupRequestNext",QStringLiteral("下一页"),[this]{if(!requestNext_.isEmpty()){requestCursor_=requestNext_;loadRequests();}});
  accept_=button(row,"groupRequestAccept",QStringLiteral("接受"),[this]{respond("ACCEPT");});
  reject_=button(row,"groupRequestReject",QStringLiteral("拒绝"),[this]{respond("REJECT");});
  tabs->addTab(requestPage,QStringLiteral("申请与邀请"));
  subgroups_=new SubgroupPageDXX(c); tabs->addTab(subgroups_,QStringLiteral("临时子群"));
  connect(subgroups_,&RequestPanelDXX::changed,this,[this]{refresh();emit changed();});
  connect(subgroups_,&RequestPanelDXX::authenticationRequired,this,&RequestPanelDXX::authenticationRequired);
  connect(groups_,&QListWidget::currentRowChanged,this,[this](int r){
    if(r<0 || r>=int(items_.size()))return;
    auto id=items_[r]["group_pk"].integer();
    if(group_!=id){clearDetail();group_=id;memberCursor_.clear();}
    loadDetail();
  });
  connect(members_,&QListWidget::currentRowChanged,this,[this]{updateButtons();});
  connect(requests_,&QListWidget::currentRowChanged,this,[this]{updateButtons();});
  connect(tabs,&QTabWidget::currentChanged,this,[this](int index){
    if(index==1)loadDetail(); else if(index==2)loadRequests(); else if(index==3)subgroups_->refresh();
  });
  clearDetail();
}
bool GroupPageDXX::pending()const{return mutationPending()||subgroups_->mutationPending();}
void GroupPageDXX::resetGroupUser(const QString &user){
  resetForUser(user);subgroups_->resetForUser(user);clearDetail();subgroups_->clearGroup();
  items_.clear();requestItems_.clear();groups_->clear();requests_->clear();
}
void GroupPageDXX::setService(const QString &s){
  if(pending())return;
  if(service_!=s){service_=s;clearDetail();subgroups_->clearGroup();groups_->clear();requests_->clear();items_.clear();requestItems_.clear();}
  refresh();
}
void GroupPageDXX::refresh(){
  if(service_=="WEIBO")return;
  cursor_.clear();requestCursor_.clear();load();loadRequests();if(group_)loadDetail();
}
void GroupPageDXX::clearDetail(){
  group_=0;detail_=JsonDXX();permissions_=JsonDXX();memberItems_.clear();members_->clear();
  summary_->setText(QStringLiteral("请选择群查看详情。群主不能直接退出，请使用解散。"));
  subgroups_->clearGroup(); updateButtons();
}
JsonDXX GroupPageDXX::selectedMember()const{
  int r=members_->currentRow();return r<0||r>=int(memberItems_.size())?JsonDXX():memberItems_[r];
}
JsonDXX GroupPageDXX::selectedRequest()const{
  int r=requests_->currentRow();return r<0||r>=int(requestItems_.size())?JsonDXX():requestItems_[r];
}
void GroupPageDXX::updateButtons(){
  for(auto p:std::vector<std::pair<QPushButton*,const char*>>{{apply_,"can_apply"},{invite_,"can_invite"},{save_,"can_manage_announcement"},{renameButton_,"can_rename"},{policy_,"can_switch_policy"},{disband_,"can_disband"},{leave_,"can_leave"}})
    p.first->setEnabled(group_ && permissions_[p.second].boolean());
  auto m=selectedMember();
  kick_->setEnabled(group_ && m["can_kick"].boolean());
  admin_->setEnabled(group_ && permissions_["can_set_admin"].boolean() && m["can_set_admin"].boolean() && m["stored_role"].text()=="MEMBER");
  revoke_->setEnabled(group_ && permissions_["can_set_admin"].boolean() && m["can_set_admin"].boolean() && m["stored_role"].text()=="ADMIN");
  auto r=selectedRequest();accept_->setEnabled(r["can_respond"].boolean());reject_->setEnabled(r["can_respond"].boolean());
}
void GroupPageDXX::load(){
  auto service=service_;
  read("group.list",{{"service_type",service.toStdString()},{"query",query_->text().toStdString()},
    {"mine_only",mine_->isChecked()},{"limit",20},{"cursor",cursor_.isEmpty()?"0":cursor_.toStdString()}},[this,service](const JsonDXX &d){
    if(service_!=service)return;
    items_=d["items"].items();next_=QString::fromStdString(d["next_cursor"].text());
    groups_->clear();int row=-1;
    for(int i=0;i<int(items_.size());++i){const auto &g=items_[i];
      groups_->addItem(QStringLiteral("%1 · %2 · %3 人 · %4%5").arg(g["group_number"].integer()).arg(QString::fromStdString(g["name"].text())).arg(g["member_count"].integer()).arg(QString::fromStdString(g["policy_type"].text())).arg(g["is_member"].boolean()?QStringLiteral(" · 已加入"):QString()));
      if(g["group_pk"].integer()==group_)row=i;
    }
    if(row>=0)groups_->setCurrentRow(row);
    status->setText(QStringLiteral("共 %1 个群").arg(d["total"].integer()));
  });
}
void GroupPageDXX::loadDetail(){
  if(!group_)return;auto id=group_;auto service=service_;
  read("group.detail",{{"group_pk",id},{"limit",20},{"cursor",memberCursor_.isEmpty()?"0":memberCursor_.toStdString()}},[this,id,service](const JsonDXX &d){
    if(group_!=id||service_!=service)return;
    detail_=d["group"];permissions_=d["permissions"];memberItems_=d["members"].items();
    memberNext_=QString::fromStdString(d["next_cursor"].text());
    summary_->setText(QStringLiteral("%1 · %2\n群主 %3 · %4 人 · %5 规则 v%6\n公告：%7\n我的申请/邀请：%8").arg(detail_["group_number"].integer()).arg(QString::fromStdString(detail_["name"].text())).arg(QString::fromStdString(detail_["owner_display_id"].text())).arg(detail_["member_count"].integer()).arg(QString::fromStdString(detail_["policy_type"].text())).arg(detail_["policy_version"].integer()).arg(QString::fromStdString(detail_["announcement"].text())).arg(d["my_request"].isNull()?QStringLiteral("无"):QString::fromStdString(d["my_request"]["status"].text())));
    members_->clear();for(const auto &m:memberItems_)
      members_->addItem(QString::fromStdString(m["display_id"].text()+" · "+m["nickname"].text()+" · 保存角色 "+m["stored_role"].text()+" · 当前角色 "+m["effective_role"].text()));
    updateButtons();
    subgroups_->setGroup(id,detail_["is_member"].boolean() && detail_["policy_type"].text()=="QQ",permissions_["can_create_subgroup"].boolean());
  });
}
void GroupPageDXX::loadRequests(){
  auto service=service_;
  read("group.request.list",{{"service_type",service.toStdString()},{"limit",20},{"cursor",requestCursor_.isEmpty()?"0":requestCursor_.toStdString()}},[this,service](const JsonDXX &d){
    if(service_!=service)return;
    requestItems_=d["items"].items();requestNext_=QString::fromStdString(d["next_cursor"].text());requests_->clear();
    for(const auto &r:requestItems_)requests_->addItem(QString::fromStdString(r["group_name"].text()+" · "+r["kind"].text()+" · "+r["direction"].text()+" · "+r["initiator_display_id"].text()+" → "+r["target_display_id"].text()+" · "+r["status"].text()));
    updateButtons();
  });
}
void GroupPageDXX::mutation(const QString &type,JsonDXX data){
  if(!group_ || subgroups_->mutationPending())return;
  if (!data.isObject()) data = JsonDXX({});
  data.set("group_pk",group_);data.set("expected_policy_version",detail_["policy_version"]);
  mutate(type,data,[this,type](const JsonDXX &){
    if(type=="group.disband"||type=="group.member.leave")clearDetail();
    refresh();
  });
}
void GroupPageDXX::respond(const char *decision){
  auto r=selectedRequest();if(subgroups_->mutationPending() || !r["can_respond"].boolean())return;
  mutate("group.request.respond",{{"request_pk",r["request_pk"]},{"decision",decision}},[this](const JsonDXX &){refresh();});
}
