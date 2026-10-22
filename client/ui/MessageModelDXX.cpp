#include "MessageModelDXX.h"
#include <algorithm>
int MessageModelDXX::rowCount(const QModelIndex &p)const{return p.isValid()?0:int(rows_.size());}
QVariant MessageModelDXX::data(const QModelIndex &i,int role)const{
 if(!i.isValid()||i.row()<0||i.row()>=int(rows_.size()))return {};
 const auto &m=rows_[i.row()];
 if(role==Qt::UserRole)return QString::fromStdString(m["request_id"].text());
 if(role==Qt::UserRole+1)return qlonglong(m["message_id"].integer());
 if(role!=Qt::DisplayRole)return {};
 QString state=QString::fromStdString(m["status"].text());if(m["delivered_count"].integer()>0)state+=QStringLiteral(" · delivered %1/%2").arg(m["delivered_count"].integer()).arg(m["recipient_count"].integer());
 return QStringLiteral("%1 · %2 · %3\n%4").arg(QString::fromStdString(m["sender_display_id"].text()),QString::fromStdString(m["created_at"].text()),state,QString::fromStdString(m["content"].text()));
}
bool MessageModelDXX::merge(const JsonDXX &m){
 bool added=true;beginResetModel();
 for(auto &r:rows_)if((m["message_id"].integer()>0&&r["message_id"].integer()==m["message_id"].integer())||(!m["request_id"].text().empty()&&r["request_id"].text()==m["request_id"].text()&&r["sender_display_id"].text()==m["sender_display_id"].text())){
  auto oldDelivered=r["delivered_count"].integer();auto oldRecipients=r["recipient_count"].integer();auto oldState=r["status"].text();auto delivered=oldState.find("delivered")==0;r=m;if(delivered)r.set("status",oldState);r.set("delivered_count",std::max(oldDelivered,r["delivered_count"].integer()));r.set("recipient_count",std::max(oldRecipients,r["recipient_count"].integer()));added=false;break;
 }
 if(added)rows_.push_back(m);
 std::stable_sort(rows_.begin(),rows_.end(),[](const JsonDXX&a,const JsonDXX&b){auto x=a["message_id"].integer(),y=b["message_id"].integer();return x>0&&(y==0||x<y);});
 endResetModel();return added;
}
void MessageModelDXX::setState(const QString &id,const QString &s){for(int i=0;i<int(rows_.size());++i)if(QString::fromStdString(rows_[i]["request_id"].text())==id){rows_[i].set("status",s.toStdString());emit dataChanged(index(i),index(i));}}
void MessageModelDXX::clear(){beginResetModel();rows_.clear();endResetModel();}

void MessageModelDXX::delivery(qint64 id,qint64 delivered,qint64 recipients){for(int i=0;i<int(rows_.size());++i)if(rows_[i]["message_id"].integer()==id){rows_[i].set("delivered_count",std::max<std::int64_t>(rows_[i]["delivered_count"].integer(),delivered));rows_[i].set("recipient_count",std::max<std::int64_t>(rows_[i]["recipient_count"].integer(),recipients));emit dataChanged(index(i),index(i));}}
