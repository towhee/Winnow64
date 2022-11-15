#include "Main/mainwindow.h"
#include <QApplication>
#include "qtsingleapplication.h"

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

//    /* Single instance version
//    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
                Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QtSingleApplication instance("Winnow", argc, argv);

    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        args += argv[i];
        if (i < argc - 1) args += delimiter;
    }
//    Utilities::log("WinnowMain", args);

    // instance already running
    if (instance.sendMessage(args)) {
//        Utilities::log(CLASSFUNCTION, "Instance already running");
        return 0;
    }

    // start program
    QCoreApplication::addLibraryPath("./");
    MW mw(args);
    mw.show();

    // connect message when instance already running
    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
             &mw, SLOT(handleStartupArgs(const QString&)));

    instance.setActivationWindow(&mw, false);
    QObject::connect(&mw, SIGNAL(needToShow()), &instance, SLOT(activateWindow()));

    return instance.exec();             //    return QApp.exec();
    //*/
}
