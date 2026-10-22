#include "TcpClientDXX.h"
#include <QDateTime>
#include <QUuid>
TcpClientDXX::TcpClientDXX(quint16 port,QObject* parent):QObject(parent),port_(port) {
    reconnect_.setSingleShot(true);
    connect(&reconnect_, &QTimer::timeout, this, &TcpClientDXX::connectServer);
    connect(&socket_,&QTcpSocket::connected,this,[this] { emit statusChanged(QStringLiteral("已连接"),true); if(!credentials_.isNull()) { restoring_=true; restoreId_=request("auth.login", credentials_); } });
    connect(&socket_,&QTcpSocket::disconnected,this,[this] { const auto lost=pending_; pending_.clear(); deadlines_.clear(); loginRequests_.clear(); decoder_=ProtocolDXX(); for(auto i=lost.begin();i!=lost.end();++i)emit requestTimedOut(i.key(),i.value()); emit statusChanged(QStringLiteral("连接丢失，正在尝试恢复会话"),false); scheduleReconnect(); });
    connect(&socket_,&QTcpSocket::errorOccurred,this,[this](QAbstractSocket::SocketError) { emit statusChanged(QStringLiteral("连接失败 / 断线：请先启动服务器，再重试连接"),false); emit problem(QStringLiteral("无法连接本机服务器，请检查服务器是否启动。")); scheduleReconnect(); });
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
                const auto type=pending_.take(id); deadlines_.remove(id);
                if(type=="auth.login") { auto candidate=loginRequests_.take(id); if(r["success"].boolean()){credentials_=candidate; if(!restoring_)activeServices_.clear();} }
                if(type=="account.service.activate"&&r["success"].boolean()) { activeServices_.clear();for(auto s:r["data"]["services"].items())if(s["active"].boolean())activeServices_.insert(QString::fromStdString(s["service_type"].text())); }
                if(type=="auth.logout"&&r["success"].boolean()) { credentials_=JsonDXX();activeServices_.clear();reconnect_.stop();restoring_=false; }
                if(id==restoreId_) {
                    if(!r["success"].boolean()){restoring_=false;credentials_=JsonDXX();emit problem(QStringLiteral("会话恢复失败，请重新登录。"));}
                    else if(type=="auth.login"){ restoredAccount_=r["data"];restoreServices_.clear();for(auto s:activeServices_)restoreServices_.enqueue(s);restoreNext(); }
                    else {restoredAccount_.set("services",r["data"]["services"]);restoreNext();}
                    continue;
                }
                emit responseReceived(r);
            }
        } catch(...) { emit problem(QStringLiteral("服务器响应格式错误，请重新连接。")); socket_.abort(); }
    });
    timer_.setInterval(250);
    connect(&timer_,&QTimer::timeout,this,[this] {
        for(const auto& id:deadlines_.keys()) if(deadlines_.value(id)<QDateTime::currentMSecsSinceEpoch()) { const auto type=pending_.take(id); deadlines_.remove(id);loginRequests_.remove(id); emit requestTimedOut(id,type); if(id==restoreId_){restoreId_.clear();socket_.abort();} }
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
    if(type=="auth.logout"){credentials_=JsonDXX();loginRequests_.clear();activeServices_.clear();restoreServices_.clear();restoreId_.clear();restoring_=false;reconnect_.stop();}
    if(!connected()) { emit problem(QStringLiteral("尚未连接服务器，请先重试连接。")); return {}; }
    if(socket_.bytesToWrite()>1024*1024) { emit problem(QStringLiteral("发送队列已满，请稍后重试。")); return {}; }
    const auto id=requestId.isEmpty()?QUuid::createUuid().toString(QUuid::WithoutBraces):requestId;
    if(pending_.contains(id)) return {};
    try {
        JsonDXX request{{"kind","request"},{"type",type.toStdString()},{"request_id",id.toStdString()},{"data",data}};
        const auto bytes=ProtocolDXX::frame(request);
        if(socket_.write(bytes.data(),static_cast<qint64>(bytes.size()))<0) { emit problem(QStringLiteral("请求未能发送，请重试连接。")); return {}; }
        if(type=="auth.login")loginRequests_[id]=data;
        pending_[id]=type; deadlines_[id]=QDateTime::currentMSecsSinceEpoch()+15000; return id;
    } catch(...) { emit problem(QStringLiteral("请求过大或格式有误。")); return {}; }
}

void TcpClientDXX::scheduleReconnect(){
 if(credentials_.isNull()||reconnect_.isActive()||connected())return;
 if(reconnectAttempt_>=5){restoring_=false;emit statusChanged(QStringLiteral("自动重连已停止，请点击重试连接"),false);return;}
 restoring_=true;reconnect_.start(qMin(8000,500*(1<<reconnectAttempt_)));++reconnectAttempt_;
}
void TcpClientDXX::restoreNext(){
 if(!restoreServices_.isEmpty()){auto service=restoreServices_.dequeue();restoreId_=request("account.service.activate",{{"service_type",service.toStdString()}});return;}
 restoreId_.clear();restoring_=false;reconnectAttempt_=0;emit statusChanged(QStringLiteral("连接与服务会话已恢复"),true);emit sessionRestored(restoredAccount_);
}
