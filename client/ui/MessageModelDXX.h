#pragma once
#include "ProtocolDXX.h"
#include <QAbstractListModel>
#include <vector>
class MessageModelDXX : public QAbstractListModel {
 Q_OBJECT
public:
 explicit MessageModelDXX(QObject *parent=nullptr):QAbstractListModel(parent){}
 int rowCount(const QModelIndex &parent={}) const override;
 QVariant data(const QModelIndex &index,int role) const override;
 bool merge(const JsonDXX &message);
 void setState(const QString &request,const QString &state);
 void delivery(qint64 id,qint64 delivered,qint64 recipients);
 void clear();
 const std::vector<JsonDXX>& messages()const{return rows_;}
private:
 std::vector<JsonDXX> rows_;
};
