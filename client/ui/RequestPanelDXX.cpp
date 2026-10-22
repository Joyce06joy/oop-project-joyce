#include "RequestPanelDXX.h"
#include <QLabel>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QStyle>
namespace {
QString readKey(const QString &type) {
  return type.startsWith("friend.") ? QStringLiteral("friend.query") : type;
}
} // namespace
RequestPanelDXX::RequestPanelDXX(TcpClientDXX &client, QWidget *parent)
    : QWidget(parent), client_(client) {
  layout = new QVBoxLayout(this);
  layout->setContentsMargins(8,8,8,8); layout->setSpacing(8);
  body = new QWidget;
  layout->addWidget(body,1);
  status = new QLabel;
  status->setObjectName("pageStatus");
  status->setWordWrap(true);
  status->setTextFormat(Qt::PlainText);
  layout->addWidget(status);
  setState(QStringLiteral("idle"));
  retry_ = new QPushButton(QStringLiteral("重试原操作"));
  retry_->setObjectName("retryMutation");
  retry_->hide();
  layout->addWidget(retry_);
  connect(retry_, &QPushButton::clicked, this, [this] {
    if (mutationId_.isEmpty())
      return;
    const auto call = calls_.value(mutationId_);
    if (!client_.request(std::get<0>(call), std::get<1>(call), mutationId_)
             .isEmpty()) {
      retry_->hide();
      setState(QStringLiteral("loading"));
      status->setText(QStringLiteral("正在重试原操作…"));
    }
  });
  connect(
      &client_, &TcpClientDXX::responseReceived, this,
      [this](const JsonDXX &r) {
        const auto id = QString::fromStdString(r["request_id"].text());
        if (!calls_.contains(id))
          return;
        const auto call = calls_.value(id);
        if (!std::get<3>(call)) {
          const auto key = readKey(std::get<0>(call));
          readIds_.remove(key);
          if (queuedReads_.contains(key)) {
            const auto queued = queuedReads_.take(key);
            calls_.remove(id);
            read(std::get<0>(queued), std::get<1>(queued), std::get<2>(queued));
            return;
          }
          if (mutationPending()) {
            calls_.remove(id);
            return;
          }
        }
        if (!r["success"].boolean()) {
          const auto code = r["error"]["code"].text();
          setState(QStringLiteral("error"));
          status->setText(QString::fromStdString(r["error"]["message"].text()));
          if (std::get<3>(call) &&
              (code == "DATABASE_ERROR" || code == "SERVER_ERROR" ||
               code == "AUTH_REQUIRED" || code == "SERVICE_NOT_ACTIVE")) {
            uncertain();
            if (code == "AUTH_REQUIRED")
              emit authenticationRequired();
            return;
          }
          calls_.remove(id);
          if (std::get<3>(call)) {
            mutationId_.clear();
            body->setEnabled(true);
            retry_->hide();
          }
          return;
        }
        calls_.remove(id);
        if (std::get<3>(call)) {
          mutationId_.clear();
          body->setEnabled(true);
          retry_->hide();
        }
        setState(QStringLiteral("success"));
        status->setText(QStringLiteral("已更新"));
        std::get<2>(call)(r["data"]);
        if (std::get<3>(call))
          emit changed();
      });
  connect(&client_, &TcpClientDXX::requestTimedOut, this,
          [this](const QString &id, const QString &) {
            if (id == mutationId_)
              uncertain();
            else if (calls_.contains(id)) {
              const auto type = std::get<0>(calls_.take(id));
              setState(QStringLiteral("error"));
              status->setText(QStringLiteral("查询超时，输入已保留，请刷新。"));
              finishRead(type);
            }
          });
  connect(&client_, &TcpClientDXX::statusChanged, this,
          [this](const QString &, bool online) {
            if (!online) {
              setState(QStringLiteral("offline"));
              status->setText(QStringLiteral("连接已断开，输入已保留。请在顶部重试连接。"));
              readIds_.clear();
              queuedReads_.clear();
              for (const auto &id : calls_.keys())
                if (id != mutationId_)
                  calls_.remove(id);
              if (!mutationId_.isEmpty())
                uncertain();
            }
          });
}
void RequestPanelDXX::uncertain() {
  setState(QStringLiteral("unknown"));
  body->setEnabled(false);
  retry_->show();
  status->setText(QStringLiteral("操作结果尚未确认，输入已保留。请恢复连接、登"
                                 "录并激活服务后重试原操作。"));
}
bool RequestPanelDXX::mutationPending() const { return !mutationId_.isEmpty(); }
void RequestPanelDXX::resetForUser(const QString &user) {
  if (user_ != user) {
    calls_.clear();
    readIds_.clear();
    queuedReads_.clear();
    mutationId_.clear();
    body->setEnabled(true);
    retry_->hide();
    status->clear();
    setState(QStringLiteral("idle"));
    user_ = user;
  }
}
void RequestPanelDXX::read(const QString &type, const JsonDXX &data,
                           std::function<void(const JsonDXX &)> done) {
  if (mutationPending())
    return;
  // Keep one read in flight per query family and retain only its newest
  // successor. Binding and service-list reads are independent and must not
  // cancel each other.
  const auto key = readKey(type);
  if (readIds_.contains(key)) {
    queuedReads_.insert(key, Call{type, data, std::move(done), false});
    setState(QStringLiteral("loading"));
  status->setText(QStringLiteral("加载中…"));
    return;
  }
  const auto id = client_.request(type, data);
  if (id.isEmpty()) {
    setState(QStringLiteral("offline"));
    status->setText(QStringLiteral("尚未连接，输入已保留。"));
    return;
  }
  calls_.insert(id, Call{type, data, std::move(done), false});
  readIds_.insert(key, id);
  setState(QStringLiteral("loading"));
  status->setText(QStringLiteral("加载中…"));
}
void RequestPanelDXX::mutate(const QString &type, const JsonDXX &data,
                             std::function<void(const JsonDXX &)> done) {
  if (mutationPending())
    return;
  mutationId_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  calls_.insert(mutationId_, Call{type, data, std::move(done), true});
  body->setEnabled(false);
  setState(QStringLiteral("loading"));
  status->setText(QStringLiteral("处理中…"));
  if (client_.request(type, data, mutationId_).isEmpty())
    uncertain();
}

void RequestPanelDXX::finishRead(const QString &type) {
  const auto key = readKey(type);
  readIds_.remove(key);
  if (queuedReads_.contains(key)) {
    const auto queued = queuedReads_.take(key);
    read(std::get<0>(queued), std::get<1>(queued), std::get<2>(queued));
  }
}

void RequestPanelDXX::setState(const QString &state) {
  setProperty("state",state);
  status->setProperty("state",state);
  status->style()->unpolish(status); status->style()->polish(status);
}
void RequestPanelDXX::enableBodyScrolling() {
  if (body->parentWidget() && qobject_cast<QScrollArea*>(body->parentWidget()->parentWidget())) return;
  layout->removeWidget(body);
  auto *scroll = new QScrollArea(this);
  scroll->setObjectName("managementScroll");
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(body);
  layout->insertWidget(0,scroll,1);
}
