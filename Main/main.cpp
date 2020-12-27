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
        return 0;
    }

    // start program
    QCoreApplication::addLibraryPath("./");
    MW mw;
    mw.handleStartupArgs(args);
    mw.show();

    // connect message when instance already running
    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
             &mw, SLOT(handleStartupArgs(const QString&)));

    instance.setActivationWindow(&mw, false);
    QObject::connect(&mw, SIGNAL(needToShow()), &instance, SLOT(activateWindow()));

    return instance.exec();             //    return QApp.exec();
}

/*
#include "Main/mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
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
}
*/

/*
QCommandLineParser parser;
const QCommandLineOption openFolderOption("o", "Open <folder>.", "folder");
parser.addOption(openFolderOption);
const QCommandLineOption exportOption("e", "Export <file>.", "file");
parser.addOption(exportOption);
const QCommandLineOption templateOption("t", "Use template <template>.", "template");
parser.addOption(templateOption);
parser.process(QApp);
msg += "\n";
msg += "o: " + parser.value(openFolderOption) + "\n";
msg += "e: " + parser.value(exportOption) + "\n";
msg += "t: " + parser.value(templateOption) + "\n";
QMessageBox::information(nullptr, "Args", msg, QMessageBox::Ok);
*/

/*
"D:/My Projects/Winnow Project/Winnow64/release/winnow.exe" -o "c:/users/rory" -e "c:/users/rory/wizard with a dash.jpg" -t Zen2048
"D:/My Projects/Winnow Project/Winnow64/release/winnow.exe" c:/users/rory
"D:/My Projects/Winnow Project/Winnow64/release/winnow.exe" -e D:/Pictures/test_export/2020-11-25_0007.jpg -t Zen2048
"D:/My Projects/Winnow Project/Winnow64/release/winnow_zen2048.exe" -e D:/Pictures/test_export/2020-11-25_0007.jpg -t Zen2048
"D:/Documents/Bat/bat2exe/winnow_zen2048.exe" D:/Pictures/test_export/2020-11-25_0007.jpg
"D:/My Projects/Winnow Project/Winnow64/release/winnow_zen2048.exe" "c:/users/rory"
"D:/My Projects/Winnow Project/Winnow64/release/winnow_zen2048.exe" "d:/Pictures/test_export/2011-01-09_0034.jpg"
*/
