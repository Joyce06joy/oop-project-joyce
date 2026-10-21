#pragma once
#include "RequestPanelDXX.h"
class QListWidget;
class FriendRequestPageDXX : public RequestPanelDXX {
  Q_OBJECT
public:
  explicit FriendRequestPageDXX(TcpClientDXX &client);
  void setService(const QString &service);
  void refresh();

private:
  QString service_ = "QQ", cursor_, next_;
  QListWidget *list_;
  std::vector<JsonDXX> items_;
  void load();
  void respond(bool accept);
};
