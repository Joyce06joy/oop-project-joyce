#ifndef MAINWINDOWDXX_H
#define MAINWINDOWDXX_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindowDXX;
}
QT_END_NAMESPACE

class MainWindowDXX : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindowDXX(QWidget *parent = nullptr);
    ~MainWindowDXX() override;

private:
    Ui::MainWindowDXX *ui;
};
#endif // MAINWINDOWDXX_H
