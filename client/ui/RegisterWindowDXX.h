#pragma once
#include "TcpClientDXX.h"
#include <QWidget>
class QLineEdit;
class QDateEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class RegisterWindowDXX : public QWidget {
    Q_OBJECT
public:
    explicit RegisterWindowDXX(TcpClientDXX& client,QWidget* parent=nullptr);
private:
    TcpClientDXX& client_;
    QLineEdit *name_,*nickname_,*password_,*confirm_,*location_;
    QDateEdit* birthday_;
    QCheckBox *hasBirthday_,*qq_,*wechat_,*weibo_;
    QLabel *message_,*result_;
    QPushButton *submit_,*copy_;
    QWidget* fields_;
    QString requestId_;
    JsonDXX payload_;
    bool completed_=false;
    void submit();
    void uncertain();
};
