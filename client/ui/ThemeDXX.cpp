#include "ThemeDXX.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QResource>
void ThemeDXX::apply() {
    static bool applied = false;
    if (applied || !qApp) return;
    applied = true;
    Q_INIT_RESOURCE(ui);
    auto font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(13);
    qApp->setFont(font);
    QFile theme(QStringLiteral(":/ui/theme.qss"));
    if (theme.open(QIODevice::ReadOnly)) qApp->setStyleSheet(QString::fromUtf8(theme.readAll()));
}
