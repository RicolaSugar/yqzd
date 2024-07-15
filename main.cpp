#include "MainWindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    {
        const auto id = QFontDatabase::addApplicationFont(":/yahei.ttf");
        const auto fonts = QFontDatabase::applicationFontFamilies(id);
        qDebug()<<"fonts: "<<fonts;
    }

    {
        const auto id = QFontDatabase::addApplicationFont(":/yuanti.ttf");
        const auto fonts = QFontDatabase::applicationFontFamilies(id);
        qDebug()<<"fonts: "<<fonts;
    }

    {
        const auto id = QFontDatabase::addApplicationFont(":SourceHanSansCN-Normal.ttf");
        const auto fonts = QFontDatabase::applicationFontFamilies(id);
        qDebug()<<"fonts: "<<fonts;
    }


    MainWindow w;
    w.show();
    return a.exec();
}
