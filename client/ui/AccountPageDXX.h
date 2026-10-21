#pragma once
#include "RequestPanelDXX.h"
class QLabel;
class QComboBox;
class AccountPageDXX : public RequestPanelDXX {
  Q_OBJECT
public:
  explicit AccountPageDXX(TcpClientDXX &client);
  void refresh();
  void showServices(const JsonDXX &services);
signals:
  void servicesChanged(const JsonDXX &services);

private:
  QLabel *services_, *binding_;
  QComboBox *choice_;
};
