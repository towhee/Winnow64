#include "Main/mainwindow.h"
#include <QApplication>
#include "qtsingleapplication.h"
#include <QMediaPlayer>
#include <QStandardPaths>
#ifdef Q_OS_MAC
#include "Utilities/mac.h"
#endif

static QString rory = "Rory";

// Define the custom message handler
void winnowMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Filter out the specific font warnings
    if (
        msg.contains("OpenType support missing") ||
        msg.contains("qt.text.font.db")
       )
    {
        return;
    }

    // Pass everything else to the default handler
    QByteArray localMsg = msg.toLocal8Bit();
    fprintf(stderr, "%s\n", localMsg.constData());
    fflush(stderr);
}

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

    // QLoggingCategory::setFilterRules("qt.text.font.db.debug=false");
    // silence multimedia reporting to console
    // qputenv("QT_LOGGING_RULES", "qt.multimedia.ffmpeg.*=false");
    // qputenv("QT_LOGGING_RULES", "qt.multimedia.ffmpeg.*=false;qt.text.font.db=false");

    // Install the message handler at the very start of main
    qInstallMessageHandler(winnowMessageHandler);

    QLoggingCategory::setFilterRules(
        "qt.multimedia.ffmpeg.*=false\n"
        "qt.text.font.db=false"
    );
    qputenv("QT_FFMPEG_LOG", "0");

    // Headless self-test mode (smoke test layer, see tests/):
    //   Winnow --selftest <folder>
    // Settle time defaults to 8s, override with WINNOW_SELFTEST_MS. Runs against
    // an isolated settings location so it never touches the user's real config,
    // and bypasses the single-instance forwarding so it always starts fresh.
    bool isSelfTest = false;
    bool isMetaTest = false;
    QString selfTestFolder;
    QString metaTestFile;
    int selfTestMs = qEnvironmentVariableIntValue("WINNOW_SELFTEST_MS");
    if (selfTestMs <= 0) selfTestMs = 8000;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--selftest") isSelfTest = true;
        else if (arg == "--metatest") isMetaTest = true;
        else if (isMetaTest && metaTestFile.isEmpty()) metaTestFile = arg;
        else if (isSelfTest && selfTestFolder.isEmpty()) selfTestFolder = arg;
    }
    if (isSelfTest || isMetaTest) QStandardPaths::setTestModeEnabled(true);

    // /*Single instance version
    QtSingleApplication instance("Winnow", argc, argv);

    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        args += argv[i];
        if (i < argc - 1) args += delimiter;
    }
    // The test modes open their target explicitly (runSelfTest / runMetaTest),
    // not via args, and must always start a fresh instance.
    if (isSelfTest || isMetaTest) args.clear();

    // terminate if Winnow already open and no arguments to pass
    if (!isSelfTest && !isMetaTest && args == "" && instance.isRunning()) {
        QString msg = "Winnow or a Winnow report is open.";
        // G::popUp->showPopup(msg);
        return 0;
    }

    // instance already running
    if (!isSelfTest && !isMetaTest && instance.sendMessage(args)) {
        if (G::isRunByExtern) Utilities::log("WinnowMain", "Instance already running");
        QString msg = "Winnow or a Winnow report is open.";
        // G::popUp->showPopup(msg);
        return 0;
    }
    else {
        if (G::isRunByExtern) Utilities::log("WinnowMain", "New Instance");
    }

#ifdef Q_OS_MAC
    Mac::initializeAppDelegate();
    // MyApplicationDelegate *delegate = [[MyApplicationDelegate alloc] init];
    // [NSApp setDelegate:delegate];
#endif

    // start program
    QCoreApplication::addLibraryPath("./");
    // QApplication::setStyle(QStyleFactory::create("Fusion"));
    MW mw(args);

    // // hide root in FSTree after loaded
    // QObject::connect(&instance, &QGuiApplication::applicationStateChanged,
    //                  &mw, &MW::whenActivated);

    mw.show();

    if (isSelfTest) {
        mw.runSelfTest(selfTestFolder, selfTestMs);
    }
    else if (isMetaTest) {
        mw.runMetaTest(metaTestFile);
    }

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
