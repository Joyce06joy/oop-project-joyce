#pragma once
#include "RequestPanelDXX.h"
class QLineEdit;
class QListWidget;
class FriendPageDXX : public RequestPanelDXX {
  Q_OBJECT
public:
  explicit FriendPageDXX(TcpClientDXX &client);
  void setService(const QString &service);
  void refresh();
  void resetView();

signals:
  void chatRequested(const QString &target);
private:
  QString service_ = "QQ", cursor_, next_;
  bool searching_ = false;
  QLineEdit *query_, *message_, *remark_;
  QListWidget *results_;
  std::vector<JsonDXX> items_;
  JsonDXX selected() const;
  void load();
};
