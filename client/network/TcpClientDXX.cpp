#include "TcpClientDXX.h"
#include <QDateTime>
#include <QUuid>
TcpClientDXX::TcpClientDXX(quint16 port,QObject* parent):QObject(parent),port_(port) {
    connect(&socket_,&QTcpSocket::connected,this,[this] { emit statusChanged(QStringLiteral("已连接"),true); });
    connect(&socket_,&QTcpSocket::disconnected,this,[this] { pending_.clear(); deadlines_.clear(); decoder_=ProtocolDXX(); emit statusChanged(QStringLiteral("连接丢失，请重试连接并重新登录"),false); });
    connect(&socket_,&QTcpSocket::errorOccurred,this,[this](QAbstractSocket::SocketError) { emit statusChanged(QStringLiteral("连接失败 / 断线：请先启动服务器，再重试连接"),false); emit problem(QStringLiteral("无法连接本机服务器，请检查服务器是否启动。")); });
    connect(&socket_,&QTcpSocket::readyRead,this,[this] {
        const auto bytes=socket_.readAll();
        try {
            for(const auto& r:decoder_.feed(bytes.data(),static_cast<std::size_t>(bytes.size()))) {
                if(r["kind"].text()=="event") {
                    if(!r["event_id"].isString()||r["event_id"].text().empty()||!r["type"].isString()||!r["data"].isObject()) throw std::runtime_error("invalid event");
                    const auto eventId=QString::fromStdString(r["event_id"].text());
                    if(eventIds_.contains(eventId)) continue;
                    eventIds_.insert(eventId); eventOrder_.enqueue(eventId);
                    while(eventOrder_.size()>1024) eventIds_.remove(eventOrder_.dequeue());
                    emit eventReceived(r); continue;
                }
                const auto id=QString::fromStdString(r["request_id"].text());
                if(r["kind"].text()!="response"||!r["type"].isString()||!r["request_id"].isString()) throw std::runtime_error("invalid response");
                if(!pending_.contains(id)) continue; // Expired responses cannot drive UI state.
                if(pending_[id]!=QString::fromStdString(r["type"].text())) throw std::runtime_error("response type mismatch");
                pending_.remove(id); deadlines_.remove(id); emit responseReceived(r);
            }
        } catch(...) { emit problem(QStringLiteral("服务器响应格式错误，请重新连接。")); socket_.abort(); }
    });
    timer_.setInterval(250);
    connect(&timer_,&QTimer::timeout,this,[this] {
        for(const auto& id:deadlines_.keys()) if(deadlines_.value(id)<QDateTime::currentMSecsSinceEpoch()) { const auto type=pending_.take(id); deadlines_.remove(id); emit requestTimedOut(id,type); }
    }); timer_.start();
}
TcpClientDXX::~TcpClientDXX() {
    // QTcpSocket may emit disconnected during destruction. Stop callbacks before
    // decoder/pending members and the parent window begin destruction.
    socket_.disconnect(this); socket_.abort();
}
bool TcpClientDXX::connected() const { return socket_.state()==QAbstractSocket::ConnectedState; }
void TcpClientDXX::connectServer() {
    if(connected()||socket_.state()==QAbstractSocket::ConnectingState) return;
    pending_.clear(); deadlines_.clear(); decoder_=ProtocolDXX(); socket_.abort();
    emit statusChanged(QStringLiteral("连接中…"),false); socket_.connectToHost(QStringLiteral("127.0.0.1"),port_);
}
QString TcpClientDXX::request(const QString& type,const JsonDXX& data,const QString& requestId) {
    if(!connected()) { emit problem(QStringLiteral("尚未连接服务器，请先重试连接。")); return {}; }
    const auto id=requestId.isEmpty()?QUuid::createUuid().toString(QUuid::WithoutBraces):requestId;
    if(pending_.contains(id)) return {};
    try {
        JsonDXX request{{"kind","request"},{"type",type.toStdString()},{"request_id",id.toStdString()},{"data",data}};
        const auto bytes=ProtocolDXX::frame(request);
        if(socket_.write(bytes.data(),static_cast<qint64>(bytes.size()))<0) { emit problem(QStringLiteral("请求未能发送，请重试连接。")); return {}; }
        pending_[id]=type; deadlines_[id]=QDateTime::currentMSecsSinceEpoch()+15000; return id;
    } catch(...) { emit problem(QStringLiteral("请求过大或格式有误。")); return {}; }
}
