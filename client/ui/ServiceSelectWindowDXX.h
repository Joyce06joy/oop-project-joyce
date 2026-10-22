#pragma once
#include "TcpClientDXX.h"
#include <QWidget>
class QLabel;
class WorkspaceDXX;
class QHBoxLayout;
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
    QHBoxLayout* list_;
    QString pendingId_;
    QString selected_;
    JsonDXX account_;
    WorkspaceDXX* workspace_;
};
