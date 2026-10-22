#include "GroupPolicyDialogDXX.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
GroupPolicyDialogDXX::GroupPolicyDialogDXX(const QString &current, qint64 members, QWidget *parent)
    : QDialog(parent) {
  setObjectName("groupPolicyDialog");
  setWindowTitle(QStringLiteral("切换群管理模式"));
  auto *layout = new QVBoxLayout(this);
  auto *impact = new QLabel(QStringLiteral("现有 %1 位成员与群主保持不变，群所属平台不变。\n"
      "微信规则下原管理员权限与子群暂停，切回 QQ 后恢复。\n"
      "全部待决申请与邀请将取消，切回时不会自动恢复。" ).arg(members));
  impact->setObjectName("groupPolicyImpact");
  impact->setWordWrap(true);
  layout->addWidget(impact);
  choice_ = new QComboBox;
  choice_->setObjectName("groupPolicyChoice");
  choice_->addItems({"QQ", "WECHAT"});
  choice_->setCurrentText(current == "QQ" ? "WECHAT" : "QQ");
  layout->addWidget(choice_);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->setObjectName("groupPolicyButtons");
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    if (QMessageBox::question(this, QStringLiteral("再次确认切换"),
        QStringLiteral("确认按以上影响切换到 %1 规则？待决请求将取消。" ).arg(targetPolicy())) == QMessageBox::Yes)
      accept();
  });
}
QString GroupPolicyDialogDXX::targetPolicy() const { return choice_->currentText(); }
