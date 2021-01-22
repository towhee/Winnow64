#include "Main/mainwindow.h"
#include <QApplication>
#include "qtsingleapplication.h"

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QtSingleApplication instance("Winnow", argc, argv);

    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        args += argv[i];
        if (i < argc - 1) args += delimiter;
    }

    // instance already running
    if (instance.sendMessage(args)) {
        /* log
        Utilities::log(__FUNCTION__, "Instance already running");
//        */
        return 0;
    }

    // start program
    QCoreApplication::addLibraryPath("./");
    /* log
    Utilities::log(__FUNCTION__, "Start Winnow");
//    */
    MW mw(args);
    mw.show();

    // connect message when instance already running
    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
             &mw, SLOT(handleStartupArgs(const QString&)));

    instance.setActivationWindow(&mw, false);
    QObject::connect(&mw, SIGNAL(needToShow()), &instance, SLOT(activateWindow()));

    return instance.exec();             //    return QApp.exec();
}
