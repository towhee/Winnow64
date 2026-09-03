#include "Main/mainwindow.h"
#include "Cache/catalogprobe.h"
#include <QApplication>
#include "qtsingleapplication.h"
#include <QMediaPlayer>
#include <QStandardPaths>
#include <QFontDatabase>
#include <tiffio.h>
#include <cstdarg>
#include <cstdlib>
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

/*
    libtiff routes its warnings through a global handler that prints to stderr by
    default, producing noisy console output (eg "unknown field with tag", "wrong
    data type"). Route warnings through this handler so they can be gated by
    G::suppressTiffWarnings. Errors are left to libtiff's default handler since
    they signal genuine decode failures.
*/
static void winnowTiffWarningHandler(const char *module, const char *fmt, va_list ap)
{
    if (G::suppressTiffWarnings) return;
    if (module) fprintf(stderr, "%s: Warning, ", module);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ".\n");
    fflush(stderr);
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_MAC
    // Suppress the system-appended "Start Dictation" / "Emoji & Symbols" items
    // in the Edit menu. Must run before the menu (and ideally the app) is built.
    Mac::disableExtraEditMenuItems();
#endif

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

    // Gate libtiff console warnings behind G::suppressTiffWarnings
    TIFFSetWarningHandler(winnowTiffWarningHandler);

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
    bool isSoakTest = false;
    /* Headless DEVELOP stress mode (ThreadSanitizer layer):
         Winnow --devtest <folder>
       Duration via WINNOW_DEVTEST_MS. Unlike --selftest it enters Develop, builds a
       multi-submask scope and drives a brush drag, which is the only way anything in the
       suite reaches the worker-thread proxy render. See MW::runDevelopStressTest. */
    bool isDevTest = false;
    /* Catalog cost probe (see Cache/catalogprobe.h):
         Winnow --catalogprobe [path substring]
       Unlike every mode above it must NOT run in QStandardPaths test mode, because the
       whole point is to measure the user's REAL index -- an isolated settings location
       would open an empty database and report that browsing a catalog is free. It reads
       and writes nothing else, and exits before any window is created. */
    bool isCatalogProbe = false;
    QString catalogProbeFilter;
    /*  Winnow --catalogload [folder] -- the real index, the real load path, headless.
        Like --catalogprobe it must NOT enter QStandardPaths test mode, because the whole
        point is the catalog the user actually has. */
    bool isCatalogLoad = false;
    QString catalogLoadFilter;
    QString selfTestFolder;
    QString metaTestFile;
    QString devTestFolder;
    QStringList soakFolders;   // one or more folders to bounce between
    int selfTestMs = qEnvironmentVariableIntValue("WINNOW_SELFTEST_MS");
    if (selfTestMs <= 0) selfTestMs = 8000;
    int devTestMs = qEnvironmentVariableIntValue("WINNOW_DEVTEST_MS");
    if (devTestMs <= 0) devTestMs = 30000;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--selftest") isSelfTest = true;
        else if (arg == "--metatest") isMetaTest = true;
        else if (arg == "--soaktest") isSoakTest = true;
        else if (arg == "--devtest") isDevTest = true;
        else if (arg == "--catalogprobe") isCatalogProbe = true;
        else if (arg == "--catalogload") isCatalogLoad = true;
        else if (isCatalogProbe && catalogProbeFilter.isEmpty()) catalogProbeFilter = arg;
        else if (isCatalogLoad && catalogLoadFilter.isEmpty()) catalogLoadFilter = arg;
        else if (isMetaTest && metaTestFile.isEmpty()) metaTestFile = arg;
        else if (isSelfTest && selfTestFolder.isEmpty()) selfTestFolder = arg;
        else if (isDevTest && devTestFolder.isEmpty()) devTestFolder = arg;
        else if (isSoakTest) soakFolders << arg;
    }
    /* The probe joins these for the SINGLE-INSTANCE bypass only -- it must always start
       fresh rather than handing its arguments to a running Winnow -- and deliberately not
       for the test-mode settings isolation below. */
    const bool isTestMode = isSelfTest || isMetaTest || isSoakTest || isDevTest
                            || isCatalogProbe || isCatalogLoad;
    const bool usesRealIndex = isCatalogProbe || isCatalogLoad;
    if (isTestMode && !usesRealIndex) QStandardPaths::setTestModeEnabled(true);

    // /*Single instance version
    QtSingleApplication instance("Winnow", argc, argv);

    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        args += argv[i];
        if (i < argc - 1) args += delimiter;
    }
    // The test modes open their target explicitly (runSelfTest / runMetaTest /
    // runSoakTest), not via args, and must always start a fresh instance.
    if (isTestMode) args.clear();

    // terminate if Winnow already open and no arguments to pass
    if (!isTestMode && args == "" && instance.isRunning()) {
        QString msg = "Winnow or a Winnow report is open.";
        // G::popUp->showPopup(msg);
        return 0;
    }

    // instance already running
    if (!isTestMode && instance.sendMessage(args)) {
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
    // Strip the Sequoia "Writing Tools" / "Autofill" items AppKit appends to
    // the Edit menu (no defaults switch exists for these).
    Mac::stripEditMenuExtras();
    // MyApplicationDelegate *delegate = [[MyApplicationDelegate alloc] init];
    // [NSApp setDelegate:delegate];
#endif

    // start program
    QCoreApplication::addLibraryPath("./");
    // QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Use the OS system UI font (San Francisco on macOS, Segoe UI on Windows).
    // Only the family is set here; text size is driven by the per-widget
    // font-size in G::css (G::fontSize).
    instance.setFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));

    /*  Before MW: the probe needs qApp (Metadata resolves the ExifTool path from the
        application directory) and nothing else. Constructing the main window would load a
        folder and start the caches, which is exactly the activity whose cost is being
        measured. */
    if (isCatalogProbe) {
        const int rc = CatalogProbe::run(catalogProbeFilter);
        std::_Exit(rc);
    }

    MW mw(args);

    // // hide root in FSTree after loaded
    // QObject::connect(&instance, &QGuiApplication::applicationStateChanged,
    //                  &mw, &MW::whenActivated);

    mw.show();

    if (isCatalogLoad) {
        mw.runCatalogLoadTest(catalogLoadFilter);
    }
    else if (isSelfTest) {
        mw.runSelfTest(selfTestFolder, selfTestMs);
    }
    else if (isDevTest) {
        mw.runDevelopStressTest(devTestFolder, devTestMs);
    }
    else if (isMetaTest) {
        mw.runMetaTest(metaTestFile);
    }
    else if (isSoakTest) {
        // Long-running race/leak soak (see tests/soak). The bounce loop blocks
        // on its own processEvents, so defer it into the event loop — that lets
        // the orderly window-close teardown at the end run, which is what makes
        // LeakSanitizer's exit report meaningful. Duration/pace/seed via env.
        int soakMs   = qEnvironmentVariableIntValue("WINNOW_SOAK_MS");
        if (soakMs <= 0) soakMs = 60000;                  // 60 s default
        int soakImgMs = qEnvironmentVariableIntValue("WINNOW_SOAK_IMG_MS");
        if (soakImgMs <= 0) soakImgMs = 50;               // 50 ms between images
        bool seedSet = false;
        uint soakSeed = qEnvironmentVariableIntValue("WINNOW_SOAK_SEED", &seedSet);
        if (!seedSet) soakSeed = 1;                       // fixed default → reproducible
        QTimer::singleShot(0, &mw, [&mw, soakFolders, soakMs, soakImgMs, soakSeed]() {
            mw.runSoakTest(soakFolders, soakMs, soakImgMs, soakSeed);
        });
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
