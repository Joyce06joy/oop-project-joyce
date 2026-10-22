#pragma once
#include <QStyledItemDelegate>
class QListView;
// Uses the view's live viewport width for both painting and height calculation.
class MessageBubbleDelegateDXX : public QStyledItemDelegate {
 Q_OBJECT
public:
 explicit MessageBubbleDelegateDXX(QListView *view,QObject *parent=nullptr);
 void paint(QPainter *painter,const QStyleOptionViewItem &option,const QModelIndex &index)const override;
 QSize sizeHint(const QStyleOptionViewItem &option,const QModelIndex &index)const override;
private:
 QListView *view_;
 int availableWidth(const QStyleOptionViewItem &option)const;
};
