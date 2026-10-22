#pragma once
#include "TcpClientDXX.h"
#include "MessageModelDXX.h"
#include <QWidget>
#include <QHash>
#include <QSet>
#include <functional>
#include <tuple>
class QListWidget;class QListView;class QTextEdit;class QLabel;class QPushButton;
class ChatPageDXX : public QWidget {
 Q_OBJECT
public:
 explicit ChatPageDXX(TcpClientDXX &client,QWidget *parent=nullptr);
 void setAccount(const QString &user,const QString &service,const QString &display,bool active);
 void openPrivate(const QString &target);
 void openGroup(qint64 group,qint64 subgroup=0);
 void refresh();
 void sendText(const QString &text);
 MessageModelDXX *currentModel()const;
signals:
 void messageMerged(qint64 messageId,const QString &content);
 void conversationOpened(qint64 conversationId);
private:
 using Call=std::tuple<QString,JsonDXX,std::function<void(const JsonDXX&)>,QString>;
 using Outgoing=std::tuple<QString,JsonDXX,JsonDXX,qint64>;
 TcpClientDXX &client_;QString user_,service_="QQ",display_;bool active_=false;
 quint64 openGeneration_=0;
 qint64 current_=0;JsonDXX selected_;
 QListWidget *conversations_;QListView *messages_;QTextEdit *input_;QLabel *status_,*title_,*empty_,*conversationEmpty_,*access_;QPushButton *older_,*send_;
 QHash<qint64,JsonDXX> summaries_;QHash<qint64,MessageModelDXX*> models_;QHash<qint64,QString> drafts_;QHash<qint64,int> unread_;
 QHash<QString,Call> calls_;QHash<QString,Outgoing> outgoing_;QSet<qint64> acked_;QQueue<qint64> ackQueue_;qint64 ackInFlight_=0;QHash<qint64,qint64> before_;QSet<qint64> historyBusy_;QHash<QString,qint64> cursors_;bool syncing_=false,listBusy_=false;QString next_;
 QString call(const QString &type,const JsonDXX &data,std::function<void(const JsonDXX&)> done,const QString &id={});
 MessageModelDXX *model(qint64 id);void merge(const JsonDXX &message,bool notify);void select(const JsonDXX &summary);void renderConversations();void history(bool older);void sync();void list(const QString &cursor);void retry();void drainAcks();void updateFeedback();
};
