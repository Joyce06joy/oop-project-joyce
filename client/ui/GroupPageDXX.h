#pragma once
#include "RequestPanelDXX.h"
class QLineEdit;
class QTextEdit;
class QListWidget;
class QLabel;
class QCheckBox;
class QPushButton;
class SubgroupPageDXX;
class GroupPageDXX : public RequestPanelDXX {
  Q_OBJECT
public:
  explicit GroupPageDXX(TcpClientDXX &client);
  void setService(const QString &service);
  void refresh();
  void resetGroupUser(const QString &user);
  bool pending() const;
private:
  QString service_ = "QQ", cursor_, next_, memberCursor_, memberNext_, requestCursor_, requestNext_;
  qint64 group_ = 0;
  JsonDXX detail_, permissions_;
  std::vector<JsonDXX> items_, memberItems_, requestItems_;
  QLineEdit *query_, *name_, *target_, *rename_;
  QTextEdit *announcement_;
  QCheckBox *mine_;
  QListWidget *groups_, *members_, *requests_;
  QLabel *summary_;
  SubgroupPageDXX *subgroups_;
  QPushButton *apply_, *invite_, *accept_, *reject_, *admin_, *revoke_, *kick_, *save_, *renameButton_, *leave_, *disband_, *policy_;
  JsonDXX selectedMember() const;
  JsonDXX selectedRequest() const;
  void load();
  void loadDetail();
  void loadRequests();
  void clearDetail();
  void updateButtons();
  void mutation(const QString &type, JsonDXX data);
  void respond(const char *decision);
};
