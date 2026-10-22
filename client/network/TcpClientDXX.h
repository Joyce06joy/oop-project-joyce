#pragma once
#include "ProtocolDXX.h"
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QQueue>
class TcpClientDXX : public QObject {
    Q_OBJECT
public:
    explicit TcpClientDXX(quint16 port=8888,QObject* parent=nullptr);
    ~TcpClientDXX() override;
    void connectServer();
    bool connected() const;
    bool restoringSession() const { return restoring_; }
    QString request(const QString& type,const JsonDXX& data,const QString& requestId={});
signals:
    void statusChanged(const QString& status,bool online);
    void eventReceived(const JsonDXX& event);
    void responseReceived(const JsonDXX& response);
    void sessionRestored(const JsonDXX &account);
    void problem(const QString& message);
    void requestTimedOut(const QString& requestId,const QString& type);
private:
    QTcpSocket socket_;
    QTimer reconnect_;
    int reconnectAttempt_=0;
    bool restoring_=false;
    JsonDXX credentials_, candidate_, restoredAccount_;
    QHash<QString,JsonDXX> loginRequests_;
    QSet<QString> activeServices_;
    QString restoreId_;
    QQueue<QString> restoreServices_;
    void scheduleReconnect();
    void restoreNext();
    quint16 port_;
    ProtocolDXX decoder_;
    QHash<QString,QString> pending_;
    QHash<QString,qint64> deadlines_;
    QTimer timer_;
    QSet<QString> eventIds_;
    QQueue<QString> eventOrder_;
};
