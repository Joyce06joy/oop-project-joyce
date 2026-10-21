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
    QString request(const QString& type,const JsonDXX& data,const QString& requestId={});
signals:
    void statusChanged(const QString& status,bool online);
    void eventReceived(const JsonDXX& event);
    void responseReceived(const JsonDXX& response);
    void problem(const QString& message);
    void requestTimedOut(const QString& requestId,const QString& type);
private:
    QTcpSocket socket_;
    quint16 port_;
    ProtocolDXX decoder_;
    QHash<QString,QString> pending_;
    QHash<QString,qint64> deadlines_;
    QTimer timer_;
    QSet<QString> eventIds_;
    QQueue<QString> eventOrder_;
};
