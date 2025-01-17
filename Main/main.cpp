#include "Main/mainwindow.h"
#include <QApplication>
#include "qtsingleapplication.h"
#include "Utilities/mac.h"

static QString rory = "Rory";

int main(int argc, char *argv[])
{
    /* Original multi instance version
    QApplication a(argc, argv);
    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        args += argv[i];
        if (i < argc - 1) args += delimiter;
    }
    QCoreApplication::addLibraryPath("./");

    MW mw(args);
    mw.show();
    return a.exec();
    //*/

    /* Single instance version
       see https://github.com/itay-grudev/SingleApplication if replacement for
       QtSingleApplication required
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
                Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    //*/

    // /*Single instance version
    QtSingleApplication instance("Winnow", argc, argv);

    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        args += argv[i];
        if (i < argc - 1) args += delimiter;
    }

    // terminate if Winnow already open and no arguments to pass
    if (args == "" && instance.isRunning()) {
        QString msg = "Winnow or a Winnow report is open.";
        // G::popUp->showPopup(msg);
        return 0;
    }

    // instance already running
    if (instance.sendMessage(args)) {
        if (G::isFileLogger) Utilities::log("WinnowMain", "Instance already running");
        QString msg = "Winnow or a Winnow report is open.";
        // G::popUp->showPopup(msg);
        return 0;
    }

#ifdef Q_OS_MAC
    Mac::initializeAppDelegate();
    // MyApplicationDelegate *delegate = [[MyApplicationDelegate alloc] init];
    // [NSApp setDelegate:delegate];
#endif

    // start program
    QCoreApplication::addLibraryPath("./");
    MW mw(args);

    // // hide root in FSTree after loaded
    // QObject::connect(&instance, &QGuiApplication::applicationStateChanged,
    //                  &mw, &MW::whenActivated);

    mw.show();

    // connect message when instance already running
    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
             &mw, SLOT(handleStartupArgs(const QString&)));

    instance.setActivationWindow(&mw, false);

    // not being used
    QObject::connect(&mw, SIGNAL(needToShow()), &instance, SLOT(activateWindow()));

    // // hide root in FSTree after loaded
    // QObject::connect(&instance, &QGuiApplication::applicationStateChanged,
    //                  &mw, &MW::whenActivated);

    return instance.exec();
    //*/
}
