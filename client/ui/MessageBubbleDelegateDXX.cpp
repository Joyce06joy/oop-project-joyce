#include "MessageBubbleDelegateDXX.h"
#include "MessageModelDXX.h"
#include <QListView>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>
#include <QStyle>
#include <algorithm>
#include <cmath>
namespace {
// QTextLayout consumes plain Unicode and never interprets user text as markup.
qreal layoutText(QPainter *p,const QString &text,const QFont &font,qreal width,const QPointF &origin={}){
 qreal height=0;
 const auto paragraphs=text.split(QChar('\n'));
 for(const auto &paragraph:paragraphs){
  QTextLayout layout(paragraph.isEmpty()?QStringLiteral(" "):paragraph,font);
  QTextOption wrapping;wrapping.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);layout.setTextOption(wrapping);
  layout.beginLayout();
  while(true){auto line=layout.createLine();if(!line.isValid())break;line.setLineWidth(std::max(qreal(1),width));line.setPosition({0,height});height+=line.height();}
  layout.endLayout();if(p)layout.draw(p,origin);
 }
 return height;
}
QString header(const QModelIndex &i){
 auto time=i.data(MessageModelDXX::TimestampRole).toString();
 return i.data(MessageModelDXX::SenderRole).toString()+(time.isEmpty()?QString():QStringLiteral(" · ")+time);
}
QString footer(const QModelIndex &i){
 auto delivery=i.data(MessageModelDXX::DeliveryRole).toString();
 return i.data(MessageModelDXX::StateRole).toString()+(delivery.isEmpty()?QString():QStringLiteral(" · ")+delivery);
}
QFont secondaryFont(QFont font){font.setPointSizeF(std::max(9.0,font.pointSizeF()-1.0));return font;}
}
MessageBubbleDelegateDXX::MessageBubbleDelegateDXX(QListView *view,QObject *parent):QStyledItemDelegate(parent),view_(view){}
int MessageBubbleDelegateDXX::availableWidth(const QStyleOptionViewItem &o)const{return std::max(1,view_?view_->viewport()->width():o.rect.width());}
QSize MessageBubbleDelegateDXX::sizeHint(const QStyleOptionViewItem &o,const QModelIndex &i)const{
 const int width=availableWidth(o),bubble=std::max(1,std::min(580,int((width-24)*0.86))),inside=std::max(1,bubble-24);
 const auto small=secondaryFont(o.font);
 const qreal h=layoutText(nullptr,header(i),small,inside)+layoutText(nullptr,i.data(MessageModelDXX::ContentRole).toString(),o.font,inside)+layoutText(nullptr,footer(i),small,inside)+40;
 return {width,int(std::ceil(h))};
}
void MessageBubbleDelegateDXX::paint(QPainter *p,const QStyleOptionViewItem &o,const QModelIndex &i)const{
 p->save();p->setClipRect(o.rect);p->setRenderHint(QPainter::Antialiasing);
 const bool own=i.data(MessageModelDXX::OwnMessageRole).toBool();
 const int width=availableWidth(o),bubble=std::max(1,std::min(580,int((width-24)*0.86)));
 QRectF box(o.rect.x()+(own?width-bubble-12:12),o.rect.y()+6,bubble,o.rect.height()-12);
 const bool selected=o.state.testFlag(QStyle::State_Selected);
 p->setPen(selected?QPen(QColor("#2264B0"),2):Qt::NoPen);
 p->setBrush(own?QColor("#2264B0"):QColor("#E9EFF7"));p->drawRoundedRect(box,10,10);
 auto small=secondaryFont(o.font);const qreal inside=std::max(qreal(1),box.width()-24);qreal y=box.top()+10;
 p->setPen(own?QColor("#FFFFFF"):QColor("#52657C"));
 y+=layoutText(p,header(i),small,inside,{box.left()+12,y})+6;
 p->setPen(own?QColor("#FFFFFF"):QColor("#172B46"));
 y+=layoutText(p,i.data(MessageModelDXX::ContentRole).toString(),o.font,inside,{box.left()+12,y})+6;
 p->setPen(own?QColor("#FFFFFF"):QColor("#52657C"));
 layoutText(p,footer(i),small,inside,{box.left()+12,y});
 if(o.state.testFlag(QStyle::State_HasFocus)){p->setBrush(Qt::NoBrush);p->setPen(QPen(own?QColor("#FFFFFF"):QColor("#2264B0"),1,Qt::DashLine));p->drawRoundedRect(box.adjusted(3,3,-3,-3),8,8);}
 p->restore();
}
