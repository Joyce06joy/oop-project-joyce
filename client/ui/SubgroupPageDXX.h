#pragma once
#include "RequestPanelDXX.h"
class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;
class SubgroupPageDXX : public RequestPanelDXX {
  Q_OBJECT
public:
  explicit SubgroupPageDXX(TcpClientDXX &client);
  void setGroup(qint64 id, bool enabled, bool canCreate);
  void refresh();
  void clearGroup();
signals:
  void chatRequested(qint64 group,qint64 subgroup);
private:
  qint64 group_ = 0, subgroup_ = 0;
  bool enabled_ = false, create_ = false;
  QString cursor_, next_, memberCursor_, memberNext_;
  QLineEdit *name_, *targets_;
  QListWidget *groups_, *members_;
  QLabel *summary_;
  QPushButton *createButton_, *add_, *remove_;
  std::vector<JsonDXX> items_;
  JsonDXX selected() const;
  void load();
  void loadMembers();
  void updateMembers(bool add);
};
