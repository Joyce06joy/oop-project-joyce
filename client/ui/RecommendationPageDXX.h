#pragma once
#include "RequestPanelDXX.h"
class QComboBox;
class QLineEdit;
class QListWidget;
class RecommendationPageDXX : public RequestPanelDXX {
  Q_OBJECT
public:
  explicit RecommendationPageDXX(TcpClientDXX &client);
  void setService(const QString &service);
  void refresh();
signals:
  void settingsRequested();

private:
  QComboBox *source_, *target_, *mode_;
  QLineEdit *mutualTarget_;
  QListWidget *list_;
  std::vector<JsonDXX> items_;
  QString cursor_, next_;
  void load();
};
