#pragma once
#include "TcpClientDXX.h"
#include <QMainWindow>
class QLineEdit;
class QLabel;
class QPushButton;
class QStackedWidget;
class RegisterWindowDXX;
class ServiceSelectWindowDXX;
class LoginWindowDXX : public QMainWindow {
    Q_OBJECT
public:
    explicit LoginWindowDXX(quint16 port=8888,QWidget* parent=nullptr);
private:
    TcpClientDXX client_;
    QLineEdit *name_,*password_;
    QLabel *status_,*message_;
    QPushButton* login_;
    QStackedWidget* pages_;
    RegisterWindowDXX* registration_;
    ServiceSelectWindowDXX* services_;
    QString pendingId_;
};
