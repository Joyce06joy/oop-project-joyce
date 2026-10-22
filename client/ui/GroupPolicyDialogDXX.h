#pragma once
#include <QDialog>
class QComboBox;
class GroupPolicyDialogDXX : public QDialog {
  Q_OBJECT
public:
  GroupPolicyDialogDXX(const QString &current, qint64 members, QWidget *parent = nullptr);
  QString targetPolicy() const;
private:
  QComboBox *choice_;
};
