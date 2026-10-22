#pragma once
#include "TcpClientDXX.h"
#include <QHash>
#include <QWidget>
#include <functional>
#include <tuple>
class QLabel;
class QPushButton;
class QVBoxLayout;
// Per-page asynchronous requests; an uncertain mutation retains its exact
// envelope.
class RequestPanelDXX : public QWidget {
  Q_OBJECT
public:
  explicit RequestPanelDXX(TcpClientDXX &client, QWidget *parent = nullptr);
  void read(const QString &type, const JsonDXX &data,
            std::function<void(const JsonDXX &)> done);
  void mutate(const QString &type, const JsonDXX &data,
              std::function<void(const JsonDXX &)> done);
  bool mutationPending() const;
  void resetForUser(const QString &user);
  void enableBodyScrolling();
  void setState(const QString &state);
  QLabel *status;
  QWidget *body;
  QVBoxLayout *layout;
signals:
  void changed();
  void authenticationRequired();

private:
  using Call =
      std::tuple<QString, JsonDXX, std::function<void(const JsonDXX &)>, bool>;
  TcpClientDXX &client_;
  QHash<QString, Call> calls_;
  QHash<QString, QString> readIds_;
  QHash<QString, Call> queuedReads_;
  void finishRead(const QString &type);
  QString mutationId_, user_;
  QPushButton *retry_;
  void uncertain();
};
