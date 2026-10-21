#include "mainwindowdxx.h"
#include "./ui_mainwindowdxx.h"

MainWindowDXX::MainWindowDXX(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowDXX)
{
    ui->setupUi(this);
}

MainWindowDXX::~MainWindowDXX()
{
    delete ui;
}
