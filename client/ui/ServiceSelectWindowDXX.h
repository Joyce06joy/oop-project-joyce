#pragma once
#include "TcpClientDXX.h"
#include <QWidget>
class QLabel;
class QVBoxLayout;
class ServiceSelectWindowDXX : public QWidget {
    Q_OBJECT
public:
    explicit ServiceSelectWindowDXX(TcpClientDXX& client,QWidget* parent=nullptr);
    void showAccount(const JsonDXX& account);
signals:
    void loggedOut();
private:
    TcpClientDXX& client_;
    QLabel* profile_;
    QLabel* message_;
    QWidget* choices_;
    QVBoxLayout* list_;
    QString pendingId_;
    QString selected_;
    JsonDXX account_;
};
