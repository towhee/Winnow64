#include "Main/mainwindow.h"
#include "Cache/catalogprobe.h"
#include "Main/keywordauditprobe.h"
#include <QApplication>
#include "qtsingleapplication.h"
#include <QMediaPlayer>
#include <QStandardPaths>
#include <QFontDatabase>
#include <QAccessible>
#include <tiffio.h>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <QImageReader>
#include "Develop/develop.h"
#include "Develop/inputtransform.h"
#include "Develop/workingdump.h"
#include "ImageFormats/Raw/rawformat.h"
#include "ImageFormats/Raw/rawcolor.h"
#include "Metadata/metadata.h"
#include "Utilities/screenfit.h"
#ifdef Q_OS_MAC
#include "Utilities/mac.h"
#include <QAccessibleWidget>
#endif
#ifdef Q_OS_LINUX
#include "Utilities/linux.h"
#endif

static QString rory = "Rory";

#ifdef Q_OS_MAC
static QAccessibleInterface *winnowItemViewAccessible(const QString &classname,
                                                      QObject *object)
{
/*
    THE IMAGE VIEWS ARE A LIST TO ACCESSIBILITY, NOT A TABLE.

    Qt's own interface for QListView/QTableView carries a table interface, and on macOS
    every dataChanged, row insert or reset a view receives makes the Cocoa bridge rebuild
    its element array for EVERY row in the model (QCocoaAccessibility::
    notifyAccessibilityUpdate -> -[QMacAccessibilityElement updateTableModel] ->
    populateTableArray). That is O(rows) per notification, for each of the three views,
    hidden ones included: ~27 ms at 43,000 rows. It is what made a switch from a folder
    into the Catalog take 8 s against 1.5 s at startup, and what every per-row
    notification fix in DataModel has been working around.

    THE BRIDGE CANNOT BE SUSPENDED RELIABLY. Qt activates accessibility on the first
    system accessibility query to any Winnow window (qnsview_accessibility.mm,
    activateQtAccessibility) -- window managers, clipboard and text-expander utilities
    all send them -- and it stays on for the session. Startup is fast only because no
    query has arrived yet. G::A11ySuspend and WINNOW_NO_A11Y are undone by the next one.

    SO THE TABLE INTERFACE IS NOT OFFERED. Qt tries factories against the MOST-DERIVED
    class name first (QAccessible::queryAccessibleInterface), so matching IconView,
    TableView and TableView's frozen-column overlay (FrozenTableView -- a fourth view on
    the same model, and a quarter of the cost when it was missed) here pre-empts the built-in QListView/QTableView interface. With no
    tableInterface() both the Qt-side modelChange and the Cocoa rebuild are skipped.

    THE TRADE: VoiceOver still reaches the views and the rest of the UI, but can no
    longer step through individual thumbnails or table rows. At catalog scale that
    navigation froze the app on every change anyway. macOS only: the O(rows) rebuild is
    the Cocoa bridge's, and Windows UIA has not been measured to need this.
*/
    if (classname == QLatin1StringView("IconView")
        || classname == QLatin1StringView("TableView")
        || classname == QLatin1StringView("FrozenTableView")) {
        if (auto *w = qobject_cast<QWidget *>(object))
            return new QAccessibleWidget(w, QAccessible::List);
    }
    return nullptr;
}
#endif

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
    libtiff routes its warnings and errors through global handlers that print to
    stderr by default, producing noisy console output (eg "unknown field with tag",
    "wrong data type", "TIFF directory is missing required \"ImageLength\" field").
    Many of these come from raw files whose maker IFDs are only nominally TIFF, so
    they are not actionable. Route both through these handlers so they can be gated
    by G::suppressTiffWarnings. Failed decodes are reported by the caller (which
    checks the libtiff return value), not by these messages.
*/
static void winnowTiffWarningHandler(const char *module, const char *fmt, va_list ap)
{
    if (G::suppressTiffWarnings) return;
    if (module) fprintf(stderr, "%s: Warning, ", module);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ".\n");
    fflush(stderr);
}

static void winnowTiffErrorHandler(const char *module, const char *fmt, va_list ap)
{
    if (G::suppressTiffWarnings) return;
    if (module) fprintf(stderr, "%s: Error, ", module);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ".\n");
    fflush(stderr);
}

/*
    Winnow --dumpworking <image> <out.wkg> [engine=apple]

    HEADLESS CALIBRATION DUMP: decode one raw's sensor image with the in-house decoder
    (the same decodeRawWorking path the export's denoise base uses; engine=apple takes
    the Core Image engine instead, as the Develop dock's Demosaic setting can), convert
    it to the working space with its as-shot white balance, write it as a .wkg
    (Develop/workingdump.h) and exit. No window, no catalog, no settings, and it runs
    beside an open Winnow -- it
    branches before the single-instance check. With tests/unit/developrender this closes
    the calibration loop without the UI: dump once per raw, render any recipe, compare
    against Lightroom. A JPEG / HEIC / TIFF is dumped as the display-referred image
    Develop builds from it. Returns 0 on success.
*/
static int runDumpWorking(int argc, char *argv[], const QString &raw, const QString &out,
                          bool appleEngine)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    G::isAutomatedRun = true;
    /* The engine is a Develop-dock setting, not a launch default; calibration wants the
       in-house decoder (full sensor area, the pipeline Winnow owns) unless told otherwise. */
    G::decodeRawEngine = appleEngine ? G::DecodeRawEngine::appleDecodeRawEngine
                                     : G::DecodeRawEngine::winnowDecodeRawEngine;

    Metadata metadata;
    const QFileInfo info(raw);
    if (!info.exists() ||
        !metadata.loadImageMetadata(info, -1, 0, true, true, false, false,
                                    "dumpworking", /*isRemote*/true)) {
        std::fprintf(stderr, "dumpworking: cannot read metadata: %s\n", qPrintable(raw));
        return 1;
    }
    const ImageMetadata m = metadata.m;
    const QString ext = info.suffix().toLower();

    std::unique_ptr<RawFormat> rawFormat = RawFormat::Create(ext);
    if (!rawFormat) {
        /* Not a raw: the JPEG / HEIC / TIFF Develop works on, built as ImageDecoder::
           applyDevelop builds it -- the decoded, ROTATED pixels through the file's
           embedded profile (sourceIcc) -- so the orientation is already applied. */
        QImageReader reader(raw);
        reader.setAutoTransform(true);
        const QImage decoded = reader.read();
        WorkingImage w;
        const QByteArray icc = metadata.iccFormats.contains(ext) ? m.iccBuf : QByteArray();
        if (decoded.isNull() || !InputTransform().FromImage(decoded, w, icc)) {
            std::fprintf(stderr, "dumpworking: cannot decode %s\n", qPrintable(raw));
            return 1;
        }
        if (!WorkingDump::write(w, 0, out)) {
            std::fprintf(stderr, "dumpworking: cannot write %s\n", qPrintable(out));
            return 1;
        }
        std::fprintf(stdout, "dumpworking: %s -> %s (%dx%d, display-referred)\n",
                     qPrintable(raw), qPrintable(out), w.width, w.height);
        return 0;
    }
    QFile file(raw);
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "dumpworking: no raw decoder for %s\n", qPrintable(raw));
        return 1;
    }
    QImage throwaway;
    const EditParams identity;
    std::shared_ptr<const WorkingImage> work;
    const bool ok = rawFormat->Decode(file, m, throwaway, &identity, nullptr, &work);
    file.close();
    if (!ok || !work || !work->isValid()) {
        std::fprintf(stderr, "dumpworking: sensor decode failed: %s\n", qPrintable(raw));
        return 1;
    }

    WorkingImage w = *work;
    Develop::ToWorkingSpace(w, &identity);
    /* The loupe's rotation, as developOrientationDegrees reads it from the model. */
    const int degrees = m.orientation == 3 ? 180 : m.orientation == 6 ? 90
                      : m.orientation == 8 ? 270 : 0;
    if (!WorkingDump::write(w, degrees, out)) {
        std::fprintf(stderr, "dumpworking: cannot write %s\n", qPrintable(out));
        return 1;
    }
    std::fprintf(stdout, "dumpworking: %s -> %s (%dx%d, %d deg)\n", qPrintable(raw),
                 qPrintable(out), w.width, w.height, degrees);
    return 0;
}

/*
    Winnow --dumpwb <raw> [<raw> ...]

    HEADLESS AS-SHOT WHITE BALANCE AUDIT: for each raw, read the colour characterisation
    exactly as the decoders do (RawFormat::ReadAsShotColor, seeded from the parse-time
    rawInfo) and print the model, the as-shot multipliers and the Kelvin/tint the WB row
    would show. No decode, no window, runs beside an open Winnow. "neutral" in the output
    means the file gave no multipliers and the matrix neutral was used -- the 6500 K tell.
    Compare the R/G and B/G columns against ExifTool's WB levels. Returns 0.
*/
static int runDumpWb(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    G::isAutomatedRun = true;
    for (int a = 2; a < argc; ++a) {
        const QString raw = QString::fromLocal8Bit(argv[a]);
        const QFileInfo info(raw);
        Metadata metadata;
        if (!info.exists() ||
            !metadata.loadImageMetadata(info, -1, 0, true, true, false, false,
                                        "dumpwb", /*isRemote*/true)) {
            std::fprintf(stdout, "%s\tno metadata\n", qPrintable(raw));
            continue;
        }
        const ImageMetadata m = metadata.m;
        std::unique_ptr<RawFormat> rawFormat = RawFormat::Create(info.suffix().toLower());
        QFile file(raw);
        if (!rawFormat || !file.open(QIODevice::ReadOnly)) {
            std::fprintf(stdout, "%s\t%s\tno raw decoder\n", qPrintable(raw),
                         qPrintable(m.model));
            continue;
        }
        RawSensorInfo ci = m.rawInfo;
        const bool read = rawFormat->ReadAsShotColor(file, m, ci);
        file.close();
        CameraColor cam;
        RawColor::Characterise(ci.xyzToCam, ci.camMul, cam);
        const float *c = ci.camMul;
        const bool neutral = c[0] == c[1] && c[1] == c[2];
        std::fprintf(stdout, "%s\t%s\tread=%d matrix=%d\tmul %.1f %.1f %.1f %.1f\t"
                     "R/G %.4f B/G %.4f\t%s\t%.0f K\ttint %.1f\n",
                     qPrintable(raw), qPrintable(m.model), read, ci.hasColorMatrix,
                     c[0], c[1], c[2], c[3], c[1] > 0 ? c[0] / c[1] : 0.0,
                     c[1] > 0 ? c[2] / c[1] : 0.0, neutral ? "neutral" : "as-shot",
                     cam.asShotK, cam.asShotTint);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    /* SET, NOT DERIVED. Every settings, cache and model path is keyed on the application
       name, which Qt otherwise takes from the executable -- "Winnow" on all three
       platforms until the Linux AppImage, whose launcher execs the binary as
       "AppRun.wrapped" and put everything under that name instead. */
    QCoreApplication::setApplicationName("Winnow");

    if (argc >= 4 && QString::fromLocal8Bit(argv[1]) == "--dumpworking")
        return runDumpWorking(argc, argv, QString::fromLocal8Bit(argv[2]),
                              QString::fromLocal8Bit(argv[3]),
                              argc >= 5 && QString::fromLocal8Bit(argv[4]) == "engine=apple");
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == "--dumpwb")
        return runDumpWb(argc, argv);

#ifdef Q_OS_LINUX
    Linux::useBradientDecoration();         // title bar in the Winnow background
#endif

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

    /* Gate libtiff console noise behind G::suppressTiffWarnings. It arrives by two
       separate routes: Winnow's own TIFFOpen calls go through libtiff's global
       handlers set here, while QImageReader hands TIFF (and any raw with TIFF magic)
       to Qt's qtiff plugin, which STATICALLY links its own private libtiff and
       re-emits its messages as qWarnings in the qt.imageformats.tiff logging
       category (eg "TIFF directory is missing required \"ImageLength\" field").
       That copy's handlers are unreachable from here, so the category is turned off
       in the filter rules below instead.
    */
    TIFFSetWarningHandler(winnowTiffWarningHandler);
    TIFFSetErrorHandler(winnowTiffErrorHandler);

    /* All Qt logging category rules must be set here in ONE call: setFilterRules
       REPLACES the previously set rules, it does not add to them. */
    QString filterRules =
        "qt.multimedia.ffmpeg.*=false\n"
        "qt.text.font.db=false\n"
        "qt.gui.imageio.jpeg.debug=false\n"
        "qt.gui.imageio.jpeg.warning=false";
    // Qt's qtiff plugin reports its private libtiff's warnings here (see above)
    if (G::suppressTiffWarnings) filterRules += "\nqt.imageformats.tiff=false";
    QLoggingCategory::setFilterRules(filterRules);
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
    /*  Winnow --keywordaudit [keyword substring] -- Audit Keywords on the real index and
        keyword list, headless (Main/keywordauditprobe.h). Real paths like the two
        probes beside it; it stores any image vectors it computes, as the dialog would. */
    bool isKeywordAuditProbe = false;
    QString keywordAuditFilter;
    /*  Winnow --catalogload [folder] -- the real index, the real load path, headless.
        Like --catalogprobe it must NOT enter QStandardPaths test mode, because the whole
        point is the catalog the user actually has. */
    bool isCatalogLoad = false;
    QString catalogLoadFilter;
    /*  Winnow --perfprobe -- an ORDINARY interactive session with the [PERF] lines on.

        Every other way of setting G::isPerfProbe measures a load and then exits: the env
        var only applies in QStandardPaths test mode, and --catalogload is headless. Some
        of what hurts is not in the load at all -- scrolling a catalog scope is a person
        dragging a scrollbar, which no headless run reproduces -- so there has to be a way
        to watch a real session. It changes nothing except that diagnostics are printed
        (the scroll probe, the GUI stall watchdog, the Phase 1/2 load lines); settings,
        catalog and cache are the user's own, exactly as in a normal launch. */
    bool isPerfProbeArg = false;
    /*  Winnow --ingestprobe -- an ordinary interactive session that starts with the
        ingest probe already recording, for a cull that begins the moment the app opens.
        Separate from --perfprobe: that one measures a LOAD and prints a line per scroll,
        which is exactly the noise a cull-length session does not need. */
    bool isIngestProbeArg = false;
    /*  Winnow --panelprobe -- an ordinary interactive session recording the dock
        geometry negotiation from the first show (see Utilities/panelprobe.h).  It has to
        be an ARGUMENT as well as a menu toggle because the glitch it was written for --
        panels that open narrow -- is over before any menu can be reached.  The menu
        toggle persists instead, arming the next launch; this arms exactly one run. */
    bool isPanelProbeArg = false;
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
        else if (arg == "--keywordaudit") isKeywordAuditProbe = true;
        else if (arg == "--catalogload") isCatalogLoad = true;
        else if (arg == "--perfprobe") isPerfProbeArg = true;
        else if (arg == "--ingestprobe") isIngestProbeArg = true;
        else if (arg == "--panelprobe") isPanelProbeArg = true;
        else if (isCatalogProbe && catalogProbeFilter.isEmpty()) catalogProbeFilter = arg;
        else if (isKeywordAuditProbe && keywordAuditFilter.isEmpty())
            keywordAuditFilter = arg;
        else if (isCatalogLoad && catalogLoadFilter.isEmpty()) catalogLoadFilter = arg;
        else if (isMetaTest && metaTestFile.isEmpty()) metaTestFile = arg;
        else if (isSelfTest && selfTestFolder.isEmpty()) selfTestFolder = arg;
        else if (isDevTest && devTestFolder.isEmpty()) devTestFolder = arg;
        else if (isSoakTest) soakFolders << arg;
    }
    if (isPerfProbeArg) G::isPerfProbe = true;
    if (isIngestProbeArg) G::isIngestProbe.store(true, std::memory_order_relaxed);
    if (isPanelProbeArg) G::isPanelProbe.store(true, std::memory_order_relaxed);

    /* The probe joins these for the SINGLE-INSTANCE bypass only -- it must always start
       fresh rather than handing its arguments to a running Winnow -- and deliberately not
       for the test-mode settings isolation below. */
    const bool isTestMode = isSelfTest || isMetaTest || isSoakTest || isDevTest
                            || isCatalogProbe || isCatalogLoad || isKeywordAuditProbe;
    const bool usesRealIndex = isCatalogProbe || isCatalogLoad || isKeywordAuditProbe;
    if (isTestMode && !usesRealIndex) QStandardPaths::setTestModeEnabled(true);
    /*  Told separately from the settings isolation above, which the probe runs opt out
        of: this says "a harness is driving", which is true for all of them. */
    G::isAutomatedRun = isTestMode || isCatalogProbe || isCatalogLoad
                        || isKeywordAuditProbe;

    // /*Single instance version
    QtSingleApplication instance("Winnow", argc, argv);
    /* Every QMessageBox button fits its label: Utilities::fitMessageBoxButtons. */
    Utilities::installMessageBoxButtonFitter(&instance);

    QString args;
    QString delimiter = "\n";
    for (int i = 1; i < argc; ++i) {
        /*  Not an argument to pass on: a running instance handed "--perfprobe" would try
            to open it as a file path. */
        if (QString::fromLocal8Bit(argv[i]) == "--perfprobe") continue;
        if (QString::fromLocal8Bit(argv[i]) == "--ingestprobe") continue;
        if (QString::fromLocal8Bit(argv[i]) == "--panelprobe") continue;
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
    /*  No Mac::initializeAppDelegate() -- it replaced Qt's application delegate, and
        with it every quit that did not come from Winnow's own menu skipped closeEvent.
        See the note in Utilities/mac.mm. */
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

    /*  WINNOW_NO_A11Y=1 TURNS THE ACCESSIBILITY BRIDGE OFF, to confirm what a sample of a
        stalled process pointed at: 94% of the main thread's time during a 42-second
        Catalog freeze was

            DataModel::setValSf -> QSortFilterProxyModel::setData -> dataChanged
              -> QAbstractItemView::dataChanged
                -> -[QMacAccessibilityElement updateTableModel]
                  -> populateTableArray -> objc alloc

        Every per-row write reaches the view, the view notifies Cocoa accessibility, and
        the bridge rebuilds its element array for ALL 42,979 rows -- one Obj-C object per
        cell, per write. QAccessible::setActive(false) is what QAccessible::update-
        Accessibility checks before dispatching, so this stops it at the source.

        A SWITCH RATHER THAN A DEFAULT: turning accessibility off for everyone would fix
        the stall by removing VoiceOver support, which is not a trade to make silently.
        The real fix is to stop emitting one dataChanged per row. */
    /*  Before any window exists, so no view has had the built-in table interface
        created and cached for it. See winnowItemViewAccessible. */
#ifdef Q_OS_MAC
    QAccessible::installFactory(winnowItemViewAccessible);
#endif

    if (qEnvironmentVariableIntValue("WINNOW_NO_A11Y") == 1) {
        QAccessible::setActive(false);
        fprintf(stderr, "WINNOW: accessibility bridge DISABLED (WINNOW_NO_A11Y=1)\n");
        fflush(stderr);
    }

    /*  Before MW: the probe needs qApp (Metadata resolves the ExifTool path from the
        application directory) and nothing else. Constructing the main window would load a
        folder and start the caches, which is exactly the activity whose cost is being
        measured. */
    if (isCatalogProbe) {
        const int rc = CatalogProbe::run(catalogProbeFilter);
        std::_Exit(rc);
    }
    /* Before MW for the same reason: Audit Keywords against the real index, no window. */
    if (isKeywordAuditProbe) {
        const int rc = KeywordAuditProbe::run(keywordAuditFilter);
        std::_Exit(rc);
    }

    /*  WINNOW_STARTUP_TIMELINE=1: the central-area timeline (MW::startCentralTimeline)
        from before the window is built -- what a launch shows, and when. */
    auto startupClock = std::make_shared<QElapsedTimer>();
    startupClock->start();
    const bool startupTimeline = qEnvironmentVariableIntValue("WINNOW_STARTUP_TIMELINE") == 1;

    // every window opens inside the usable screen area (not under the taskbar)
    ScreenFit::install();

    MW mw(args);
    if (startupTimeline) {
        mw.startCentralTimeline(startupClock);
        mw.timelineMark("MW constructed");
    }

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
