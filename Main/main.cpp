#include "Main/mainwindow.h"
#include <QApplication>
#include "qtsingleapplication.h"

// see https://github.com/itay-grudev/SingleApplication if replacement for
// QtSingleApplication required

int main(int argc, char *argv[])
{
    /* Original multi instance version
    QApplication QApp(argc, argv);
        if (QCoreApplication::arguments().size() > 2)
        {
            qDebug() << QObject::tr("Usage: Winnow [FILE or DIRECTORY]...");
            return -1;
        }
        QCoreApplication::addLibraryPath("./");
        MW MW;
        MW.show();
        return QApp.exec();
        //*/

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
                Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QtSingleApplication instance("Winnow", argc, argv);

    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        args += argv[i];
        if (i < argc - 1) args += delimiter;
    }
    // if (G::isFileLogger) Utilities::log("WinnowMain", args);

    // terminate if Winnow already open and no arguments to pass
    if (args == "" && instance.isRunning()) return 0;

    // instance already running
    if (instance.sendMessage(args)) {
        if (G::isFileLogger) Utilities::log("WinnowMain", "Instance already running");
        //qDebug() << "main" << "instance already running, sent args";
        return 0;
    }

    // start program
    QCoreApplication::addLibraryPath("./");
    MW mw(args);

    // connect message when instance already running
    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
             &mw, SLOT(handleStartupArgs(const QString&)));

    instance.setActivationWindow(&mw, false);
    QObject::connect(&mw, SIGNAL(needToShow()), &instance, SLOT(activateWindow()));

    // if (G::isFileLogger) Utilities::log("WinnowMain", "Start new instance");

    return instance.exec();             //    return QApp.exec();
}
