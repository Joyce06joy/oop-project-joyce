#pragma once
#include "RequestPanelDXX.h"
class FriendPageDXX;
class FriendRequestPageDXX;
class RecommendationPageDXX;
class AccountPageDXX;
class QStackedWidget;
class QTabWidget;
class QLabel;
class QTimer;
class WorkspaceDXX : public RequestPanelDXX {
  Q_OBJECT
public:
  explicit WorkspaceDXX(TcpClientDXX &client);
  void showAccount(const JsonDXX &account);
signals:
  void servicesChanged(const JsonDXX &services);

private:
  QString service_ = "QQ", user_;
  JsonDXX services_;
  QLabel *gate_;
  QTabWidget *tabs_;
  FriendPageDXX *friends_;
  FriendRequestPageDXX *requests_;
  RecommendationPageDXX *recommendations_;
  AccountPageDXX *accounts_;
  bool active() const;
  void selectService(const QString &service);
  void refreshCurrent();
  void reloadServices();
  QTimer *refreshTimer_;
  bool accountEvent_ = false;
  void scheduleRefresh(bool accounts = false);
};
