#include "LoginWindowDXX.h"
#include <QApplication>
#include <QCommandLineParser>
int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setApplicationName("IMClient");
    QCommandLineParser parser; parser.addHelpOption(); parser.addOption({"port","本机服务器端口","port","8888"}); parser.process(app);
    bool ok=false; const auto port=parser.value("port").toInt(&ok); if(!ok||port<1||port>65535) return 2;
    LoginWindowDXX window(static_cast<quint16>(port)); window.show(); return app.exec();
}
