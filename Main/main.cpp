#include "Main/mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication QApp(argc, argv);
    if (QCoreApplication::arguments().size() > 2)
    {
        qDebug() << G::t.restart() << "\t" << QObject::tr("Usage: Winnow [FILE or DIRECTORY]...");
        return -1;
    }

    MW MW;
    MW.show();
    return QApp.exec();
}
