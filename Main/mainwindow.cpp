#include "Main/mainwindow.h"
#include "Utilities/fileops.h"
#include "Cache/catalog.h"
#include "Cache/devpreviewcache.h"
#include "Cache/thumbcache.h"
#include "Main/global.h"
#include "Develop/workingimage.h"
#include "Develop/workingimagecache.h"
#include "Develop/inputtransform.h"
#include "Develop/brushstamp.h"
#include "Develop/maskedge.h"
#include "Develop/maskhalo.h"
#include "Develop/maskfalloff.h"
#include "Develop/rangemask.h"
#include "Develop/subjectmask.h"
#include "Develop/skymask.h"
#include "Develop/depthmask.h"
#include "Develop/objectmask.h"
#include "Develop/develop.h"
#include "Develop/sharpen.h"        // updateSharpenMaskPreview: the edge gate
#include "Develop/detailroi.h"      // Detail panel 1:1 preview: the patch geometry
#include <opencv2/imgproc.hpp>     // updateSharpenMaskPreview: GaussianBlur + Sobel
#include "ImageFormats/Raw/pmrid.h"
#include "Utilities/inference/miganfill.h"
#include "Utilities/inference/lamafill.h"
#include "Cache/imagedecoder.h"
#include "Utilities/subjectpredictor.h"
#include "Utilities/skypredictor.h"
#include "Utilities/depthpredictor.h"
#include "Utilities/objectmaskpredictor.h"
#include "Develop/Transform/croptransform.h"
#include <QMutex>
#include <QScopeGuard>      // updateMaskOverlayTint probe (it has many early returns)
#include <QtMath>          // qSin (runDevelopStressTest's synthetic stroke)
#include <memory>
#include <QMetaEnum>
#include <cmath>            // std::sqrt (updateDevelopScopes sampling stride)
#include <cstdlib>          // std::_Exit (used by runSelfTest)
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QSaveFile>
#include <QProcess>

/*
   Program notes / documentation: see notes/Documentation.txt
*/

/* Defined below with the mask compositor, inside this file's anonymous namespace;
   declared here because the folder-change reset runs earlier in the file. The namespace
   matters: declaring it at global scope instead makes the later call ambiguous. */
namespace { void maskFoldCacheClear(); }

void MW::updateDockTabGraphics(QTabBar *tabBar)
{
/*
    Responsive dock tab titles (gated by G::useDockTitleGraphic).

    When all the text titles in a dock tab bar fit, the tabs show text;
    otherwise they switch to white graphics. QMainWindow does not paint a dock's
    windowIcon on tabs, and on macOS the tab is painted natively (a QProxyStyle
    drawControl override and setTabButton geometry are both bypassed), so each
    graphic is a QLabel parented to the tab bar and positioned by hand over the
    centre of its tab; an invisible spacer button gives a graphic tab a minimum
    width of 150% of the graphic.

    A tab is identified by its (unique) text title when shown, and by tabData
    (set when the title is cleared) when in graphic mode, so identity survives
    the switch. If both are lost - a full tab-bar rebuild while in graphic mode -
    the titles are restored so identity can be re-established on the next pass.

    setWindowTitle / setTabButton / new QLabel all post events back through the
    application event filter, so a re-entrancy guard prevents recursion.
*/
    static bool busy = false;
    if (busy || !tabBar) return;

    // Is this one of the dock tab bars? (QMainWindowTabBar, or - on styles where
    // dock tabs are plain QTabBars - any tab currently showing a dock title.)
    bool isDockBar = QString(tabBar->metaObject()->className()) == "QMainWindowTabBar";
    if (!isDockBar)
        for (int i = 0; i < tabBar->count(); ++i)
            if (dockTextNames.contains(tabBar->tabText(i))) { isDockBar = true; break; }
    if (!isDockBar) return;

    const QHash<QString, QString> graphicFor = {
        {folderDockTabText,   ":/images/icon16/foldertree_white.png"},
        {favDockTabText,      ":/images/icon16/bookmarks_white.png"},
        {filterDockTabText,   ":/images/icon16/filters_white.png"},
        /* The filter dock keeps the "Filters" tab text even when G::useFindDock folds
           the Catalog panel into it; the Catalog entry simply never matches then,
           because there is no separate Catalog tab to draw. */
        {catalogDockTabText,  ":/images/icon16/catalog_white.png"},
        {metadataDockTabText, ":/images/icon16/metadata_white.png"},
        {embelDockTabText,    ":/images/icon16/embellish_white.png"},
        {developDockTabText,  ":/images/icon16/develop_white.png"},
        {historyDockTabText,  ":/images/icon16/history_white.png"},
        {presetsDockTabText,  ":/images/icon16/presets_white.png"},
    };
    const QHash<QString, QDockWidget*> dockFor = {
        {folderDockTabText,   folderDock},
        {favDockTabText,      favDock},
        {filterDockTabText,   filterDock},
        {catalogDockTabText,  catalogDock},
        {metadataDockTabText, metadataDock},
        {embelDockTabText,    embelDock},
        {developDockTabText,  developDock},
        {historyDockTabText,  historyDock},
        {presetsDockTabText,  presetsDock},
    };

    busy = true;

    // Identity per tab. QMainWindow stores its own stable per-tab key in
    // tabData (a pointer); it overwrites anything we put there on a title
    // change, so we must not use tabData as our own store. Instead learn
    // key -> title while the text is visible, and look it up when titles are
    // cleared (graphic mode).
    QStringList id;
    bool lostIdentity = false;
    bool anyText = false;
    for (int i = 0; i < tabBar->count(); ++i) {
        QString t = tabBar->tabText(i);
        quint64 key = tabBar->tabData(i).toULongLong();
        if (!t.isEmpty()) {
            anyText = true;
            if (key) dockTabTitleByKey.insert(key, t);   // learn
        }
        else {
            t = dockTabTitleByKey.value(key);            // recover learned title
        }
        if (t.isEmpty()) lostIdentity = true;
        id << t;
    }
    // Recover after a rebuild that wiped empty-text/empty-data tabs: restore
    // text titles so the next pass can identify and re-evaluate.
    if (lostIdentity) {
        for (auto it = dockFor.begin(); it != dockFor.end(); ++it)
            if (it.value() && it.value()->windowTitle().isEmpty())
                it.value()->setWindowTitle(it.key());
        busy = false;
        return;
    }

    // Width the text titles need vs. the width available. In text mode the tab
    // bar's sizeHint is exact; remember it so graphic mode uses the *same*
    // threshold (otherwise an under-estimate makes the two modes disagree and
    // flip-flop at the boundary). Falls back to a font estimate only before the
    // first text-mode pass / right after a rebuild.
    int needTextWidth;
    if (anyText) {
        needTextWidth = tabBar->sizeHint().width();
        tabBar->setProperty("dockTabTextWidth", needTextWidth);
    }
    else {
        needTextWidth = tabBar->property("dockTabTextWidth").toInt();
        if (needTextWidth <= 0) {
            int pad = qMax(tabBar->style()->pixelMetric(QStyle::PM_TabBarTabHSpace, nullptr, tabBar), 16);
            needTextWidth = 0;
            for (const QString &t : id)
                needTextWidth += tabBar->fontMetrics().horizontalAdvance(t) + pad;
        }
    }
    // Available width is the dock AREA width, not tabBar->width() (the tab bar
    // shrinks to fit its tabs). Use the FRONT (visible, un-occluded) dock: a
    // background tabbed dock can keep a stale width, so a plain max() over the
    // group may report an old, wider value and never switch when narrowed.
    int avail = 0;
    for (const QString &t : id) {
        QDockWidget *d = dockFor.value(t, nullptr);
        if (d && d->isVisible() && !d->visibleRegion().isEmpty())
            avail = qMax(avail, d->width());
    }
    if (avail <= 0)   // none clearly visible: fall back to any group dock
        for (const QString &t : id) {
            QDockWidget *d = dockFor.value(t, nullptr);
            if (d) avail = qMax(avail, d->width());
        }
    if (avail <= 0) avail = tabBar->width();
    bool fits = needTextWidth <= avail;

    // Hide every graphic up front; the loop re-shows only the tabs in graphic
    // mode. A dock can leave this tab bar (e.g. re-tabified into another dock)
    // yet its label stays parented here - without this, those stale labels
    // would linger on top of the remaining tabs (and on text titles).
    const QList<QLabel*> existingLabels =
        tabBar->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    for (QLabel *lbl : existingLabels)
        if (lbl->objectName().startsWith("dockTabGraphic_")) lbl->hide();

    // Bound by id.size() as well as the live tab count. id is a snapshot taken
    // above, but the setWindowTitle/setTabButton calls below (and a restoreState
    // in progress at startup) can change tabBar->count() under us. Indexing
    // id.at(i) past its size reads freed memory — Qt's at() is unchecked in
    // release — and hashing that stale QString in dockFor.value() crashes. Process
    // only the tabs we have an id for; any new tabs are handled on the next pass.
    for (int i = 0; i < id.size() && i < tabBar->count(); ++i) {
        QDockWidget *dock = dockFor.value(id.at(i), nullptr);
        if (!dock) continue;
        dock->toggleViewAction()->setText(id.at(i));   // keep popup-menu label

        QString labelName = "dockTabGraphic_" + id.at(i);
        QLabel *graphic = tabBar->findChild<QLabel*>(labelName, Qt::FindDirectChildrenOnly);

        if (fits) {
            if (dock->windowTitle() != id.at(i)) dock->setWindowTitle(id.at(i));
            if (tabBar->tabButton(i, QTabBar::LeftSide))
                tabBar->setTabButton(i, QTabBar::LeftSide, nullptr);   // deletes spacer
            // graphic already hidden by the bulk-hide above
        }
        else {
            QPixmap pm(graphicFor.value(id.at(i)));
            if (pm.isNull()) continue;   // missing resource: leave the text title
            if (!dock->windowTitle().isEmpty()) dock->setWindowTitle(QString());
            if (!graphic) {
                graphic = new QLabel(tabBar);
                graphic->setObjectName(labelName);
                graphic->setPixmap(pm);
                graphic->setAttribute(Qt::WA_TransparentForMouseEvents);
                graphic->setAttribute(Qt::WA_TranslucentBackground);
                graphic->setStyleSheet("background: transparent;");
                graphic->resize(graphic->pixmap().size());
            }
            // Minimum tab width of 150% of the graphic, via an invisible spacer
            // button (the tab has no text to size it).
            if (tabBar->tabButton(i, QTabBar::LeftSide) == nullptr) {
                QWidget *spacer = new QWidget;
                spacer->setFixedSize(graphic->pixmap().width() * 3 / 2,
                                     graphic->pixmap().height());
                spacer->setAttribute(Qt::WA_TransparentForMouseEvents);
                tabBar->setTabButton(i, QTabBar::LeftSide, spacer);
            }
            QRect g = graphic->rect();
            g.moveCenter(tabBar->tabRect(i).center());
            graphic->setGeometry(g);
            graphic->raise();
            graphic->show();
        }
    }
    busy = false;
}

void MW::scheduleDockTabUpdate()
{
/*
    Re-evaluate every dock tab bar after the layout settles. Connected to each
    tabified dock's dockLocationChanged / topLevelChanged: dragging a dock into
    a tab group adds a tab but fires no reliable resize/show on the surviving
    docks, so the tab count can change without the event-filter path noticing.
    Deferred with a zero timer so the new tab and final geometry exist when it
    runs.
*/
    if (!G::useDockTitleGraphic) return;
    QTimer::singleShot(0, this, [this]() {
        const QList<QTabBar *> bars = findChildren<QTabBar *>();
        for (QTabBar *b : bars) updateDockTabGraphics(b);
    });
}

QDockWidget* MW::dockForTabText(const QString &tabText)
{
    if (tabText == folderDockTabText)   return folderDock;
    if (tabText == favDockTabText)      return favDock;
    if (tabText == filterDockTabText)   return filterDock;
    /* catalogDockTabText is empty with G::useFindDock (the dock is never created), so
       guard against an empty tabText matching it. */
    if (!catalogDockTabText.isEmpty() && tabText == catalogDockTabText) return catalogDock;
    if (tabText == metadataDockTabText) return metadataDock;
    if (tabText == embelDockTabText)    return embelDock;
    if (tabText == developDockTabText)  return developDock;
    if (tabText == historyDockTabText)  return historyDock;
    if (tabText == presetsDockTabText)  return presetsDock;
    return nullptr;
}

void MW::moveDroppedDockLast()
{
/*
    WORK IN PROGRESS - currently DISABLED (the dockLocationChanged connection in
    initialize() is commented out). It broke dock-tab selection: clicking a tab
    could no longer raise its dock. Cause not yet diagnosed (likely the re-tabify /
    raise() firing on interactions that are not true drops). Left intact for a
    later fix; do not re-enable the connection until tab selection is verified.

    When a dock is dropped into an existing tab group, QMainWindow inserts it at
    the drop position (often the front). We want a dropped dock to always become
    the LAST (rightmost) tab in its group, regardless of where it was dropped.

    Connected to each dock's dockLocationChanged. After the drop settles (deferred
    zero timer), find the tab bar now holding the moved dock, read the group in
    visual (tab) order, and if the moved dock is not already last, re-tabify it
    after the current last dock. tabifyDockWidget(last, moved) moves moved's tab
    immediately after last's, so it becomes the rightmost. The "already last"
    early-out makes the re-tabify (which re-fires this signal) a no-op the second
    time, so there is no recursion.

    Tab identity is by text title, falling back to the learned key map when the
    tabs are in graphic mode (empty text) - the same identity scheme as
    updateDockTabGraphics.
*/
    QDockWidget *moved = qobject_cast<QDockWidget*>(sender());
    if (!moved) return;
    QTimer::singleShot(0, this, [this, moved]() {
        if (moved->isFloating()) return;
        const QList<QTabBar *> bars = findChildren<QTabBar *>();
        for (QTabBar *bar : bars) {
            QList<QDockWidget*> ordered;
            int movedIndex = -1;
            for (int i = 0; i < bar->count(); ++i) {
                QString title = bar->tabText(i);
                if (title.isEmpty())   // graphic mode: recover learned title
                    title = dockTabTitleByKey.value(bar->tabData(i).toULongLong());
                QDockWidget *d = dockForTabText(title);
                if (!d) continue;
                if (d == moved) movedIndex = ordered.size();
                ordered << d;
            }
            if (movedIndex < 0) continue;                    // not this bar
            if (ordered.size() < 2) return;                  // nothing to reorder
            if (movedIndex == ordered.size() - 1) return;    // already last
            tabifyDockWidget(ordered.last(), moved);
            moved->raise();
            return;
        }
    });
}

MW::MW(const QString args, QWidget *parent) : QMainWindow(parent)
{
    if (G::isLogger || G::isFlowLogger) G::log("MW::MW", "START APPLICATION", true);
    setObjectName("MW");

    // Check if modifier key pressed while program opening
    isShiftOnOpen = false;
    Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
    if (modifiers & Qt::ShiftModifier) {
        isShiftOnOpen = true;
        G::isEmbellish = false;
    }

    // check args to see if program was started by another process (winnet)
    QString delimiter = "\n";
    QStringList argList = args.split(delimiter);
    if (argList.length() > 1)
        isStartupArgs = true;

    /* TESTING / DEBUGGING FLAGS
       Note G::isLogger is in globals.cpp */
    G::showAllTableColumns = false;     // show all table fields for debugging
    simulateJustInstalled = false;
    G::isStressTest = false;
    G::isTimer = true;                  // Global timer
    G::isTest = false;                  // test performance timer

    G::guiThread = QCoreApplication::instance()->thread();

    // Initialize some variables (must precede loadSettings)
    initialize();

    // persistant settings between sessions
    migrateOldSettings();
    iniPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/settings.ini";
    settings = new QSettings(iniPath, QSettings::IniFormat);
    G::settings = settings;
    // test if new user
    if (settings->contains("slideShowDelay") && !simulateJustInstalled) isSettings = true;
    else isSettings = false;
    loadSettings();     // except settings with dependencies ie for actions not created yet

    // update executable location - req'd by Winnets (see MW::handleStartupArgs)
    settings->setValue("appPath", qApp->applicationFilePath());
    // settings->setValue("appPath", qApp->applicationDirPath());

    // Loggers
    /*
    if (G::isLogger && G::sendLogToConsole == false) startLog();
    if (G::isErrorLogger) startErrLog();
    */

    // app stylesheet and QSetting font size and background from last session
    createAppStyle();
    createCentralWidget();      // req'd by ImageView, CompareView, ...
    createFilterView();         // req'd by DataModel (dm)
    createDataModel();          // dependent on FilterView, creates Metadata, Thumb
    createThumbView();          // dependent on QSetting, filterView
    createGridView();           // dependent on QSetting, filterView
    createTableView();          // dependent on centralWidget
    createSelectionModel();     // dependent on ThumbView, ImageView
    createSelection();          // dependent on DataModel, ThumbView, GridView, TableView
    createInfoString();         // dependent on QSetting, DataModel, //EmbelProperties?
    createInfoView();           // dependent on DataModel, Metadata, ThumbView, Filters, BuildFilters
    createImageCache();         // dependent on DataModel, Metadata, ThumbView
    createMetaRead();            // dependent on DataModel, Metadata, ThumbView, VideoView, ImageCache
    createCatalogScanner();      // dependent on nothing but Catalog + the progress bar
    createImageView();          // dependent on centralWidget, ThumbView, ImageCache
    createVideoView();          // dependent on centralWidget, ThumbView
    createCompareView();        // dependent on centralWidget
    createFSTree();             // dependent on Metadata
    createBookmarks();          // dependent on loadSettings, FSTree
    createDocks();              // dependent on FSTree, Bookmarks, ThumbView, Metadata,
                                //              InfoView, EmbelProperties
    createEmbel();              // dependent on EmbelView, EmbelDock
    createStatusBar();
    createMessageView();
    createActions();            // dependent on above
    createMenus();              // dependent on createActions and loadSettings

    /* Apply the persisted Develop enabled state now that BOTH the dock (createDocks) and
       developAction (createActions) exist -- createDocks runs first, so this cannot live in
       createDevelopDock. */
    syncDevelopPanelEnabled();   // gated by developAction AND Develop operation mode

    loadShortcuts(true);        // dependent on createActions
    setupCentralWidget();
    createStressTest();         // dependent on DataModel

    // platform specific settings (must follow dock creation)
    setupPlatform();

    // enable / disable rory functions
    // rory();

    refreshAfterImageCacheSizeChange();

    // recall previous thumbDock state in case last closed in Grid mode
    if (wasThumbDockVisible) thumbDockVisibleAction->setChecked(wasThumbDockVisible);

    // intercept events to thumbView to monitor splitter resize of thumbDock
    qApp->installEventFilter(this);

    // if isShift then set "Do not Embellish".  Note isShift also used in MW::showEvent.
    if (isShiftOnOpen) {
        embelTemplatesActions.at(0)->setChecked(true);
        embelProperties->doNotEmbellish();
    }

    qRegisterMetaType<ImageMetadata>();
    qRegisterMetaType<QVector<int>>();
    qRegisterMetaType<QSharedPointer<Issue>>("QSharedPointer<Issue>");
    qRegisterMetaType<QList<QSharedPointer<Issue>>>("QList<QSharedPointer<Issue>>");
    qRegisterMetaType<G::FolderOp>("FolderOp");

    // create popup window used for messaging
    G::newPopUp(this, centralWidget);

    // create issue log used to report errors and issues.  If fails, show popup after
    // show main window
    G::newIssueLog();

    /* Simulate startup arguments for debugging. Format matches a winnet:
       arg[0] = "Embellish<Template>", arg[1+] = image path(s).
    QString simArgs =
        "EmbellishZen2048\n"
        "/Users/roryhill/Pictures/Zen2048/2008-05-09_0063.jpg";
    // "/Users/roryhill/Pictures/2025/202503/2025-03-27/2025-03-25_0002.jpg";
    handleStartupArgs(simArgs);
    return;
    //*/

    if (isStartupArgs) {
        if (G::useProcessEvents) qApp->processEvents();
        handleStartupArgs(args);
        return;
    }
    else {
        // First use of app
        if (!isSettings) {
            centralLayout->setCurrentIndex(StartTab);
        }
        else {
            // default to loupe display at start
            loupeDisplay("Opening app");
            // process the persistant folder if available
            if (rememberLastDir && !isShiftOnOpen) {
                if (isFolderValid(lastDir, true, true)) {
                    if (G::isLogger || G::isFlowLogger) G::log("MW::MW", "Loading lastDir " + lastDir);
                    centralLayout->setCurrentIndex(LoupeTab);
                    if (fsTree->select(lastDir)) {
                        folderSelectionChange(lastDir, G::FolderOp::Add);
                        // folderSelectionChange(lastDir, "Add");
                        updateIconRange("MW::MW rememberLastDir");
                    }
                }
            }

            // show start message
            else {
                QString msg = "Select a folder or bookmark to get started.";
                setCentralMessage(msg);
                prevMode = "Loupe";
            }
        }
    }

    // return;  // ignore recover from crash when debugging

    // recover from prior crash
    /* Skip the (modal, blocking) recovery prompt under QStandardPaths test mode
       — the soak/self-test harnesses run headless and end via std::_Exit/kill,
       which leaves hasCrashed=true; without this the next automated launch hangs
       on a dialog waiting for an answer. */
    if (settings->value("hasCrashed").toBool() && !isShiftOnOpen
        && !QStandardPaths::isTestModeEnabled()) {
        int picks = pickLogCount();
        int ratings = ratingLogCount();
        int colors = colorClassLogCount();
        QString picksRecoverable = " picks recoverable";
        if (picks == 1) picksRecoverable = " pick recoverable";
        QString ratingsRecoverable = " ratings recoverable";
        if (picks == 1) ratingsRecoverable = " rating recoverable";
        QString colorsRecoverable = " color labels recoverable";
        if (picks == 1) colorsRecoverable = " color label recoverable";
        if (picks || ratings || colors) {
            QString msg = "It appears Winnow did not close properly.  Do you want to "
                          "recover the most recent picks, ratings and color categories?\n";
            msg += "\nFolder: " + QFileInfo(lastFileIfCrash).dir().path();
            msg += "\n";
            if (picks) msg += "\n" + QString::number(picks) + picksRecoverable;
            if (ratings) msg += "\n" + QString::number(ratings) + ratingsRecoverable;
            if (colors) msg += "\n" + QString::number(colors) + colorsRecoverable;
            msg += "\n";
            int ret = QMessageBox::warning(this, "Recover Prior State", msg,
                                           QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                folderAndFileSelectionChange(lastFileIfCrash, "lastFileIfCrash");
                recoverPickLog();
                recoverRatingLog();
                recoverColorClassLog();
            }
        }
    }

    // crash log
    settings->setValue("hasCrashed", true);

    // Memory-overrun watchdog: GUI-thread QTimer that polls phys_footprint
    // independently of any subsystem. Runs continuously so it catches growth
    // during folder enumeration and queue draining, not just MetaRead bursts.
    // Interval is short (50 ms) because at high caps (e.g. 16 GB on a 24 GB
    // Mac) we have very little headroom between detecting overrun and macOS
    // killing the process — every extra millisecond risks the abort racing
    // a SIGKILL or a heap-corruption crash inside PowerLog/CoreFoundation.
    memoryWatchdog = new QTimer(this);
    memoryWatchdog->setTimerType(Qt::PreciseTimer);
    memoryWatchdog->setInterval(50);
    connect(memoryWatchdog, &QTimer::timeout, this, &MW::memoryWatchdogTick);
    memoryWatchdog->start();

    if (G::isLogger) G::log("MW::MW",  "(end of MW::MW)");
}

void MW::runSelfTest(const QString &folderPath, int settleMs)
{
/*
    Headless smoke test entry point (see tests/smoke). Opens folderPath via the
    same path the app uses at startup, lets it load for settleMs, then exits the
    app with code 0 if the data model loaded at least one image, else 2. main.cpp
    enables QStandardPaths test mode first, so this never touches real settings.
*/
    if (G::isLogger) G::log("MW::runSelfTest", folderPath);
    centralLayout->setCurrentIndex(LoupeTab);

    /* Optional concurrency-stress knobs (env-gated so the default smoke test is
       unaffected). The TSan proxy run (tests/tsan/run_tsan_proxy.sh) sets these to
       reproduce the QSortFilterProxyModel data race: a recursive multi-subfolder
       load (structural proxy inserts on the GUI thread) running concurrently with
       navigation that keeps the image-cache/decoder threads reading dm->sf. */
    const bool recurse = qEnvironmentVariableIntValue("WINNOW_SELFTEST_RECURSE") == 1;
    const int navMs = qEnvironmentVariableIntValue("WINNOW_SELFTEST_NAV_MS");

    /*  WINNOW_SELFTEST_MODELTEST=1 attaches QAbstractItemModelTester to the
        datamodel and the proxy for the whole load.

        It is here permanently, rather than as a one-off, because DataModel is a
        hand-written QAbstractTableModel now: the model contract -- index and
        parent consistency, the begin/end pairing around every insertion and
        removal, headerData, flags, the row and column counts -- is this code's
        responsibility and no longer the base class's. Fatal mode so a violation
        fails the run rather than scrolling past in a log; ctest's
        model_contract target is what runs it. Verified to catch a real
        violation (beginInsertRows off by one) before being trusted. */
    if (qEnvironmentVariableIntValue("WINNOW_SELFTEST_MODELTEST") == 1) {
        new QAbstractItemModelTester(dm, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
        new QAbstractItemModelTester(dm->sf, QAbstractItemModelTester::FailureReportingMode::Fatal, this);
        fprintf(stderr, "SELFTEST: model contract tester attached\n");
    }

    if (fsTree->select(folderPath))
        folderSelectionChange(folderPath, G::FolderOp::Add, /*resetDataModel*/true, recurse);

    /*  WINNOW_SELFTEST_CATALOG_QUERY loads a CATALOG SEARCH instead of stopping at
        the folder, so the other of the two load paths gets exercised headlessly.

        It had none. --selftest loads a FOLDER, and MW::loadCatalogResults --
        images from any number of folders as one browsable set, which is the whole
        point of Catalog scope -- was reachable only by a person typing in the Find
        dock. Every measurement of catalog behaviour so far has been indirect:
        through the unit tests, or by reading the database afterwards. Restructuring
        the load path with one of its two callers uncovered is how something breaks
        quietly.

        The folder load above still runs first, and deliberately: it is what puts
        rows in the catalog for the query to find. An empty query is legal and
        means "the most recent images", which is what Catalog scope shows when
        nothing has been typed.

        Timed from the folder load's settle so the catalog commit has happened;
        the query then replaces the model exactly as the Find dock would. */
    const QString catalogQuery = qEnvironmentVariable("WINNOW_SELFTEST_CATALOG_QUERY");
    if (!catalogQuery.isNull()) {
        QTimer::singleShot(settleMs / 2, this, [this, catalogQuery]() {
            QThreadPool::globalInstance()->waitForDone(30000);   // let the capture land
            CatalogQuery q;
            q.text = catalogQuery;
            int total = 0;
            /*  searchRows AND loadCatalogRows, because that is what the Find dock does.
                This used to run search() + loadCatalogResults(), which is now the older
                paths shape kept for the separate Catalog panel -- leaving the live path
                uncovered is exactly the hole this test was written to close. */
            const QVector<CatalogRow> hits = Catalog::instance().searchRows(q, 100000,
                                                                           &total);
            fprintf(stderr, "SELFTEST: catalogQuery='%s' hits=%lld total=%d\n",
                    catalogQuery.toLocal8Bit().constData(), (long long)hits.size(), total);
            fflush(stderr);
            if (hits.isEmpty()) {
                fprintf(stderr, "SELFTEST: FAIL the catalog query matched nothing\n");
                fflush(stderr);
                std::_Exit(4);
            }
            setScope(G::Scope::Catalog, "selftest");
            loadCatalogRows(hits, false, q);
        });
    }

    /*  WINNOW_SELFTEST_CATALOG_SCAN=<folder> DRIVES THE BACKGROUND SCANNER, the one
        way to say "index my library" and the last part of the catalog with no coverage
        at all. It was reachable only by opening Preferences > Catalog > Catalogued
        Folders and clicking Scan Now, which nothing headless does -- and it crashed on
        the first line of MW::startCatalogScan for every user, because catalogView was
        the one member of its group declared without an initialiser and an
        uninitialised pointer passes `if (p)`. The scan was never queued, so the only
        images that ever reached the catalog were the ones the opportunistic capture
        picked up from folders the user had browsed.

        THE ASSERTION IS `scanned`, NOT `indexed`. The scanner skips work already done
        (Catalog::staleOf), and the fixture folder this runs against has just been
        loaded and catalogued by the folder load above -- so a correct scan indexes
        nothing and `indexed` is legitimately 0. `scanned` is the number of files it
        actually looked at, which is what "the scanner ran" means here.

        The result is held in a shared_ptr rather than a member because both this
        lambda and the settle-exit one below need it and nothing else does. */
    struct ScanResult { bool finished = false; int scanned = 0; int indexed = 0;
                        bool aborted = false; };
    auto scanResult = std::make_shared<ScanResult>();

    const QString catalogScanRoot = qEnvironmentVariable("WINNOW_SELFTEST_CATALOG_SCAN");
    if (!catalogScanRoot.isNull()) {
        if (catalogScanner) {
            connect(catalogScanner, &CatalogScanner::finished, this,
                    [scanResult](int scanned, int indexed, bool aborted) {
                        scanResult->finished = true;
                        scanResult->scanned = scanned;
                        scanResult->indexed = indexed;
                        scanResult->aborted = aborted;
                    });
        }
        /*  Started after the folder load has settled, deliberately: the scanner pauses
            whenever the datamodel is being modified, so kicking it off during the load
            would measure the pause rather than the scan. */
        QTimer::singleShot(settleMs / 2, this, [this, catalogScanRoot]() {
            catalogRoots = QStringList{catalogScanRoot};
            catalogRootsRecurse = true;
            fprintf(stderr, "SELFTEST: catalogScan root=%s\n",
                    catalogScanRoot.toLocal8Bit().constData());
            fflush(stderr);
            startCatalogScan();
        });
    }

    /* Drive navigation during the settle window. Sweeps forward to the last row
       (reporting when it gets there), then ping-pongs, keeping the cache target
       range moving through rows the GUI thread is still inserting/sorting.
       WINNOW_SELFTEST_STRESS=1 additionally replicates the pick + ingest workflow
       and reverses the sort mid-load (sortReverse) — proxy/filter mutation running
       concurrently with the worker threads reading dm->sf, the suspected race. */
    const bool stress = qEnvironmentVariableIntValue("WINNOW_SELFTEST_STRESS") == 1;
    if (navMs > 0) {
        QTimer *navTimer = new QTimer(this);
        navTimer->setInterval(navMs);
        int *tick = new int(0);
        int *cursor = new int(0);          // explicit sweep position (single-select)
        int *dir = new int(1);             // +1 forward, -1 backward
        bool *reachedEnd = new bool(false);
        connect(navTimer, &QTimer::timeout, this, [this, tick, cursor, dir, reachedEnd, stress]() {
            if (!dm || dm->sf->rowCount() < 2) return;
            const int last = dm->sf->rowCount() - 1;
            ++(*tick);

            // advance the sweep cursor; ping-pong at the ends, report reaching the end once
            *cursor += *dir;
            if (*cursor >= last) {
                *cursor = last;
                *dir = -1;
                if (!*reachedEnd) {
                    *reachedEnd = true;
                    fprintf(stderr, "SELFTEST: reached end at row %d\n", last);
                    fflush(stderr);
                }
            }
            else if (*cursor <= 0) { *cursor = 0; *dir = 1; }

            // setCurrentRow single-selects (ClearAndSelect) and drives the image cache
            sel->setCurrentRow(*cursor);

            if (stress) {
                /* Mutate the proxy/filter during load to stress the worker
                   threads reading dm->sf. Picks and ingest were removed: they
                   write the pick/ingest logs, so a crash or kill mid-run left
                   state that triggered the "recover prior state" dialog on the
                   next launch, blocking automated reruns. sortReverse is the
                   side-effect-free proxy stressor. */
                if (*tick % 90 == 0) {                          // re-sort proxy during load
                    isReverseSort = !isReverseSort;
                    sortReverse();
                }
            }
        });
        navTimer->start();
    }

    QTimer::singleShot(settleMs, this, [this, folderPath, scanResult]() {
        const int rows = dm ? dm->rowCount() : 0;
        if (!qEnvironmentVariable("WINNOW_SELFTEST_CATALOG_QUERY").isNull()) {
            /*  The catalog load replaced the model; a run that ends with the
                folder's rows still in it loaded nothing, which is the failure
                this mode exists to catch. */
            fprintf(stderr, "SELFTEST: catalogLoaded rows=%d scope=%d\n",
                    rows, int(G::scope));
            fflush(stderr);
            if (rows == 0) std::_Exit(5);
        }
        fprintf(stderr, "SELFTEST: folder=%s rows=%d\n",
                folderPath.toLocal8Bit().constData(), rows);
        /*  WINNOW_SELFTEST_CATALOG=1 asserts that the folder just loaded actually
            reached the catalog. That path had NO automated coverage and was
            silently broken: MW::folderChangeCompleted read catalogRows() inline,
            catalogRows() skips any row not yet G::MetaLoaded, and the writes that
            set that status arrive as queued events -- so the list was often empty
            and the folder was never indexed. It depended on nothing but timing.

            The wait is here and not on the default path because waiting lets pool
            tasks queue GUI callbacks that then run around the model-contract
            test's own row removal, which made that gate flaky. The two modes are
            deliberately separate. */
        if (qEnvironmentVariableIntValue("WINNOW_SELFTEST_CATALOG") == 1) {
            QThreadPool::globalInstance()->waitForDone(30000);
            const int catalogued = Catalog::instance().count();
            fprintf(stderr, "SELFTEST: catalogued=%d rows=%d\n", catalogued, rows);
            fflush(stderr);
            if (catalogued <= 0) {
                fprintf(stderr, "SELFTEST: FAIL the folder did not reach the catalog\n");
                fflush(stderr);
                std::_Exit(3);
            }
        }
        /*  WINNOW_SELFTEST_CATALOG_SCAN asserts the background scanner actually ran.
            Reaching here at all is most of the point -- startCatalogScan used to
            dereference an uninitialised catalogView and take the process with it, so a
            crash IS the failure this guards against -- but "did not crash" is not
            enough on its own: a scan that silently walked nothing would look identical.
            scanned > 0 is what says it examined files. See the kickoff above for why
            the assertion is not on `indexed`. */
        if (!qEnvironmentVariable("WINNOW_SELFTEST_CATALOG_SCAN").isNull()) {
            fprintf(stderr,
                    "SELFTEST: catalogScan finished=%d scanned=%d indexed=%d aborted=%d\n",
                    int(scanResult->finished), scanResult->scanned,
                    scanResult->indexed, int(scanResult->aborted));
            fflush(stderr);
            if (!scanResult->finished || scanResult->aborted || scanResult->scanned <= 0) {
                fprintf(stderr, "SELFTEST: FAIL the catalog scan did not run\n");
                fflush(stderr);
                std::_Exit(6);
            }
        }

        /*  A REMOVAL, so the model-contract run covers more than insertion.
            Loading a folder only ever APPENDS, so with the tester attached and
            nothing else done the begin/endRemoveRows pairing went unchecked --
            found by deliberately breaking it and watching the test still pass.
            Taken from the MIDDLE, which is the case that re-addresses every row
            after it and the one a resize() would get wrong. */
        if (dm && rows > 2 &&
            qEnvironmentVariableIntValue("WINNOW_SELFTEST_MODELTEST") == 1) {
            dm->removeRows(1, 1);
            fprintf(stderr, "SELFTEST: removed row 1, rows now %d\n", dm->rowCount());
        }
        fflush(stderr);
        // Exit immediately, skipping Qt/C++ teardown. We've measured health at the
        // loaded steady state; forcing the event loop to unwind here delivers
        // in-flight queued signals to a half-destroyed MW (teardown assert). The
        // orderly shutdown path (window close) is not what this smoke test covers.
        std::_Exit(rows > 0 ? 0 : 2);
    });
}

void MW::runDevelopStressTest(const QString &folderPath, int durationMs)
{
/*
    Headless DEVELOP stress driver, for ThreadSanitizer (tests/tsan/run_tsan_develop.sh).

    WHY IT EXISTS: --selftest and --soaktest exercise the folder-load concurrency only.
    Neither enters Develop, builds a mask or triggers a proxy render, so nothing in the
    suite went near the interactive render path -- which is precisely the part that now
    runs on a worker (developProxyPool) while the GUI thread keeps touching the same
    caches. This drives that collision on purpose.

    WHAT IT RACES:
      o a simulated brush drag -- the pending submask's stroke grows and is pushed through
        setActiveMaskParams, which is exactly what ImageView emits, so every tick runs
        updateMaskOverlayTint + renderDevelopPreview -> worker;
      o a simulated ADJUSTMENT drag alternating the Global and mask scopes
        (selfTestNudgeAdjustment). A brush drag moves the MASK; this moves PARAMS, which
        is the path that writes the render scratch buffers and re-arms the hot
        prefix/layer, so without it the whole adjustment side went unraced;
      o veil toggles, which flip whether the GUI thread composites at all;
      o IMAGE SWITCHES mid-drag, so a worker completes against a changed currentFilePath
        and its result must be discarded;
      o direct renderDevelopPreview() calls, standing in for the crop/warp/level callers
        that re-render WITHOUT bumping developParamsGen -- the case developProxyPending
        exists for;
      o folder re-selection, which clears developStackCache and the fold cache from the GUI
        thread while a worker may be reading them.

    Exits by std::_Exit like runSelfTest: TSan reports each race live as a WARNING, and the
    script scans the log rather than trusting an exit code.
*/
    if (G::isLogger) G::log("MW::runDevelopStressTest", folderPath);
    centralLayout->setCurrentIndex(LoupeTab);

    const int submasks = qMax(1, qEnvironmentVariableIntValue("WINNOW_DEVTEST_SUBMASKS"));
    const int loadMs   = qMax(1000, qEnvironmentVariableIntValue("WINNOW_DEVTEST_LOAD_MS"));
    const int tickMs   = qMax(1, qEnvironmentVariableIntValue("WINNOW_DEVTEST_TICK_MS"));

    /* WINNOW_DEVTEST_SERIAL=1 caps the GLOBAL pool at one thread, which makes every
       developParallelRows / maskParallelFor / Develop::parallelFor take its SERIAL path.
       That removes the QtConcurrent boundary from the picture, leaving exactly one axis of
       concurrency -- the GUI thread against the proxy worker -- which is what this test
       exists to validate.

       It matters because the default log is dominated by an ARTIFACT, not by findings:
       Homebrew Qt is not TSan-instrumented, so QFuture::waitForFinished()'s happens-before
       edge is invisible to the tool. A parallel-for's lambdas capture the caller's stack by
       reference; once the caller returns and reuses that stack, every one of those reads is
       reported as a race. Confirmed by A/B: the same reports appear with the proxy render
       forced back onto the GUI thread, where that concurrency cannot exist. */
    if (qEnvironmentVariableIntValue("WINNOW_DEVTEST_SERIAL") == 1) {
        QThreadPool::globalInstance()->setMaxThreadCount(1);
        fprintf(stderr, "DEVTEST: global pool capped at 1 (parallel-for serial)\n");
        fflush(stderr);
    }

    /* NO SIDECAR WRITES. The driver makes thousands of edits; left on, the debounce would
       litter the test folder with .xmp files -- and the first run of this test did exactly
       that to tests/fixtures. flushAll() on quit is moot: the driver ends with _Exit. */
    G::isDevelopDebounceWrite = false;

    if (fsTree->select(folderPath))
        folderSelectionChange(folderPath, G::FolderOp::Add, /*resetDataModel*/true, false);

    /* Let the folder load and the first image decode before Develop is switched on: the
       mode change rebuilds the image cache, and racing it with the initial load would
       measure a different thing (that path is what run_tsan.sh already covers). */
    QTimer::singleShot(loadMs, this, [this, folderPath, submasks, tickMs, durationMs]() {
        const int rows = dm ? dm->sf->rowCount() : 0;
        if (rows < 1) {
            fprintf(stderr, "DEVTEST: no images in %s\n", folderPath.toLocal8Bit().constData());
            fflush(stderr);
            std::_Exit(2);
        }
        sel->setCurrentRow(0);
        setOperationMode(G::OperationMode::Develop);

        if (!developProperties->selfTestAddMaskScope(submasks)) {
            fprintf(stderr, "DEVTEST: could not build a mask scope (no current image)\n");
            fflush(stderr);
            std::_Exit(2);
        }
        fprintf(stderr, "DEVTEST: rows=%d submasks=%d tick=%dms\n", rows, submasks, tickMs);
        fflush(stderr);

        /* The drag. Each tick appends a point to the pending stroke and hands it over the
           way ImageView does, then every so often perturbs the state the worker is racing
           against. Counters are heap-allocated because the lambda outlives this scope. */
        QTimer *drag = new QTimer(this);
        drag->setInterval(tickMs);
        int *tick = new int(0);
        connect(drag, &QTimer::timeout, this, [this, tick, rows, submasks]() {
            ++(*tick);
            /* Re-arm after a churn: re-selecting the folder resets the datamodel, which
               reloads each image's stack from its sidecar -- and the scope built above was
               never written to one. Without this the drag silently degrades into rendering
               an empty stack, which is what the first run of this driver actually did. */
            if (developProperties->activeScopeComponents().size() < submasks) {
                if (!developProperties->selfTestAddMaskScope(submasks)) return;
            }
            const int n = 2 + (*tick % 40);          // stroke grows, then restarts
            QString pts;
            for (int i = 0; i < n; ++i) {
                const double t = double(i) / double(n);
                pts += QString("%1,%2").arg(0.10 + 0.80 * t)
                                       .arg(0.30 + 0.35 * qSin(t * 6.0));
                if (i < n - 1) pts += ",";
            }
            developProperties->setActiveMaskParams(
                QString("{\"size\":22,\"flow\":100,\"autoMask\":false,\"strokes\":"
                        "[{\"size\":22,\"feather\":0,\"flow\":100,\"erase\":false,"
                        "\"autoMask\":false,\"pts\":[%1]}]}").arg(pts));

            /* ADJUSTMENT drag, alternating Global and the mask scope. The brush drag
               above moves the MASK; this moves PARAMS, which is a different path through
               the interactive cache and the only one that touches the render scratch
               buffers the worker writes (accScratch / layScratch / outScratch /
               preScratch) and the A/B alternation that keeps the worker off a buffer the
               cache is serving as const. Global and mask scope alternate because they
               invalidate different halves: a Global nudge moves the base signature, so
               the hot prefix is recaptured every tick; a mask-scope nudge leaves the base
               alone, so the prefix survives and the layer churns instead.

               TWO cadences, both measured rather than guessed, because a render spans
               ~16 ticks and anything faster than that never lets a cache entry survive:

                 . PAUSE the nudge for runs of ticks. Nudging every tick means a scope's
                   params ALWAYS differ between renders, so hotParamsSame is never true
                   and the cached LAYER is never reused -- `prefix +layer` was 0 of 79
                   renders. The quiet windows are brush-only, which is what makes the
                   layer survive, and that path is the whole point of the stack cache.
                 . HOLD a scope across several windows. Alternating Global/mask per tick
                   moves the base constantly, so hotAt() rejects the prefix every render:
                   `resume -1 / noPrefix` on 7 of 7. Holding gave 8 of 79 resuming. */
            const bool nudging = (*tick / 30) % 2 == 0;
            if (nudging)
                developProperties->selfTestNudgeAdjustment((*tick / 120) % 2 == 0,
                                                           0.05f * float(*tick % 20) - 0.5f);

            if (*tick % 7 == 0)  imageView->toggleMaskTint();      // veil on/off mid-drag
            if (*tick % 11 == 0) renderDevelopPreview(false);      // the crop-style caller
            if (*tick % 23 == 0 && rows > 1)                       // switch image mid-drag
                sel->setCurrentRow(*tick / 23 % rows);
        });
        drag->start();

        /* Periodically yank the ground out from under the worker: a folder re-selection
           clears developStackCache and the mask fold cache on the GUI thread. */
        QTimer *churn = new QTimer(this);
        churn->setInterval(qMax(500, durationMs / 8));
        connect(churn, &QTimer::timeout, this, [this, folderPath]() {
            folderSelectionChange(folderPath, G::FolderOp::Add, /*resetDataModel*/true, false);
        });
        churn->start();

        QTimer::singleShot(durationMs, this, [this, tick]() {
            fprintf(stderr, "DEVTEST: done ticks=%d\n", *tick);
            fflush(stderr);
            std::_Exit(0);
        });
    });
}

void MW::runMetaTest(const QString &filePath)
{
/*
    End-to-end metadata read (see tests/metadata). Reads filePath through the full
    Metadata pipeline (the same loadImageMetadata the Reader uses), prints the key
    parsed fields, and exits 0 if make/model/dimensions came back (optionally
    matching WINNOW_METATEST_MAKE/MODEL substrings), else 2. main.cpp enables
    QStandardPaths test mode first, so this never touches real settings.
*/
    if (G::isLogger) G::log("MW::runMetaTest", filePath);
    QFileInfo fi(filePath);
    const bool ok = metadata->loadImageMetadata(fi, 0, 0, true, true, false, false,
                                                "MW::runMetaTest");
    const ImageMetadata &m = metadata->m;
    fprintf(stderr, "METATEST: ok=%d make=[%s] model=[%s] w=%d h=%d\n",
            ok ? 1 : 0,
            m.make.toLocal8Bit().constData(),
            m.model.toLocal8Bit().constData(),
            m.width, m.height);
    fflush(stderr);

    // Expected make/model substrings are supplied by the test (env), so this code
    // stays generic and the fixture's identity lives in the test registration.
    const QByteArray expMake  = qgetenv("WINNOW_METATEST_MAKE");
    const QByteArray expModel = qgetenv("WINNOW_METATEST_MODEL");
    const bool pass = ok
        && m.width > 0 && m.height > 0
        && !m.make.isEmpty() && !m.model.isEmpty()
        && (expMake.isEmpty()
            || m.make.contains(QString::fromLocal8Bit(expMake), Qt::CaseInsensitive))
        && (expModel.isEmpty()
            || m.model.contains(QString::fromLocal8Bit(expModel), Qt::CaseInsensitive));
    std::_Exit(pass ? 0 : 2);
}

void MW::whenActivated(Qt::ApplicationState state)
{
    // NOT BEING USED (REMOVED FROM MAIN.CPP)
/*
    Invoked after the application becomes active.

    This is signalled when QGuiApplication::applicationStateChanged (connect
    in Main()).

    Update FSTree after it has been opened.
    Update display resolution and resize main window
*/
    #ifdef Q_OS_MAC
    fsTree->setRootIndex(fsTree->model()->index(0,0));
    #endif
    #ifdef Q_OS_WIN
    // sometimes windows not shown all drives yet
    fsTree->refreshModel();
    #endif

    qDebug() << "MW::whenActivated";

    // display resolution
    setDisplayResolution();
    updateDisplayResolution();
    emit resizeMW(this->geometry(), centralWidget->geometry());

    // moved from contructor because created a glitch in dock title bar - don't know why.
    createPreferences();

    if (G::issueLog->failedToOpen) {
        QString popupMsg = "The issue log file is open, preventing Winnow from writing issues<br>"
                           "to file.<p>";
        G::popup->showPopup(popupMsg, 3000, true, 0.75, Qt::AlignLeft);
        qDebug() << "MW::whenActivated" << popupMsg;
    }

    /* Settings that could not be read at startup (see G::startupWarnings). loadSettings
       runs before the popup exists, so the report waits until here. Shown as ONE popup so
       several bad settings cannot bury the user in toasts. */
    if (!G::startupWarnings.isEmpty()) {
        G::popup->showPopup(G::startupWarnings.join("<p>"), 5000, true, 0.75,
                            Qt::AlignLeft);
        qDebug() << "MW::whenActivated startupWarnings" << G::startupWarnings;
        G::startupWarnings.clear();
    }
}

//   EVENT HANDLERS

void MW::showEvent(QShowEvent *event)
{
/*

*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::showEvent");

    // exit if already initialized (ie when moving window)
    if (!G::isInitializing) {
        QMainWindow::showEvent(event);
        return;
    }

    // restore prior geometry and state
    if (isSettings) {
        /* Versioned restore (winnowStateVersion). A WindowState saved by a build that
           predates a dock has no place for it; left to Qt that dock is dumped loose into
           an area, which is how developDock once ended up as orphaned empty tab bars whose
           layout never converged (the tabbed docks flickered continuously -- diagnosed via
           persistent count==0 tab bars in a perpetual LayoutRequest loop). The version tag
           is what identifies such a state, but it is MIGRATED, not discarded:
           restoreWindowState() restores it at its own version and then docks the panels
           added since (see MW::placeDocksAddedSince). Rejecting it instead, as this used
           to, quietly reset the user's entire layout -- thumbDock included, which fell
           back to the left area under the folder group -- every time a dock was added. */
        restoreGeometry(settings->value("Geometry").toByteArray());
        bool restored = restoreWindowState(settings->value("WindowState").toByteArray());
        restoreGeometry(settings->value("Geometry").toByteArray());
        // unreadable state (or none): the initialize() layout is not a usable one
        if (!restored) defaultWorkspace();
    }
    else {
        defaultWorkspace();
    }

    // Apply persisted per-dock collapsed flag. Deferred so the just-restored
    // dock geometries have settled before setCollapsed() snapshots them.
    QTimer::singleShot(0, this, &MW::applyDockCollapseState);

    if (G::mode == "Loupe" && !thumbDock->isVisible()) {
        thumbDock->setVisible(true);
        thumbDock->raise();
        thumbDockVisibleAction->setChecked(true);
        qDebug() << "MW::showEvent2"
                 << "thumbView->iconWidth =" << thumbView->iconWidth
                 << "thumbView->iconHeight =" << thumbView->iconHeight;
    }

    // initial status bar icon state
    updateStatusBar();

    // set initial visibility in embellish template
    embelTemplateChange(embelProperties->templateId);

    // size columns if device pixel ratio > 1
    embelProperties->resizeColumns();

    // req'd for color management
    getDisplayProfile();

    // set screen attributes in global
    setDisplayResolution();

    /* Hide the Develop tool at startup: Winnow always opens in Preview mode. History and
       Presets are part of the Develop tool (tabbed with it, shown and hidden with it), so
       they must be hidden here too -- restoreState() above re-shows every dock the last
       session had visible, and hiding Develop alone left them on screen, visible but
       disabled. All three actions are unchecked so the View menu agrees. */
    closeDevelopDock();     // hides develop + history + presets, unchecks their actions

    QMainWindow::showEvent(event);

    qApp->setStyleSheet(G::css);

    fsTree->setRootIndex(fsTree->model()->index(0,0));

    G::isInitializing = false;

    /*  Seed the Catalog rows once the index can be asked. Not earlier: the
        catalog opens lazily and a count taken before that reads as "no index". */
    updateCatalogScopeRows();

    G::issueBeginSession();

    if (G::issueLog->failedToOpen) {
        QString popupMsg = "The issue log file failed to open, preventing Winnow<br>"
                           "from writing issues to file.<p>"
                           "File: " +
                           QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                           "/Log/WinnowIssueLog.txt<p>"
                           "Press ESC to continue."
                            ;
        G::popup->reset();
        G::popup->showPopup(popupMsg, 0, true, 1.0, Qt::AlignCenter);
        qDebug() << "MW::showEvent" << popupMsg;
    }

    /* Silent check for a newer Winnow, deferred so it never delays launch.  Only when
       the user preference is on and not in an automated test run. */
    if (checkIfUpdate && !G::isTest)
        QTimer::singleShot(4000, this, [this]{ checkForUpdate(/*silent*/true); });
}

void MW::closeEvent(QCloseEvent *event)
{
    if (G::isLogger || G::isFlowLogger) {
        G::log();
        G::log("MW::closeEvent");
    }
    setCentralMessage("Closing Winnow ...");

    // do not allow if there is a background ingest in progress
    if (G::isRunningBackgroundIngest) {
        QString msg =
                "There is a background ingest in progress.  When it<br>"
                "has completed Winnow will close."
                ;
        G::popup->showPopup(msg, 0);
        while (G::isRunningBackgroundIngest) G::wait(100);
    }

    // for debugging crash test
    //if (testCrash) return;

    // persist any unsaved per-image Develop edits to their sidecars before teardown
    if (developProperties) developProperties->flushAll();

    /* Persist the develop-preview index. Cheap (one small JSON write) and skipped when
       nothing changed. Losing it costs only re-renders, so it is not worth guarding. */
    DevPreviewCache::instance().save();

    /*  Let the queued thumbnails finish. Bounded by the queue cap, so this is a
        short wait, not a save -- losing them would only cost re-decoding on the
        next visit, but finishing is nearly free and the alternative is throwing
        away work already paid for. */
    ThumbCache::instance().flush();

    stop("MW::closeEvent");

    // metaRead->stopReaders();
    metaRead->stop();
    imageCache->stop();

    /* End the background catalog scan. Its thread checks between files, so this returns
       promptly, and anything already committed is kept -- the scan resumes from staleOf
       next time rather than starting over. */
    if (catalogScanner) catalogScanner->stop();

    if (filterDock->isVisible()) {
        folderDock->raise();
        folderDockVisibleAction->setChecked(true);
    }

    G::issueLog->stop();

    clearPickLog();
    clearRatingLog();
    clearColorClassLog();

    // close slide show
    if (G::isSlideShow) slideShow();

    // crash log
    settings->setValue("hasCrashed", false);

    if (G::popup != nullptr) G::popup->close();
    if (zoomDlg != nullptr) zoomDlg->close();
    hide();

    // close all open dialogs
    if (preferencesDlg != nullptr) {
        delete pref;
        delete preferencesDlg;
    }
    for (QWidget *w : openWindows) {
        if (w && w->isVisible()) {
            w->close();
        }
    }
    // close any HtmlWindow help pages parented anywhere under MW
    for (auto *hw : findChildren<HtmlWindow*>()) {
        hw->close();
    }

    // check and delete focus stack
    if (fsPipeline) {
        fsPipeline->requestAbort();
        fsThread->quit();
        fsThread->wait();
    }

    if (!simulateJustInstalled) {
        writeSettings();
    }
    delete workspaces;
    delete recentFolders;
    delete ingestHistoryFolders;
    delete embel;
    G::deleteIssueLog();
    /* Null the global before destroying: posted events can still drain after
       closeEvent and many call sites guard on G::popup != nullptr. */
    Popup *popup = G::popup;
    G::popup = nullptr;
    delete popup;

    event->accept();
}

void MW::moveEvent(QMoveEvent *event)
{
/*
    When the main winnow window is moved the zoom dialog, if it is open, must be moved as
    well. Also we need to know if the app has been dragged onto another monitor, which may
    have different dimensions and a different icc profile (win only).
*/
    if (G::isLogger) G::log("MW::moveEvent");

    QMainWindow::moveEvent(event);
    if (!G::isInitializing) {
        setDisplayResolution();
        updateDisplayResolution();
        emit resizeMW(this->geometry(), centralWidget->geometry());
    }
//    setWindowFlags(windowFlags() & (~Qt::WindowStaysOnTopHint));
//    show();
}

void MW::resizeEvent(QResizeEvent *event)
{
    QString fun = "MW::resizeEvent";
    if (G::isLogger) {
        G::log(fun);
    }
    QMainWindow::resizeEvent(event);

    // re-position zoom dialog
    emit resizeMW(this->geometry(), centralWidget->geometry());

    // prevent progressBar overlapping in statusBar
    updateProgressBarWidth();

    // update current workspace
    ws.isMaximised = isMaximized();
}

void MW::changeEvent(QEvent *event) {
/*
    Not being used.
*/
    QMainWindow::changeEvent(event);
}

void MW::keyPressEvent(QKeyEvent *event)
{
    if (G::isLogger) G::log("MW::keyPressEvent");
    // qDebug() << "MW::keyPressEvent" << event;

    if (G::isEnterKey(event)) {
        if (G::mode == "Loupe") {
            if (dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool()) {
                if (G::useMultimedia) videoView->playOrPause();
            }
        }
        else {
            loupeDisplay("MW::keyPressEvent");
        }
    }
}

void MW::keyReleaseEvent(QKeyEvent *event)
{
    if (G::isLogger) G::log("MW::keyReleaseEvent");

    // qDebug() << "MW::keyReleaseEvent" << event;

    if (event->key() == Qt::Key_Escape) {
        /* Cancel the current operation without exiting from full screen mode.  If no current
           operation, then okay to exit full screen.  escapeFullScreen must be the last option
           tested.
        */
        // qDebug() << "event->key() == Qt::Key_Escape";
        G::popup->reset();
        // end stress test
        if (G::isStressTest) G::isStressTest = false;
        // stop selecting a new folder in FSTree
        // else if (fsTree->isSelectingFolders) {
        //     G::stop = true;
        //     qDebug() << "";
        //     return;
        // }
        // stop loading a new folder
        else if (!G::allMetadataAttempted) stop("Escape key");
        // stop background ingest
        // else if (G::isRunningBackgroundIngest) backgroundIngest->stop();
        // stop file copying
        else if (G::isCopyingFiles) G::stopCopyingFiles = true;
        // cancel slideshow
        else if (G::isSlideShow) slideShow();
        // abort Embellish export process
        else if (G::isProcessingExportedImages) emit abortEmbelExport();
        // abort color analysis
        else if (G::isRunningColorAnalysis) emit abortHueReport();
        // abort focus stacking
        else if (fsPipeline) emit abortFocusStack();
        // exit full screen mode
        else if (fullScreenAction->isChecked()) escapeFullScreen();
        // FSTree or Bookmarks disabled
        else if (!fsTree->isEnabled()) fsTree->setEnabled(true);
        else if (!bookmarks->isEnabled()) bookmarks->setEnabled(true);
        // stop building filters
        // else if (filters->buildingFilters) buildFilters->stop();
    }

    if (G::isSlideShow) {
        int n = event->key() - 48;
        QVector<int> delay {0,1,2,3,5,10,30,60,180,600};

        if (slideShowTimer->isActive()) {
            if (event->key() == Qt::Key_X) {
                nextSlide();
            }
            else if (event->key() == Qt::Key_Backspace) {
                prevRandomSlide();
            }
            else if (event->key() == Qt::Key_S) {
                //qDebug() << "MW::keyReleaseEvent" << "slideCount =" << slideCount << event;
                if (slideCount > 1) slideShow();
            }
            else if (event->key() == Qt::Key_W) {
                // slideShowTimer->stop();
                isSlideShowWrap = !isSlideShowWrap;
                isSlideShowHelpVisible = true;
                QString msg;
                if (isSlideShowWrap) msg = "Slide wrapping is on.";
                else msg = "Slide wrapping is off.";
                G::popup->showPopup(msg);
            }
            else if (event->key() == Qt::Key_H) {
                slideShowTimer->stop();
                slideshowHelpMsg();
            }
            else if (event->key() == Qt::Key_Space) {
                slideShowTimer->stop();
                G::popup->showPopup("Slideshow is paused", 0);
            }
            else if (event->key() == Qt::Key_R) {
                isSlideShowRandom = !isSlideShowRandom;
                slideShowResetSequence();
                QString msg;
                if (isSlideShowRandom) msg = "Random selection enabled.";
                else msg = "Sequential selection enabled.";
                G::popup->showPopup(msg);
            }
            // quick change slideshow delay 1 - 9 seconds
            else if (n > 0 && n <= 9) {
                slideShowDelay = delay[n];
                slideShowResetDelay();
                QString msg = "Slideshow interval set to " + QString::number(slideShowDelay) + " seconds.";
                G::popup->showPopup(msg);
            }
        }
        else {  // slideshow is inactive
            if (isSlideShowHelpVisible) {
                G::popup->reset();
                isSlideShowHelpVisible = false;
            }
            if (event->key() == Qt::Key_Space) {
                G::popup->showPopup("Slideshow is active");
                nextSlide();
                slideShowTimer->start(slideShowDelay * 1000);
            }
        }

    }

    bool isVideoMode = centralLayout->currentIndex() == VideoTab;
    if (isVideoMode) {
        if (event->key() == Qt::Key_Space) {
            if (G::useMultimedia) videoView->playOrPause();
        }
        if (event->key() == Qt::Key_Escape) {
            if (G::useMultimedia) videoView->pause();
        }
    }

    // if (event->key() == Qt::Key_Space) {
    //     G::popUp->reset();
    // }

    QMainWindow::keyReleaseEvent(event);
}

bool MW::thumbViewHasFocus() const
{
    const QWidget *fw = QApplication::focusWidget();
    if (!fw || !thumbView) return false;
    return fw == thumbView || thumbView->isAncestorOf(fw);
}

/* The bare/Shift'd navigation keys, plus the pick-nav and random-image combos, all move
   the selection off the current image. In Develop mode they are suppressed unless the
   thumbnails have the focus (see developShortcutIntercept). Alt+key is EXCLUDED:
   Alt+arrow / Alt+Home etc scroll and pan within the image rather than changing the
   selection, which is useful while developing -- but Ctrl+Shift+Alt+Left/Right IS pick
   navigation, hence the Ctrl test. */
static bool isSelectionKey(const QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Left:   case Qt::Key_Right:
    case Qt::Key_Up:     case Qt::Key_Down:
    case Qt::Key_PageUp: case Qt::Key_PageDown:
    case Qt::Key_Home:   case Qt::Key_End:
        break;
    default:
        return false;
    }
    const Qt::KeyboardModifiers m = e->modifiers();
    if ((m & Qt::AltModifier) && !(m & Qt::ControlModifier)) return false;  // scroll/pan
    return true;
}

/* Is the focus widget a place where a typed LETTER is text rather than a shortcut? Only
   the search field and an editable combo qualify. A spin box's internal QLineEdit does
   NOT: it takes numbers, so a letter typed there is a Develop shortcut.

   A READ-ONLY combo does not qualify either, and the distinction is not academic: the
   Develop dock's rows are combos (View transform is the very first row, WB preset the
   next), a combo keeps the focus after a pick, and treating it as text entry bailed out
   of the whole arbiter -- so the next R/S/O/N/M/W/X/H/P went to the global action bound
   to that key (usually nothing) instead of the Develop one. All a read-only combo loses
   is its type-to-select-an-item search, which the mode-local keys outrank. */
static bool isTextEntryWidget(const QWidget *fw)
{
    if (!fw) return false;
    if (const QLineEdit *le = qobject_cast<const QLineEdit *>(fw))
        return !qobject_cast<QAbstractSpinBox *>(le->parentWidget());
    const QComboBox *cb = qobject_cast<const QComboBox *>(fw);
    return cb && cb->isEditable();
}

bool MW::developShortcutIntercept(QEvent *event)
{
/*
    Develop mode's shortcut arbiter, called from eventFilter for both
    QEvent::ShortcutOverride and QEvent::KeyPress. Two rules, in order:

      1. A key in developShortcuts runs its Develop action in place of the global action
         bound to the same key (S = spot tool here, Slideshow in Preview).
      2. A selection key is swallowed unless thumbView has the focus, so arrows and page
         keys cannot navigate off the image being developed.

    Everything else falls through and behaves exactly as it does in Preview mode. Returns
    true when the key has been consumed.

    Both event types are handled, in the two-step ReplacePanel::eventFilter established:
    accept the ShortcutOverride (which is what frees the key from the global QAction --
    Qt sends it before the shortcut system runs), then act on the KeyPress that follows.
    Doing the work on the KeyPress rather than the override matters twice over. It is the
    only event Qt guarantees for keys NO global action owns (R/N/M/H), and consuming it
    makes rule 2 a real "ignore": accepting only the override would free an arrow from
    keyDownAction and then let it reach the loupe, where QGraphicsView scrolls on arrows.

    Tool-local keys ([ ] etc) are not in the table, so an armed tool still gets them from
    its own ShortcutOverride claim in ImageView::event -- tool-local outranks mode-local.
*/
    QKeyEvent *e = static_cast<QKeyEvent *>(event);
    QWidget *fw = QApplication::focusWidget();
    const bool isOverride = event->type() == QEvent::ShortcutOverride;

    /* Never arbitrate a dialog's keystrokes. This mirrors the MODELESS DIALOG SHORTCUT
       GUARD below, which cannot cover these keys: it works off ownsShortcut(), and a
       Develop action has no QKeySequence for that to match. Without this a bare S typed
       into a Preferences field would arm the spot tool. Floating QDockWidgets are also
       separate top-level windows but belong to the MW workspace, hence the QDialog
       test. */
    QWidget *win = fw ? fw->window() : nullptr;
    if (win && win != this && qobject_cast<QDialog *>(win)) return false;

    /* Esc collapses the active MASK tool (hide its settings), like the other Develop
       tools exit on Esc. Checked BEFORE the value-editor guard below: while a mask is
       edited focus is usually on a dock slider (Hue/Sat) or the Color Range wheel, so the
       guard would otherwise swallow Esc first. Yields to the WB dropper and Transform,
       whose own Esc handlers run below. Escape owns no global QAction, so only KeyPress
       arrives. */
    if (e->key() == Qt::Key_Escape && !e->isAutoRepeat() && !isOverride && developProperties
        && !developProperties->isWbDropperActive() && !developCropEditing
        && developProperties->escapeMaskTool()) {
        event->accept();
        return true;
    }

    /* A value editor owns its keys: arrows nudge a Develop slider, letters type into the
       search field. But a slider or spin box has no use for a LETTER, and focus sits on a
       Develop dock slider for most of a session -- bailing there sent the letter on to
       the GLOBAL action bound to it (O opened the folder dialog instead of toggling the
       overlay, S started a slideshow, X rejected the image), which is exactly what this
       arbiter exists to prevent. So a letter in developShortcuts is still arbitrated over
       a slider/spin box; only real text entry keeps its letters unconditionally. */
    const int key = e->key();
    const bool developLetter = key >= Qt::Key_A && key <= Qt::Key_Z
                               && G::bareModifiers(e) == Qt::NoModifier
                               && !e->isAutoRepeat() && developShortcuts.contains(key);
    if (isTextEntryWidget(fw)) return false;
    if (!developLetter &&
        (qobject_cast<QAbstractSlider *>(fw) || qobject_cast<QAbstractSpinBox *>(fw) ||
         qobject_cast<QLineEdit *>(fw)))
        return false;

    /* 1b. Esc disarms the white-balance dropper. It is armed from the Develop dock, so
       focus is in the dock and ImageView never sees the key -- the arbiter is the only
       place that reliably gets it, the same reason Transform's Esc lives here. Checked
       BEFORE the Transform block: the dropper is the more recently armed tool, so it
       owns Esc while it is up. Escape owns no global QAction, so only KeyPress
       arrives. */
    if (developProperties && developProperties->isWbDropperActive()
        && e->key() == Qt::Key_Escape && !e->isAutoRepeat()) {
        event->accept();
        if (!isOverride) developProperties->cancelWbDropper();
        return true;
    }

    /* 1b(ii). Esc disarms the Detail preview's location picker, for exactly the reason
       the dropper above needs it here: it is armed from the Develop dock, so focus is in
       the dock and ImageView::keyPressEvent never sees the key. */
    if (developProperties && developProperties->isDetailPickActive()
        && e->key() == Qt::Key_Escape && !e->isAutoRepeat()) {
        event->accept();
        if (!isOverride) developProperties->cancelDetailPick();
        return true;
    }

    /* 1a. While a Transform (crop/level/warp) is active, its A/F/C/L/W act on the
       Transform panel and must beat the window-level shortcuts on those keys (A Run
       Droplet, C Compare ...). This runs before Qt's shortcut system and independent of
       which widget holds focus, so it works whether the panel or the crop overlay is
       focused. Text editors (aspect combo, angle field) are already excluded by the
       value-editor guard above, so typing still works. Contextual, so NOT in
       developShortcuts. */
    if (developCropEditing && transformPanel && G::bareModifiers(e) == Qt::NoModifier
        && !e->isAutoRepeat()) {
        const int k = e->key();
        if (k == Qt::Key_A || k == Qt::Key_F || k == Qt::Key_C ||
            k == Qt::Key_L || k == Qt::Key_W) {
            event->accept();
            if (!isOverride) transformPanel->handleTransformShortcut(k);
            return true;
        }
        /* Esc cancels the whole transform session (discard + hide the panel), not just
           the active mode. Escape owns no global QAction, so only KeyPress arrives. */
        if (k == Qt::Key_Escape) {
            event->accept();
            if (!isOverride) cancelDevelopTransform();
            return true;
        }
        /* Enter/Return commits the transform and closes the panel, like pressing R again
           (toggleDevelopTransform's commit-on-hide). EXCEPTION: while a warp quad is
           traced, Enter commits the quad (rectify) instead -- leave it to ImageView's own
           warp-commit path (ImageView::event / keyPressEvent), so fall through here. */
        if (G::isEnterKey(k) && !(imageView && imageView->cropIsWarp())) {
            event->accept();
            if (!isOverride) toggleDevelopTransform();
            return true;
        }
    }

    /* 1. Develop mode local shortcut beats the global action on the same key. Held keys
       must not re-fire: every one of these is a toggle or pops a dialog. */
    if (G::bareModifiers(e) == Qt::NoModifier && !e->isAutoRepeat()) {
        if (QAction *a = developShortcuts.value(e->key())) {
            event->accept();
            if (!isOverride) {          // the override only frees the key; act now
                if (G::isLogger) G::log("MW::developShortcutIntercept", a->objectName());
                a->trigger();
            }
            return true;
        }
    }

    // 2. Selection keys are inert unless the user is deliberately in the thumbnails
    if (isSelectionKey(e) && !thumbViewHasFocus()) {
        event->accept();
        return true;
    }

    return false;
}

bool MW::eventFilter(QObject *obj, QEvent *event)
{
    // return false to propagate events

    /* ALL EVENTS (G::showAllEvents) */
    {
        if (G::showAllEvents)
        {
            if (event->type()
                                     != QEvent::Paint
                    && event->type() != QEvent::UpdateRequest
                    && event->type() != QEvent::ZeroTimerEvent
                    && event->type() != QEvent::Timer
                    && event->type() != QEvent::MouseMove
                    && event->type() != QEvent::HoverMove
                    )
            {
                qDebug() << "MW::eventFilter"
                         << "event:" << event << "\t"
                         << "event->type:" << event->type() << "\t"
                         << "obj:" << obj << "\t"
                         << "obj->objectName:" << obj->objectName()
                         << "object->metaObject()->className:" << obj->metaObject()->className()
                            ;
                //return QWidget::eventFilter(obj, event);
            }
        }
    }

    /* TOOLTIP POSITION

       Show every widget tooltip ourselves via showDockToolTip so the gap below
       the cursor is consistent and matches Windows (macOS otherwise places
       tooltips too far below the cursor). As a qApp event filter this runs
       before each widget's own ToolTip handling, so it covers all widgets that
       use setToolTip(), including the dock title bars. Widgets with a
       dynamically computed tooltip (dock tabs, thumbDock) have an empty
       toolTip() and fall through to their specific branches below.
    */
    if (event->type() == QEvent::ToolTip) {
        if (QWidget *w = qobject_cast<QWidget *>(obj)) {
            QHelpEvent *he = static_cast<QHelpEvent *>(event);
            QString tip = w->toolTip();
            /* Item views deliver the ToolTip event to their viewport and supply
               the text via the model's ToolTipRole rather than setToolTip().
               IconView is excluded: its delegate (IconViewDelegate::helpEvent)
               builds per-symbol tooltips itself and already routes through
               showDockToolTip, so we let that event through. */
            bool isItemViewport = false;
            if (tip.isEmpty()) {
                QAbstractItemView *view = qobject_cast<QAbstractItemView *>(w->parentWidget());
                if (view && view->viewport() == w && !qobject_cast<IconView *>(view)) {
                    isItemViewport = true;
                    QModelIndex idx = view->indexAt(he->pos());
                    if (idx.isValid()) tip = idx.data(Qt::ToolTipRole).toString();
                }
            }
            if (!tip.isEmpty()) {
                showDockToolTip(he->globalPos(), tip, w);
                return true;
            }
            /* An item-view cell with no ToolTipRole must show nothing. Actively
               hide any tip still on screen (e.g. left over from a previously
               hovered cell, or the same cell on the previous image) and consume
               the event so the default delegate can't leave a stale tooltip up. */
            if (isItemViewport) {
                QToolTip::hideText();
                return true;
            }
        }
    }

    /* DEBUG KEY PRESSES (uncomment to use)
    if(event->type() == QEvent::ShortcutOverride && obj->objectName() == "MWClassWindow")
    {
        G::log("MW::eventFilter", "Performance profiling");
        qDebug() << event <<  obj;
    }
    //    */

    /* ALL KEY PRESSES HIDE POPUP
    if (!G::isInitializing &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)
        )
    {
        if (event->type() == QEvent::ShortcutOverride) {
            QShortcutEvent *e = static_cast<QShortcutEvent *>(event);
            qDebug() << "MW::eventFilter" << e->type() << e;
        }
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *e = static_cast<QKeyEvent *>(event);
            qDebug() << "MW::eventFilter" << e->type() << e << e->modifiers()
                     << "obj:" << obj << "\t"
                     << "obj->objectName:" << obj->objectName()
                     << "object->metaObject()->className:" << obj->metaObject()->className()
                ;
        }
        if (G::popUp->isVisible()) {
            qDebug() << "MW::eventFilter ending popup" << event->type();
            G::popUp->end();
        }
    }
    //*/

    /* MOUSE BUTTON CLICK
    {
        if (event->type() == QEvent::MouseButtonPress) {
            qDebug() << event << obj << QCursor::pos();
        }
    }   //*/

    /* MOUSE BUTTON DOUBLE CLICK
    {
        if (event->type() == QEvent::MouseButtonDblClick) {
            qDebug() << event << obj << QCursor::pos();
        }
    }   //*/

    /* DEBUG SPECIFIC EVENT (uncomment to use)
    if (obj->objectName() == "VideoWidget") {
        if (event->type() == QEvent::MouseMove) {
            qDebug() << event << obj << QCursor::pos();
        }
    }//*/

    /* DEBUG SPECIFIC OBJECTNAME (uncomment to use)
    if (obj->objectName() == "QTabBar") {
       // if (event->type()        != QEvent::Paint
       //         && event->type() != QEvent::UpdateRequest
       //         && event->type() != QEvent::ZeroTimerEvent
       //         && event->type() != QEvent::Timer
       //         && event->type() == QEvent::Enter
       //         )
        {
            qDebug() << "MW::eventFilter"
                     << event << "\t"
                     << event->type() << "\t"
                     << obj << "\t"
                     << obj->objectName();
//            qDebug() << event;
        }
    }
//    */

    /* NATIVE GESTURE LOGITECH SIDE KEY IN CENTRAL WIDGET
       This is used when navigating and the current image cannot be displayed in
       ImageView.  The central widget is switched to the MessageTab to explain
       that the image cannot be rendered.
    */
    {
        if (!G::isInitializing && (event->type() == QEvent::NativeGesture)) {
            if (obj->objectName() == "centralWidget") {
                static int prevLayoutIndex = -1;
                if (prevLayoutIndex == MessageTab) {
                    /*
                    qDebug() << "MW::eventFilter QEvent::NativeGesture"
                             << "obj->objectName:" << obj->objectName()
                             << "row =" << dm->currentSfRow
                                ; //*/
                    QNativeGestureEvent *e = static_cast<QNativeGestureEvent *>(event);
                    int direction = static_cast<int>(e->value());
                    mouseSideKeyPress(direction);
                }
                prevLayoutIndex = centralLayout->currentIndex();
            }
        }
    }

    /* DEVELOP MODE LOCAL SHORTCUTS
       The ShortcutOverride pass runs before Qt's shortcut system, so a Develop key beats
       the global action bound to it; the KeyPress pass does the work (and swallows the
       selection keys). Must precede the navigation intercept below, which would otherwise
       act on an arrow first. See MW::developShortcutIntercept. */
    {
        if (!G::isInitializing && G::operationMode == G::OperationMode::Develop
            && (event->type() == QEvent::ShortcutOverride
                || event->type() == QEvent::KeyPress)) {
            if (developShortcutIntercept(event)) return true;
        }
    }

    /* DEVELOP MODE: hold Space to borrow the loupe zoom/pan gesture over a mask / spot /
       crop tool (click toggles zoom, drag pans a zoomed image); release resumes the tool.
       ImageView usually lacks keyboard focus in Develop, so drive it from this global
       filter, which sees KeyPress AND KeyRelease. Space is bound to zoomToggleAction
       globally, so the ShortcutOverride pass is accepted to free the key (as
       developShortcutIntercept does) and stop that action firing. A value editor keeps
       Space as a literal character; auto-repeat is ignored so a held key can't
       flicker. */
    {
        if (!G::isInitializing && G::operationMode == G::OperationMode::Develop && imageView
            && (event->type() == QEvent::ShortcutOverride
                || event->type() == QEvent::KeyPress
                || event->type() == QEvent::KeyRelease)) {
            QKeyEvent *e = static_cast<QKeyEvent *>(event);
            if (e->key() == Qt::Key_Space) {
                QWidget *fw = QApplication::focusWidget();
                const bool editor =
                    qobject_cast<QAbstractSlider *>(fw)  || qobject_cast<QAbstractSpinBox *>(fw) ||
                    qobject_cast<QLineEdit *>(fw)        || qobject_cast<QComboBox *>(fw);
                if (!editor) {
                    /* Accept frees the key from the global shortcut on the override
                       pass (so zoomToggleAction can't fire); KeyPress/Release acts. */
                    event->accept();
                    if (event->type() == QEvent::KeyPress && !e->isAutoRepeat())
                        imageView->setSpacePanOverride(true);
                    else if (event->type() == QEvent::KeyRelease && !e->isAutoRepeat())
                        imageView->setSpacePanOverride(false);
                    return true;
                }
            }
        }
    }

    /* DEVELOP MODE: mask combine modifiers + commit, while a submask is OPEN.
       Opt previews Subtract and Shift+Opt previews Intersect -- momentary, so the veil
       must follow the key with the mouse stationary, and the panel usually holds focus
       (the same reason Space and Esc are driven from this filter). Return commits with
       whatever is held, like the Transform panel's Enter-commit. Auto-repeat is ignored.
       The Alt KeyPress is accepted so Windows does not open the menu bar under us.

       The gate is isSubmaskOpen(), not isMaskPanelOpen(): a submask RE-OPENED from the
       submask list is not "pending", but Return still closes it ("Done") and Shift still
       retargets its brush attributes at the last stroke, so the panel's scope header has
       to follow the key there too. The op preview itself stays pending-only --
       syncPendingMaskOp guards that for us. */
    {
        if (!G::isInitializing && G::operationMode == G::OperationMode::Develop
            && developProperties && developProperties->isSubmaskOpen()
            && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease
                || event->type() == QEvent::ShortcutOverride)) {
            QKeyEvent *e = static_cast<QKeyEvent *>(event);
            const int k = e->key();
            if ((k == Qt::Key_Alt || k == Qt::Key_Shift) && !e->isAutoRepeat()) {
                /* The event's own modifiers do not yet include the key being pressed (nor
                   exclude the one being released), so read the live state instead. */
                syncPendingMaskOp();
                /* Shift retargets the brush attributes at the last stroke: say so while
                   it is held, so the header names the scope the NEXT drag will use. */
                developProperties->syncAttributeScopeLabel();
                /* The brush cursor's centre glyph (+ / - / x) reads the held modifier
                   directly, so it has to repaint even when syncPendingMaskOp changes
                   nothing -- the op is pinned to Add on the first submask, and unchanged
                   ops return early without touching the veil. */
                if (imageView) imageView->viewport()->update();
                if (k == Qt::Key_Alt && event->type() != QEvent::ShortcutOverride) {
                    event->accept();
                    return true;
                }
            }
            /* No value-editor bail here, unlike the rest of this filter: while a
               submask is being defined the focus is almost always on one of the
               panel's own sliders (Size/Feather/Flow), which is exactly when Return
               must still commit. Same reasoning as the Esc-collapses-a-mask rule in
               developShortcutIntercept, likewise checked BEFORE the editor bail.
               Only a text field keeps Return. */
            else if (G::isEnterKey(k) && !e->isAutoRepeat()
                     && !qobject_cast<QLineEdit *>(QApplication::focusWidget())) {
                event->accept();       // frees the key on the override pass
                if (event->type() == QEvent::KeyPress)
                    developProperties->commitPendingMask();   // resolves the op itself
                return true;
            }
        }
    }

    /* DEVELOP MODE: the sharpening MASK preview is momentary in TWO inputs -- Opt held,
       and the Masking slider's handle held -- so both edges have to be reported: the Opt
       key (reached only when the mask combine modifiers above did not consume it; while a
       submask is open, mask editing owns Opt) and the mouse press/release that starts and
       ends the drag. The Alt press is NOT accepted: it carries no action of its own here,
       and swallowing it would take Alt away from everything else in the window.
       DevelopProperties decides whether the Masking slider is the one being dragged; this
       only reports that something changed. */
    if (!G::isInitializing && G::operationMode == G::OperationMode::Develop
        && developProperties) {
        const QEvent::Type t = event->type();
        if (t == QEvent::KeyPress || t == QEvent::KeyRelease) {
            QKeyEvent *e = static_cast<QKeyEvent *>(event);
            if (e->key() == Qt::Key_Alt && !e->isAutoRepeat())
                developProperties->syncSharpenMaskPreview();
        }
        /* Posted AFTER the press/release has been delivered: the slider sets isSliderDown
           in its own handler, so reading it here (before it runs) would be one event
           stale -- the preview would appear on the release and vanish on the press. */
        else if (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonRelease) {
            QMetaObject::invokeMethod(developProperties,
                                      &DevelopProperties::syncSharpenMaskPreview,
                                      Qt::QueuedConnection);
        }
    }

    /* Clear the Space zoom/pan override if the app deactivates while Space is held -- a
       release delivered to another app would otherwise leave the override stuck on. */
    if (event->type() == QEvent::ApplicationDeactivate && imageView)
        imageView->setSpacePanOverride(false);

    /* A modifier released while another app had the focus never reaches us, which would
       leave the overlay previewing Subtract for ever; re-read it on the way back in. */
    if (event->type() == QEvent::ApplicationActivate) syncPendingMaskOp();
    /* Same for the sharpening mask preview, in both directions: deactivating with Opt
       held must drop the tint, since the release will be delivered elsewhere. */
    if ((event->type() == QEvent::ApplicationActivate
         || event->type() == QEvent::ApplicationDeactivate) && developProperties)
        developProperties->syncSharpenMaskPreview();

    /* KEYPRESS INTERCEPT (NAVIGATION and MODIFIERS) */
    {
        if (!G::isInitializing && (event->type() == QEvent::KeyPress)) {
            QKeyEvent *e = static_cast<QKeyEvent *>(event);
            Qt::KeyboardModifiers k = e->modifiers();
            /*
            qDebug() << "MW::eventFilter"
                  << "obj->objectName:" << obj->objectName().leftJustified(25)
                  << "key =" << e->key()
                  << k
                     ; //*/
            // Return/Enter show loupe mode
            if (G::isEnterKey(e)) {
                if (obj->objectName() == "Thumbnails" ||
                    obj->objectName() == "Grid"
                   )
                {
                    loupeDisplay("MW::eventFilter Enter");
                }
            }

            if (obj->objectName() == "MWWindow") {
                /*
                qDebug() << "MW::eventFilter"
                         << "obj->objectName:" << obj->objectName().leftJustified(25)
                         << "key =" << e->key()
                         << k
                            ; //*/

                // if (e->key() == Qt::Key_Return) loupeDisplay("MW::eventFilter Key_Return");  // search filter not mix with sel->save/recover

                /* Don't navigate images when a value editor (e.g. a Develop slider) has focus:
                   it consumes arrows to nudge its value. This is insurance -- the slider also
                   accepts the key so it should not reach here -- against the app event filter
                   seeing the key at MWWindow as it propagates up an unconsumed key. */
                QWidget *fw = QApplication::focusWidget();
                bool editorHasFocus =
                    qobject_cast<QAbstractSlider*>(fw)  ||
                    qobject_cast<QAbstractSpinBox*>(fw) ||
                    qobject_cast<QLineEdit*>(fw)        ||
                    qobject_cast<QComboBox*>(fw);

                /* In Develop mode an arrow only reaches here when the thumbnails have the
                   focus -- developShortcutIntercept (above) consumes the others. */

                // faster than using menu shortcuts
                if (e->key() == Qt::Key_Right && !editorHasFocus) {
                    if (G::isLogger || G::isFlowLogger) G::log("MW::eventFilter Key_Right");
                    sel->next(e->modifiers());
                }
                if (e->key() == Qt::Key_Left && !editorHasFocus) {
                    if (G::isLogger || G::isFlowLogger) G::log("MW::eventFilter Key_Left");
                    sel->prev(e->modifiers());
                }
                // if (e->key() == Qt::Key_Up) sel->up(e->modifiers());
                // if (e->key() == Qt::Key_Down) sel->down(e->modifiers());
                // if (e->key() == Qt::Key_Home) sel->first(e->modifiers());
                // if (e->key() == Qt::Key_End) sel->last(e->modifiers());
                // if (e->key() == Qt::Key_PageUp) sel->prevPage(e->modifiers());
                // if (e->key() == Qt::Key_PageDown) sel->nextPage(e->modifiers());
            }
        }
    }

    /* QUIT FULLSCREEN */
    {
        if (obj->objectName() == "MWWindow") {
            // try using raise()
            if (event->type() == QEvent::WindowStateChange) {
                if (wasFullSpaceOnDiffScreen) {
                    wasFullSpaceOnDiffScreen = false;
                    // wait for transition to showNormal is finished
                    QTimer::singleShot(100, this, &MW::invokeCurrentWorkspace);
                }
            }
        }
    }

    /* KEEP WINDOW ON TOP WHEN DRAGGING TO ANOTHER SCREEN
       Window is underneath apps on another screen on MacOS (sometimes)
       Set and unset bit flags example:
       setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
       setWindowFlags(windowFlags() & (~Qt::WindowStaysOnTopHint));
    */
    /*
    PREVENTING MACOS MINIMIZE (SEEMS TO NOT BE REQ'D)
    {
        if (obj->objectName() == "MW") {
            // try using raise()
            if (event->type() == QEvent::NonClientAreaMouseButtonPress) {
                QMouseEvent *e = static_cast<QMouseEvent *>(event);
                if (e->button() == Qt::LeftButton) {
                    raise();
                }
            }
            if (event->type() == QEvent::NonClientAreaMouseButtonRelease) {
                QMouseEvent *e = static_cast<QMouseEvent *>(event);
                if (e->button() == Qt::LeftButton) {
                    raise();
                }
            }
        }
    }
    CHATGPT SUGGESTED CODE:
    static bool dragging = false; // Track dragging state

    if (obj->objectName() == "MW") {
        if (event->type() == QEvent::NonClientAreaMouseButtonPress) {
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::LeftButton) {
                dragging = true;  // User started dragging
            }
        }
        if (event->type() == QEvent::NonClientAreaMouseButtonRelease) {
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::LeftButton) {
                dragging = false;  // User released the window
            }
        }
        if (dragging && event->type() == QEvent::Move) {
            raise();  // Keep window on top only while dragging
        }
    }
    */

    /* EMBEL DOCK TITLE

    Set dock title if not tabified

    if (obj->objectName() == "embelDock" && event->type() == QEvent::Enter) {
        qDebug() << event;
        if (!embelDockTabActivated) embelTitleBar->setTitle("Embellish");
        else embelTitleBar->setTitle("");
        embelDockTabActivated = false;
    }
    */

    /* CONTEXT MENU
        - Intercept context menu to enable/disable and rename based on folder:
            - eject usb drive menu item
            - add bookmarks menu item
        - mouseOverFolder is used in folder related context menu actions instead of
          currentViewDir
    */
    {

        if (event->type() == QEvent::ContextMenu) {

            // default enable
            // qDebug() << "MW::eventFilter QEvent::ContextMenu";
            copyFolderPathFromContextAction->setEnabled(true);
            revealFileActionFromContext->setEnabled(true);
            deleteFSTreeFolderAction->setEnabled(true);
            eraseUsbActionFromContextMenu->setEnabled(true);
            ejectActionFromContextMenu->setEnabled(true);
            addBookmarkActionFromContext->setEnabled(true);
            pasteFilesAction->setEnabled(Utilities::clipboardHasUrls());

            QString folderName = "";
            mouseOverFolderPath = "";

            /*  Alternative ways to get folder and path
                path = idx0.data(Qt::ToolTipRole).toString();
                folder = idx0.data().toString();
                folder = idx0.data(QFileSystemModel::FileNameRole).toString()
                folder = QFileInfo(path).fileName()
            */

            // FSTree
            if (obj == fsTree->viewport()) {
                QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
                QModelIndex idx = fsTree->indexAt(e->pos());
                if (idx.isValid()) {
                    QModelIndex idx0 = idx.sibling(idx.row(), 0);
                    folderName = idx0.data(QFileSystemModel::FileNameRole).toString();
                    mouseOverFolderPath = idx0.data(QFileSystemModel::FilePathRole).toString();

                    // is there a bookmark for this folder
                    if (bookmarks->bookmarkPaths.contains(mouseOverFolderPath)) {
                        addBookmarkActionFromContext->setEnabled(false);
                    }
                    /*
                    qDebug() << "MW::eventFilter QEvent::ContextMenu"
                             << "folderName =" << folderName
                             << "mouseOverFolderPath =" << mouseOverFolderPath
                             // << "event =" << event
                             // << "obj->objectName() =" << obj->objectName()
                             // << " =" <<
                                ; //*/
                }
            }

            // Bookmarks (favActions menu)
            if (obj == bookmarks->viewport()) {
                QContextMenuEvent *e = static_cast<QContextMenuEvent *>(event);
                QModelIndex idx = bookmarks->indexAt(e->pos());
                // mouseOverFolderPath = idx.data(Qt::ToolTipRole).toString();
                if (idx.isValid()) {
                    QModelIndex idx0 = idx.sibling(idx.row(), 0);
                    folderName = idx0.data().toString();
                    // tooltip is set to path when bookmark is added
                    mouseOverFolderPath = idx0.data(Qt::ToolTipRole).toString();
                    /*
                    qDebug() << "MW::eventFilter QEvent::ContextMenu"
                             << "mouseOverFolderPath =" << mouseOverFolderPath
                             << "folderName =" << folderName
                             << "event =" << event
                             << "obj->objectName() =" << obj->objectName()
                                ; //*/
                }
            }

            // disable if not click on folder
            if (mouseOverFolderPath == "") {
                copyFolderPathFromContextAction->setEnabled(false);
                revealFileActionFromContext->setEnabled(false);
                deleteFSTreeFolderAction->setEnabled(false);
                eraseUsbActionFromContextMenu->setEnabled(false);
                ejectActionFromContextMenu->setEnabled(false);
                addBookmarkActionFromContext->setEnabled(false);
                // pasteFilesAction->setEnabled(false);
            }

            // rename
            if (folderName.length()) {
                renameCopyFolderPathAction(folderName);
                renameRevealFileAction(folderName);
                renameDeleteFolderAction(folderName);
                renameEraseMemCardFromContextMenu(mouseOverFolderPath);
                renameEjectUsbMenu(mouseOverFolderPath);
                renamePasteFilesAction(folderName);
                renameAddBookmarkAction(folderName);
                renameRemoveBookmarkAction(folderName);
            }

            // continue to open context menu
            return false;
        }
    }

    /* THUMBVIEW ZOOMCURSOR
    Turn the cursor into a frame showing the ImageView zoom amount in the thumbnail.  The
    ImageView must be zoomed and no modifier keys pressed to show zoom cursor.
    */
    {
        if (thumbView->mouseOverThumbView) {
            if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
                QKeyEvent *e = static_cast<QKeyEvent *>(event);
                if (e->modifiers() != 0) {
                    thumbView->setCursor(Qt::ArrowCursor);
                    //qDebug() << "MW::eventFilter" << "Modifier pressed" << event;
                }
                else {
                    QPoint pos = thumbView->mapFromGlobal(QCursor::pos());
                    QModelIndex idx = thumbView->indexAt(pos);
                    // qDebug() << "MW::eventFilter" << "Modifier not pressed" << event << pos << idx;
                    if (idx.isValid()) {
                        QString src = "MW::eventFilter ZoomCursor";
                        thumbView->zoomCursor(idx, src, /*forceUpdate=*/true, pos);
                    }
                }
            }
        }

        if (obj == thumbView->viewport()) {
            if (event->type() == QEvent::MouseMove) {
                QMouseEvent *e = static_cast<QMouseEvent *>(event);
                bool noModifiers = e->modifiers() == 0;
                const QModelIndex idx = thumbView->indexAt(e->pos());

                // in thumb part of cell?
                QRect *tRect = nullptr;
                if (thumbView->iconViewDelegate->thumbRectCache.contains(idx.row()))
                    tRect = thumbView->iconViewDelegate->thumbRectCache[idx.row()];
                bool inThumb = false;
                if (tRect && tRect->contains(e->pos())) inThumb = true;

                // qDebug() << idx.isValid() << noModifiers << "In thumb =" << inThumb;
                if (idx.isValid() && noModifiers && inThumb) {
                    QString src = "MW::eventFilter";
                    thumbView->zoomCursor(idx, src, /*forceUpdate=*/false, e->pos());
                }
                else {
                    thumbView->setCursor(Qt::ArrowCursor);
                }
            }
        }
    }

    /* DOCK TAB TOOLTIPS
       Call filterDockTabMousePress if filter tab.
       // Show a tooltip for docked widget tabs. // not used
    */
    {
        static int prevTabIndex = -1;
        // QString tabBarClassName = "QTabBar";
        // #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        // tabBarClassName = "QMainWindowTabBar";
        // #endif
        QString tb = QString(obj->metaObject()->className());
        bool isTabBar = tb == "QTabBar" || tb == "QMainWindowTabBar";
        if (isTabBar) {
            /*
            qDebug() << "MW::eventFilter obj->metaObject()->className() ="
                     << obj->metaObject()->className();
            */
            // Responsive dock tab titles: show text when the titles fit, else
            // switch to white graphics (see MW::updateDockTabGraphics).
            if (G::useDockTitleGraphic
                    && event->type() != QEvent::Paint
                    && event->type() != QEvent::UpdateRequest) {
                updateDockTabGraphics(qobject_cast<QTabBar *>(obj));
            }

            // build filters when filter tab mouse clicked
            if (event->type() == QEvent::MouseButtonPress) {
                QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
                QMouseEvent *e = static_cast<QMouseEvent *>(event);
                int i = tabBar->tabAt(e->pos());
                // A graphic-mode tab has empty text; resolve its identity from
                // the learned QMainWindow tab key.
                QString id = tabBar->tabText(i);
                if (id.isEmpty()) id = dockTabTitleByKey.value(tabBar->tabData(i).toULongLong());
                if (id == filterDockTabText) {
                    /*
                    qDebug() << "MW::eventFilter filterDock mouse press";*/
                    filterDockTabMousePress();
                }
            }

            // dynamic tooltip on dock tabs (computed at hover time)
            if (event->type() == QEvent::ToolTip) {
                QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
                QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
                int i = tabBar->tabAt(helpEvent->pos());
                if (i >= 0) {
                    // A graphic-mode tab has empty text; resolve its identity
                    // (for the tooltip) from the learned QMainWindow tab key.
                    QString id = tabBar->tabText(i);
                    if (id.isEmpty()) id = dockTabTitleByKey.value(tabBar->tabData(i).toULongLong());
                    QString tip = dockTabToolTip(id);
                    if (!tip.isEmpty()) {
                        showDockToolTip(helpEvent->globalPos(), tip, tabBar);
                        return true;
                    }
                }
            }
        }

        // A dock resize/show changes the available width but may not resize the
        // tab bar (which shrinks to its tabs), so the tab-bar events above can
        // miss it - re-evaluate the dock tab graphics on dock resize/show too.
        // This also covers workspace switches: invokeWorkspace's restoreState
        // resizes and shows the docks, firing these events.
        if (G::useDockTitleGraphic
                && (event->type() == QEvent::Resize || event->type() == QEvent::Show)
                && qobject_cast<QDockWidget *>(obj)) {
            const QList<QTabBar *> bars = findChildren<QTabBar *>();
            for (QTabBar *b : bars) updateDockTabGraphics(b);
        }

        // thumbDock uses Qt's default title bar (no DockTitleBar widget).
        // ToolTip event only reaches thumbDock when cursor is on a non-child
        // area — i.e. the title bar — so no geometry check needed.
        if (obj == thumbDock && event->type() == QEvent::ToolTip) {
            QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
            showDockToolTip(helpEvent->globalPos(),
                            dockTabToolTip(thumbDockTabText), thumbDock);
            return true;
        }
    }
    /*
        {
            static int prevTabIndex = -1;
            QString tabBarClassName = "QTabBar";
#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
            tabBarClassName = "QMainWindowTabBar";
#endif
            if (QString(obj->metaObject()->className()) == tabBarClassName) {

            qDebug() << event << obj;

            // // Set rich text label to tabified dock widgets tabbar tabs
            // if (event->type() == QEvent::ChildAdded) {
            //     QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
            //     if (tabBarContainsDocks(tabBar)) {
            //         if (!dockTabBars.contains(tabBar)) {
            //             dockTabBars.append(tabBar);
            //             tabBarAssignRichText(tabBar);
            //             // qDebug() << "Event ChildAdded" << "dockTabBars =" << dockTabBars;
            //         }
            //     }
            // }

            // Set tab tooltip and maybe use tooltip so can identify if change tab text
            if (event->type() == QEvent::ChildAdded) {
                QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
                QFont glyphs;
                int id = QFontDatabase::addApplicationFont("/Users/roryhill/Downloads/glyphs.ttf");
                QString family = QFontDatabase::applicationFontFamilies(id).at(0);
                glyphs = QFont(family);
                tabBar->setFont(glyphs);
                for (int i = 0; i < tabBar->count(); ++i) {
                    if (tabBar->tabText(i) == folderDockTabText) {
                        tabBar->setTabToolTip(i, "System Folders Panel (F3)");
                    }
                    if (tabBar->tabText(i) == favDockTabText) {
                        tabBar->setTabToolTip(i, "Bookmarks Panel (F4)");
                    }
                    if (tabBar->tabText(i) == filterDockTabText) {
                        // tabBar->setTabIcon(i, QIcon(":/images/branch-closed-winnow.png"));
                        // tabBar->setTabData(i, filterDockTabText);
                        tabBar->setTabToolTip(i, "Filter Panel (F5)");
                        // tabBar->setTabText(i, "Filters");  not working
                    }
                    if (tabBar->tabText(i) == metadataDockTabText) {
                        tabBar->setTabToolTip(i, "Metadata Panel (F6)");
                    }
                    if (tabBar->tabText(i) == embelDockTabText) {
                        tabBar->setTabToolTip(i, "Embellish Panel (F8)");
                    }
                 }
            }

            // build filters when filter tab mouse clicked
            if (event->type() == QEvent::MouseButtonPress) {
                QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
                QMouseEvent *e = static_cast<QMouseEvent *>(event);
                int i = tabBar->tabAt(e->pos());
                if (tabBar->tabText(i) == filterDockTabText) {
                    filterDockTabMousePress();
                }

                // // if rename tab text then use tooltips to id tab
                // if (tabBar->tabToolTip(i) == "Filter Panel (F5)") {
                //     filterDockTabMousePress();
                //     qDebug() << "PRESSED";
                // }

            }

            // show tool tip for tab
            if (event->type() == QEvent::MouseMove) {      // HoverMove / MouseMove work
                QTabBar *tabBar = qobject_cast<QTabBar *>(obj);
                QMouseEvent *e = static_cast<QMouseEvent *>(event);
                int i = tabBar->tabAt(e->pos());
                QToolTip::showText(e->globalPos(), tabBar->tabToolTip(i));

            }
            if (event->type() == QEvent::Leave) {
                prevTabIndex = -1;
            }

            // // remove tab event
            // if (event->type() == QEvent::ChildRemoved) {
            //     qDebug() << event << obj;
            // }
        }
    }
    */

    /* THUMBDOCK SPLITTER

       A splitter resize of top/bottom thumbDock is happening:

       Events are filtered from qApp here by an installEventFilter in the MW contructor to
       monitor the splitter resize of the thumbdock when it is docked horizontally. In this
       situation, as the vertical height of the thumbDock changes the size of the thumbnails is
       modified to fit the thumbDock by calling thumbsFitTopOrBottom. The mouse events determine
       when a mouseDrag operation is happening in combination with thumbDock resizing. The
       thumbDock is referenced from the parent because thumbView is a friend class to MW.
    */
    {
        if (event->type() == QEvent::MouseButtonPress) {
            if (obj->objectName() == "FiltersViewport") return false;
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::LeftButton) isLeftMouseBtnPressed = true;
            /*
            qDebug() << "MW::eventFilter" << "MouseButtonPress"
                     << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                     << "isMouseDrag" << isMouseDrag
                     << obj->objectName()
                        ;
                        //*/
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *e = static_cast<QMouseEvent *>(event);
            if (e->button() == Qt::LeftButton) {
                isLeftMouseBtnPressed = false;
                isMouseDrag = false;
                // if (thumbView->thumbSplitDrag) {
                //    thumbView->scrollToRow(thumbView->midVisibleCell, "MW::eventFilter thumbSplitter");
                //    thumbView->thumbSplitDrag = false;
                // }
                /*
                qDebug() << "MW::eventFilter" << "MouseButtonRelease"
                         << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                         << "isMouseDrag" << isMouseDrag
                         << obj->objectName();
                            //*/
            }
        }

        if (event->type() == QEvent::MouseMove /*&& obj->objectName() == "MWWindow"*/) {
            if (isLeftMouseBtnPressed) isMouseDrag = true;
            /*
            // G::popUp->showPopup(msg);
            if (obj->objectName() == "ThumbnailsViewPort") {
                QMouseEvent *e = static_cast<QMouseEvent *>(event);
                if (thumbView->iconViewDelegate->missingIconRect.contains(e->pos())) {
                    QString msg = "Image does not have an embedded thumbnail";
                    int x = e->globalPos().x();
                    int y = e->globalPos().y() - 40;
                    QToolTip::showText(QPoint(x,y), msg);
                    qDebug() << "MW::eventFilter" << "MouseMove"
                         << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                         << "isMouseDrag" << isMouseDrag
                         << thumbView->iconViewDelegate->missingIconRect
                         << e->pos()
                         << obj->objectName();
                }
                // else QToolTip::hideText();
            }
            //*/
        }

        if (event->type() == QEvent::MouseButtonDblClick) {
            isMouseDrag = false;
        }

        // make thumbs fit the resized thumb dock
        if (obj == thumbDock) {
            if (event->type() == QEvent::Resize) {
                /*
                qDebug() << "MW::eventFilter" << "thumbDock::Resize"
                         << "isLeftMouseBtnPressed =" << isLeftMouseBtnPressed
                         << "isMouseDrag" << isMouseDrag
                         << obj->objectName();
                         //*/
                if (isMouseDrag) {
                    if (!thumbDock->isFloating()) {
                        Qt::DockWidgetArea area = dockWidgetArea(thumbDock);
                        if (area == Qt::BottomDockWidgetArea
                            || area == Qt::TopDockWidgetArea
                            || !thumbView->isWrapping())
                        {
                            thumbView->thumbSplitDrag = true;
                            thumbView->thumbsFitTopOrBottom();
                        }
                    }
                }
            }
        }
    }

    /* CACHE PROGRESSBAR OR CACHE STATUS MOUSE CLICK
       Show cache preferences.
    */
    {
        if (event->type() == QEvent::MouseButtonPress) {
            if (obj->objectName() == "StatusProgressLabel" ||
                obj->objectName() == "ImageCacheStatus")
            {
                preferences("ProductivityHeader");
            }
        }
    }

    /* FILTERS DOCK MOUSE CLICK
       Not allowed when in Compare Mode.
    */
    {
        if (event->type() == QEvent::MouseButtonPress) {
            if (obj->objectName() == "Filters" && G::mode == "Compare")
            {
                QString msg = "Filtering is verboten in Compare Mode";
                G::popup->showPopup(msg, 3000);
            }
        }
    }

    /* VIDEOVIEW MOUSE MOVE SHOW CURSOR
       QVideoWidget hides mouse movement - detect be monitoring paint and mouse pos.
    */
    {
        if (obj->objectName() == "VideoView") {
            if (event->type() == QEvent::Paint) {
                QPoint diff = QCursor::pos() - videoView->mousePos;
                if (qAbs(diff.x()) > 5 || qAbs(diff.y()) > 5) showMouseCursor();
            }
        }
    }

    /* MODELESS DIALOG SHORTCUT GUARD
       MW's single-key action shortcuts (rate "1", label "6", etc.) use the default
       Qt::WindowShortcut context.  Qt activates a WindowShortcut whenever the action's
       window is anywhere in the parent chain of the active window, so a modeless dialog
       parented to MW (e.g. the Preferences dialog) does NOT shield these shortcuts: a key
       typed into a Preferences spin box fires the MW shortcut instead of entering text.
       When keyboard focus is in another window and the key matches an MW-owned shortcut,
       accept the ShortcutOverride so the key is delivered to the focused widget as normal
       input rather than triggering the MW action.  (ShortcutOverride is dispatched before
       the QAction fires, so the DISABLED SHORTCUT FEEDBACK KeyPress handler below cannot
       intercept the enabled-action case on its own.)
    */
    if (event->type() == QEvent::ShortcutOverride) {
        QWidget *fw = QApplication::focusWidget();
        QWidget *win = fw ? fw->window() : nullptr;
        /* Scope to dialog windows: floating QDockWidgets are also separate top-level
           windows but are part of the MW workspace and keep their shortcut behaviour. */
        if (win && win != this && qobject_cast<QDialog *>(win)) {
            QKeyEvent *e = static_cast<QKeyEvent *>(event);
            if (ownsShortcut(QKeySequence(e->keyCombination()))) {
                e->accept();
                return true;
            }
        }
    }

    /* DISABLED SHORTCUT FEEDBACK
       Qt does not fire a disabled QAction and does not consume its shortcut, so the key
       falls through as a normal KeyPress.  When the pressed key sequence maps to a disabled
       action, show a short popup explaining why nothing happened (and consume the key).
    */
    if (!G::isInitializing && event->type() == QEvent::KeyPress) {
        QKeyEvent *e = static_cast<QKeyEvent *>(event);
        int key = e->key();
        bool isModifierOnly = key == Qt::Key_Control || key == Qt::Key_Shift ||
                              key == Qt::Key_Alt || key == Qt::Key_Meta || key == 0;
        /* Don't interfere with text entry (search filter, rename, etc.), and don't speak
           for a dialog's keystrokes (the ShortcutOverride guard above already handed those
           to the focused widget). */
        QWidget *fw = QApplication::focusWidget();
        bool isTextEntry = qobject_cast<QLineEdit *>(fw) ||
                           qobject_cast<QTextEdit *>(fw) ||
                           qobject_cast<QPlainTextEdit *>(fw);
        bool inDialog = fw && qobject_cast<QDialog *>(fw->window());
        if (!isModifierOnly && !e->isAutoRepeat() && !isTextEntry && !inDialog) {
            QKeySequence seq(e->keyCombination());
            if (QAction *a = disabledActionForShortcut(seq)) {
                G::popup->showPopup(actionDisabledReason(a), 2000);
                return true;
            }
        }
    }

    return false;
}

void MW::focusChange(QWidget *previous, QWidget *current)
{
    // if (G::isLogger) {
    //     QString s;
    //     if (previous != nullptr) s = "Previous = " + previous->objectName();
    //     if (current != nullptr) s += " Current = " + current->objectName();
    //     G::log("MW::focusChange", s);
    // }
    // // qDebug() << "MW::focusChange" << previous << current;
    // if (current == nullptr) return;
    // // following does nothing, enableGoKeyActions has been commented out.  Remove??
    // if (current->objectName() == "DisableGoActions") enableGoKeyActions(false);
    // else enableGoKeyActions(true);
    // if (previous == nullptr) return;    // suppress compiler warning
}

void MW::resetFocus()
{
    if (G::isLogger) G::log("MW::resetFocus");
    activateWindow();
    setFocus();
}

void MW::ingestFinished()
{
    if (G::isLogger) G::log("MW::ingestFinished");
    //qDebug() << "MW::ingestFinished";
    delete backgroundIngest;
    backgroundIngest = nullptr;
    G::isRunningBackgroundIngest = false;
    // Audible to signal completion
    QApplication::beep();
}

void MW::appStateChange(Qt::ApplicationState state)
{
/*
    If operating system focus changes to another app then hide the zoom dialog if it is
    visible.  If the focus is returning to Winnow then check if the zoom dialog was visible
    before and make it visible again.
*/
    if (G::isLogger) G::log("MW::appStateChange");
    if (!zoomDlg) return;
    if (state == Qt::ApplicationActive)
    {
        if (isZoomDlgVisible) zoomDlg->setVisible(true);
        resetFocus();
    }
    else {
        zoomDlg->setVisible(false);
    }
}

void MW::checkForUpdate(bool silent)
{
/*
    Checks whether a newer Winnow is available.  Distribution is plain downloads (a
    notarized DMG on macOS, an Inno Setup installer on Windows) from the DigitalOcean
    server at https://winnow.ca, so this only *notifies* the user and, on request, opens
    the platform download URL in the default browser - it does not auto-install.

    A small per-platform descriptor is fetched:
        macOS:   https://winnow.ca/winnow_mac/version.json
        Windows: https://winnow.ca/winnow_win/version.json
    Shape: { "version": "2.05",
             "url":     "https://winnow.ca/winnow_mac/current/Winnow2.05.dmg",
             "notes":   "https://winnow.ca/winnow/versions.html" }

    The fetch is asynchronous (no blocking wait); onUpdateCheckReply() handles the
    result.  Called loudly from the Help menu (silent == false) and silently at startup
    from showEvent() when checkIfUpdate is true (silent == true: only speaks when an
    update is available and has not been skipped via updateSkipVersion).
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::checkForUpdate");

    // never reach out to the network during automated / headless test runs
    if (G::isTest || G::isStressTest) return;

    QString platform;
#ifdef Q_OS_MAC
    platform = "winnow_mac";
#endif
#ifdef Q_OS_WIN
    platform = "winnow_win";
#endif
    if (platform.isEmpty()) return;     // unsupported platform

    QUrl url("https://winnow.ca/" + platform + "/version.json");

    if (updateNetManager == nullptr) {
        updateNetManager = new QNetworkAccessManager(this);
        connect(updateNetManager, &QNetworkAccessManager::finished,
                this, &MW::onUpdateCheckReply);
    }

    QNetworkRequest request(url);
    /* carry the silent flag through to the reply handler */
    request.setAttribute(QNetworkRequest::User, silent);
    updateNetManager->get(request);
}

void MW::onUpdateCheckReply(QNetworkReply *reply)
{
    QString srcFun = "MW::onUpdateCheckReply";
    if (G::isLogger || G::isFlowLogger) G::log(srcFun);

    reply->deleteLater();

    const bool silent = reply->request().attribute(QNetworkRequest::User, false).toBool();

    if (reply->error() != QNetworkReply::NoError) {
        if (!silent)
            G::popup->showPopup("Could not check for updates:<br>" + reply->errorString(), 2500);
        return;
    }

    // Get current version.json from server (from checkForUpdate())
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (!silent) G::popup->showPopup("Could not read update information.", 2000);
        return;
    }

    QJsonObject obj = doc.object();
    QString latest   = obj.value("version").toString();
    QString dmgUrl   = obj.value("url").toString();
    QString notesUrl = obj.value("notes").toString();

    QVersionNumber latestVer  = QVersionNumber::fromString(latest);
    QVersionNumber currentVer = QVersionNumber::fromString(versionNumber);

    /* Test alternative: confirm the update + download flow end-to-end against the
       server staging folder (sftp://root@165.227.46.158/var/www/html/winnow_mac/test
       -> https://winnow.ca/winnow_mac/test) instead of the live release folder.
       The deploy script uploads each new build to winnow_mac/test/ before promoting
       it to winnow_mac/current/, so flip isTest to true to have Download fetch the
       staged DMG from test/ rather than the released one from current/. */
    bool isTest = false;
    if (isTest) {
        dmgUrl.replace("/winnow_mac/current/", "/winnow_mac/test/");
        currentVer = QVersionNumber::fromString("2.04");
        qDebug() << srcFun << "TEST mode — dmgUrl redirected to test folder:" << dmgUrl;
        qDebug() << srcFun << "latest =" << latest;
    }


    if (latest.isEmpty()) {
        if (!silent) G::popup->showPopup("Could not read update information.", 2000);
        return;
    }

    if (latestVer <= currentVer) {
        if (!silent)
            G::popup->showPopup("Winnow is up to date (version " + versionNumber + ").", 2000);
        return;
    }

    // a newer version is available
    if (silent && latest == updateSkipVersion) return;      // user chose to skip this version

    updateAppDlg = new UpdateAppDlg(latest, versionNumber, notesUrl, G::css, this);
    int ret = updateAppDlg->exec();

    /* "Don't ask again for this version" is independent of the button pressed: record the
       skip if ticked, and still open the download if Download was clicked. */
    if (updateAppDlg->skipRequested()) {
        updateSkipVersion = latest;
        if (settings != nullptr) settings->setValue("updateSkipVersion", updateSkipVersion);
    }
    if (ret == QDialog::Accepted && !dmgUrl.isEmpty()) {
        downloadAndOpenUpdate(dmgUrl);
    }

    updateAppDlg->deleteLater();
}

void MW::downloadAndOpenUpdate(const QString &dmgUrl)
{
/*
    Downloads the update installer directly (no browser) and opens it, so the user
    never sees their default browser.  On macOS `open` mounts the DMG and brings its
    Finder volume window to the foreground, landing on top of the Winnow window; on
    Windows the downloaded Inno Setup installer is launched.  Progress is reported in
    G::popup.  Called from onUpdateCheckReply() when the user clicks Download.
*/
    QString srcFun = "MW::downloadAndOpenUpdate";
    if (G::isLogger || G::isFlowLogger) G::log(srcFun);

    QUrl url(dmgUrl);
    QString fileName = url.fileName();
    if (fileName.isEmpty()) fileName = "WinnowUpdate";

    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    QString destPath = QDir(dir).filePath(fileName);

    /* QSaveFile writes to a temp file and atomically renames on commit(), so a failed
       or aborted download never leaves a half-written installer at destPath. */
    auto *file = new QSaveFile(destPath);
    if (!file->open(QIODevice::WriteOnly)) {
        G::popup->showPopup("Could not save the update to<br>" + destPath, 3000);
        delete file;
        return;
    }

    /* Dedicated manager: updateNetManager's finished signal is wired to
       onUpdateCheckReply(), which would try to JSON-parse the DMG bytes. */
    auto *netManager = new QNetworkAccessManager(this);
    QNetworkReply *reply = netManager->get(QNetworkRequest(url));

    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(100);
    G::popup->setProgress(0);
    G::popup->showPopup("Downloading " + fileName + "…", 0, true, 1);

    // stream incoming bytes straight to disk rather than buffering the whole DMG
    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
        file->write(reply->readAll());
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [](qint64 received, qint64 total) {
        if (total > 0)
            G::popup->setProgress(static_cast<int>(received * 100 / total));
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, netManager, file, destPath, fileName]() {
        reply->deleteLater();
        netManager->deleteLater();

        G::popup->setProgressVisible(false);
        G::popup->reset();

        if (reply->error() != QNetworkReply::NoError) {
            file->cancelWriting();
            delete file;
            G::popup->showPopup("Update download failed:<br>" + reply->errorString(), 3500);
            return;
        }

        file->write(reply->readAll());      // flush any bytes not yet consumed
        bool committed = file->commit();    // atomically publish destPath
        delete file;
        if (!committed) {
            G::popup->showPopup("Could not finish saving the update.", 3500);
            return;
        }

        #ifdef Q_OS_MAC
            /* `open` mounts the DMG and opens its Finder window in the foreground,
               so it appears on top of the Winnow window. */
            QProcess::startDetached("/usr/bin/open", QStringList() << destPath);
        #elif defined(Q_OS_WIN)
            /* Launch the downloaded Inno Setup installer. */
            QDesktopServices::openUrl(QUrl::fromLocalFile(destPath));
        #else
            QDesktopServices::openUrl(QUrl::fromLocalFile(destPath));
        #endif
    });
}

void MW::handleStartupArgs(const QString &args)
{
/*
    Argument options:

    arg[0]  = srcProgram - usually path to launching program (Winnet)
            = <Module><Method/Template>
              "EmbellishZen2048" then "Zen2048" = Method

    arg[1+] = path to each image to view in Winnow. Only arg[1] is used to
              determine the source directory.

    Winnets are small executables that act like photoshop droplets. They reside in
    QStandardPaths::AppDataLocation (Windows: user/AppData/Roaming/Winnow/Winnets
    and Mac::/Users/user/Library/Application Support/Winnow/Winnets). They send a
    list of files and a template name to Winnow to be embellished. For example, in
    order for Winnow to embellish a series of files that have been exported from
    lightroom, Winnow needs to know which embellish template to use. Instead of
    sending the files directly to Winnow, thay are sent to an intermediary program
    (a Winnet) that is named "Embellish<Template>" (ie EmbellishZen2048). The
    Winnet forwards its own name as arg[0] followed by the file paths; Winnow
    derives the module ("Embellish") and template ("Zen2048") from arg[0].
*/
    QString fun = "MW::handleStartupArgs";
    if (G::isLogger) G::log(fun, args);

    // if (args.length() < 2) return;

    QString delimiter = "\n";
    QStringList argList = args.split(delimiter);

    // Utilities::log("MW::handleStartupArgs test", argList.join(" | "));

    // qDebug() << "MW::handleStartupArgs" << argList;
    if (argList.length() > 1) {
        if (G::isRunByExtern) Utilities::log("MW::handleStartupArgs Winnow Location", qApp->applicationDirPath());
        if (G::isRunByExtern)
            Utilities::log("MW::handleStartupArgs", argList.join(" | "));
    }

    QString srcProgram = argList.at(0);
    QStringList pathList;
    QString templateName;

    // FOCUSSTACK
    if (srcProgram.startsWith("FocusStack")) {

        // qDebug() << fun << argList;

        // check if at least 2 image paths sent, if not, close Winnow
        if (argList.length() < 3) {
            close();
            return;
        }

        QString msg = "MW::handleStartupArgs " + argList.at(0);
        Utilities::log("MW::handleStartupArgs", msg);

        show();
        raise();

        QString method = fsMethod;
        // QString method = srcProgram.mid(QString("FocusStack").length());

        QStringList paths;
        for (int i = 1; i < argList.count(); i++) {
            paths << argList.at(i);
        }
        paths.sort();

        generateFocusStack(paths, method, /*isLocal*/false);
    }

    // EMBELLISH
    else if (srcProgram.startsWith("Embellish")) {
        /* This means a remote embellish has been invoked.
                arg 0 = srcProgram = "Embellish<Template>" ie "EmbellishZen2048"
                arg 1 = Path to first image being exported to be embellished in folder
                arg 2 = Path to 2nd image
                arg...

        The template name is derived from srcProgram (arg 0): the launching winnet
        is named "Embellish<Template>", so the module and template are not sent
        explicitly.

        The information is gathered and sent to EmbelExport::exportRemoteFiles, where the
        images are embellished and saved in the manner defined by the embellish template
        in a subfolder, and then the temp image files are deleted in the folder arg 1. */

        /* show main window now.  If we don't, then the update progress popup will not be
        visible.  If there is a significant delay, when a lot of images have to be processed,
        this would be confusing for the user.  */
        show();
        raise();

        G::mode = "Loupe";

        // check if any image path sent (arg 0 + at least one path), if not, return
        if (argList.length() < 2) return;

        // get the embellish template to use (derived from srcProgram, arg 0)
        templateName = srcProgram.mid(QString("Embellish").length());

        // /* log
        // if (G::isRunByExtern)
            Utilities::log("MW::handleStartupArgs", "Template to use: " + templateName);
        //*/

        // get the folder where the files to embellish are located (arg 1)
        QFileInfo info(argList.at(1));
        QString folderPath = info.dir().absolutePath();

        // list of all supported files in the folder
        QStringList fileFilters;
        foreach (const QString &str, metadata->supportedFormats)
                fileFilters.append("*." + str);
        QDir dir;
        dir.setNameFilters(fileFilters);
        dir.setFilter(QDir::Files);
        dir.setSorting(QDir::Time );
        dir.setPath(folderPath);

        if (!dir.entryInfoList().count()) {
            qWarning() << "There are no files in folder" << folderPath;
            return;
        }

        /* Get earliest lastModified time (t) for incoming files, then choose all files
        in the folder that are Winnow supported formats and have been modified after (t).
        This allows unlimited files to be received, getting around the command argument
        buffer limited size.

        The earliest modified date for incoming files is a little bit tricky. The
        incoming files have been saved to the folder folderPath by the exporting program
        (ie lightroom). However, this folder might already have existing files. If the
        command argument buffer has been exceeded then the argument list may not contain
        the earliest modified file. To determine which files are part of the incoming,
        the modified date of the first file in the command argument buffer is used as a
        seed value, and any file with a modified date up to 10 seconds earlier becomes
        the new seed value. After reviewing all the eligible files in folderPath the seed
        value will be the earliest modified incoming file. */

        // get seed time (t) to start
        info = dir.entryInfoList().at(0);
        QDateTime t = info.lastModified();
        // tMinus10 is ten seconds earlier
        QDateTime tMinus10;
        for (int i = 0; i < dir.entryInfoList().size(); ++i) {
            QString fPath = dir.entryInfoList().at(i).absoluteFilePath();
            info.setFile(fPath);
            QDateTime tThis = info.lastModified();
            QDateTime tOld = t;
            // time first file last modified
            tMinus10 = t.addSecs(-10);
            if (tThis < t && tThis > tMinus10) t = tThis;
            /* log
            QString msg = QString::number(i).rightJustified(3) +
                          " tOld = " + tOld.toString("yyyy-MM-dd hh:mm:ss") +
                          " tMinus10 = " + tMinus10.toString("yyyy-MM-dd hh:mm:ss") +
                          " tThis = " + tThis.toString("yyyy-MM-dd hh:mm:ss") +
                          " t = " + t.toString("yyyy-MM-dd hh:mm:ss") +
                          "  " + fPath
                          ;
            if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", msg);
            //*/
        }

        /* log
        if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", QString::number(dir.entryInfoList().size()) + " files " +
                       folderPath + "  Cutoff = " + t.toString("yyyy-MM-dd hh:mm:ss"));
        //*/

        // for debugging ...
        bool doThemAll = false;

        // add the recently modified incoming files to pathList
        for (int i = 0; i < dir.entryInfoList().size(); ++i) {
            info = dir.entryInfoList().at(i);
            // only add files just modified
            if (info.lastModified() >= t || doThemAll) {
                pathList << info.filePath();
                /* log
                QString msg = QString::number(i) +
                        " Adding " + info.lastModified().toString("yyyy-MM-dd hh:mm:ss") +
                        " " + info.filePath();
                if (G::isFileLogger) Utilities::log("MW::handleStartupArgs", msg);
                //*/
            }
        }

        // create an instance of EmbelExport
        EmbelExport embelExport(metadata, dm, icd, embelProperties);

        // embellish src images (pathList) and return paths to embellished images
        QStringList embellishedPaths = embelExport.exportRemoteFiles(templateName, pathList);
        if (!embellishedPaths.size()) return;

        qDebug() << "MW::handleStartupArgs" << embellishedPaths;

        // sort embellishedPaths
        embellishedPaths.sort(Qt::CaseInsensitive);

        // go to first embellished image
        info.setFile(embellishedPaths.at(0));
        QString fDir = info.dir().absolutePath();
        QString fPath = embellishedPaths.at(0);

        // if folder with embellished images is not already selected
        if (!dm->folderList.contains(fDir)) {
            // fsTree->select(fDir, "", "handleStartupArgs");
            folderAndFileSelectionChange(fPath, "handleStartupArgs");
        }
        else {
            imageView->currentImageHasChanged = true;
            insertFiles(embellishedPaths);
            // update filter counts
            buildFilters->recount();
            filterChange("MW::handleStartupArgs_remoteEmbellish");
            // select first new embellished image
            sel->select(fPath);
        }
    }

    // startup not triggered by embellish winnet
    else {
        //qDebug() << "MW::handleStartupArgs:  startup not triggered by embellish winnet";
        QFileInfo f(argList.at(1));
        f.dir().path();
        fsTree->select(f.dir().path());
        // folderSelectionChange(); // not req'd after multi-select FSTree
        if (G::isRunByExtern) Utilities::log("MW::handleStartupArgs", "startup not triggered by embellish winnet");
    }

    if (G::isRunByExtern) Utilities::log("MW::handleStartupArgs", "done");
    return;
}

void MW::resetDevelopCachesForNewFolder()
{
/*
    A new set of images invalidates the current image's develop caches -- they hold the
    PREVIOUS image's ~60MP bases. Drop them so revisiting an image RE-DECODES (showing
    progress again) rather than short-circuiting on a stale hit, and so hundreds of MB are
    not pinned across a folder change. The pre-develop WorkingImageCache is cleared
    separately on the ImageCache thread (its folder reset).

    Shared by folderSelectionChange and loadCatalogResults: a catalog result set replaces
    the model's contents exactly as a folder change does, so it must invalidate exactly
    the same caches. Missing one here would show the previous image's pixels.
*/
    if (G::isLogger) G::log("MW::resetDevelopCachesForNewFolder");

    developDenoised.reset();
    developDenoisedKey.clear();
    developPmridFull.reset();
    developPmridKey.clear();
    developPmridResSource.clear();     // drop the noise-model snapshot with its base
    developPmridResK = developPmridResB = 0.0;
    developPmridResHadNP = false;
    developProxy.reset();
    developProxyPath.clear();
    developFrame = QImage();           // the frame the sharpening mask preview derives from
    developFramePath.clear();
    developFrameFaithful = false;
    developFrameRecipe.clear();
    developFullFrame = QImage();       // the full-res frame the devPreview is encoded from
    developFullFramePath.clear();
    developFullFrameFaithful = false;
    developFullFrameRecipe.clear();
    developStackCache.clear();         // its entries are sized to the old proxy
    maskFoldCacheClear();              // ditto, and its refs belong to the old folder
    developWorkTriedPath.clear();
}

void MW::folderSelectionChange(QString folderPath, G::FolderOp op, bool resetDataModel, bool recurse)
{
/*
    This is invoked when there is a folder selection change in the folder or bookmark views.

    If the source is FSTree then the op is defined by the mouse click modifier
    - NoModifier: Add folder and resetDataModel = true
    - Ctrl/Cmd:   Toggle folder Add or Remove
    - Alt/Opt:    Recurse subfolders = true

    op = operation to perform:
    - Add
    - Remove
    - Toggle (applies if recurse = true)
*/
    G::t.start();

    /*  Choosing a folder IS choosing the Folders scope -- that is what makes the
        Catalog row an alternative to the tree rather than a separate feature.
        Ahead of everything else so the panel and both Catalog rows are already
        consistent by the time the load starts. setScope early-returns when the
        scope has not changed, so the ordinary folder click costs nothing. */
    setScope(G::Scope::Folders, "MW::folderSelectionChange");

    QString fun = "MW::folderSelectionChange";
    if (G::isLogger || G::isFlowLogger)
    {
        // G::log("","");
        {
            QString msg = "op = " + G::enumClassToString(op) +
                " recurse = " + QVariant(recurse).toString() +
                " fsTree->selectionCount() = " + QVariant(fsTree->selectionCount()).toString() +
                " folderPath = " + folderPath;
            G::log(fun, msg);
        }
    }
    /*
    qDebug() << fun << op
             << "resetDataModel =" << resetDataModel
             << "recurse =" << recurse
             << folderPath; //*/

    /* The develop preview cache folder can be browsed -- it is a folder of JPEGs like any
       other -- but nothing in it may be written. Say so up front, on selection, rather
       than letting the user rate or rename a preview and only then explain: the write
       guards below the UI refuse silently by design. See "The Preview Cache Folder
       Is Read-Only" in notes/Documentation.txt. */
    if (op == G::FolderOp::Add && DevPreviewCache::instance().isCachePath(folderPath)) {
        if (G::popup) {
            QString msg = "This is Winnow's develop preview cache.<p>"
                          "The images here can be viewed, but not edited, renamed, <br>"
                          "rated, deleted or developed -- they are renders Winnow <br>"
                          "manages itself, and editing them would corrupt the cache.";
            G::popup->showPopup(msg, 8000);
        }
        /* Leave Develop on the way in: setOperationMode refuses to ENTER it here, but the
           user may already be in it from the previous folder. */
        if (G::operationMode == G::OperationMode::Develop)
            setOperationMode(G::OperationMode::Preview);
    }

    G::allMetadataAttempted = false;
    G::iconChunkLoaded = false;
    G::isModifyingDatamodel = true;

    resetDevelopCachesForNewFolder();

    // block repeated clicks to folders or bookmarks while processing this one.
    bookmarks->setEnabled(false);
    fsTree->setEnabled(false);

    // save the current datamodel selection before removing a folder from datamodel
    if (op == G::FolderOp::Remove) sel->save(fun);

    // folder selection cleared and new folder selected
    if (resetDataModel) {
        QString step = "Loading folders.\n";
        QString escapeClause = "\nPress \"Esc\" to stop.";
        setCentralMessage(step + escapeClause);
        // qApp->processEvents();
        // stop existing processes
        stop(fun);
        // sync bookmarks if exists
        bookmarks->select(folderPath);

    }
    else {
        // stop building but do not clear filters
        buildFilters->abortProcessing();
    }

    /*  Walk the tree once: get the subfolder count AND the list of
        subfolder paths in a single multi-threaded pass, then pass the
        list straight to enqueueFolderSelection so it doesn't redo the
        walk with QDirIterator. */
    QStringList subDirs;
    if (recurse) {
        setCentralMessage("Determining subfolder tree count...");
        dm->subFolderTreeCount = Utilities::subFolderTree(folderPath, subDirs);
        dm->subFolderTreeCounter = 0;
        QString msg = "Processing " + folderPath + "\n" +
                      "Subfolder tree count = " +
                      QVariant(dm->subFolderTreeCount).toString();
        setCentralMessage(msg);
    }

    /* put folder in datamodel queue to add or remove if main thread
       is not blocking */
    dm->abort = false;
    ScopeRequest req;
    req.scope = G::Scope::Folders;
    req.query.folder = folderPath;
    req.recurse = recurse;
    req.op = op;
    req.subDirs = subDirs;
    req.reconcile = true;                  // the directory listing IS the set
    QTimer::singleShot(0, this, [this, req]{
        dm->setScope(req);
    });

    // dm->enqueueFolderSelection(folderPath, op, recurse);
    // qDebug() << fun << "finished dm->enqueueFolderSelection";

}

void MW::startCatalogScan()
{
/*
    Kick off the background scan over the folders the user nominated. Queued onto the
    scanner's own thread; nothing here blocks.
*/
    if (G::isLogger) G::log("MW::startCatalogScan");
    if (!catalogScanner) return;
    if (catalogScanner->isRunning()) return;
    if (catalogRoots.isEmpty()) return;

    if (progress) {
        progress->setRowText(progressCatalogRow, "Catalog");
        progress->showRow(progressCatalogRow, true);
    }
    if (catalogView) catalogView->setScanning(true);
    if (findPanel) findPanel->setScanning(true);
    if (catalogRootsDlg) catalogRootsDlg->setScanning(true);
    QMetaObject::invokeMethod(catalogScanner, "scan", Qt::QueuedConnection,
                              Q_ARG(QStringList, catalogRoots),
                              Q_ARG(bool, catalogRootsRecurse));
}

void MW::manageCatalogRoots()
{
/*
    Open the Catalogued Folders editor.

    KEPT RATHER THAN REBUILT, so it can stay open across a scan and be raised again
    without losing what the user was looking at. It is a Qt::Tool window and not modal:
    a scan runs for minutes, and this is where its state is shown.

    MW REMAINS THE OWNER of catalogRoots. The dialog is only an editor and hands back
    what changed -- the root list is the one piece of catalog state that is user intent
    rather than derived data, so it must not depend on a widget's lifetime.
*/
    if (G::isLogger) G::log("MW::manageCatalogRoots");

    if (!catalogRootsDlg) {
        catalogRootsDlg = new CatalogRootsDlg(this);
        connect(catalogRootsDlg, &CatalogRootsDlg::rootsChanged, this,
                [this](const QStringList &roots, bool recurse) {
                    catalogRoots = roots;
                    catalogRootsRecurse = recurse;
                });
        connect(catalogRootsDlg, &CatalogRootsDlg::scanRequested,
                this, &MW::startCatalogScan);
        connect(catalogRootsDlg, &CatalogRootsDlg::stopScanRequested,
                this, &MW::stopCatalogScan);
    }

    catalogRootsDlg->setRoots(catalogRoots, catalogRootsRecurse);
    catalogRootsDlg->setScanning(catalogScanner && catalogScanner->isRunning());
    catalogRootsDlg->setCatalogStatus(
        Catalog::instance().isAvailable()
            ? QString("%1 images catalogued in %2 folders.")
                  .arg(Catalog::instance().count())
                  .arg(Catalog::instance().folderCount())
            : QString("The catalog is unavailable -- the local index database could "
                      "not be opened, so nothing can be indexed."));
    catalogRootsDlg->show();
    catalogRootsDlg->raise();
    catalogRootsDlg->activateWindow();
}

void MW::stopCatalogScan()
{
    if (G::isLogger) G::log("MW::stopCatalogScan");
    if (catalogScanner) catalogScanner->stop();
}

void MW::loadCatalogResults(const QStringList &paths, bool append, const CatalogQuery &query)
{
/*
    Load a catalog search result given as PATHS -- the shape the separate Catalog panel
    still uses. Each path is stat'd as it is added and each row is then read from its
    file. loadCatalogRows is the same load from index rows, which does neither.
*/
    if (G::isLogger || G::isFlowLogger)
        G::log("MW::loadCatalogResults",
               QString::number(paths.size()) + (append ? " results (add)" : " results"));

    if (paths.isEmpty()) return;

    ScopeRequest req;
    req.scope = G::Scope::Catalog;
    req.query = query;
    req.paths = paths;
    req.append = append;
    req.reconcile = false;             // no directory to enumerate
    loadCatalogScope(req, paths);
}

void MW::loadCatalogRows(const QVector<CatalogRow> &rows, bool append,
                         const CatalogQuery &query)
{
/*
    The same load, from whole index rows. Nothing here opens or stats a file: the rows
    came out of the query that found them (Catalog::searchRows) already holding
    everything a datamodel row displays.
*/
    if (G::isLogger || G::isFlowLogger)
        G::log("MW::loadCatalogRows",
               QString::number(rows.size()) + (append ? " rows (add)" : " rows"));

    if (rows.isEmpty()) return;

    QStringList paths;
    paths.reserve(rows.size());
    for (const CatalogRow &r : rows) paths << r.path;

    ScopeRequest req;
    req.scope = G::Scope::Catalog;
    req.query = query;
    req.rows = rows;
    req.append = append;
    req.reconcile = false;
    loadCatalogScope(req, paths);
}

void MW::loadCatalogScope(const ScopeRequest &req, const QStringList &paths)
{
/*
    Load a catalog search result -- images from any number of folders, as one browsable
    set. See notes/Documentation.txt "The Catalog".

    REPLACE IS THE SAME RESET AS A FOLDER CHANGE, deliberately. Everything downstream of
    the model -- MetaRead, the icon chunks, the image cache, Develop -- assumes that when
    the model's contents are replaced it was told to stop first and its caches were
    dropped. A result set replaces the contents just as thoroughly as a folder does, so it
    takes the identical path: stop(), reset the develop caches, then fill and let
    folderChangeCompleted run as usual.

    APPEND DOES NOT RESET ANY OF THAT, and must not. The rows already loaded stay, the
    image being viewed stays selected, and its develop caches are still describing the
    right picture -- dropping them would make the current image re-decode for no reason
    the user could see. This is the same shape as ctrl-clicking a second folder
    (G::FolderOp::Add), and it is what makes comparing two searches possible at all:
    without it, every search throws the previous one away.

    THE MODEL FILLS ADDITIVELY -- both addPaths and addCatalogRows start at rowCount() and
    skip what is already loaded -- so append is a matter of NOT tearing down first, rather
    than a second load path. That is also why a path in both searches is not loaded twice.

    THE FOLDER PANEL DELIBERATELY DOES NOT FOLLOW. The results span many folders, so there
    is no one folder to select; highlighting an arbitrary one of them would misrepresent
    what is loaded. The dock's own result header is what says where these came from.
*/
    QString fun = "MW::loadCatalogScope";

    G::allMetadataAttempted = false;
    G::iconChunkLoaded = false;
    G::isModifyingDatamodel = true;

    if (!req.append) {
        resetDevelopCachesForNewFolder();

        bookmarks->setEnabled(false);
        fsTree->setEnabled(false);

        setCentralMessage("Loading search results.\n\nPress \"Esc\" to stop.");
        stop(fun);
    }

    dm->abort = false;
    /* Queued for the same reason enqueueFolderSelection is: stop() has just torn down the
       reader threads, and the fill must not run inside the signal that asked for it. On
       the append path nothing was torn down, but the queueing is kept so both paths reach
       the model the same way -- one of them running inline would be a difference waiting
       to matter. */
    QTimer::singleShot(0, this, [this, req, paths]{
        /*  AFTER THE FILL, NOT AFTER setScope. The rows path fills in batches posted to
            the event loop, so setScope returns with the first batch in and the rest still
            coming -- a pass launched here would look up paths that are not rows yet and
            silently mark none of them. folderChange is emitted when the fill is done,
            whichever shape it took, so both paths wait exactly as long as they need to.

            The connection is single-shot: a later folder load must not drag this set's
            availability answers along behind it. */
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(dm, &DataModel::folderChange, this,
                        [this, paths, conn](bool){
            disconnect(*conn);
            queueAvailabilityPass(paths);
        });
        dm->setScope(req);
    });
}

void MW::queueAvailabilityPass(const QStringList &paths)
{
/*
    ASK WHY EACH ROW IS NOT OPENABLE, once, off the GUI thread. A catalog row can outlive
    its file, and the two ways that happens are different things the user can act on
    differently -- see Catalog::Availability. A FOLDER load never comes here, and its rows
    stay Present, which is correct: the filesystem just listed them.

    This is the ONLY thing that looks at the filesystem on the rows path, and it is the
    right shape for it: one database pass and one mount-table walk for the whole set
    rather than a stat per row, with the answers written back on the GUI thread because
    the model is not thread-safe.
*/
    if (paths.isEmpty()) return;
    {
        QThreadPool::globalInstance()->start([this, paths]{
            const auto avail = Catalog::instance().availabilityOf(paths);
            if (avail.isEmpty()) return;
            QMetaObject::invokeMethod(this, [this, avail]{
                for (auto it = avail.cbegin(); it != avail.cend(); ++it) {
                    if (it.value() == Catalog::Availability::Present) continue;
                    const int row = dm->rowFromPath(it.key());
                    if (row < 0) continue;
                    dm->setData(dm->index(row, G::AvailabilityColumn), int(it.value()));
                }
            }, Qt::QueuedConnection);
        });
    }
}

void MW::runCatalogLoadTest(const QString &pathFilter)
{
/*
    WHAT CLICKING CATALOG COSTS, headlessly and against the REAL index.

    Every automated check of this path runs on a fixture folder of about a thousand rows,
    and a thousand rows is not the case that hurts: two bugs in a row -- a pump that never
    pumped and a sort that built a million QFileInfos -- were invisible at that size and
    obvious at 43,000. This drives the same entry point the Find dock does, on whatever
    the user actually has catalogued, and prints where the time went.

    It is not a test in tests/: it needs the user's own index, which no fixture can carry
    and which QStandardPaths test mode deliberately hides. Same reasoning as
    Cache/catalogprobe.h, and it shares that flag's convention of a path substring to
    restrict the set.
*/
    if (G::isLogger) G::log("MW::runCatalogLoadTest", pathFilter);

    G::isPerfProbe = true;

    CatalogQuery q;
    q.includeMissing = true;
    if (!pathFilter.isEmpty()) q.folder = pathFilter;

    QElapsedTimer t;
    t.start();
    int total = 0;
    const QVector<CatalogRow> rows = Catalog::instance().searchRows(q, 0, &total);
    const qint64 queryMs = t.elapsed();

    fprintf(stderr, "CATALOGLOAD: query %lld ms, %lld rows of %d matching\n",
            (long long)queryMs, (long long)rows.size(), total);
    fflush(stderr);

    if (rows.isEmpty()) { std::_Exit(2); }

    /*  Timed from the load request to the FIRST batch and to the LAST, because those are
        two different user-visible facts: when the first thumbnail could appear, and when
        the set stops growing. A single total would hide a fill that delivers nothing for
        a minute and then everything at once -- which is exactly the failure being chased. */
    auto started = std::make_shared<QElapsedTimer>();
    auto firstSeen = std::make_shared<bool>(false);
    started->start();

    connect(dm, &DataModel::rowsInserted, this,
            [this, started, firstSeen](const QModelIndex &, int, int){
        if (*firstSeen) return;
        *firstSeen = true;
        fprintf(stderr, "CATALOGLOAD: first batch inserted at %lld ms\n",
                (long long)started->elapsed());
        fflush(stderr);
    });

    connect(dm, &DataModel::folderChange, this, [this, started](bool aborted){
        fprintf(stderr, "CATALOGLOAD: fill complete at %lld ms, rows=%d, aborted=%d\n",
                (long long)started->elapsed(), dm->rowCount(), aborted ? 1 : 0);
        fflush(stderr);

        /*  THE FILL IS NOT THE END OF THE WAIT, which is the whole reason this keeps
            running. Measured, the fill of 43,050 rows costs ~12 s and a person reported
            SEVERAL MINUTES before thumbnails appeared -- so most of the wait is what
            happens after: the icon chunk, the filter tree over every row, and the first
            image decode. Sampling those every second is what says which.

            Ends at WINNOW_CATALOGLOAD_MS (default 180 s) or when icons stop arriving. */
        int watchMs = qEnvironmentVariableIntValue("WINNOW_CATALOGLOAD_MS");
        if (watchMs <= 0) watchMs = 180000;

        auto watch = new QTimer(this);
        auto ticks = std::make_shared<int>(0);
        auto lastIcons = std::make_shared<int>(-1);
        auto stable = std::make_shared<int>(0);
        connect(watch, &QTimer::timeout, this, [this, watch, started, ticks, lastIcons,
                                                stable, watchMs]{
            const int icons = dm->iconCount();
            /*  HOW BIG THE FILTER TREE IS, because a catalog scope now fills it from the
                datamodel and the datamodel is the whole library: one item per distinct
                title, day, camera and keyword across 43,000 images. A QTreeWidget being
                laid out for the first time is O(items), and the dock is SHOWN by the same
                click that changes the scope. */
            int treeItems = 0;
            if (filters) {
                QTreeWidgetItemIterator tit(filters);
                while (*tit) { ++treeItems; ++tit; }
            }
            fprintf(stderr, "CATALOGLOAD: t=%lld ms  icons=%d  metaAttempted=%d  "
                            "iconChunk=%d  treeItems=%d  dockVisible=%d\n",
                    (long long)started->elapsed(), icons,
                    (bool)G::allMetadataAttempted ? 1 : 0,
                    dm->iconChunkSize.load(), treeItems,
                    filterDock && filterDock->isVisible() ? 1 : 0);
            fflush(stderr);

            /*  Stop when the icon count has not moved for five ticks: the work this is
                waiting for has finished, and sitting out the rest of the budget would
                only make the run longer than the thing it measures. */
            if (icons == *lastIcons) ++(*stable); else *stable = 0;
            *lastIcons = icons;
            if (*stable >= 5 || ++(*ticks) * 1000 >= watchMs) {
                watch->stop();
                fprintf(stderr, "CATALOGLOAD: settled at %lld ms with %d icons\n",
                        (long long)started->elapsed(), icons);
                fflush(stderr);

                /*  AND NOW CLICK IT AGAIN, which is what a person did to find the second
                    beachball. The second click is not the first: the model already holds
                    every row, so the fill has nothing to add, and everything the panel
                    and the proxy do around it happens over 43,000 loaded rows rather
                    than over an empty model. */
                if (findPanel && qEnvironmentVariableIntValue("WINNOW_CATALOGLOAD_TWICE") == 1) {
                    auto again = std::make_shared<QElapsedTimer>();
                    again->start();
                    fprintf(stderr, "CATALOGLOAD: --- second click ---\n");
                    fflush(stderr);
                    /*  THROUGH MW::setScope, which is what the Catalog row in the Folders
                        panel calls -- and which also shows and raises the filter dock.
                        Showing a dock whose tree holds tens of thousands of items is
                        itself work, and it was outside everything measured so far. */
                    setScope(G::Scope::Folders, "catalogload second click");
                    const qint64 beforeShow = again->elapsed();
                    setScope(G::Scope::Catalog, "catalogload second click");
                    fprintf(stderr, "CATALOGLOAD: setScope(Catalog) took %lld ms\n",
                            (long long)(again->elapsed() - beforeShow));
                    fflush(stderr);
                    fprintf(stderr, "CATALOGLOAD: second click returned at %lld ms\n",
                            (long long)again->elapsed());
                    fflush(stderr);
                    QTimer::singleShot(20000, this, [again]{
                        fprintf(stderr, "CATALOGLOAD: 20 s after second click (%lld ms)\n",
                                (long long)again->elapsed());
                        fflush(stderr);
                        std::_Exit(0);
                    });
                    return;
                }
                std::_Exit(0);
            }
        });
        watch->start(1000);
    });

    /*  THROUGH THE PANEL, not straight into the model, when the panel exists.

        The first version called loadCatalogRows directly, which is a real path but not
        the one a PERSON takes: clicking Catalog goes through FindPanel::applyScope, which
        also re-points the category tree and runs its own search. A beachball that lives
        in applyScope was therefore invisible to this driver while it reported healthy
        load numbers -- so it now drives the click.

        WINNOW_CATALOGLOAD_DIRECT=1 forces the old model-only path, for isolating the fill
        from everything the panel does around it. */
    setScope(G::Scope::Catalog, "runCatalogLoadTest");
    if (findPanel && qEnvironmentVariableIntValue("WINNOW_CATALOGLOAD_DIRECT") != 1) {
        fprintf(stderr, "CATALOGLOAD: driving FindPanel::setScope(CatalogScope)\n");
        fflush(stderr);
        findPanel->setScope(FindPanel::CatalogScope);
    }
    else {
        loadCatalogRows(rows, false, q);
    }
}

void MW::fileSelectionChange(QModelIndex current, QModelIndex previous, bool clearSelection, QString src)
{
/*
    Triggered when file selection changes (folder change selects new image, so it also
    triggers this function). The new image is loaded, the pick status is updated and the
    infoView metadata is updated. The imageCache is updated if necessary. The imageCache
    will not be updated if triggered by folderSelectionChange since a new one will be
    created. The metadataCache is updated to include metadata and icons for all the
    visible thumbnails.

    Note that the datamodel includes multiple columns for each row and the index sent to
    fileSelectionChange could be for a column other than 0 (from tableView) so scrollTo
    and delegate use of the current index must check the column.
*/
    // if starting program, return
    QString fun = "MW::fileSelectionChange";
    if (current.row() == -1) {
        if (G::isLogger || G::isFlowLogger)
            qDebug() << fun <<  "Invalid row";
        return;
    }

    if (G::isLogger || G::isFlowLogger)
    {
        G::log(fun,
               "row = " + QString::number(current.row()) +
               " src = " + src);
    }

    if (G::stop) {
        if (G::isLogger || G::isFlowLogger)
        qDebug() << fun << "G::stop = true so exit";
        return;
    }

    /* debug
    {
    qDebug() << fun
             << "G::fileSelectionChangeSource =" << G::fileSelectionChangeSource
             << "G::mode =" << G::mode
             // << "current =" << current
             // << current.data(G::PathRole).toString()
             << "row =" << current.row()
             << "rate =" << G::t.restart()
             // << "dm->currentDmIdx =" << dm->currentDmIdx
             // << "G::isInitializing =" << G::isInitializing
             // << "isFilterChange =" << isFilterChange
             << dm->sf->index(current.row(), 0).data(G::PathRole).toString()
             // << "G::isLinearLoadDone =" << G::isLinearLoadDone
             // << "isFirstImageNewInstance =" << imageView->isFirstImageNewInstance
             // << "icon row =" << thumbView->currentIndex().row()
                ;

    } //*/

    if (!rememberLastDir) {
        if (G::isInitializing || isFilterChange) {
            qDebug() << fun << "G::isInitializing || isFilterChange so exit";
            return;
        }
    }

    /* folder does not exist
    if (!currRootDir.exists()) {
        if (G::isLogger || G::isFlowLogger) G::log(fun,
            "Folder does not exist so exit");
        refreshFolders();
        return;
    }
    //*/

    // if current is not first and !G::allMetadataAttempted
    // waitUntilMetadataLoaded(5000, fun);

    // if new folder and 1st file is a video and mode == "Table"
    if (G::mode == "Table" && centralLayout->currentIndex() != TableTab) {
        tableDisplay();
    }

    // if folderchange triggered by bookmark mouse click then scroll FSTree
    if (fsTree->selectSrc == "Bookmark") {
        fsTree->selectSrc = "";
        fsTree->scrollToCurrent();
    }

    // Check if anything selected.  If not disable menu items dependent on selection
    enableSelectionDependentMenus();
    enableStatusBarBtns();

    // the file path is used as an index in ImageView
    QString fPath = dm->sf->index(current.row(), 0).data(G::PathRole).toString();
    settings->setValue("lastFileSelection", fPath);

    /* Per-image Develop edit state: load this image's saved EditStack into the dock (also flushes
       the previous image's edits to its sidecar). The developed preview is applied after the
       loupe image is shown (applyDevelopPreviewIfEdited). Videos are not developable, so hand the
       dock an empty path -- it still flushes the image we are leaving, then points at nothing.
       In Preview mode the Develop panel is disabled and no edits can be made, so leave it untouched
       (no edits are pending to flush either); setOperationMode re-syncs it on entering Develop. */
    if (developProperties && G::operationMode == G::OperationMode::Develop) {
        const bool selIsVideo = dm->sf->index(current.row(), G::VideoColumn).data().toBool();
        developProperties->setCurrentImage(selIsVideo ? QString() : fPath);
        /* The Detail preview's sample point named a place in the PREVIOUS picture, so it
           goes with it -- keeping it would silently magnify a different subject. */
        resetDetailPoint();
        /* The banner and the panel's enabled state both follow the current index as well
           as the selection set, and are refreshed together further down (once the central
           widget has been switched to the loupe or the video player). */
    }

    /* SCROLL CONTROL:
       When an item (icon or row) is selected the default behavior is to scroll the item
       to the center of the view (thumbView, gridView or tableView) so the user does not
       have to scroll as they move through the images. However, when the user mouse
       clicks on an item scrolling to center is disorienting so we do not scroll.
    */

    // Mouse clicks on a view item
    if (G::fileSelectionChangeSource.right(5) == "Click") {
        if (G::fileSelectionChangeSource == "ThumbMouseClick") {
            gridView->scrollToCurrent(fun);
            tableView->scrollToCurrent();
            // thumbView->updateMidVisibleCell(fun);
        }
        else if (G::fileSelectionChangeSource == "GridMouseClick") {
            thumbView->scrollToCurrent(fun);
            tableView->scrollToCurrent();
            // gridView->updateMidVisibleCell(fun);
        }
        else if (G::fileSelectionChangeSource == "TableMouseClick") {
            gridView->scrollToCurrent(fun);
            thumbView->scrollToCurrent(fun);
        }
        // is source is doubleMouseClick then only thumbView will be visible
        else if (G::fileSelectionChangeSource == "IconMouseDoubleClick") {
            thumbView->scrollToCurrent(fun);
        }
    }
    // other selection methods (keyboard, folderAndFileSelectionChange, handleStartupArgs...
    else {
        thumbView->scrollToCurrent(fun);
        gridView->scrollToCurrent(fun);
        tableView->scrollToCurrent();
    }

    dm->scrollToIcon = dm->currentSfRow;

    // new file name appended to window title
    setWindowTitle(winnowWithVersion + "   " + fPath);

    bool isVideo = dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();

    // update loupe/video view
    videoView->stop();
    if (G::mode == "Loupe" || G::mode == "Grid" || G::mode == "Table") {
        if (isVideo) {
            G::isFirstImageNewInstance = false;
            updateClassification();
            if (G::mode == "Loupe" || G::fileSelectionChangeSource == "IconMouseDoubleClick") {
                // loupeDisplay(fun);
                if (G::useMultimedia) {
                    centralLayout->setCurrentIndex(VideoTab);
                    videoView->load(fPath);
                    // videoView->play();
                }
            }
            if (G::mode == "Grid") gridView->refreshIcons(src);
            // if (G::mode == "Grid") gridDisplay();
            // if (G::mode == "Table") tableDisplay();
        }
        else if (G::useImageView) {
            // Mirror the unconditional centralLayout switch used in the video branch.
            // imageView->loadImage can return false (cache miss, other early-returns),
            // which would skip loupeDisplay and leave the central widget on VideoTab
            // from a prior video selection.
            if ((G::mode == "Loupe" || G::fileSelectionChangeSource == "IconMouseDoubleClick")
                && centralLayout->currentIndex() != LoupeTab)
            {
                centralLayout->setCurrentIndex(LoupeTab);
            }
            /* Develop mode: remember the zoom/pan of the image being left BEFORE it is
               replaced, so the developed image shows the same area when it arrives
               (which can be seconds later, after the demosaic).  Preview mode gets this
               for free because the swap is synchronous.  ImageView keeps the capture in
               step with any pan/zoom made meanwhile (eg a thumbnail click pans to the
               clicked point on mouse release, after this selection change). */
            if (G::operationMode == G::OperationMode::Develop) {
                imageView->captureDevelopView(fPath);
            }
            if (imageView->loadImage(fPath, false, fun)) {
                if (G::mode == "Loupe" || G::fileSelectionChangeSource == "IconMouseDoubleClick") {
                    loupeDisplay(fun);
                }
                applyDevelopPreviewIfEdited();   // overlay saved develop edits, if any
            }
            /* Image-cache miss in Develop mode on a RAW: the scene-linear sensor decode
               is slow (~2-3s), so paint the embedded JPG preview immediately instead of a
               blank loupe. The developed image replaces it when the decode lands
               (refreshViewsOnCacheChange). */
            else if (G::operationMode == G::OperationMode::Develop && G::useRaw
                     && isFileRaw(fPath) && !icd->contains(fPath)) {
                /* Prefer a cached develop preview over the camera's embedded JPG: for
                   an edited image the embedded JPG shows the UNDEVELOPED picture, so the
                   loupe used to spend those 2-3s displaying the one thing the user had
                   already decided to change. */
                const QImage cachedPreview = devPreview(fPath);
                developInterimIsDevPreview = !cachedPreview.isNull();
                if (imageView->loadImageInterim(fPath, cachedPreview)) {
                    if (G::mode == "Loupe" ||
                        G::fileSelectionChangeSource == "IconMouseDoubleClick") {
                        loupeDisplay(fun);
                    }
                }
                /* If the saved recipe has a "Denoise raw" amount, start the denoise
                   decode now (not after the ImageCache decode + settle): it shows from
                   the start and produces the clean + PMRID bases in one pass, publishing
                   the clean base so the ImageCache decode reuses it. No-op without a
                   denoise edit or off the Winnow engine. */
                /* Gated on the RECIPE (EditParams::denoiseRaw), which falls back to the
                   Auto run preference when the image says nothing -- so a manual
                   "Denoise" is honoured here too, not just in the session that made it.
                   stackJobFor(fPath), not stackJob(): the panel may still be pointed at
                   the image being left. */
                if (developProperties) {
                    const auto mj = developProperties->stackJobFor(fPath);
                    if (mj.global.wantsDenoiseRaw(G::autoRunDenoise))
                        ensureRawDenoise(fPath, mj.global,
                                         WorkingImageCache::instance().get(fPath),
                                         currentImageIso());
                }
            }
        }
    }

    /* Develop operates on decoded still frames, so a video selection greys the whole
       panel and raises a "not applicable" alert row (the mode itself is
       NOT changed -- Preview/Develop is the user's choice, and auto-flipping it would
       churn G::useRaw and the image cache on every video/still crossing in a mixed
       folder). Selecting a still again re-enables everything. The status bar carries the
       same message because the Develop dock may be tabbed away behind History or
       Presets, where the alert row cannot be seen. */
    if (G::operationMode == G::OperationMode::Develop) {
        syncDevelopPanelEnabled();
        updateDevelopSelectionWarning();
        if (isVideo)
            updateStatus(true, "Video selected - Develop applies to still images only", fun);
    }

    G::fileSelectionChangeSource = "";

    // update ImageCache
    if (!(G::isSlideShow && isSlideShowRandom)
        && (!workspaceChanged)
        && (G::mode != "Compare")
        && (G::useImageCache)
        && (!G::removingRowsFromDM)
       )
    {
        // Arm MetaRead's navigation gate so its reader pool yields CPU
        // to the ImageCache decoder for this selection.
        QMetaObject::invokeMethod(metaRead, "setAwaitingDecode",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, dm->currentSfRow));
        emit setImageCachePosition(dm->currentFilePath, "MW::fileSelectionChange");
    }

    workspaceChanged = false;

    // update the metadata panel
    if (G::useInfoView) infoView->updateInfo(dm->currentSfRow);

    // initialize the thumbDock if just opened app
    if (G::isInitializing) {
        if (dockWidgetArea(thumbDock) == Qt::BottomDockWidgetArea ||
            dockWidgetArea(thumbDock) == Qt::TopDockWidgetArea)
        {
            thumbView->setWrapping(false);
        }
        else {
            thumbView->setWrapping(true);
        }
        if (thumbDock->isFloating()) thumbView->setWrapping(true); // nada
    }

    // update cursor position on progressBar
    if (progress->isVisible()) {
        progress->updateCursor(dm->currentSfRow, dm->sf->rowCount());
    }

    // Remember last folder (showWindow not completed when initiated)
    if (dm->instance == 0 && dm->primaryFolderPath() == lastDir) {
        fsTree->scrollToCurrent();
    }

    if (G::isLogger) G::log("MW::fileSelectionChange", "Finished " + fPath);
}

void MW::folderAndFileSelectionChange(QString fPath, QString src)
{
/*
    Loads the folder containing the image and then selects the image.  Used by
    MW::handleStartupArgs and MW::handleDrop.  After the folder change a delay
    is req'd before the initial metadata has been cached and the image can be
    selected.
*/
    QString fun = "MW::folderAndFileSelectionChange";
    if (G::isLogger || G::isFlowLogger) {
        QString msg = "src = " + src + " fPath = " + fPath;
        G::log("MW::folderAndFileSelectionChange", msg);
    }

    // // reset frozen columns
    // if (tableView->isVisible()) {
    //     tableView->resizeColumns();
    // }

    setCentralMessage("Loading " + fPath + " ...");

    // used by scrolling in MW::fileSelectionChange
    G::fileSelectionChangeSource = "folderAndFileSelectionChange";

    embelProperties->setCurrentTemplate("Do not Embellish");
    QFileInfo info(fPath);
    QString folder = info.dir().absolutePath();

    // handle drop
    if (src == "handleDropOnCentralView") {
        if (dm->folderList.contains(folder)) {
            if (dm->proxyIndexFromPath(fPath).isValid()) {
                sel->setCurrentPath(fPath);
            }
            return;
        }
    }

    // /*
    qDebug() << fun
             << "isStartupArgs =" << isStartupArgs
             << "folder =" << folder
             << "fPath =" << fPath
                ;
                //*/

    // path to image, used in folderChanged() to select image
    folderAndFileChangePath = fPath;

    if (centralLayout->currentIndex() == CompareTab) {
        centralLayout->setCurrentIndex(LoupeTab);
    }

    // handle StartupArgs (embellish call from remote source ie Lightroom)
    if (!fsTree->select(folder)) {
        QString msg = "fsTree failed to select folder.";
        G::issue("Warning", msg, "MW::folderAndFileSelectionChange", -1, folder);
        qWarning() << msg;
        return;
    }

    qDebug() << fun << "4";

    if (G::isRunByExtern) Utilities::log("MW::folderAndFileSelectionChange", "call folderSelectionChange for " + folderAndFileChangePath);
    return;
}

void MW::updateImageCount()
{
    if (G::isLogger) G::log("MW::updateImageCount");
    fsTree->updateCount();
    bookmarks->updateCount();
}

void MW::refresh()
{
/*
    Get a list of source image files (in the selected folders) that have been added,
    removed or modified. Refresh the datamodel, filters and views to match without
    reloading all the folders. Update the image counts in FSTree and Bookmarks.
    Update ImageCache.

    Called by MW::saveAsFile if saved to a currently selected folder.
              MW::deleteFiles
              Folders dock refresh button
              Bookmarks dock refresh button
              Menu File > Refresh

*/
    QString srcFun = "MW::refresh";
    if (G::isLogger) G::log(srcFun);
    // update image counts
    fsTree->updateCount();
    bookmarks->updateCount();
    dm->refresh();


    if (!dm->sf->rowCount()) {
        buildFilters->rebuild();
    }

    // Point-in-time check: dm->insert() bumps rowCount, so isMetaReadFinished()
    // reads false immediately while the new row is unread, routing fresh inserts
    // to the positioning path below. (Reading the G::allMetadataAttempted global
    // here would risk a stale value if a stray MetaRead cycle republished it.)
    if (dm->isMetaReadFinished()) {
        // Normal refresh (delete, save-as, menu/dock refresh): metadata for all
        // rows is loaded, so the proxy can be re-filtered and re-sorted and the
        // ImageCache rebuilt to match.
        filterChange(srcFun);
    }
    else {
        /* A row was just inserted (e.g. a focus-stack result), so not every row
           has been attempted yet. MW::filterChange()/sortChange() won't fully
           apply here; position the new row and load its metadata/icon without
           disturbing the current selection.

           dm->insert() placed the row at its sorted position in the SOURCE
           model, but the proxy appended it. Re-evaluate the proxy so it mirrors
           source order (the default name order). sf->sort() can't do this - the
           default order uses no active proxy sort column. */
        dm->sf->filterChange(srcFun);   // invalidateRowsFilter() -> source order
        dm->currentSfRow = dm->sf->mapFromSource(dm->currentDmIdx).row();

        // Find the inserted row (first whose metadata has not been attempted)
        // so MetaRead loads its icon/metadata; fall back to the current row.
        int loadRow = dm->currentSfRow;
        for (int r = 0; r < dm->sf->rowCount(); ++r) {
            if (dm->sf->index(r, G::MetadataStatusColumn).data().toInt() == G::MetaNotAttempted) {
                loadRow = r;
                break;
            }
        }

        // MetaRead caches dmRowCount (= dm->rowCount()) in initialize() and
        // bounds its reader dispatch by it; after dm->insert() that cache is one
        // short, so the inserted row is never read. Re-initialize (queued, ahead
        // of the setStartRow that updateChange() queues) so it sees the new row.
        // The canonical load paths (loadConcurrent, MW::filterChange) do the same.
        QMetaObject::invokeMethod(metaRead, "initialize", Qt::QueuedConnection,
                                  Q_ARG(QString, srcFun));

        /* The inserted row appears at the END until the proxy is re-sorted: with no
           active sort column sf appends it, and the addMetadataForItem dataChanged
           emitted as its metadata loads re-appends it. The re-assert of source order
           is done synchronously at the tail of MW::insertFiles (after it loads the
           inserted rows' metadata), not here - the metadataLoaded signal this used to
           rely on does not reliably fire in the synchronous insert flow, and a
           lingering SingleShotConnection could misfire on a later folder load. */

        // Point MetaRead at the inserted row so its icon range covers it (icons
        // only load within the range updateChange() centres on the given row);
        // isFileSelectionChange = false leaves the selection unchanged.
        updateChange(loadRow, /*isFileSelectionChange*/false, srcFun);
    }

    buildFilters->recount();
    thumbView->iconViewDelegate->currentRow = dm->currentSfRow;
    gridView->iconViewDelegate->currentRow = dm->currentSfRow;

    // Grid view does not update automatically when insertion or deletion
    if (thumbView->isVisible()) {
        // Force the IconView to clear its internal pixmap cache
        thumbView->refreshIcons(srcFun);
        // Sync the scroll position to the model's preferred icon
        thumbView->scrollToRow(dm->currentSfRow, srcFun);
    }
    if (gridView->isVisible()) {
        // Force the IconView to clear its internal pixmap cache
        gridView->refreshIcons(srcFun);
        // Sync the scroll position to the model's preferred icon
        gridView->scrollToRow(dm->currentSfRow, srcFun);
    }

    /* MW::filterChange recovers the selection via Selection::recover(), which restores
       the current index with QItemSelectionModel::NoUpdate and so does NOT re-emit
       fileSelectionChange. A still image keeps showing because imageView retains its
       decoded pixmap, but a video's first frame is rendered by videoView (VideoTab),
       which is left stale/blank without an explicit reload. Re-assert the current
       video's first frame when in Loupe; the guard skips a clip already shown paused
       on its first frame so there is no redundant reload. */
    if (G::useMultimedia && G::mode == "Loupe" && dm->sf->rowCount()) {
        bool isVideo = dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();
        if (isVideo && !dm->currentFilePath.isEmpty()) {
            QMediaPlayer *mp = videoView->video->mediaPlayer;
            bool sameSourceShown = mp->source().toLocalFile() == dm->currentFilePath
                                   && mp->playbackState() == QMediaPlayer::PausedState;
            if (centralLayout->currentIndex() != VideoTab)
                centralLayout->setCurrentIndex(VideoTab);
            if (!sameSourceShown)
                videoView->load(dm->currentFilePath);
        }
    }

    /* Same reason as the video reload above: this path can leave the current row on a
       different item without fileSelectionChange firing, so the Develop panel's greyed
       state and banner have to be re-asserted from the current selection or they go
       stale (live-looking sliders on a video, or a greyed panel on a still). */
    if (G::operationMode == G::OperationMode::Develop) {
        syncDevelopPanelEnabled();
        updateDevelopSelectionWarning();
    }
}

bool MW::allIdle() const {
    for (const auto& v : stopped) {
        if (!v) {
            // G::log("MW::allIdle", stopped.key(v) + " = " + QVariant(v).toString());
            return false;
        }
    }
    // G::log("MW::allIdle", "allIdle = true");
    return true;
}

void MW::stop(QString src)
{
/*
    Stops and clears all folder loading processes and data.

    Triggered when:
    - a folder is selected in the folder panel or open menu.
    - a bookmark is selected.
    - a drive is ejected and the resulting folder does not have any eligible images.
    - ESC is pressed while folder is loading.

    DataModel instances:

    The datamodel instance, dm->instance, starts at zero and is incremented after the
    datamodel is cleared. G::dmInstance is the global variant. This is required as the
    image decoders, running in separate threads, may still be decoding an image from a
    prior folder. See ImageCache::fillCache.

*/
    QString srcFun = "MW::stop";
    if (G::isLogger || G::isFlowLogger)
        G::log(srcFun, "instance = " + QString::number(dm->instance) +
               " src = " + src);

    // ignore if already stopping
    if (G::stop) return;

    // stop flags
    G::stop = true;
    dm->abort = true;

    /* Abandon any devPreview build: its queue holds paths from the folder being left, and
       the render in flight is about to be irrelevant. The queue is rebuilt from what is
       actually missing next time the command runs, so nothing is lost by dropping it. */
    cancelDevPreviewBuild();

    // initialize stopped state for MetaRead, ImageCache, BuildFilters
    stopped.clear();
    stopped["MetaRead"] = metaRead->isIdle();
    stopped["ImageCache"] = imageCache->isIdle();
    stopped["BuildFilters"] = buildFilters->isIdle();

    // stop slideshow
    if (G::isSlideShow && !G::isStressTest) slideShow();

    /* Connect subsystem idle/aborted signals to update the stopped map */
    QList<QMetaObject::Connection> conns;
    conns << connect(metaRead, &MetaRead::stopped,
        this, [this](QString){ stopped["MetaRead"] = true; }, Qt::QueuedConnection);
    conns << connect(imageCache, &ImageCache::stopped,
        this, [this](QString){ stopped["ImageCache"] = true; }, Qt::QueuedConnection);
    conns << connect(buildFilters, &BuildFilters::stopped,
        this, [this](QString){ stopped["BuildFilters"] = true; }, Qt::QueuedConnection);

    if (!stopped["MetaRead"]) emit abortMetaRead();
    if (!stopped["ImageCache"]) emit abortImageCache();
    if (!stopped["BuildFilters"]) emit abortBuildFilters();

    // Wait for the subsystems to acknowledge abort. Event-driven: wake the
    // instant a queued stopped signal arrives (WaitForMoreEvents) instead of
    // polling in fixed 10 ms sleeps, so teardown returns as soon as the
    // workers are idle. User input stays excluded so a second folder click
    // can't reenter the teardown.
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (!allIdle() && waitTimer.elapsed() < 3000) {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents |
                            QEventLoop::WaitForMoreEvents, 50);
    }

    if (!allIdle()) qWarning() << "NOT ALLIDLE! STOP FAILED";

    // Clean up connections
    for (const auto& c : conns) QObject::disconnect(c);

    // reset all parameters
    reset(src);

    G::log(""); // force show prev G::log in reset()

    // setCentralMessage("");

    if (src == "Escape key") {
        setCentralMessage("Image loading has been aborted.");
    }


    // qDebug() << "RGH";
    // qDebug() << "MW::stop() Status Report | Source:" << src << "| Instance:" << (int)dm->instance;

    // if (metaRead)
    //     qDebug() << "  MetaRead:     " << (metaRead ? "RUNNING" : "Stopped")
    //              << " | isIdle:" << metaRead->isIdle();

    // metaRead->debugRunStatus();

    // if (buildFilters)
    //     qDebug() << "  BuildFilters: " << (buildFilters->isRunning() ? "RUNNING" : "Stopped")
    //              << " | isIdle:" << buildFilters->isIdle();

    // if (imageCache) {
    //     qDebug() << "  ImageCache:   " << (imageCache->isIdle() ? "IDLE   " : "BUSY   ")
    //              << " | (Main cache state)";

    //     imageCache->debugRunStatus();
    // }

    // qDebug() << "  Global Flags:  G::stop:" << (bool)G::stop << " | dm->abort:" << (bool)dm->abort;





    G::stop = false;
    G::isModifyingDatamodel = false;

    if (G::isLogger || G::isFlowLogger) G::log(srcFun, "done");

}

bool MW::reset(QString src)
{
/*
    New instance and resets everything.
    Only called from MW::stop.
*/

    if (G::isLogger || G::isFlowLogger)
        G::log("MW::reset", "Source: " + src);
    // qDebug() << "MW::reset" << src;

    // datamodel
    dm->selectionModel->clear();
    dm->currentSfRow = 0;
    dm->clearDataModel();
    // new instance: only done here and if sort/filter operation
    dm->newInstance();
    emit initializeImageCache();    // may not be req'd

    G::allMetadataAttempted = false;
    G::iconChunkLoaded = false;

    // filters
    buildFilters->reset();

    setWindowTitle(winnowWithVersion);
    if (G::useInfoView) {
        infoView->clearInfo();
        updateDisplayResolution();
    }
    isDragDrop = false;

    fsTree->setEnabled(true);
    bookmarks->setEnabled(true);
    progress->reset();
    // updateImageCacheStatus();
    filterStatusLabel->setVisible(false);
    updateClassification();
    thumbView->setUpdatesEnabled(false);
    gridView->setUpdatesEnabled(false);
    tableView->setUpdatesEnabled(false);
    tableView->setSortingEnabled(false);
    thumbView->setUpdatesEnabled(true);
    gridView->setUpdatesEnabled(true);
    tableView->setUpdatesEnabled(true);
    tableView->setSortingEnabled(true);
    imageView->clear();
    if (scopesView) scopesView->clear();
    G::isFirstImageNewInstance = true;

    // dm->newInstance();       // newInstance moved to folderSelectionChange()

    // Image cache
    // icd->clear();

    // used by updateStatus
    pickMemSize = "";
    updateStatus(false, "", "MW::reset");

    // // initialize metaRead only when new instance and first folder loaded
    // QMetaObject::invokeMethod(metaRead, "initialize", Qt::QueuedConnection);

    // update metadata read status light
    updateMetadataThreadRunStatus(false, true);
    // updateMetadataThreadRunStatus(true, true, true, "MW::reset");

    // stop slideshow if a new folder is selected
    if (G::isSlideShow && !G::isStressTest) slideShow();

    // if previously in compare mode switch to loupe mode
    if (asCompareAction->isChecked()) {
        asCompareAction->setChecked(false);
        asLoupeAction->setChecked(true);
        updateState();
    }

    // if at welcome or message screen and then select a folder
    if (centralLayout->currentIndex() == StartTab ||
        centralLayout->currentIndex() == MessageTab)
    {
        if (prevMode == "Loupe") asLoupeAction->setChecked(true);
        else if (prevMode == "Grid") asGridAction->setChecked(true);
        else if (prevMode == "Table") asTableAction->setChecked(true);
        else if (prevMode == "Compare") asLoupeAction->setChecked(true);
        else {
            prevMode = "Loupe";
            asLoupeAction->setChecked(true);
        }
    }

    // turn thread activity buttons gray
    setThreadRunStatusInactive();

    // do not embellish
    if (turnOffEmbellish) embelProperties->doNotEmbellish();

    // bookmarkBlocker.unblock();
    // fsTreeBlocker.unblock();

    return true;
}

void MW::waitUntilMetadataLoaded(int ms, QString src)
{
/*
    Wait until metadata has been loaded or ms milliseconds have elapsed
    Not being used
*/
    QString srcFun = "MW::waitUntilMetadataLoaded";
    QEventLoop loop;
    QTimer timeout;

    // qDebug() << srcFun << "G::allMetadataAttempted =" << G::allMetadataAttempted;

    timeout.setSingleShot(true);
    timeout.setInterval(ms);     // millisecond timeout

    // When metadata fully loads, exit the loop
    connect(this, &MW::metadataLoaded, &loop, &QEventLoop::quit);
    // Also quit on timeout
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();

    int t = timeout.remainingTime();
    if (t < 0) t = ms; else t = ms - t;
}

void MW::memoryWatchdogTick()
{
/*
    Periodic GUI-thread probe of the process's resident footprint. Runs
    independently of any other subsystem so it catches runaway allocations
    that happen outside MetaRead::dispatch (folder enumeration, queued
    setData/setIcon events draining on the GUI thread, ImageCache idle
    growth, etc).

    Cheap on macOS (single task_info syscall, microseconds). The cap and
    latch live in Main/global.h.

    Response is graduated rather than a single hard abort:

      • footprint >= cap (G::memoryAbortMB): engage the ImageCache decode throttle —
        park decoders and shrink/trim the cache to free memory. No teardown, no dialog.
        This is the normal Decode-Raw pressure case (full-res raws are large).
      • footprint drops below the resume threshold (cap with ~12% hysteresis): release
        the throttle and resume caching.
      • footprint >= critical (cap + headroom): genuine runaway the throttle could not
        contain — fall back to the old hard onMemoryOverrun abort dialog to avoid OS OOM.

    With Decode Raw off, small JPEG/HEIC decodes never reach the cap, so none of this
    engages — the raw-off path is unchanged.
*/
    if (G::memoryOverrunFlag.load(std::memory_order_relaxed)) return;
    const quint64 cap = G::memoryAbortMB;
    if (cap == 0) return;
    const quint64 footprintMB = G::processFootprintMB();
    if (footprintMB == 0) return;

    const quint64 resumeMB   = (cap * 88) / 100;                 // ~12% hysteresis
    const quint64 criticalMB = cap + qMax<quint64>(2048, cap / 10);

    // Last resort: throttle could not contain the footprint → hard abort (dialog).
    if (footprintMB >= criticalMB) {
        bool expected = false;
        if (G::memoryOverrunFlag.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel, std::memory_order_relaxed))
        {
            if (imageCache) {
                imageCache->setMemoryThrottled(false);
                imageCache->noteMemoryWarning(
                    QString("CRITICAL footprint=%1 MB >= %2 MB; throttle failed, "
                            "hard abort.").arg(footprintMB).arg(criticalMB));
            }
            memoryThrottleActive = false;
            onMemoryOverrun(footprintMB, criticalMB);
        }
        return;
    }

    if (footprintMB >= cap) {
        // Engage throttle (idempotent). setMemoryThrottled only flips an atomic, so the
        // decode choke point stops immediately; memoryPause does the freeing on the
        // ImageCache thread.
        if (!memoryThrottleActive && imageCache) {
            memoryThrottleActive = true;
            imageCache->setMemoryThrottled(true);
            QMetaObject::invokeMethod(imageCache, "memoryPause", Qt::QueuedConnection,
                                      Q_ARG(quint64, footprintMB), Q_ARG(quint64, cap));
        }
    }
    else if (footprintMB <= resumeMB) {
        // Recovered: release throttle and resume caching.
        if (memoryThrottleActive && imageCache) {
            memoryThrottleActive = false;
            imageCache->setMemoryThrottled(false);
            QMetaObject::invokeMethod(imageCache, "memoryResume", Qt::QueuedConnection,
                                      Q_ARG(quint64, footprintMB));
        }
    }
    // Between resumeMB and cap while throttled: hold — let the trim keep draining.
}

void MW::onMemoryOverrun(quint64 footprintMB, quint64 capMB)
{
/*
    Either fired by MetaRead::memoryOverrun (queued from the metaRead
    thread) or called directly from the GUI-thread watchdog tick. In both
    cases we tear down in-flight async work and surface a critical dialog.
*/
    if (G::isLogger || G::isFlowLogger)
        G::log("MW::onMemoryOverrun",
               "footprintMB = " + QString::number(footprintMB)
               + " capMB = " + QString::number(capMB));

    // Latch (idempotent if MetaRead already set it).
    G::memoryOverrunFlag.store(true, std::memory_order_release);

    // Suppress duplicate dialogs if multiple subsystems trip in close succession.
    if (memoryDialogActive) return;
    memoryDialogActive = true;

    // Stop the watchdog while we tear down — no point re-firing during cleanup.
    if (memoryWatchdog) memoryWatchdog->stop();

    // Tear down all in-flight async work that could keep allocating.
    G::stop = true;
    if (dm) dm->abort = true;
    emit abortMetaRead();
    if (imageCache) {
        QMetaObject::invokeMethod(imageCache, "abortProcessing", Qt::QueuedConnection);
    }

    /* Drain queued events first so slots that early-return on the latch
       finish before we block on the dialog. Without this, the QMessageBox
       can spin up while reader-thread emits are still piling onto the
       queue, racing macOS's OOM/crash window. */
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);

    QString msg =
        "<p><b>Winnow stopped loading the folder to avoid running out of memory.</b></p>"
        "<p>Process footprint reached <b>" + QString::number(footprintMB)
        + " MB</b>, exceeding the configured cap of <b>"
        + QString::number(capMB) + " MB</b>.</p>"
        "<p>This is most often triggered by recursing into folders that contain"
        " huge numbers of small JPEG/HEIC files.</p>";

    QMessageBox box(QMessageBox::Critical,
                    "Winnow — memory limit reached",
                    msg, QMessageBox::Ok, this);
    box.setTextFormat(Qt::RichText);
    box.exec();

    memoryDialogActive = false;
    // Clear the latch so a fresh folder load is not pre-tripped, and
    // restart the watchdog so the next load is also protected.
    G::memoryOverrunFlag.store(false, std::memory_order_release);
    G::stop = false;
    if (memoryWatchdog) memoryWatchdog->start();
}

void MW::nullFiltration()
{
/*
    The datamodel sortfilter proxy has 0 rows. Report and clear.
*/
    if (G::isLogger) G::log("MW::nullFiltration");
    QString msg;
    if (dm->rowCount()) msg = "No images match the filtration.";
    else if (dm->folderList.count() == 1)
        msg = "No images in the folder.";
    else msg = "No images in the folders.";
    updateStatus(false, msg, "MW::nullFiltration");
    setCentralMessage(msg);
    infoView->clearInfo();
    imageView->clear();
    if (scopesView) scopesView->clear();
    progress->reset();
    isDragDrop = false;
}


void MW::updateIconRange(QString src)
{
/*
    Polls thumbView, gridView and tableView to determine the first and last thumbnail
    visible. This is used in the metaReadThread to determine the range of thumbnails to
    cache.

    The number of thumbnails to cache in the DataModel (dm->iconChunkSize) is increased if
    it is less than the visible thumbnails.
*/
    if (G::isInitializing) return;

    if (G::isLogger || G::isFlowLogger)
        G::log("MW::updateIconRange", "src = " + src);

    /*
    qDebug().noquote() << "MW::updateIconRange  src =" << src
            << "G::iconChunkLoaded =" << G::iconChunkLoaded
            << "dm->currentSfRow =" << dm->currentSfRow
            << "G::isInitializing =" << G::isInitializing
            << "G::mode =" << G::mode
               ; //*/

    // the chunk range floats within the DataModel range so recalc
    int firstVisible = dm->sf->rowCount();
    int lastVisible = 0;
    bool chunkSizeChanged = false;

    // Grid might not be selected in CentralWidget
    if (G::mode == "Grid") centralLayout->setCurrentIndex(GridTab);

    if (thumbView->isVisible()) {
        thumbView->updateVisible("MW::updateIconRange");
        if (thumbView->firstVisibleCell < firstVisible) firstVisible = thumbView->firstVisibleCell;
        if (thumbView->lastVisibleCell > lastVisible) lastVisible = thumbView->lastVisibleCell;
    }

    if (gridView->isVisible()) {
        gridView->updateVisible("MW::updateIconRange");
        if (gridView->firstVisibleCell < firstVisible) firstVisible = gridView->firstVisibleCell;
        if (gridView->lastVisibleCell > lastVisible) lastVisible = gridView->lastVisibleCell;
    }

    if (tableView->isVisible()) {
        tableView->updateVisible("MW::updateIconRange");
        if (tableView->firstVisibleRow < firstVisible) firstVisible = tableView->firstVisibleRow;
        if (tableView->lastVisibleRow > lastVisible) lastVisible = tableView->lastVisibleRow;
    }

    // visible icons
    int midVisible = (firstVisible + lastVisible) / 2;
    int visibleIcons = lastVisible - firstVisible + 1;

    // publish visibleIcons first so dm->iconChunkFloor() sees the current value
    dm->firstVisibleIcon = firstVisible;
    dm->lastVisibleIcon = lastVisible;
    dm->visibleIcons = visibleIcons;

    /* chunk size: in JIT, hold at least the 3x-visible floor (overrides the memory budget,
       so switching to a denser view e.g. Grid re-applies it); in brute force, just ensure
       the visible page fits (iconChunkSize == rowCount, so this never fires). */
    int minChunk = G::useJitIconCache
        ? qMin(dm->sf->rowCount(), dm->iconChunkFloor())
        : visibleIcons;
    if (dm->iconChunkSize < minChunk) {
        dm->setChunkSize(minChunk);
        chunkSizeChanged = true;
    }

    // Set icon range and G::iconChunkLoaded
    dm->setIconRange(dm->currentSfRow);

    // update icons cached only when the icon or viewport size changes
    if (chunkSizeChanged) {
        bool fileSelectionChange = false;
        G::iconChunkLoaded = false;
        updateChange(midVisible, fileSelectionChange, "MW::updateIconRange");
    }

    /* debug
    qDebug().noquote()
         << "MW::updateIconRange" << "row =" << dm->currentSfRow << "src =" << src
         << "dm->iconChunkSize =" << dm->iconChunkSize
         << "chunkSizeChanged =" << chunkSizeChanged
         // << "G::loadOnlyVisibleIcons =" << G::loadOnlyVisibleIcons
         // << "G::isInitializing =" << G::isInitializing
         << "\n\tthumbView->firstVisibleCell =" << thumbView->firstVisibleCell
         << "thumbView->lastVisibleCell  =" << thumbView->lastVisibleCell
         // << "\n\tgridView->firstVisibleCell =" << gridView->firstVisibleCell
         // << "gridView->lastVisibleCell  =" << gridView->lastVisibleCell
         // << "\n\ttableView->firstVisibleCell =" << tableView->firstVisibleRow
         // << "tableView->lastVisibleCell  =" << tableView->lastVisibleRow
         << "\n\tfirstVisible =" << firstVisible
         << "lastVisible =" << lastVisible
         << "\n\tdm->startIconRange =" << dm->startIconRange
         << "dm->endIconRange =" << dm->endIconRange
            ;
//        */


    return;
}

void MW::reloadIconChunk()
{
/*
    Signalled by DataModel::iconChunkResized when the icon chunk is resized outside the
    normal scroll/selection flow (Layer 2 refineIconChunkSize, Layer 3
    applyIconCachePressure). The range was already updated by setIconRange; here we just
    re-dispatch MetaRead at the current row so the newly in-range icons (re)load.
*/
    if (G::isLogger) G::log("MW::reloadIconChunk");
    if (G::isInitializing || !G::useReadMeta) return;
    QMetaObject::invokeMethod(metaRead, "setStartRow", Qt::QueuedConnection,
                              Q_ARG(int, dm->currentSfRow),
                              Q_ARG(bool, false),
                              Q_ARG(QString, QString("MW::reloadIconChunk"))
                              );
}

void MW::folderChanged(bool aborted)
{
/*
    Signaled from DataModel::processNextFolder after all folders in the DataModel
    folderQueue have been added or deleted.
*/
    QString fun = "MW::folderChanged";
    QString msg = " dm->folderList.count = " + QString::number(dm->folderList.count());
    msg += " dm->rowCount = " + QString::number(dm->rowCount());
    if (G::isLogger || G::isFlowLogger)
        G::log(fun, msg);

    bookmarks->setEnabled(true);
    fsTree->setEnabled(true);
    // update FSTree image count if fsModel isMaxRecurse is true
    if (fsTree->fsModel->isMaxRecurse) fsTree->updateCount();
    if (aborted) {
        fsTree->clearFolderOverLimit();
        fsTree->selectionModel()->clear();
    }

    int startRow = 0;

    // datamodel has rows
    if (dm->rowCount()) {
        infoView->enable(true);  // not setEnabled() because infoView uses a delegate
    }
    // else datamodel is empty
    else {
        QString msg;
        if (aborted) msg = "Loading folder(s) was aborted";
        else msg = "No supported images";
        updateStatus(false, msg, "MW::folderChanged");
        setCentralMessage(msg);
        infoView->enable(false);  // not setEnabled() because infoView uses a delegate
    }

    // if there was a folder and file change
    if (folderAndFileChangePath != "") {
        dm->setCurrent(folderAndFileChangePath, instance);
        startRow = dm->rowFromPath(folderAndFileChangePath);
        qDebug() << fun
                 << "startRow =" << startRow
                 << "folderAndFileChangePath =" << folderAndFileChangePath;
        folderAndFileChangePath = "";
    }
    // reset datamodel current index if it has been removed
    else if (!dm->currentDmIdx.isValid()) {
        dm->setCurrent(dm->index(0,0), dm->instance);
        G::isFirstImageNewInstance = true;
    }

    /*
    qDebug()  << "MW::folderChanged"
              << "startRow =" << startRow
              << "folderAndFileChangePath =" << folderAndFileChangePath;
              //*/

    // Only one folder selected
    if (dm->folderList.count() == 1) {
        // sync bookmarks without triggering another folderSelectionChange
        QSignalBlocker bookmarkBlocker(bookmarks);
        bookmarks->select(dm->folderList.at(0));
        bookmarkBlocker.unblock();

        settings->setValue("lastDir", dm->folderList.at(0));
        addRecentFolder(dm->folderList.at(0));
    }

    /* The memory required for the datamodel (metadata + icons) has to be estimated since the
       ImageCache is starting before all the metadata has been read.  Icons average ~180K and
       metadata ~20K per image */
    int rows = dm->rowCount();
    const int chunk = dm->iconChunkSize.load();
    int maxIconsToLoad = rows < chunk ? rows : chunk;
    G::metaCacheMB = (maxIconsToLoad * 0.18) + (rows * 0.02);

    /*  METADATA DONE IS NOT WORK DONE, and conflating the two is why a catalog scope
        showed filters instantly and never showed a thumbnail.

        This early return means "nothing was added, so there is nothing to read" -- true
        when a load only removed rows or found an empty folder, which is all it ever saw.
        isMetaReadFinished() asks ONLY about metadata (every row attempted), and a
        hydrated catalog scope arrives with every row already MetaLoaded from the index
        and NOT ONE ICON loaded. So the condition was satisfied, metaRead->initialize and
        setStartRow were both skipped, and nothing ever asked for a thumbnail.

        The icon range has to be set before it can be asked about, which is why that call
        moved up from the block below -- it only computes first/last around startRow.

        THE ICONS ARE THE PART THE INDEX CANNOT SERVE. Every other column of a hydrated
        row comes out of the image table; the thumbnail comes from ThumbCache or from
        opening the file, and either way it is MetaRead's job. */
    dm->setIconRange(startRow);
    if (dm->isMetaReadFinished() && dm->isIconRangeLoaded()) {
        G::allMetadataAttempted = true;
        G::iconChunkLoaded = true;
        folderChangeCompleted();
        emit initializeImageCache();    // may not be req'd
        return;
    }

    reverseSortBtn->setEnabled(false);
    filters->setEnabled(false);
    // filters->loadingDataModel(false);   // isLoaded = false  rgh why not req'd?
    filterMenu->setEnabled(false);
    sortMenu->setEnabled(false);
    // Combine Raw/JPG cannot be toggled mid-load (re-enabled in folderChangeCompleted)
    rawJpgStatusBtn->setEnabled(false);
    combineRawJpgAction->setEnabled(false);

    // initialize metaRead only when new instance and first folder loaded
    // Queued: MetaRead lives on metaReadThread; direct call would race with
    // MetaRead::dispatch running on that thread.
    QMetaObject::invokeMethod(metaRead, "initialize",
                              Qt::QueuedConnection,
                              Q_ARG(QString, fun));

    // initialize imageCache
    // guard for BlockingQueued connection
    if (imageCache->thread() != QThread::currentThread()) {
        emit initializeImageCache();
    }

    // rev up metaRead
    if (G::useReadMeta) {
        updateMetadataThreadRunStatus(true);
        QMetaObject::invokeMethod(metaRead, "setStartRow", Qt::QueuedConnection,
                                  Q_ARG(int, startRow),
                                  Q_ARG(bool, true),
                                  Q_ARG(QString, fun)
                                  );
    }
}

void MW::updateChange(int sfRow, bool isFileSelectionChange, QString src)
/*
    Starts or redirects MetaRead metadata and thumb loading at sfRow.  If all
    metadata and icons have been read then folderChangeCompleted is called.

    Called after a scroll event in IconView or TableView by thumbHasScrolled,
    gridHasScrolled or tableHasScrolled.  updateIconRange has been called.

    Signaled by Selection::currentIndex when a file selection change occurs.

*/
{
    if (G::stop || dm->abort) return;

    QString fun = "MW::updateChange";
    if (G::isLogger || G::isFlowLogger)
    {
        G::log("MW::updateChange", "row = " + QString::number(sfRow)
        + " isFileSelectionChange = " + QVariant(isFileSelectionChange).toString()
        + " G::allMetadataAttempted = " + QVariant(G::allMetadataAttempted).toString()
        + " G::iconChunkLoaded = " + QVariant(G::iconChunkLoaded).toString()
        + " src = " + src);
    }

    // set icon range and G::iconChunkLoaded
    dm->setIconRange(sfRow);
    bool metaLoaded = G::allMetadataAttempted && G::iconChunkLoaded;

    /* debug
    {
        qDebug().noquote()
         << "MW::updateChange  sfRow =" << QVariant(sfRow).toString().leftJustified(5)
         << "isFileSelectionChange =" << QVariant(isFileSelectionChange).toString().leftJustified(5)
         << "src =" << src
         << "metaLoaded =" << QVariant(metaLoaded).toString().leftJustified(5)
         << "G::allMetadataAttempted =" << QVariant(G::allMetadataAttempted).toString().leftJustified(5)
         << "G::iconChunkLoaded =" << QVariant(G::iconChunkLoaded).toString().leftJustified(5)
         << "dm->startIconRange =" << dm->startIconRange
         << "dm->endIconRange =" << dm->endIconRange
         // << "dm->iconCount =" << dm->iconCount()
         // << "dm->abortLoadingModel =" << dm->abortLoadingModel
            ;
    } //*/

    if (!metaLoaded) {
        if (G::useReadMeta) {
            // updateMetadataThreadRunStatus(true, true, false, fun);
            QMetaObject::invokeMethod(metaRead, "setStartRow", Qt::QueuedConnection,
                                      Q_ARG(int, sfRow),
                                      Q_ARG(bool, isFileSelectionChange),
                                      Q_ARG(QString, src)
                                      );
        }
    }
    // else {
    //     dm->clearIconsOutsideChunkRange(instance);
    // }

    /* No delay now, but leaving old comment just in case...
       Calling fileSelectionChange while imageView->isFirstImageNewInstance == true
       results in an approx 2 second delay showing the new folder in the thumbView
       and the first image in the loupe view.  Also must check if first file is a video*/
    if (isFileSelectionChange) {
        fileSelectionChange(dm->sf->index(sfRow,0), QModelIndex(), true, "MW::updateChange");
    }
}

QImage MW::devPreview(const QString &fPath)
{
/*
    The cached screen-resolution develop preview for fPath, or a null QImage.

    Keyed on the recipe currently in effect (which during the write debounce may be ahead
    of the sidecar), so the pixels always match what the user last saw. A miss is normal
    and costs nothing -- the caller falls back to the embedded camera JPG.
*/
    if (G::isLogger) G::log("MW::devPreview");
    if (fPath.isEmpty() || !developProperties) return QImage();

    const QString blob = developProperties->developBlobFor(fPath);
    if (blob.isEmpty()) return QImage();          // no edits: the camera JPG is correct

    const QByteArray jpg = DevPreviewCache::instance().get(
        fPath, Metadata::devPreviewKey(blob).toLatin1());
    if (jpg.isEmpty()) return QImage();

    QImage im;
    if (!im.loadFromData(jpg, "JPG") || im.isNull()) return QImage();
    return im;
}

QByteArray MW::developRecipeKey(const QString &fPath)
{
/*
    The hash of the develop recipe fPath carries RIGHT NOW (empty when it has none). A
    render captures this when it is launched; the devPreview provider compares it against
    the recipe being flushed, so a frame rendered from an earlier recipe can never be
    written out as a preview of a later one. See MW::developFrameRecipe.
*/
    if (fPath.isEmpty() || !developProperties) return QByteArray();
    const QString blob = developProperties->developBlobFor(fPath);
    if (blob.isEmpty()) return QByteArray();
    return Metadata::devPreviewKey(blob).toLatin1();
}

void MW::devPreviewUpdated(const QString &fPath, const QImage &thumb)
{
/*
    A develop edit has just been written to fPath's sidecar. Bring the grid into line.

    thumb non-null : the newly rendered 256px preview -> paint it now.
    thumb null     : no preview could be made for the new recipe (the usual case is a
                     multi-image propagation target, which has no proxy in memory), so the
                     thumbnail on screen is now stale. Forget it and let the icon loader
                     re-read the camera thumbnail.

    Both views keep a per-row scaled QPixmap in their delegate, so the row's cache entry
    has to be dropped or they keep painting the old thumb.
*/
    if (G::isLogger) G::log("MW::devPreviewUpdated");
    if (fPath.isEmpty() || !dm) return;

    const int dmRow = dm->rowFromPath(fPath);
    if (dmRow < 0) return;

    /* Keep the develop badge and the devPreview key in step with the recipe that was just
       written. The key is what a decoder thread matches against the devPreview cache, so
       leaving it stale would keep serving the picture the user has just changed. */
    const QString blob = developProperties->developBlobFor(fPath);
    dm->setData(dm->index(dmRow, G::DevelopColumn), !blob.isEmpty());
    dm->setData(dm->index(dmRow, G::DevPreviewKeyColumn),
                blob.isEmpty() ? QString() : Metadata::devPreviewKey(blob));

    /* The full-size image cached for this path was decoded from the OLD recipe (or from
       the camera render), so it no longer depicts the image. Drop it: the next visit
       re-reads, which outside Develop mode now means loading the new devPreview. */
    if (icd) icd->remove(fPath);

    if (!thumb.isNull()) dm->setDevelopIcon(dmRow, thumb);
    else dm->clearDevelopIcon(dmRow);

    const int sfRow = dm->proxyRowFromPath(fPath);
    if (sfRow >= 0) {
        if (thumbView && thumbView->iconViewDelegate)
            thumbView->iconViewDelegate->clearCacheItem(sfRow);
        if (gridView && gridView->iconViewDelegate)
            gridView->iconViewDelegate->clearCacheItem(sfRow);
    }

    /* The cleared row needs the loader to run again to get its camera thumb back -- and it
       needs MetaRead told that a loaded icon was discarded, or in the default brute-force
       icon chunk it refuses to re-read the row and the thumbnail just disappears. See
       MetaRead::invalidateLoadedIcons and MW::setPreviewSource. */
    if (thumb.isNull()) {
        QMetaObject::invokeMethod(metaRead, "invalidateLoadedIcons", Qt::QueuedConnection);
        reloadIconChunk();
    }
}

void MW::updateCatalogForRow(int dmRow)
{
/*
    Keep the catalog in step with a rating or colour label the user has just set.

    WHY IT IS NEEDED AT ALL. An edit writes the sidecar, which is the source of
    truth, and the catalog notices on the NEXT visit to that folder: writeXMP
    moves the sidecar's mtime, the freshness stamp stops matching, and the folder
    is re-indexed. That is enough while folders are how images are reached.

    IT IS NOT ENOUGH IN CATALOG SCOPE, which is the point of that scope: the user
    is working from search results and may never open the containing folder
    again. Without this, rating an image in Catalog scope and then searching for
    that rating would not find it -- the catalog would still hold the old value,
    indefinitely.

    THE WHOLE ROW GOES, not just the field that changed, and through the same
    DataModel::catalogRowFor the bulk capture uses. Pushing only the changed
    field would leave the freshness stamp saying "current" over whatever else had
    drifted; sending the row the model actually holds makes the entry true rather
    than less wrong.

    Off the GUI thread, because it is SQL. commit() skips a row whose stamp
    already matches, so this costs nothing when the catalog is already right.
*/
    if (dmRow < 0 || !dm) return;
    if (!Catalog::instance().isAvailable()) return;

    CatalogRow r;
    if (!dm->catalogRowFor(dmRow, r)) return;

    QThreadPool::globalInstance()->start([r]{ Catalog::instance().commit({r}); });
}

void MW::folderChangeCompleted()
{
/*
    Signalled by MetaRead::dispatchFinished (done) when finished reading all metadata.
    Also called by folderChanged when a folder has been removed.

    - check for missing thumbnails.
    - update filters
    - resize tableView columns
*/
    if (G::isLogger || G::isFlowLogger)
    {
        int rows = dm->rowCount();
        QString msg = QString::number(rows) + " images";
        G::log("MW::folderChangeCompleted", msg);
    }
    QString fun = "MW::folderChangeCompleted";

    /* One-shot develop-preview cache sweep, deferred to here so it never competes with
       the folder load the user is waiting for, and run off the GUI thread because it
       stats every cached entry. Deliberately NOT at shutdown: closeEvent is already
       doing synchronous teardown, and a force-quit or crash would skip it entirely.
       See Cache/devpreviewcache.h for why it demotes rather than deletes. */
    if (!devPreviewSweepDone) {
        devPreviewSweepDone = true;
        QThreadPool::globalInstance()->start([]{
            DevPreviewCache::instance().sweep();
            DevPreviewCache::instance().save();
        });
    }

    /* Catalog the folder that just loaded, so its keywords, titles and camera data stay
       searchable after the user navigates away. Deferred to here and run off the GUI
       thread for the same reasons as the sweep above: metadata is complete, and the load
       the user was waiting on must not carry the insert.

       The rows are read from the model HERE, on the GUI thread -- the model is not
       thread-safe -- and only the resulting values cross to the pool. Unchanged rows are
       skipped inside commit(), so pacing back and forth between two folders is a read of
       one row per image and no writes at all. */
    /*  POSTED, NOT RUN INLINE, and that is a fix rather than a tidy-up.

        catalogRows() skips any row that is not yet G::MetaLoaded, and the writes
        that set that status arrive from the Reader and Thumb threads as QUEUED
        setValDm events. MetaRead::done -- which is what brings us here -- can
        therefore fire while those events are still sitting in the GUI thread's
        queue, so catalogRows() returns an EMPTY list and the folder is silently
        not catalogued at all. It depends on nothing but timing, which is why the
        catalog had entries for some folders and not others, and why a headless
        run never catalogued anything: the app exits before the queue drains.

        A queued invocation runs after every event already posted, which is
        exactly the set of status writes being waited for. */
    /*  AND THE OTHER HALF OF IT: WHAT IS NO LONGER THERE.

        A folder scope reconciles the index against the filesystem -- that is what
        distinguishes it from a catalog scope, which has no directory to enumerate (see
        ScopeRequest) -- and the commit above is only the half that says what IS here.
        Without the other half a file deleted outside Winnow stays live in the index and
        keeps turning up in searches until the once-a-session global sweep happens to
        stat it. The enumeration just listed the folder, so answering it costs no stat at
        all: whatever the index still calls live and the listing did not find is gone.

        ONLY FOR A SCOPE THAT ACTUALLY ENUMERATED, and only for a load that FINISHED. A
        catalog scope's paths are a search result from a hundred folders, not a listing of
        any one of them, and treating them as one would demote most of the library. An
        aborted or memory-capped load is a partial listing for the same reason. Both are
        the same precondition Catalog::reconcileFolder states, checked here because this
        is where it is knowable. */
    const bool reconcile = dm->scopeRequest().reconcile && !dm->abort && !G::stop;

    /*  A HYDRATED CATALOG SCOPE HAS NOTHING TO COMMIT, and finding that out cost a
        minute of blocked GUI. dm->catalogRows() reads ~40 columns of every row on the
        GUI thread to build the commit payload -- measured at 56 s of dead event loop for
        43,050 rows, which is what "several minutes before any thumbnails" actually was.
        For rows that CAME from the index, unchanged, the whole exercise re-writes the
        catalog with what it already holds.

        A folder scope still commits: its rows were read from files and the index may
        never have seen them. So may a catalog scope whose rows fell through to a file
        read -- but those are the stale ones, which the verification pass will commit when
        it re-reads them, not this. */
    const bool nothingToCommit = dm->scopeRequest().scope == G::Scope::Catalog
                                 && !dm->scopeRequest().rows.isEmpty();

    QMetaObject::invokeMethod(this, [this, reconcile, nothingToCommit]{
        if (nothingToCommit) {
            if (G::isPerfProbe)
                qDebug().noquote() << "[PERF] catalog commit SKIPPED (rows came from the index)";
            return;
        }
        QElapsedTimer ct;
        if (G::isPerfProbe) ct.start();
        const QVector<CatalogRow> rows = dm->catalogRows();
        if (G::isPerfProbe)
            qDebug().noquote() << "[PERF] catalogRows()" << rows.size() << "rows in"
                               << ct.elapsed() << "ms (GUI thread)";
        const QHash<QString, QSet<QString>> present = reconcile ? dm->folderPathSets()
                                                               : QHash<QString, QSet<QString>>();
        if (!rows.isEmpty() || !present.isEmpty()) {
            QThreadPool::globalInstance()->start([this, rows, present]{
                Catalog::instance().commit(rows);
                /* After the commit, so a file seen for the first time is inserted (and
                   live) before anything is compared against the listing. */
                for (auto it = present.cbegin(); it != present.cend(); ++it)
                    Catalog::instance().reconcileFolder(it.key(), it.value());
                /* Tell the panel what just changed, but only if someone is looking at
                   it: the Keywords category is a query plus a tree rebuild, and the whole
                   point of doing the commit out here is to not spend GUI time on the
                   catalog during a folder load. Back on the GUI thread -- the widget
                   must not be touched from the pool. */
                QMetaObject::invokeMethod(this, [this]{
                    if (catalogDock && catalogDock->isVisible()) catalogView->refresh();
                    if (findPanel && filterDock->isVisible()) findPanel->refresh();
                    updateCatalogScopeRows();
                }, Qt::QueuedConnection);
            });
        }
    }, Qt::QueuedConnection);

    /* One-shot catalog sweep, paired with the devPreview one above and for the same
       reason: rows whose image is gone should stop appearing in searches. It demotes
       rather than deletes, and skips unmounted volumes, so an ejected card is not a mass
       deletion. */
    if (!catalogSweepDone) {
        catalogSweepDone = true;
        QThreadPool::globalInstance()->start([]{ Catalog::instance().sweep(); });
    }

    QMetaObject::invokeMethod(imageCache, "updateInstance", Qt::QueuedConnection);

    /* Optional background devPreview build for edited images in this folder that have no
       current preview. Off by default: building one means decoding and rendering the
       image, which is the work the byproduct rule exists to avoid. Queued here, after the
       load, for the same reason as the sweep above. */
    queueBackgroundDevPreviewBuild();

    // req'd when rememberLastDir == true and loading folder at startup
    fsTree->scrollToCurrent();

    // // update FSTree image count if fsModel isMaxRecurse is true
    // if (fsTree->fsModel->isMaxRecurse) fsTree->updateCount();

    // hide metadata read progress
    progress->clearProgress(progressMetaReadRow);
    updateMetadataThreadRunStatus(false, true);

    // build filters if filter dock is visible
    /*
    qDebug() << "MW::folderChangecompleted"
             << "dm->folderList.count() =" << dm->folderList.count()
             << "dm->isQueueEmpty() =" << dm->isQueueEmpty()
             << "filterDock->visibleRegion().isNull() =" << filterDock->visibleRegion().isNull()
                ; //*/
    if (dm->folderList.count() > 0
        // && dm->isQueueEmpty()
        // && !filterDock->visibleRegion().isNull()
       )
    {
        buildFiltersWhenModelReady(dm->instance);
    }

    /* now okay to write to xmp sidecar, as metadata is loaded and initial
    updates to InfoView by fileSelectionChange have been completed. Otherwise,
    InfoView::dataChanged would prematurely trigger Metadata::writeXMP
    It is also okay to filter.  */

    // re-enable cause we're all loaded
    reverseSortBtn->setEnabled(true);
    filters->setEnabled(true); //x
    filterMenu->setEnabled(true);
    sortMenu->setEnabled(true);
    rawJpgStatusBtn->setEnabled(true);
    combineRawJpgAction->setEnabled(true);
    // must retain default order in datamodel as ImageCache is already working
    updateSortColumn(G::NameColumn);

    enableStatusBarBtns();
    updateStatus(true, "", fun);    // clear any status message

    // update image cache in case not already done during metaRead
    emit setImageCachePosition(dm->currentFilePath, fun);

    // resize table columns now that all data is loaded
    tableView->resizeColumns();
    // hide columns per preferences (redo because datamodel has been cleared)
    tableView->showOrHide();

    G::isModifyingDatamodel = false;

    /* signal anything waiting for the metadata update to finish, including
       dm->insert() */
    emit metadataLoaded();

    /* test if any null thumbnails
    bool isNullIcon = false;
    for (int i = 0; i < dm->rowCount(); ++i) {
        QVariant icon = dm->index(i,0).data(Qt::DecorationRole);
        if (icon.isNull()) {
            // qWarning() << "Warning: row" << i << "icon is null"
            //            << "G::iconChunkLoaded =" << G::iconChunkLoaded;
            QString fPath = dm->index(i,0).data(G::PathRole).toString();
            G::issue("Warning", "Icon is null", fun, i, fPath);
        }
    }
    //*/
}

void MW::buildFiltersWhenModelReady(int forInstance, int attempt)
{
/*
    Build the filter tree only once the DataModel is fully updated.

    Readers run on their own threads and deliver metadata/icons to the DataModel
    via QueuedConnection (Reader::addToDatamodel -> DataModel::addMetadataForItem,
    Reader::setIcon -> DataModel::setIcon1) on the GUI thread. MetaRead emits
    done() (-> folderChangeCompleted) once all readers have RETURNED, which can
    precede the application of those still-queued writes. Building filters then
    reads empty metadata columns and produces only the file-enumeration
    categories (File types, Folder) — the small-folder symptom where every reader
    finishes in one burst before any queued write is drained.

    dm->queuedReaderEvents counts reader-emitted events not yet applied: it is
    bumped before each emit (Reader) and decremented via RAII on every code path
    in addMetadataForItem/setIcon1, so == 0 means the model is fully updated. It
    is terminal here because done() fired only after every reader returned, so no
    further reader events will be produced for this instance.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MW::buildFiltersWhenModelReady");

    // folder changed (or shutting down) while we were waiting — a fresh
    // folderChangeCompleted for the new instance will drive its own build.
    if (G::stop || dm->abort || forInstance != dm->instance) return;

    /* Lazy build: do not build while the Filters panel is hidden. Building a
       hidden Filters tree is unsafe (crashes with videos in the mix) and would
       latch filters->filtersBuilt, blocking the rebuild. When hidden, leave the
       filters unbuilt; MW::showFilterDock / MW::filterDockTabMousePress build
       them when the panel is shown (the datamodel is fully loaded by then). */
    if (!filterDock->isVisible()) return;

    // wait for the reader-event queue to drain so the datamodel is fully updated
    if (dm->queuedReaderEvents.load(std::memory_order_relaxed) > 0) {
        /* Bounded fallback: queuedReaderEvents is balanced (every Reader emit has a
           matching RAII decrement in addMetadataForItem/setIcon1), so it normally
           drains to 0 once the readers finish. But a wedged GUI loop or a future
           increment/decrement imbalance could leave it stuck. Rather than reschedule
           forever — which would silently never build the filters — cap the wait and
           then build anyway with whatever has been applied. */
        constexpr int kPollMs = 10;
        constexpr int kMaxAttempts = 200;           // 200 × 10ms ≈ 2s
        if (attempt < kMaxAttempts) {
            QTimer::singleShot(kPollMs, this, [this, forInstance, attempt]{
                buildFiltersWhenModelReady(forInstance, attempt + 1);
            });
            return;
        }
        G::issue("Warning",
                 QString("Reader-event queue did not drain (queuedReaderEvents = %1) "
                         "after %2 ms; building filters anyway")
                     .arg(dm->queuedReaderEvents.load(std::memory_order_relaxed))
                     .arg(kMaxAttempts * kPollMs),
                 "MW::buildFiltersWhenModelReady");
        // fall through and build with whatever metadata has been applied
    }

    buildFilters->build();
    buildFilters->recount();
    filters->setEnabled(true);
}

void MW::thumbHasScrolled()
{
/*
    This function is called after a thumbView scrollbar change signal. The visible
    thumbnails are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined
    by G::ignoreScrollSignal == false. However, if the scroll change was caused by
    syncing with another view then we do not want to process again and get into a loop.

    MW::updateIconRange polls updateVisible in all visible views (thumbView, gridView and
    tableView) to assign the firstVisibleRow, midVisibleRow and lastVisibleRow in
    metaReadThread (Concurrent).

    The gridView and tableView, if visible, are scrolled to sync with thumbView.

    Finally, metaReadThread->setCurrentRow is called to load any necessary metadata and
    icons within the cache range.
*/
    QString fun = "MW::thumbHasScrolled";
    if (G::isLogger || G::isFlowLogger)
        G::log(fun,
               "G::ignoreScrollSignal = " + QVariant(G::ignoreScrollSignal).toString()
               + " thumbView->midVisibleCell = "
               + QVariant(thumbView->midVisibleCell).toString()
               );

    if (G::isInitializing) return;

    if (G::resizingIcons) G::resizingIcons = false;
    else {
        thumbView->updateMidVisibleCell(fun);

        if (G::ignoreScrollSignal == false) {
            G::ignoreScrollSignal = true;
            /* Only a visible thumbView may drive the shared scroll/sync; a hidden thumb strip
               reports a bogus midVisibleCell (see MW::tableHasScrolled). */
            if (thumbView->isVisible()) {
                updateIconRange("MW::thumbHasScrolled");
                if (gridView->isVisible()) {
                    gridView->scrollToRow(thumbView->midVisibleCell, "MW::thumbHasScrolled");
                }
                if (tableView->isVisible()) {
                    tableView->scrollToRow(thumbView->midVisibleCell, "MW::thumbHasScrolled");
                }
                updateChange(thumbView->midVisibleCell, false, "MW::thumbHasScrolled");
                // update thumbnail zoom frame cursor
                QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
                if (idx.isValid()) {
                    thumbView->zoomCursor(idx, "MW::thumbHasScrolled");
                }
            }
        }
        G::ignoreScrollSignal = false;
    }
}

void MW::gridHasScrolled()
{
/*
    This function is triggered after a gridView scrollbar change signal. The visible
    thumbnails are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined
    by G::ignoreScrollSignal == false. However, if the scroll change was caused by
    syncing with another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection,
    which will trigger a thumbnail update in MW::fileSelectionChange.

    MW::updateIconRange polls updateVisible in all visible views (thumbView, gridView and
    tableView) to assign the firstVisibleRow, midVisibleRow and lastVisibleRow in
    metaReadThread (Concurrent).

    The thumbView and tableView, if visible, are scrolled to sync with gridView.

    Finally, metaReadThread->setCurrentRow is called to load any necessary metadata and
    icons within the cache range.
*/
    QString fun = "MW::gridHasScrolled";
    if (G::isLogger || G::isFlowLogger)
        G::log(fun, "isVisible = " +
               QVariant(gridView->isVisible()).toString());
    if (G::isInitializing) return;

    if (G::resizingIcons) G::resizingIcons = false;
    else {
        gridView->updateMidVisibleCell(fun);

        if (G::ignoreScrollSignal == false) {
            G::ignoreScrollSignal = true;
            /* Only a visible grid may drive the shared scroll/sync; a hidden grid reports a
               bogus midVisibleCell that would yank the visible views (see MW::tableHasScrolled). */
            if (gridView->isVisible()) {
                updateIconRange("MW::gridHasScrolled");
                thumbView->scrollToRow(gridView->midVisibleCell, fun);
                tableView->scrollToRow(gridView->midVisibleCell, fun);
                updateChange(gridView->midVisibleCell, false, fun);
            }
        }
        G::ignoreScrollSignal = false;
    }
}

void MW::tableHasScrolled()
{
/*
    This function is called after a tableView scrollbar change signal. The visible
    thumbnails are determined and loaded if necessary.

    If the change was caused by the user scrolling then we want to process it, as defined
    by G::ignoreScrollSignal == false. However, if the scroll change was caused by
    syncing with another view then we do not want to process again and get into a loop.

    Also, we do not need to process scrolling if it was the result of a new selection, which
    will trigger a thumbnail update in MW::fileSelectionChange.  G::isNewFileSelection is set true
    in IconView when a selection is made, and set false in fileSelectionChange.

    MW::updateIconRange polls updateVisible in all visible views (thumbView, gridView and
    tableView) to assign the firstVisibleRow, midVisibleRow and lastVisibleRow in
    metaReadThread (Concurrent).

    The gridView and thumbView, if visible, are scrolled to sync with tableView.

    Finally, metaReadThread->setCurrentRow is called to load any necessary metadata and
    icons within the cache range.
*/
    if (G::isLogger || G::isFlowLogger)
        G::log("MW::tableHasScrolled");
    if (G::isInitializing) return;

    if (G::ignoreScrollSignal == false) {
        G::ignoreScrollSignal = true;
        /* Only a visible table may drive the shared scroll/sync. A tableHasScrolled signal
           while the table is hidden (loupe/grid mode) is a layout/sync artifact: midVisibleRow
           is a bogus 0 because the hidden viewport isn't laid out, and propagating it would
           scroll the visible thumbView back to row 0 (and load the wrong icon range).
           gridHasScrolled/thumbHasScrolled sync-scroll the hidden table, which re-enters here. */
        if (tableView->isVisible()) {
            updateIconRange("MW::tableHasScrolled");
            dm->scrollToIcon = tableView->midVisibleRow;
            if (thumbView->isVisible()) {
                thumbView->scrollToRow(tableView->midVisibleRow, "MW::tableHasScrolled");
            }
            updateChange(tableView->midVisibleRow, false, "MW::tableHasScrolled");
        }
    }
    G::ignoreScrollSignal = false;
}

void MW::loadEntireMetadataCache(QString source)
{
/*
    This is called before a filter or sort operation, which only makes sense if all the
    metadata has been loaded. This function does not load the icons. It is not run in a
    separate thread as the filter and sort operations cannot commence until all the metadata
    has been loaded.
*/
//    if (G::isLogger || G::isFlowLogger) G::log("MW::loadEntireMetadataCache", "Source: " + source);
    if (G::isLogger || G::isFlowLogger)
        qDebug() << "MW::loadEntireMetadataCache"
             << "Source: " << source
             << "G::isInitializing: " << G::isInitializing
             ;
    if (G::isInitializing) return;
    if (dm->isMetaReadFinished()) return;

    updateIconRange("MW::loadEntireMetadataCache");

    QApplication::setOverrideCursor(Qt::WaitCursor);

    /* adding all metadata in dm slightly slower than using metadataReadThread but progress
       bar does not update from separate thread.  RGH check if still true.
    */
    dm->addAllMetadata();

    QApplication::restoreOverrideCursor();

}

void MW::updateImageCacheStatus(int instruction, bool isAutoSize,
                                quint64 currMB, quint64 maxMB, int tFirst, int tLast,
                                QString source)
{
/*
    Displays a statusbar showing the image cache status. Also shows the cache size
    in the info panel. All status info is passed by copy to prevent collisions on
    source data, which is being continuously updated by ImageCache
*/
    // if (G::instanceClash(instance, "MW::updateImageCacheStatus")) return;

    if (G::isLogger) {
        QString strInstruction = imageCache->statusAction.at(instruction);
        QString s = "Instruction: " + strInstruction + "  Source: " + source;
        G::log("MW::updateImageCacheStatus", s);
    }
    if (G::isSlideShow && isSlideShowRandom) return;
    if (G::stop) return;

    /*
    QString msg = "   currMB: " + QString::number(cache.currMB) +
                  "   minMB: "  + QString::number(cache.minMB) +
                  "   maxMB: "  + QString::number(cache.maxMB);
    updateStatus(true, msg);
    //*/

    /*
    qDebug() << "MW::updateImageCacheStatus  Instruction ="
             << imageCache->statusAction.at(instruction)
             << "source =" << source
             << "currMB =" << currMB
             << "maxMB =" << maxMB
             << "tFirst =" << tFirst
             << "tLast =" << tLast
             << "rows =" << dm->sf->rowCount()
             << "cacheBarProgressWidth =" << cacheBarProgressWidth
             << "G::showProgress =" << G::showProgress
             << "G::showProgress::ImageCache =" << G::ShowProgress::ImageCache
                ; //*/

    // show cache amount ie "4.2 of 16.1GB (4 threads)" in info panel
    bool isAuto = imageCache->getAutoMaxMB();
    QString cacheMsg;
    if (isAuto) {
        QString autoStrategy = imageCache->getAutoStrategy() + " mode";
        QString autoMode = imageCache->getAutoMaxMB() ? autoStrategy : "";
        cacheMsg = QString::number(double(currMB)/1024,'f',1)
                   + " GB   " + autoStrategy;
    }
    else {
        cacheMsg = QString::number(double(currMB)/1024,'f',1)
                + " of "
                + QString::number(double(maxMB)/1024,'f',1)
                + " GB ";
    }
    QString freeMem = QString::number(double(G::availableMemoryMB)/1024,'f',1)
            + " GB" + " ("
            + QString::number(imageCache->decoderCount)
            + " threads)";
    if (G::useInfoView) {
        QStandardItemModel *k = infoView->ok;
        k->setData(k->index(infoView->CacheRow, 1, infoView->statusInfoIdx), cacheMsg);
        k->setData(k->index(infoView->FreeMemRow, 1, infoView->statusInfoIdx), freeMem);
    }

    // update tooltip
    if (instruction == ImageCache::StatusAction::Size) {
        imageThreadRunningLabel->setToolTip(getImageCacheRunningTip(isAutoSize, maxMB));
    }


    // if (G::showProgress != G::ShowProgress::ImageCache) return;

    // Clear: just repaint the progress bar gray and return.
    if (instruction == ImageCache::StatusAction::Clear) {
        progress->clearImageCacheProgress();
        return;
    }

    // Update all rows
    if (instruction == ImageCache::StatusAction::All ||
        instruction == ImageCache::StatusAction::Clear)
    {
        int rows = dm->sf->rowCount();
        // clear progress
        progress->clearImageCacheProgress();
        progress->updateImageCacheProgress(tFirst, tLast, rows,
                                         progress->targetColorGradient);
        // cached
        for (int i = tFirst; i <= tLast; ++i) {
            if (i >= rows) break;
            if (dm->sf->index(i, G::IsCachedColumn).data().toBool()) {
                progress->updateImageCacheProgress(i, i, rows,
                                  progress->imageCacheGradient);
            }
        }

        // cursor
        progress->updateCursor(dm->currentSfRow, rows);
        return;
    }

    return;
}

void MW::bookmarkClicked(QTreeWidgetItem *item, int col)
{
/*
    Called by signal itemClicked in bookmark.
*/
    if (G::isLogger) G::log("MW::bookmarkClicked");

    if (G::stop) {
        G::popup->showPopup("Busy, try new folder in a sec.", 1000);
        return;
    }

    const QString dPath = item->toolTip(col);
    if (isFolderValid(dPath, true, false)) {
        // folderSelectionChange(dPath);
        QModelIndex idx = fsTree->fsModel->index(dPath);
        QModelIndex filterIdx = fsTree->fsFilter->mapFromSource(idx);
        // fsTree->setCurrentIndex(filterIdx);
        fsTree->select(dPath, "None", "Bookmark");
        fsTree->scrollTo(filterIdx, QAbstractItemView::PositionAtCenter);
        // must have focus to show selection in blue instead of gray
        fsTree->setFocus();
    }
    else {
        stop("Bookmark clicked");
        // reset("Bookmark clicked");
        setWindowTitle(winnowWithVersion);
        enableSelectionDependentMenus();
        enableStatusBarBtns();
        infoView->enable(false);  // not setEnabled() because infoView uses a delegate
        setCentralMessage("Bookmarked folder no longer exists or is not available.");
    }
}

void MW::checkDirState(const QString &dirPath)
{
/*
    called when signal rowsRemoved from FSTree
    does this get triggered if a drive goes offline???
    rgh this needs some TLC
*/
    qDebug() << "MW::checkDirState" << dirPath;
    if (G::isLogger) G::log("MW::checkDirState");
    if (G::isInitializing) return;

    // if (!QDir().exists(G::currRootFolder)) {
    //     qDebug() << "MW::checkDirState" << "G::currRootFolder = Blank";
    //     // G::currRootFolder = "";
    // }
}

QString MW::getSelectedPath()
{
    if (G::isLogger) G::log("MW::getSelectedPath");
    if (isDragDrop)  return dragDropFolderPath;

    if (fsTree->selectionModel()->selectedRows().size() == 0) return "";

    QModelIndex idx = fsTree->selectionModel()->selectedRows().at(0);
    if (!idx.isValid()) return "";

    QString path = idx.data(QFileSystemModel::FilePathRole).toString();
    QFileInfo dirInfo = QFileInfo(path);
    currRootDir = dirInfo.dir();
    return dirInfo.absoluteFilePath();
}

// MAINWINDOW DOCK RELATED FUNCTIONS MOVED TO DOCKWIDGET.CPP

void MW::embelTemplateChange(int id)
{
    if (G::isLogger) G::log("MW::embelTemplateChange");
    //qDebug() << "MW::embelTemplateChange  embel->isRemote =" << embel->isRemote;
    if (embel->isRemote) return;
    embelTemplatesActions.at(id)->setChecked(true);
    if (id == 0) {
        embelRunBtn->setVisible(false);
        setRatingBadgeVisibility();
        // setShootingInfoVisibility();
    }
    else {
        if (dm->rowCount()) {
            loupeDisplay("MW::embelTemplateChange");
            embelRunBtn->setVisible(true);
            isRatingBadgeVisible = false;
            thumbView->refreshIcons("MW::embelTemplateChange");
            gridView->refreshIcons("MW::embelTemplateChange");
            updateClassification();
            imageView->infoOverlay->setVisible(false);
        }
    }
}

void MW::syncEmbellishMenu()
{
    if (G::isLogger) G::log("MW::syncEmbellishMenu");
    int count = embelProperties->templateList.length();
    for (int i = 0; i < 30; i++) {
        if (i < count) {
            embelTemplatesActions.at(i)->setText(embelProperties->templateList.at(i));
            embelTemplatesActions.at(i)->setVisible(true);
        }
        else {
            embelTemplatesActions.at(i)->setText("Future Template"  + QString::number(i));
            embelTemplatesActions.at(i)->setVisible(false);
        }
    }
}

void MW::refreshAfterImageCacheSizeChange()
{
/*
    This slot is called from the preferences dialog.  Any visibility
    changes are executed.
*/
    if (G::isLogger) G::log("MW::setImageCacheParameters");

    // thumbnail cache status indicators
    thumbView->refreshIcons("MW::setImageCacheParameters");
    gridView->refreshIcons("MW::setImageCacheParameters");
}

void MW::showHiddenFiles()
{
    // rgh ??
    if (G::isLogger) G::log("MW::showHiddenFiles");
//    fsTree->setModelFlags();
}

void MW::thumbsEnlarge()
{

    // if (G::isLogger)
        G::log("MW::thumbsEnlarge");
    if (gridView->isVisible()) gridView->justify(IconView::JustifyAction::Enlarge);
    if (thumbView->isVisible())  {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Enlarge);
        else thumbView->thumbsEnlarge();
    }

    // if thumbView visible and zoomed in imageView then may need to redo the zoomFrame
    if (thumbView->isVisible())  {
        QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
        if (idx.isValid()) {
            thumbView->zoomCursor(idx, "MW::thumbsEnlarge", /*forceUpdate=*/true);
        }
    }
}

void MW::thumbsShrink()
{
    // if (G::isLogger)
        G::log("MW::thumbsShrink");
    if (gridView->isVisible()) gridView->justify(IconView::JustifyAction::Shrink);
    if (thumbView->isVisible()) {
        if (thumbView->isWrapping()) thumbView->justify(IconView::JustifyAction::Shrink);
        else thumbView->thumbsShrink();
    }
    return;
    scrollToCurrentRowIfNotVisible();

    // if thumbView visible and zoomed in imageView then may need to redo the zoomFrame
    if (thumbView->isVisible())  {
        QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
        if (idx.isValid()) {
            thumbView->zoomCursor(idx, "MW::thumbsShrink", /*forceUpdate=*/true);
        }
    }
}

void MW::addRecentFolder(QString fPath)
{
    if (G::isLogger) G::log("MW::addRecentFolder");
    if (G::stop) return;

    if (recentFolders->contains(fPath) || fPath == "") return; // EXC_BAD_ACCESS (SIGSEGV)
    recentFolders->prepend(fPath);
    int n = recentFolders->count();
    for (int i = 0; i < maxRecentFolders; i++) {
        if (i < n) {
            recentFolderActions.at(i)->setText(recentFolders->at(i));
            recentFolderActions.at(i)->setVisible(true);
        }
        else {
            recentFolderActions.at(i)->setText("Future recent folder" + QString::number(i));
            recentFolderActions.at(i)->setVisible(false);
        }
    }
    // if already maxRecentFolders trim excess items
    if (n > maxRecentFolders) {
        for (int i = n; i > maxRecentFolders; --i) {
            recentFolders->removeAt(i - 1);
        }
    }

    // update settings
    settings->beginGroup("RecentFolders");
    settings->remove("");
    QString leadingZero;
    for (int i = 0; i < recentFolders->count(); i++) {
        i < 9 ? leadingZero = "0" : leadingZero = "";
        settings->setValue("recentFolder" + leadingZero + QString::number(i+1),
                          recentFolders->at(i));
    }
    settings->endGroup();
}

void MW::addIngestHistoryFolder(QString fPath)
{
    if (G::isLogger) G::log("MW::addIngestHistoryFolder");
    // keep track of ingest location if gotoIngestFolder == true
    lastIngestLocation = fPath;

    if (!ingestHistoryFolders->contains(fPath) && fPath != "")
        ingestHistoryFolders->prepend(fPath);
    int count = ingestHistoryFolders->count();
    for (int i = 0; i < maxIngestHistoryFolders; i++) {
        if (i < count) {
            ingestHistoryFolderActions.at(i)->setText(ingestHistoryFolders->at(i));
            ingestHistoryFolderActions.at(i)->setVisible(true);
        }
        else {
            ingestHistoryFolderActions.at(i)->setText("Future ingest history folder" + QString::number(i));
            ingestHistoryFolderActions.at(i)->setVisible(false);
        }
    }
}

void MW::invokeRecentFolder(QAction *recentFolderActions)
{
    QString dirPath = recentFolderActions->text();
    if (G::isLogger) G::log("MW::invokeRecentFolder", dirPath);

    fsTree->select(dirPath);
}

void MW::invokeIngestHistoryFolder(QAction *ingestHistoryFolderActions)
{
    if (G::isLogger) G::log("MW::invokeIngestHistoryFolder");
    QString dirPath = ingestHistoryFolderActions->text();
    fsTree->select(dirPath);
//    selectionChange();
    // folderSelectionChange();
//    revealInFileBrowser(dirPath);
}

void MW::about()
{
    if (G::isLogger) G::log("MW::about");
    QString qtVersion = QT_VERSION_STR;
    qtVersion.prepend("Qt: ");
    aboutDlg = new AboutDlg(this, version, qtVersion, compileDate);
    aboutDlg->exec();
}

void MW::helpThumbViewStatusBarSymbols()
{
    if (G::isLogger) G::log("MW::helpThumbViewStatusBarSymbols");

    const QSize windowSize(1601, 710);

    // Center the HtmlWindow on top of the main window
    const QRect mwRect = geometry(); // Get the geometry of the main window

    HtmlWindow *w = new HtmlWindow("Winnow - Film Strip and Status Bar Symbols",
                                   ":/Docs/helpfilmstrip.html",
                                   windowSize, mwRect, this);
    openWindows.append(w);
}

void MW::allPreferences()
{
    if (G::isLogger) G::log("MW::allPreferences");
    preferences();
}

void MW::infoViewPreferences()
{
    if (G::isLogger) G::log("MW::infoViewPreferences");
    preferences("MetadataPanelHeader");
}

void MW::cachePreferences()
{
    // if (G::isLogger)
        G::log("MW::cachePreferences");
    preferences("ProductivityHeader");
}

void MW::preferences(QString text)
{
/*

*/
    if (G::isLogger) G::log("MW::preferences");
    if (pref == nullptr) createPreferences();
    if (preferencesDlg == nullptr) {
        // pref = new Preferences(this);
        // preferencesDlg = new PreferencesDlg(nullptr, isSoloPrefDlg, pref, G::css);
        preferencesDlg = new PreferencesDlg(this, isSoloPrefDlg, pref, G::css);
    }
    /* Expand the requested branch every time, not only when the dialog is first
       created. The dialog is reused (NonModal, not deleted on close), so leaving this
       inside the creation guard meant the branch only expanded on the first open. */
    if (text != "") {
        pref->collapseAll();
        pref->expandBranch(text);
    }
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(preferencesDlg->winId(), G::backgroundColor);
    #endif
    preferencesDlg->setWindowFlag(Qt::WindowStaysOnTopHint);
    preferencesDlg->setWindowModality(Qt::NonModal);
    preferencesDlg->show();

    /* modal
    preferencesDlg->exec();
    delete pref;
    delete preferencesDlg;
    //*/

    /* Create a preferences tree as a docking panel:
    propertiesDock = new DockWidget(tr("  Preferencess  "), this);
    propertiesDock->setObjectName("Preferences");
    propertiesDock->setWidget(pref);
    propertiesDock->setFloating(true);
    propertiesDock->setGeometry(2000,600,400,800);
    propertiesDock->setVisible(true);
    propertiesDock->raise();
    return;
    //*/
}

void MW::setShowImageCount()
{
    if (G::isLogger) G::log("MW::setShowImageCount");
    if (!fsTree->isVisible()) {
        G::popup->showPopup("Show image count is only available when the Folders Panel is visible",
              1500);
    }
    bool isShow = showImageCountAction->isChecked();
    fsTree->setShowImageCount(isShow);
    fsTree->resizeColumns();
    fsTree->repaint();
    if (isShow) fsTree->fsModel->fetchMore(fsTree->rootIndex());
}

void MW::setFontSize(int fontPixelSize)
{
/*
    The font size and container sizes are updated for all objects in Winnow.
    For most objects updating the font size in the QStyleSheet is sufficient.
    For items in list and tree views the sizehint needs to be triggered, either
    by refreshing all the values or calling scheduleDelayedItemsLayout().
*/
    if (G::isLogger) G::log("MW::setFontSize");
    G::fontSize = fontPixelSize;
    G::strFontSize = QString::number(fontPixelSize);
    widgetCSS.fontSize = fontPixelSize;
    G::css = widgetCSS.css();
    setStyleSheet(G::css);

    if (G::useInfoView) infoView->refreshLayout();                   // triggers sizehint!
//    infoView->updateInfo(currentRow);                           // triggers sizehint!
    bookmarks->setStyleSheet(G::css);
    fsTree->setStyleSheet(G::css);
    filters->setStyleSheet(G::css);
    infoView->setStyleSheet(G::css);
    tableView->setStyleSheet(G::css);
    statusLabel->setStyleSheet(G::css);
    folderTitleBar->setStyle();
    favTitleBar->setStyle();
    filterTitleBar->setStyle();
    metaTitleBar->setStyle();
    embelTitleBar->setStyle();
    setCacheRunningLightsWidth();
    embelProperties->fontSizeChanged(fontPixelSize);
    pref->fontSizeChanged(fontPixelSize);
    HtmlWindow::refreshOpenWindows();       // re-scale any open help windows
    // re-scale any open Shortcuts help windows (prune ones since closed)
    shortcutsWindows.removeAll(QPointer<QScrollArea>(nullptr));
    for (const QPointer<QScrollArea> &w : shortcutsWindows)
        if (w) styleShortcutsWindow(w);
}

void MW::setBackgroundShade(int shade)
{
    if (G::isLogger) G::log("MW::setBackgroundShade");
    G::backgroundShade = shade;
    G::backgroundColor = QColor(shade,shade,shade);
    int a = shade + 5;
    int b = shade - 15;
    widgetCSS.widgetBackgroundColor = QColor(shade,shade,shade);
    G::css = widgetCSS.css();
    setStyleSheet(G::css);

    if (G::useInfoView) {
        infoView->updateInfo(dm->currentSfRow);                           // triggers sizehint!
        infoView->verticalScrollBar()->setStyleSheet(G::css);          // triggers sizehint!
    }
    bookmarks->setStyleSheet(G::css);
    bookmarks->verticalScrollBar()->setStyleSheet(G::css);
    fsTree->setStyleSheet(G::css);
    fsTree->verticalScrollBar()->setStyleSheet(G::css);
    filters->setStyleSheet(G::css);
    filters->verticalScrollBar()->setStyleSheet(G::css);
    filters->setCategoryBackground(a, b);
    /* The Find panel's scope buttons carry their own theme-derived stylesheet -- the
       app-wide QToolButton rule has no :checked state to show which scope is current. */
    if (findPanel) findPanel->updateStyle();
    if (folderCatalogScopeRow) folderCatalogScopeRow->updateStyle();
    if (favCatalogScopeRow)    favCatalogScopeRow->updateStyle();
//    if (G::useInfoView) infoView->setStyleSheet(G::css);
    imageView->setBackgroundColor(widgetCSS.widgetBackgroundColor);
    thumbView->setStyleSheet(G::css);
    thumbView->horizontalScrollBar()->setStyleSheet(G::css);
    thumbView->verticalScrollBar()->setStyleSheet(G::css);
    gridView->setStyleSheet(G::css);
    tableView->setStyleSheet(G::css);
    gridView->verticalScrollBar()->setStyleSheet(G::css);
    messageView->setStyleSheet(G::css);
    welcome->setStyleSheet(G::css);
    progress->setBackgroundColor(widgetCSS.widgetBackgroundColor);
    folderTitleBar->setStyle();
    favTitleBar->setStyle();
    filterTitleBar->setStyle();
    metaTitleBar->setStyle();
    embelTitleBar->setStyle();
    statusBar()->setStyleSheet(G::css);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

void MW::setInfoFontSize()
{
    if (G::isLogger) G::log("MW::setInfoFontSize");
    /* imageView->infoOverlayFontSize already defined in preferences - just call
       so can redraw  */
    imageView->setShootingInfo(imageView->infoText);
}

void MW::setClassificationBadgeImageDiam(int d)
{
    if (G::isLogger) G::log("MW::setClassificationBadgeImageDiam");
    // this feature is no longer used
    d = 0;
    classificationBadgeInImageDiameter = d;
    imageView->setClassificationBadgeImageDiam(d);
}

void MW::setClassificationBadgeSizeFactor(int d)
{
    if (G::isLogger) G::log("MW::setClassificationBadgeThumbDiam");
    qDebug() << "MW::setClassificationBadgeThumbDiam";
    classificationBadgeSizeFactor = d;
    thumbView->badgeSize = d;
    gridView->badgeSize = d;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
}

void MW::setIconNumberSize(int d)
{
    if (G::isLogger) G::log("MW::setIconNumberSize");
    qDebug() << "MW::setIconNumberSize";
    iconNumberSize = d;
    thumbView->iconNumberSize = d;
    gridView->iconNumberSize = d;
    thumbView->setThumbParameters();
    gridView->setThumbParameters();
}

void MW::setPrefPage(int page)
{
    if (G::isLogger) G::log("MW::setPrefPage");
    lastPrefPage = page;
}

void MW::updateDisplayResolution()
{
    if (G::isLogger) G::log("MW::updateDisplayResolution");
    // return;
    QString monitorScale = QString::number(G::actDevicePixelRatio * 100) + "%";
    QString dimensions = QString::number(G::displayPhysicalHorizontalPixels) + "x"
            + QString::number(G::displayPhysicalVerticalPixels)
            + " @ " + monitorScale;
    QString toolTip = dimensions + " (Monitor is scaled to " + monitorScale + ")";
    if (!G::useInfoView) return;
    QStandardItemModel *k = infoView->ok;
    k->setData(k->index(infoView->MonitorRow, 1, infoView->statusInfoIdx), dimensions);
    k->setData(k->index(infoView->MonitorRow, 1, infoView->statusInfoIdx), toolTip, Qt::ToolTipRole);
}

void MW::setDisplayResolution()
{
/*
    This is triggered by the mainwindow show event at startup, when the operating
    system display scale is changed and when the app window is dragged to another
    monitor. The loupe view always shows native pixel resolution (one image pixel =
    one physical monitor pixel), therefore the zoom has to be factored by the device
    pixel ratio.

    However, on Mac the device pixel ratio is arbitrary, mostly = 2.0 no matter
    which display scaling is selected, so there are two ratios defined here:

    G::actDevicePixelRatio - the actual ratio from actual to vertual pixels
    G::sysDevicePixelRatio - the system reported device pixel ratio
*/
    if (G::isLogger) G::log("MW::setDisplayResolution");

    bool monitorChanged = false;
    bool devicePixelRatioChanged = false;
//    bool monitorChanged = true;
//    bool devicePixelRatioChanged = true;

    // ignore until show event
    if (!isVisible()) {
        return;
    }

    // Screen info
    QPoint loc = centralWidget->window()->geometry().center();
    QScreen *screen = qApp->screenAt(loc);
    if (screen == nullptr) return;
    monitorChanged = screen->name() != prevScreenName;

    //monitorChanged = true;

    /*
    qDebug() << "MW::setDisplayResolution" << "1"
             << "G::isInitializing  =" << G::isInitializing
             << "isVisible()  =" << isVisible()
             << "prevDevicePixelRatio =" << prevDevicePixelRatio
             << "G::actDevicePixelRatio =" << G::actDevicePixelRatio
             << "screen->name() =" << screen->name()
             << "prevScreenName =" << prevScreenName
             << "monitorChanged =" << monitorChanged
             << "devicePixelRatioChanged =" << devicePixelRatioChanged;
    //*/
    prevScreenName = screen->name();

    // START GEMINI
    if (!screen) return;

    // Standard Qt ratio (Logical 2x)
    G::sysDevicePixelRatio = screen->devicePixelRatio();

#ifdef Q_OS_MAC
    // This calculates the 'real' hardware-to-logical ratio
    G::actDevicePixelRatio = macActualDevicePixelRatio(loc, screen);
#else
    G::actDevicePixelRatio = screen->devicePixelRatio();
#endif

    // Update virtual and physical pixel counts based on the 'real' ratio
    G::displayVirtualHorizontalPixels = screen->geometry().width();
    G::displayVirtualVerticalPixels = screen->geometry().height();
    G::displayPhysicalHorizontalPixels = qRound(screen->geometry().width() * G::actDevicePixelRatio);
    G::displayPhysicalVerticalPixels = qRound(screen->geometry().height() * G::actDevicePixelRatio);
    // END GEMINI



    // // Device Pixel Ratios
    // G::sysDevicePixelRatio = screen->devicePixelRatio();
    // #ifdef Q_OS_WIN
    // G::actDevicePixelRatio = screen->devicePixelRatio();
    // #endif
    // #ifdef Q_OS_MAC
    // G::actDevicePixelRatio = macActualDevicePixelRatio(loc, screen);
    // #endif
    devicePixelRatioChanged = !qFuzzyCompare(G::actDevicePixelRatio, prevDevicePixelRatio);
    /*
    qDebug() << "MW::setDisplayResolution" << "2"
             << "G::isInitializing =" << G::isInitializing
             << "isVisible() =" << isVisible()
             << "prevDevicePixelRatio =" << prevDevicePixelRatio
             << "G::actDevicePixelRatio =" << G::actDevicePixelRatio
             << "screen->name() =" << screen->name()
             << "prevScreenName =" << prevScreenName
             << "monitorChanged =" << monitorChanged
             << "devicePixelRatioChanged =" << devicePixelRatioChanged;
                //*/
    prevDevicePixelRatio = G::actDevicePixelRatio;

//    devicePixelRatioChanged = true;
    if (!monitorChanged && !devicePixelRatioChanged) return;

    // Device Pixel Ratio or Monitor change has occurred (default set in initialize())
    G::dpi = screen->logicalDotsPerInch();
    G::ptToPx = G::dpi / 72;
    /*
    qDebug() << "MW::setDisplayResolution"
             << "G::dpi =" << G::dpi
             << "G::ptToPx =" << G::ptToPx
                ; //*/
    G::displayVirtualHorizontalPixels = screen->geometry().width();
    G::displayVirtualVerticalPixels = screen->geometry().height();
    G::displayPhysicalHorizontalPixels = screen->geometry().width() * G::actDevicePixelRatio;
    G::displayPhysicalVerticalPixels = screen->geometry().height() * G::actDevicePixelRatio;

    /*
    double physicalWidth = screen->physicalSize().width();
    double dpmm = G::displayPhysicalHorizontalPixels * 1.0 / physicalWidth ;
    qDebug() << "MW::setDisplayResolution"
             << "G::actDevicePixelRatio =" << G::actDevicePixelRatio
             << "screen->actDevicePixelRatio() =" << screen->devicePixelRatio()
             << "VirtualHorPixels =" << G::displayVirtualHorizontalPixels
             << "PhysicalHorPixels =" << G::displayPhysicalHorizontalPixels
             //<< "screen->physicalSize() =" << screen->physicalSize()
             << "px per mm =" << dpmm
                ;
    //*/

    if (devicePixelRatioChanged) {
    //if (devicePixelRatioChanged && !G::isInitializing) {
        // refresh loupe / compare views to new scale
        if (G::mode == "Loupe") {
            // reload to force complete refresh
            imageView->loadImage(dm->currentFilePath, true, "DevicePixelRatioChange");
        }
        if (G::mode == "Compare") compareImages->zoomTo(imageView->zoom/* / G::actDevicePixelRatio*/);
    }

    // if monitor has not changed then only scale change, return
    if (!monitorChanged) return;

    // monitor has changed

    // resize if winnow window too big for new screen
    int w = this->geometry().width();
    int h = this->geometry().height();
    double fitW = w * 1.0 / screen->geometry().width();
    double fitH = h * 1.0 / screen->geometry().height();
    /*
    qDebug() << "MW::setDisplayResolution" << "MONITOR HAS CHANGED"
             << "w =" << w
             << "ScreenW =" << screen->geometry().width()
             << "fitW =" << fitW
             << "h =" << h
             << "ScreenH=" << screen->geometry().height()
             << "fitH =" << fitH
                ;
    //*/
    // does winnow fit in new screen?
    if (fitW > 1.0 || fitH > 1.0) {
        // does not fit, does width or height require the larger adjustment?
        if (fitW < fitH) {
            // width is larger adjustment
            w = static_cast<int>(w * 0.75 / fitW);
            h = static_cast<int>(h * 0.75 / fitW);
        }
        else {
            // height is larger adjustment
            w = static_cast<int>(w * 0.75 / fitH);
            h = static_cast<int>(h * 0.75 / fitH);
        }
        //qDebug() << "MW::setDisplayResolution" << "RESIZE TO:" << w << h;
        resize(w, h);
    }

    // color manage for new monitor
    getDisplayProfile();

    /*
    qDebug() << "MW::setDisplayResolution DONE"
             << "screen->name() =" << screen->name()
             << "G::actDevicePixelRatio =" << G::actDevicePixelRatio
             << "loc =" << loc
             << "G::dpi =" << G::dpi
             << "G::ptToPx =" << G::ptToPx
             << "G::displayVirtualHorizontalPixels =" << G::displayVirtualHorizontalPixels
             << "G::displayVirtualVerticalPixels =" << G::displayVirtualVerticalPixels
                ;
    //*/

    screen = nullptr;
}

void MW::getDisplayProfile()
{
/*
    This is required for color management.  It is called after the show event when the
    progam is opening and when the main window is moved to a different screen.
*/
    if (G::isLogger) G::log("MW::getDisplayProfile");

    #ifdef Q_OS_WIN
    if (G::winScreenHash.contains(screen()->name()))
        G::winOutProfilePath = "C:/Windows/System32/spool/drivers/color/" +
            G::winScreenHash[screen()->name()].profile;
    ICC::setOutProfile();
    #endif
    #ifdef Q_OS_MAC

    G::winOutProfilePath = Mac::getDisplayProfileURL();
    ICC::setOutProfile();
    #endif
}

double MW::macActualDevicePixelRatio(QPoint loc, QScreen *screen)
{
/*
    Apple makes it hard to get the display native pixel resolution, which is necessary
    to determine the true device pixel ratio to ensure images at 100% are 1:1 image and
    display pixels.
*/
    if (G::isLogger) G::log("MW::macActualDevicePixelRatio");
    #ifdef Q_OS_MAC
    // get displayID for monitor at point
    const int maxDisplays = 64;                     // 64 should be enough for any system
    CGDisplayCount displayCount;                    // Total number of display IDs
    CGDirectDisplayID displayIDs[maxDisplays];      // Array of display IDs
    CGPoint point = loc.toCGPoint();
    CGGetDisplaysWithPoint(point, maxDisplays, displayIDs, &displayCount);
    auto displayID = displayIDs[0];
    if (displayCount != 1) displayID = CGMainDisplayID();

    // get list of all display modes for the monitor
    auto modes = CGDisplayCopyAllDisplayModes(displayID, nullptr);
    auto count = CFArrayGetCount(modes);
    CGDisplayModeRef mode;

    // start with virtual pixel width from QScreen (does not know actual width in pixels)
    int screenW = screen->geometry().width();
    int screenH = screen->geometry().height();

    // the native resolution is the largest display mode
    for (long c = count; c--;) {
        mode = static_cast<CGDisplayModeRef>(const_cast<void *>(CFArrayGetValueAtIndex(modes, c)));
        int w = static_cast<int>(CGDisplayModeGetWidth(mode));
        int h = static_cast<int>(CGDisplayModeGetHeight(mode));
        if (w > screenW) screenW = w;
        if (h > screenH) screenH = h;
    }

    // The device pixel ratio
    return screenW * 1.0 / screen->geometry().width();
    #endif
    // dummy return to satisfy compiler on PC
    return 0;

  /*  MacOS Screen information
#if defined(Q_OS_MAC)
       int screenWidth = CGDisplayPixelsWide(CGMainDisplayID());
       qDebug() << "screenWidth" << screenWidth << QPaintDevice::actDevicePixelRatio();
        float bSF = QtMac::macBackingScaleFactor();
        qDebug() << G::t.restart() << "\t" << "QtMac::BackingScaleFactor()" << bSF;
#endif

        qDebug() << G::t.restart() << "\t" << "QGuiApplication::primaryScreen()->actDevicePixelRatio()"
                << QGuiApplication::primaryScreen()->actDevicePixelRatio();
        qreal dpr = QGuiApplication::primaryScreen()->actDevicePixelRatio();

        QRect rect = QGuiApplication::primaryScreen()->geometry();
        qreal screenMax = qMax(rect.width(), rect.height());

        G::actDevicePixelRatio = 1;
        G::actDevicePixelRatio = 2880 / screenMax;

        int realScreenMax = QGuiApplication::primaryScreen()->physicalSize().width();
        qreal logicalDpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();
        qreal physicalDpi = QGuiApplication::primaryScreen()->physicalDotsPerInch();
        QSizeF physicalSize = QGuiApplication::primaryScreen()->physicalSize();

        qDebug() << G::t.restart() << "\t" << "\nQGuiApplication::primaryScreen()->geometry()" << QGuiApplication::primaryScreen()->geometry().width()
                 << "\nQGuiApplication::primaryScreen()->physicalSize()" << QGuiApplication::primaryScreen()->physicalSize().width()
                 << "\ndevicePixelRatio" << dpr
                 << "\nlogicalDpi" << QGuiApplication::primaryScreen()->logicalDotsPerInch()
                 << "\nphysicalDpi" << QGuiApplication::primaryScreen()->physicalDotsPerInch()
                 << "\nphysicalSize" << QGuiApplication::primaryScreen()->physicalSize()
                 << "\nQApplication::desktop()->availableGeometry(this)"<< QApplication::desktop()->availableGeometry(this)
                 << "\n";
//                 */
}

void MW::escapeFullScreen()
{
    if (G::isLogger) G::log("MW::escapeFullScreen");
    fullScreenAction->setChecked(false);
    toggleFullScreen();
}

void MW::toggleFullScreen()
{
/*
    Toggles between the FullScreen and NormalScreen states.  When in fullscreen, all docks
    except those defined in settings are hidden. A snapshot of the current workspace state
    is taken to be re-established when showNormal.
*/
    if (G::isLogger) G::log("MW::toggleFullScreen");

    static bool wasMaximized = false;

    // show full screen
    if (fullScreenAction->isChecked())
    {
        // reportWorkspace(ws);
        snapshotWorkspace(ws);
        wasMaximized = isMaximized();
        showFullScreen();
        #ifdef Q_OS_WIN
        menuBar()->setVisible(false);
        #endif
        folderDockVisibleAction->setChecked(fullScreenDocks.isFolders);
        folderDock->setVisible(fullScreenDocks.isFolders);
        favDockVisibleAction->setChecked(fullScreenDocks.isFavs);
        favDock->setVisible(fullScreenDocks.isFavs);
        filterDockVisibleAction->setChecked(fullScreenDocks.isFilters);
        filterDock->setVisible(fullScreenDocks.isFilters);
        catalogDockVisibleAction->setChecked(fullScreenDocks.isCatalog);
        if (catalogDock) catalogDock->setVisible(fullScreenDocks.isCatalog);
        if (G::useInfoView) {
            metadataDockVisibleAction->setChecked(fullScreenDocks.isMetadata);
            metadataDock->setVisible(fullScreenDocks.isMetadata);
        }
        /* History and Presets are tabbed with Develop, so show them before Develop and
           then raise Develop, otherwise one of them becomes the front tab. */
        if (presetsDock && presetsDockVisibleAction) {
            presetsDockVisibleAction->setChecked(fullScreenDocks.isPresets);
            presetsDock->setVisible(fullScreenDocks.isPresets);
        }
        if (historyDock && historyDockVisibleAction) {
            historyDockVisibleAction->setChecked(fullScreenDocks.isHistory);
            historyDock->setVisible(fullScreenDocks.isHistory);
        }
        developDockVisibleAction->setChecked(fullScreenDocks.isDevelop);
        developDock->setVisible(fullScreenDocks.isDevelop);
        if (fullScreenDocks.isDevelop) developDock->raise();
        embelDockVisibleAction->setChecked(fullScreenDocks.isEmbellish);
        embelDock->setVisible(fullScreenDocks.isEmbellish);
        thumbDockVisibleAction->setChecked(fullScreenDocks.isThumbs);
        thumbDock->setVisible(fullScreenDocks.isThumbs);
        statusBarVisibleAction->setChecked(fullScreenDocks.isStatusBar);
        setStatusBarVisibility();
    }
    // show normal screen
    else
    {
        if (wasMaximized) showMaximized();
        else showNormal();
        invokeWorkspace(ws);
        #ifdef Q_OS_WIN
        menuBar()->setVisible(true);
        #endif
    }
}

void MW::selectAllThumbs()
{
    if (G::isLogger) G::log("MW::selectAllThumbs");
    sel->all();
}

void MW::toggleZoomDlg()
{
/*
    This function provides a dialog to change scale and to set the toggleZoom value,
    which is the amount of zoom to toggle with zoomToFit scale. The user can zoom to 100%
    (for example) with a click of the mouse, and with another click, return to the
    zoomToFit scale. Here the user can set the amount of zoom when toggled.

    The dialog is non-modal and floats at the bottom of the central widget. Adjustments
    are made when the main window resizes or is moved or when a different workspace is
    invoked. This only applies when a mode that can be zoomed is visible, so table and
    grid are not applicable.

    When the zoom dialog is created, zoomDlg->show makes the dialog visible and also
    gives it the focus, but the design is for the zoomDlg to only have focus when a
    mouseover occurs. The focus is set to MW when the zoomDlg leaveEvent fires and after
    zoomDlg->show.

    NOTE: the dialog window flag is Qt::WindowStaysOnTopHint. When The app focus changes
    to another app, the zoom dialog is hidden so it does not float on top of other apps
    (this is triggered in the slot MW::appStateChange). The windows flag
    Qt::WindowStaysOnTopHint is not changed as this automatically hides the window - it
    is easier to just hide it. The prior state of ZoomDlg is held in isZoomDlgVisible.

    When the zoom is changed this is signalled to ImageView and CompareImages, which in
    turn make the scale changes to the image. Conversely, changes in scale originating
    from toggleZoom mouse clicking in ImageView or CompareView, or scale changes
    originating from the zoomInAction and zoomOutAction are signaled and updated here.
    This can cause a circular message, which is prevented by variance checking. If the
    zoom factor has not changed more than can be accounted for in int/qreal conversions
    then the signal is not propagated.

*/
    if (G::isLogger) G::log("MW::toggleZoomDlg");
    // toggle zoomDlg (if open then close)
    if (isZoomDlgVisible) {
        if (zoomDlg->isVisible()) {
            isZoomDlgVisible = false;
            zoomDlg->close();
            return;
        }
    }

    // only makes sense to zoom when in loupe or compare view
    if (G::mode == "Table" || G::mode == "Grid") {
        G::popup->showPopup("The zoom dialog is only available in loupe view", 2000);
        return;
    }

    // the dialog positions itself relative to the main window and central widget.
    QRect a = this->geometry();
    QRect c = centralWidget->geometry();
    zoomDlg = new ZoomDlg(this, imageView->zoom, a, c);
    isZoomDlgVisible = true;

    // update the imageView and compareView classes if there is a zoom change
    connect(zoomDlg, &ZoomDlg::zoom, imageView, &ImageView::zoomTo);
    connect(zoomDlg, &ZoomDlg::zoom, compareImages, &CompareImages::zoomTo);

    // update the imageView and compareView classes if there is a toggleZoomValue change
    connect(zoomDlg, SIGNAL(updateToggleZoom(qreal)), imageView, SLOT(updateToggleZoom(qreal)));
    connect(zoomDlg, SIGNAL(updateToggleZoom(qreal)), compareImages, SLOT(updateToggleZoom(qreal)));

    // if zoom dialog signals to close (Return or Z shortcut) then update using this function
    connect(zoomDlg, &ZoomDlg::closeZoom, this, &MW::toggleZoomDlg);

    // if zoom change in parent send it to the zoom dialog
    connect(imageView, &ImageView::zoomChange, zoomDlg, &ZoomDlg::zoomChange);
    connect(compareImages, &CompareImages::zoomChange, zoomDlg, &ZoomDlg::zoomChange);

    // if main window resized then re-position zoom dialog
    connect(this, SIGNAL(resizeMW(QRect,QRect)), zoomDlg, SLOT(positionWindow(QRect,QRect)));

    // if main window loses focus, hide ZoomDlg because stayOnTop shows over other apps
    QGuiApplication *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    connect(app, &QGuiApplication::applicationStateChanged, this, &MW::appStateChange);

    // if view change other than loupe then close zoomDlg
    // connect(this, &MW::closeZoomDlg, zoomDlg, &ZoomDlg::closeZoomDlg);

    // reset focus to main window if the mouse cursor leaves the zoom dialog
    connect(zoomDlg, &ZoomDlg::leaveZoom, this, &MW::resetFocus);

    // use show() so dialog will be non-modal
    zoomDlg->show();

    // reset the focus to the main window
    resetFocus();
}

void MW::zoomIn()
{
    if (G::isLogger) G::log("MW::zoomIn");
    if (asLoupeAction) imageView->zoomIn();
    if (asCompareAction) compareImages->zoomIn();
    // if thumbView visible and zoom change in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, "MW::zoomIn", /*forceUpdate=*/true);
    }
}

void MW::zoomOut()
{
    if (G::isLogger) G::log("MW::zoomOut");
    if (asLoupeAction) imageView->zoomOut();
    if (asCompareAction) compareImages->zoomOut();
    // if thumbView visible and zoom change in imageView then may need to redo the zoomFrame
    QModelIndex idx = thumbView->indexAt(thumbView->mapFromGlobal(QCursor::pos()));
    if (idx.isValid()) {
        thumbView->zoomCursor(idx, "MW::zoomOut", /*forceUpdate=*/true);
    }
}

void MW::zoomToFit()
{
    if (G::isLogger) G::log("MW::zoomToFit");
    if (asLoupeAction) imageView->zoomToFit();
    if (asCompareAction) compareImages->zoomToFit();
}

void MW::zoomToggle()
{
    if (G::isLogger) G::log("MW::zoomToggle");
    // ignore if video
    bool isVideo = dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();
    if (isVideo) return;
    if (asLoupeAction) imageView->zoomToggle();
    if (asCompareAction) compareImages->zoomToggle();
}

void MW::rotateLeft()
{
    if (G::isLogger) G::log("MW::rotateLeft");
    setRotation(270);
}

void MW::rotateRight()
{
    if (G::isLogger) G::log("MW::rotateRight");
    setRotation(90);
}

void MW::setRotation(int degrees)
{
/*
    Rotate all selected thumbnails by the specified degrees. 90 = rotate right and
    270 = rotate left.

    Rotate all selected cached full size images by the specified degrees.

    When images are added to the image cache (ImageCache) they are rotated by the
    metadata orientation + the edited rotation.  Newly cached images are always
    rotation up-to-date.

    When there is a rotation action (rotateLeft or rotateRight) the current
    rotation amount (in degrees) is updated in the datamodel.

    If G::modifySourceFiles == true the rotation is updated in the image file EXIF using
    exifTool in separate threads.  Otherwise it is written to an XMP sidecar so the
    rotation survives a reload.
*/
    if (G::isLogger) G::log("MW::setRotation");
    qDebug() << "MW::setRotation degrees =" << degrees;

    // rotate current loupe view image unless it is a video
    bool isVideo = dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();
    if (!isVideo) {
        imageView->rotateImage(degrees);
    }

    // iterate selection
    QModelIndexList selection = dm->selectionModel->selectedRows();
    for (int i = 0; i < selection.count(); ++i) {
        // update rotation amount in the data model
        int sfRow = selection.at(i).row();
        bool isVideo = dm->sf->index(sfRow, G::VideoColumn).data().toBool();
        if (isVideo) continue;
        QModelIndex orientationIdx = dm->sf->index(sfRow, G::OrientationColumn);
        int orientation = orientationIdx.data(Qt::EditRole).toInt();
        int prevRotation = 0;
        switch (orientation) {
        case 6: prevRotation = 90;  break;
        case 3: prevRotation = 180; break;
        case 8: prevRotation = 270; break;
        }

        int newRotation = prevRotation + degrees;
        if (newRotation >= 360) newRotation = newRotation - 360;
        int newOrientation = 0;
        switch (newRotation) {
        case 90:  newOrientation = 6; break;
        case 180: newOrientation = 3; break;
        case 270: newOrientation = 8; break;
        }

        emit setValSf(sfRow, G::OrientationColumn, newOrientation, dm->instance,
                        "MW::setRotation", Qt::EditRole);

        // rotate thumbnail(s)
        QTransform trans;
        trans.rotate(degrees);
        /*  Through data()/setData(), not itemFromIndex()->icon(): the thumbnail
            lives in the path-keyed icon store now and the item's own icon is
            always null. (The old code also leaked a QStandardItem it allocated
            and then immediately overwrote.) */
        QModelIndex thumbIdx = dm->sf->index(sfRow, G::PathColumn);
        QModelIndex dmIdx = dm->sf->mapToSource(thumbIdx);
        QPixmap pm = qvariant_cast<QIcon>(dmIdx.data(Qt::DecorationRole))
                         .pixmap(G::maxIconSize, G::maxIconSize);
        if (!pm.isNull()) {
            pm = pm.transformed(QTransform().rotate(degrees));
            dm->setData(dmIdx, QVariant(QIcon(pm)), Qt::DecorationRole);
        }

        // rotate selected cached full size images
        QString fPath = thumbIdx.data(G::PathRole).toString();
        QImage image;
        if (icd->contains(fPath)) {
            image = image.transformed(QTransform().rotate(degrees), Qt::SmoothTransformation);
            icd->insert(fPath, image);
        }

        // update exif in image
        QString orient;
        switch (newRotation) {
            case 0:   orient = "1"; break;
            case 90:  orient = "6"; break;
            case 180: orient = "3"; break;
            case 270: orient = "8"; break;
        }
        if (orient.length()) {
            // note that Metadata::writeOrientation must be static!
            // writes to source EXIF if G::modifySourceFiles, otherwise to xmp sidecar
            QtConcurrent::run(&Metadata::writeOrientation, fPath, orient);
        }
    }
}

namespace {
/* Compose one Develop preview from a source WorkingImage: run Develop + OutputTransform, apply
   the EXIF rotation, and (proxy only) upscale to the displayed full-res dimensions so the loupe
   pixmap swap keeps zoom/fit/scene untouched (ImageView::setDevelopPreview's contract). Pure --
   reads only its arguments (the source WorkingImage is const), so it is safe to call on a
   background thread for the full-res settle render. degrees/fullW/fullH are computed by the
   caller on the GUI thread (orientation needs the sort/filter model). */
/* upscaleToFull=false leaves a proxy render at PROXY resolution: the loupe presents it
   at the full size itself (ScaledPixmapItem), so the ~w*h*4 byte upscale a drag tick used
   to pay is skipped. The caller then tells ImageView what size the result stands for. */
QImage developComposite(const WorkingImage &src, const EditParams &edit, int degrees,
                        bool fullRes, int fullW, int fullH,
                        WorkingImageCache::RenderTimings *timings = nullptr,
                        WorkingImageCache::OutDepth depth =
                            WorkingImageCache::OutDepth::Eight,
                        WorkingImageCache::Space space =
                            WorkingImageCache::Space::sRGB,
                        bool upscaleToFull = true,
                        WorkingImage *scratch = nullptr)
{
    QImage out;
    if (!WorkingImageCache::render(src, edit, out, timings, depth, space, scratch))
        return QImage();
    QElapsedTimer probe;
    if (timings) probe.start();
    if (degrees != 0) {
        QTransform trans;
        trans.rotate(degrees);
        /* Proxy: fast (still small); full-res: smooth for the final image. */
        out = out.transformed(trans, fullRes ? Qt::SmoothTransformation : Qt::FastTransformation);
    }
    if (upscaleToFull && !fullRes && (out.width() != fullW || out.height() != fullH))
        out = out.scaled(fullW, fullH, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    if (timings) timings->orientScaleMs = probe.elapsed();
    return out;
}

/* A mask component pre-parsed once so the per-pixel loop is branch-light. Geometry is in
   output-normalized coords (0..1 of the oriented image), matching what the ImageView overlay edits.
   Radial fields are precomputed in OUTPUT-PIXEL space (Wo,Ho) so the ellipse keeps its aspect and
   rotation; eval converts the normalized pixel to output pixels via Wo,Ho. */
struct MaskComp {
    int    tool = 0;                // 0 Linear, 1 Radial
    int    op = 0;                  // 0 Add (union), 1 Subtract (removes)
    bool   inverted = false;
    bool   hardStep = true;         // feather == 0
    bool   valid = false;
    double feat = 0.0;              // feather fraction 0..1
    /* Linear */
    double p1x = 0, p1y = 0, dx = 0, dy = 0, invLen2 = 0, invSigma = 0;
    double tShift = 0.0;            // Edge, as a shift of the 0.5 crossing in t
    /* Radial (output-pixel space) */
    double cpx = 0, cpy = 0, iax = 0, iay = 0, cosA = 1, sinA = 0;
    /* Feather profile (MaskFalloff). Radial reads the table; Linear uses the CDF and
       only needs invSigma. Built once per component, never in the per-pixel loop. */
    MaskFalloff::Lut falloff;
};

/* edgePx is the Edge slider as a signed radius in OUTPUT pixels. Both parametric tools
   fold it into their geometry rather than morphing a buffer: for these two the result is
   analytic and costs nothing. */
MaskComp parseMaskComp(const MaskComponent &m, double Wo, double Ho, double edgePx)
{
    MaskComp g;
    g.tool = m.tool;
    g.op = m.op;
    g.inverted = m.inverted;
    g.feat = qBound(0.0, double(m.feather)/100.0, 1.0);
    g.hardStep = (g.feat <= 0.0);
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(m.paramsJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return g;
    const QJsonObject o = doc.object();

    if (m.tool == 0) {              // Linear
        if (!o.contains("x1") || !o.contains("y1") || !o.contains("x2") || !o.contains("y2")) return g;
        g.p1x = o["x1"].toDouble(); g.p1y = o["y1"].toDouble();
        const double p2x = o["x2"].toDouble(), p2y = o["y2"].toDouble();
        g.dx = p2x - g.p1x; g.dy = p2y - g.p1y;
        const double len2 = g.dx*g.dx + g.dy*g.dy;
        if (len2 <= 1e-12) return g;
        g.invLen2 = 1.0 / len2;
        /* Edge: move the 0.5 crossing by edgePx along the gradient's normal. t is
           normalized along p1->p2 in OUTPUT-NORMALIZED coords, so one pixel of x moves t
           by dx/(len2*Wo) and one pixel of y by dy/(len2*Ho); the magnitude of that
           gradient converts a pixel distance into a t distance. This is EXACT, not an
           approximation -- dilating a ramp that is monotone along its normal by a
           disc is precisely a translation, so the whole feather profile rides along,
           which is what the buffer morphology does for every other tool. */
        if (edgePx != 0.0 && Wo > 0.0 && Ho > 0.0) {
            const double gx = g.dx / Wo, gy = g.dy / Ho;
            g.tShift = edgePx * std::sqrt(gx * gx + gy * gy) * g.invLen2;
        }
        const double sigma = MaskFalloff::gradientSigma(m.feather);
        g.hardStep = (sigma <= 0.0);
        g.invSigma = g.hardStep ? 0.0 : 1.0 / sigma;
        g.valid = true;
    }
    else if (m.tool == 1) {         // Radial
        if (!o.contains("cx") || !o.contains("cy") || !o.contains("rx") || !o.contains("ry")) return g;
        const double cx = o["cx"].toDouble(), cy = o["cy"].toDouble();
        const double rx = o["rx"].toDouble(), ry = o["ry"].toDouble();
        double ax = rx * Wo, ay = ry * Ho;              // semi-axes in output pixels
        if (ax <= 1e-6 || ay <= 1e-6) return g;         // authored-degenerate: no ellipse
        /* Edge grows/shrinks the ellipse by edgePx. Exact for a circle and the standard
           approximation for an ellipse (a constant normal offset of an ellipse is not an
           ellipse; the error is zero at all four vertices and grows with eccentricity).
           The feather rides the radius here rather than translating, because for a radial
           the feather has always BEEN a fraction of the radius -- Edge therefore behaves
           exactly like dragging the radius handle, which is the least surprising thing it
           can do on this tool. */
        ax += edgePx;
        ay += edgePx;
        /* A large negative Edge can push the ellipse through zero. Clamp rather than
           invalidate: the caller DROPS an invalid component, and dropping is not the same
           as an empty one -- under Intersect an empty component must zero the mask
           and under Subtract it must be a no-op. Authoring rx=0 is unreachable from the
           UI, so this path was dead until Edge made shrinking a radial a normal drag. */
        ax = qMax(ax, 1e-3);
        ay = qMax(ay, 1e-3);
        const double ang = o["angle"].toDouble() * 0.017453292519943295;   // deg -> rad
        g.cpx = cx * Wo; g.cpy = cy * Ho;
        g.iax = 1.0 / ax; g.iay = 1.0 / ay;
        g.cosA = std::cos(ang); g.sinA = std::sin(ang);
        if (!g.hardStep) g.falloff.build(m.feather);
        g.valid = true;
    }
    return g;
}

inline float evalMaskComp(const MaskComp &g, double onx, double ony, double Wo, double Ho)
{
    double v;
    if (g.tool == 1) {              // Radial: distance in the ellipse's local frame (1 = boundary)
        const double ddx = onx*Wo - g.cpx, ddy = ony*Ho - g.cpy;
        const double rdx = ddx*g.cosA + ddy*g.sinA;     // rotate by -angle
        const double rdy = -ddx*g.sinA + ddy*g.cosA;
        const double ex = rdx*g.iax, ey = rdy*g.iay;
        const double d = std::sqrt(ex*ex + ey*ey);
        /* d is already the MaskFalloff t: 0 at the centre, 1 at the ellipse. The profile
           runs past 1 (a Gaussian tail, as Lightroom's does), so a mask reaches slightly
           beyond its drawn outline -- see MaskFalloff. */
        if (g.hardStep) v = (d <= 1.0) ? 1.0 : 0.0;     // hard edge at the boundary
        else            v = g.falloff.at(d);
    }
    else {                          // Linear: projection along p1->p2
        const double t = ((onx - g.p1x)*g.dx + (ony - g.p1y)*g.dy) * g.invLen2;
        v = g.hardStep ? (t >= 0.5 - g.tShift ? 1.0 : 0.0)
                       : MaskFalloff::cdf((t - 0.5 + g.tShift) * g.invSigma);
    }
    const float r = float(v);       // both paths already produced the final 0..1 mask value
    return g.inverted ? 1.0f - r : r;
}

/* Where a mask build's time actually goes. Split out because the top-level `mask` number
   could not distinguish brush rasterisation from the per-pixel component fold, and
   reading the code guessed wrong twice: the dab counters exonerated the dabs (4 Mpx of
   dab work against a 680 ms build), so the remaining candidates had to be measured,
   not reasoned about. Diagnostic only -- passed nullptr unless G::isReportDevelopTime. */
struct MaskBuildStats {
    qint64 rasterMs = 0;    // BrushStamp::rasterize (replays every stroke's dabs)
    qint64 setupMs  = 0;    // component parse + reference lookups, minus rasterMs
    qint64 foldMs   = 0;    // the per-pixel fold over the scope's components
    qint64 morphMs  = 0;    // MaskEdge grow/shrink passes (submask + mask level)
    qint64 haloMs   = 0;    // MaskHalo refine/damp pass over the folded mask
    int    strokes  = 0;    // brush strokes replayed
    int    comps    = 0;    // components folded
    int    morphs   = 0;    // components (plus the scope) that needed a morph
    int    halos    = 0;    // scopes whose folded mask needed a halo pass
};

/* Cache of rasterized brush masks (work-space coverage, before component invert). A render replays
   every dab, so caching avoids re-rasterizing on each non-brush slider tick. Keyed by the brush
   paramsJson + target dims + degrees. Accessed from the GUI thread (proxy) AND the full-res worker
   thread, so it is mutex-guarded.

   LRU WITH A BYTE BUDGET, not a count cap. This was `if (size() > 8) clear()`, fine for
   a scope with a couple of submasks and USELESS past eight: a mask with ~19 brush
   submasks overflowed and wiped the whole cache on every build, so nothing was ever
   reused and every component was re-rasterized (and re-allocated, and re-JSON-parsed)
   on every drag tick. Measured at raster 275 ms + setup 308 ms per tick on a 2 MP proxy
   with 21 components -- the dominant cost of a brush drag, and invisible until the
   [DevTime] mask field was split (see notes/Documentation.txt "MASK-DRAG LATENCY").

   The budget is what makes evicting safe: entries are w*h*4 bytes (~8 MB at a 2 MP
   proxy), so a count cap either thrashes on a busy mask or hoards on a big one. Evict
   least-recently-USED, so the components the user is not touching -- most of them --
   survive while the one being edited churns through a new key every tick. */
QMutex g_brushCacheMutex;
QHash<QString, std::shared_ptr<const std::vector<float>>> g_brushCache;
QList<QString> g_brushCacheLru;          // least-recently-used at the front
qint64 g_brushCacheBytes = 0;
constexpr qint64 kBrushCacheBudget = 256LL * 1024 * 1024;

/* Both helpers assume g_brushCacheMutex is held. */
void brushCacheTouch(const QString &key)
{
    g_brushCacheLru.removeOne(key);
    g_brushCacheLru.append(key);
}

void brushCacheInsert(const QString &key,
                      const std::shared_ptr<const std::vector<float>> &buf)
{
    auto it = g_brushCache.find(key);
    if (it != g_brushCache.end()) {
        g_brushCacheBytes -= qint64(it.value()->size()) * qint64(sizeof(float));
        g_brushCacheLru.removeOne(key);
    }
    g_brushCache.insert(key, buf);
    g_brushCacheLru.append(key);
    g_brushCacheBytes += qint64(buf->size()) * qint64(sizeof(float));
    while (g_brushCacheBytes > kBrushCacheBudget && g_brushCacheLru.size() > 1) {
        const QString victim = g_brushCacheLru.takeFirst();
        auto v = g_brushCache.find(victim);
        if (v == g_brushCache.end()) continue;
        g_brushCacheBytes -= qint64(v.value()->size()) * qint64(sizeof(float));
        g_brushCache.erase(v);
    }
}

/* inputsReady (when given) is cleared -- never set -- if an auto-mask stroke's
   confinement input was missing, so a CALLER caching a composite built from this buffer
   knows not to. Same condition the local autoInputsReady guard uses to skip its own
   caching. */
std::shared_ptr<const std::vector<float>>
brushRasterCached(const QString &paramsJson, int w, int h, int degrees,
                  const QString &fPath, bool *inputsReady = nullptr,
                  MaskBuildStats *stats = nullptr)
{
    /* Only the (small) proxy buffer is worth caching: it is re-rasterized on every slider tick of a
       drag. The full-res buffer is huge (~w*h*4 bytes) and built once per settle, so we skip caching
       it rather than risk hundreds of MB per entry. */
    const bool cacheable = (size_t(w) * h) <= 4'000'000;
    const auto guide = BrushStamp::getGuide(fPath);   // guide the preview used
    /* The GUIDE IS PART OF THE KEY (via its dimensions). ensureAutoGuide re-builds it at a
       finer resolution when the brush gets small, and an auto-masked stroke rasterized
       against the old, coarser guide is a different buffer -- without this the stale one
       is served for the rest of the session. */
    const QString key = fPath + "|" + paramsJson + "|" + QString::number(w) + "x" + QString::number(h)
                      + "@" + QString::number(degrees)
                      + "|g" + QString::number(guide ? guide->w : 0);
    if (cacheable) {
        QMutexLocker lk(&g_brushCacheMutex);
        auto it = g_brushCache.find(key);
        if (it != g_brushCache.end()) { brushCacheTouch(key); return it.value(); }
    }
    auto buf = std::make_shared<std::vector<float>>(size_t(w) * h, 0.0f);
    std::vector<float> scratch;
    const QJsonArray strokes = QJsonDocument::fromJson(paramsJson.toUtf8())
                                   .object().value("strokes").toArray();

    /* An auto-mask stroke rasterized before the guide is registered paints UNCONFINED
       (full brush). Detect that so we don't cache the poisoned buffer: caching a guideless
       result would replay the full brush on every later slider tick (the proxy is cached)
       while the settle render -- uncached -- rebuilds it confined once the guide lands,
       so the preview flashes broad then snaps to the auto-masked shape. */
    bool autoInputsReady = true;
    for (const QJsonValue &sv : strokes) {
        const QJsonObject so = sv.toObject();
        if (!so.value("autoMask").toBool(false)) continue;
        const QJsonArray pts = so.value("pts").toArray();
        if (pts.size() < 2) continue;
        if (!guide || !guide->valid()) autoInputsReady = false;
    }

    if (inputsReady && !autoInputsReady) *inputsReady = false;

    QElapsedTimer rasterProbe;
    if (stats) { rasterProbe.start(); stats->strokes += strokes.size(); }
    BrushStamp::rasterize(strokes, buf->data(), scratch, w, h, degrees, guide.get());
    if (stats) stats->rasterMs += rasterProbe.elapsed();
    if (cacheable && autoInputsReady) {
        QMutexLocker lk(&g_brushCacheMutex);
        brushCacheInsert(key, buf);
    }
    return buf;
}

/* One component to combine: a parametric eval (Linear/Radial), a pre-rasterized Brush coverage
   buffer (work-space, indexed by pixel), or a content-range eval (Luminance/Color Range) that
   samples the display-referred RangeRef at the mapped output coords. */
struct CompDesc {
    bool isBrush = false;
    bool isRange = false;
    bool isSubject = false;         // Subject OR Background (Background = subjectBaseInvert)
    bool subjectBaseInvert = false; // Background: select 1 - subject saliency
    bool isSky = false;
    bool isDepth = false;           // Depth Range: band [rlo,rhi] over the depth field
    bool isObject = false;          // Object Mask (SAM 2): per-brush ObjectRef (not shared by path)
    std::shared_ptr<const ObjectMask::ObjectRef> objRef;  // this component's decoded coverage
    MaskComp param;                                       // parametric
    std::shared_ptr<const std::vector<float>> brush;      // brush coverage (raw, pre-invert)
    int  op = 0;
    bool inverted = false;
    /* Range (content) component. */
    int    rangeTool = 0;                                 // ColorRange / LuminanceRange
    double rlo = 0, rhi = 1, hueLo = 20, hueHi = 20, satLo = 0.25, satHi = 0.25, feather = 0;
    bool   rangeValid = false;
    std::vector<RangeMask::ColorSample> samples;          // colour samples (opponent space)
    /* Subject (AI saliency) component -- coverage is the shared SubjectRef; feather/inverted above. */

    /* Edge slider, as a signed radius in THIS BUFFER's pixels (pixelScale already
       applied). Non-zero means this component cannot be evaluated inline in the fold --
       growing or shrinking a boundary is a neighbourhood operation -- so it is
       materialized to a buffer, morphed by MaskEdge, and folded from there. The two
       analytic tools never set this: Linear and Radial carry their edge in their geometry
       instead (see parseMaskComp), which costs no buffer at all. Below
       MaskEdge::kMinRadius it is left at 0, so a sub-threshold nudge stays on the cheap
       path rather than paying a materialize for a no-op. */
    double edgePx = 0.0;
};

/*
    One component's coverage at one pixel. This is the whole per-tool switch, in ONE
    place, so the inline fold and the Edge materializer below cannot drift apart. `k` is
    the buffer index (a brush raster is indexed directly, not sampled); onx/ony are
    output-normalized coords. Every branch applies the component's own `inverted` --
    which is what puts invert BEFORE the Edge morphology, so growing an inverted mask
    shrinks the original, as the overlay shows it.
*/
inline float evalCompAt(const CompDesc &d, double onx, double ony, size_t k,
                        const RangeMask::RangeRef *refp,
                        const SubjectMask::SubjectRef *subjp,
                        const SkyMask::SkyRef *skyp,
                        const DepthMask::DepthRef *depthp,
                        double Wo, double Ho)
{
    if (d.isBrush) { const float c = (*d.brush)[k]; return d.inverted ? 1.0f - c : c; }
    if (d.isRange)
        return (d.rangeTool == int(MaskTool::LuminanceRange))
                 ? RangeMask::lumCoverage(*refp, onx, ony, d.rlo, d.rhi, d.feather,
                                          d.inverted)
                 : RangeMask::colorCoverage(*refp, onx, ony, d.samples,
                                            d.hueLo, d.hueHi, d.satLo, d.satHi,
                                            d.feather, d.inverted);
    if (d.isSubject)
        return SubjectMask::coverage(*subjp, onx, ony, float(d.feather),
                                     d.inverted ^ d.subjectBaseInvert);
    if (d.isSky)
        return SkyMask::coverage(*skyp, onx, ony, float(d.feather), d.inverted);
    if (d.isDepth)
        return DepthMask::coverage(*depthp, onx, ony, d.rlo, d.rhi, d.feather,
                                   d.inverted);
    if (d.isObject)
        return ObjectMask::coverage(*d.objRef, onx, ony, float(d.feather), d.inverted);
    return evalMaskComp(d.param, onx, ony, Wo, Ho);            // invert applied inside
}

/* ObjectRef store key: path + a hash of the brush blob. Unlike Subject/Sky/Depth (one param-
   independent ref per path), an object mask's coverage depends on its brush, so each component's
   brush gets its own ref and several object masks on one image coexist. MW::ensureObjectMask
   registers under this key; buildMaskBuffer's object sampler looks it up by the SAME key. */
QString objectRefKey(const QString &fPath, const QString &paramsJson)
{
    return fPath + "|obj|" + QString::number(qHash(paramsJson));
}

/* ---- Mask FOLD-PREFIX cache ----

   The component fold is SEQUENTIAL per pixel (m = max(m,c) / m *= 1-c / m *= c), so the
   running value after components [0..k) is a well-defined image: a prefix. Editing one
   submask leaves every earlier one untouched, so that prefix is still exactly right and
   the components inside it never have to be looked at again -- not folded, not
   rasterized, not even JSON-parsed. On a 22-submask scope that is the difference between
   touching 22 components per tick and touching one.

   Same shape as DevelopStackCache::hot one level down, but GLOBAL and mutex-guarded
   rather than owned by MW: buildMaskBuffer is called by the GUI proxy render, the veil,
   AND the off-thread settle render, and all three benefit. One entry per
   (path, dims, orientation) -- the veil now builds at the proxy's dimensions, so it
   shares the entry rather than keying its own.

   INVALIDATION. The per-component signature covers everything buildMaskBuffer reads out
   of a MaskComponent. What it does NOT cover is the path-registered references a
   component SAMPLES: MW::ensureRangeRef rebuilds when the base params change, which moves
   a content-range component's coverage without moving its signature. That one calls
   maskFoldCacheClear(). The AI/object/guide references are per-image and built once, and
   a component whose reference is not registered yet is refused entry to the prefix (see
   prefixRefsReady), so nothing else can go stale behind it. */
struct MaskFoldEntry {
    QVector<QByteArray> sigs;      // per-component sigs this prefix was built against
    int prefixCount = 0;           // how many leading components are folded into `prefix`
    std::shared_ptr<const std::vector<float>> prefix;
};

/* SEVERAL ENTRIES PER KEY, not one. The render and the veil build at the same path, dims
   and orientation -- deliberately, so they share brush rasters -- but they do NOT pass
   the same component list: the veil substitutes the pending submask's op and can carry
   one more component than the stack does. With one entry per key the two overwrote each
   other every tick and NEITHER ever resumed (measured: `fold 49 ms, 23 comps` on the
   render and `fold 57 ms, 24 comps` on the veil, i.e. the cache doing nothing at all).
   Keeping a few entries lets both callers hold their own prefix; the lookup simply takes
   whichever entry resumes deepest. */
constexpr int kMaskFoldEntriesPerKey = 3;
constexpr int kMaskFoldKeys = 4;
QMutex g_maskFoldMutex;
QHash<QString, QVector<MaskFoldEntry>> g_maskFoldCache;

void maskFoldCacheClear()
{
    QMutexLocker lk(&g_maskFoldMutex);
    g_maskFoldCache.clear();
}

/* Split `count` items across the global pool; fn(i0, i1) processes a disjoint half-open
   band. `work` is the total pixel count the pass touches, used only to stay serial below
   the threshold where dispatch would cost more than the pass itself. Same idiom as
   Develop::parallelFor and WorkingImageCache's maskParallelFor, which are file-local
   there. Rows are the common case (developParallelRows); MaskEdge's diagonal passes band
   over diagonals instead, which is why this takes a plain count. */
template <class F>
inline void developParallelBands(int count, size_t work, F fn)
{
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    if (maxThreads == 1 || count <= 1 || work < (size_t(1) << 16)) {
        fn(0, count);
        return;
    }
    const int h = count;
    const int chunks = qMin(maxThreads, h);
    const int per = (h + chunks - 1) / chunks;
    QVector<QFuture<void>> futs;
    futs.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const int y0 = k * per, y1 = qMin(h, y0 + per);
        if (y0 >= y1) break;
        futs.append(QtConcurrent::run(QThreadPool::globalInstance(), [=]{ fn(y0, y1); }));
    }
    for (QFuture<void> &f : futs) f.waitForFinished();
}

/* The row-oriented case: fn(y0, y1) over a disjoint half-open row band. */
template <class F>
inline void developParallelRows(int w, int h, F fn)
{
    developParallelBands(h, size_t(w) * size_t(h), fn);
}

/* Rasterize the scope's mask to a 0..1 buffer at the WorkingImage (pre-orientation) resolution, so
   it aligns with the linear blend before developComposite applies the EXIF rotation. Each pixel is
   mapped work-normalized -> output-normalized (output = work rotated CW by degrees) before each
   component is evaluated, so the geometry edited on the oriented loupe lines up.

   refsReady (when given) is CLEARED if any component's path-registered reference -- a
   range reference, an AI field, an object coverage, a brush auto-mask guide -- was not
   registered yet, in which case that component contributes nothing and the buffer is a
   placeholder. A caller must not cache such a buffer: the real one lands once the
   reference does, and nothing about the components changes to retire it. */
/*
    Perceptual luminance of a working image, on ITS OWN grid, for MaskHalo's guide.

    THE UNDEVELOPED IMAGE ON PURPOSE. A guide taken from the developed result would move
    every time a slider moved, so the mask would reshape itself in response to the very
    adjustment it is masking -- a feedback loop, and the oscillation the content-range
    masks avoid by keying off the base only. The working image cannot move, so the halo
    refinement is stable under a drag.

    PERCEPTUAL, NOT SCENE-LINEAR. The guided filter weights neighbours by how similar the
    guide says they are. In scene-linear a shadow-side edge is numerically tiny and would
    barely steer the filter, which is exactly where mask errors hide; the same gamma 2.2
    the mask BLEND uses (maskEnc, workingimagecache.cpp) puts dark and bright edges on
    comparable footing.

    Built ONCE per render tick and shared by every scope -- it depends only on the source.
*/
void buildHaloGuide(const WorkingImage &src, std::vector<float> &guide)
{
    const int w = src.width, h = src.height;
    if (w <= 0 || h <= 0) { guide.clear(); return; }
    guide.resize(size_t(w) * size_t(h));
    const float *rgb = src.rgb.data();
    float *g = guide.data();
    /* Weights for the space src is ACTUALLY in. The guide is built from the PRE-develop
       image, which for a raw is still camera-native (RawColor stops there), so the
       Rec.709 triple this used to hardcode would weight sensor channels as though they
       were sRGB primaries. See lumaWeightsFor in workingimage.h. */
    const ColorSpaceMath::Luma LW = lumaWeightsFor(src);
    developParallelRows(w, h, [&](int y0, int y1) {
        for (size_t k = size_t(y0) * w, e = size_t(y1) * w; k < e; ++k) {
            const float lum = LW.r * rgb[k*3+0] + LW.g * rgb[k*3+1]
                            + LW.b * rgb[k*3+2];
            g[k] = lum <= 0.0f ? 0.0f : std::pow(lum, 1.0f / 2.2f);
        }
    });
}

/*
    pixelScale converts an Edge slider value (which is FULL-RESOLUTION pixels, so it means
    the same thing in the sidecar whatever this render's size) into pixels of THIS buffer:
    this render's long edge over the full-res long edge. 1.0 for a settle or export, ~0.3
    for a screen proxy. scopeEdge is EditScope::maskEdge, applied to the folded mask.
*/
std::vector<float> buildMaskBuffer(const QVector<MaskComponent> &components, int w, int h, int degrees,
                                   const QString &fPath, bool *refsReady = nullptr,
                                   MaskBuildStats *stats = nullptr,
                                   double pixelScale = 1.0, float scopeEdge = 0.0f,
                                   float scopeHalo = 0.0f,
                                   const std::vector<float> *haloGuide = nullptr)
{
    QElapsedTimer buildProbe;
    const qint64 rasterMsOnEntry = stats ? stats->rasterMs : 0;
    if (stats) buildProbe.start();
    std::vector<float> out(size_t(w) * size_t(h), 0.0f);
    if (w <= 0 || h <= 0) return out;

    /* ONE morph scratch for the whole call: every Edge-bearing submask reuses it in turn,
       and so does the mask-level pass. At 45 MP that is 90 MB held once instead of 180 MB
       of float per submask -- the difference between a transient allocation and an OOM on
       a 22-submask scope. uint16 because MaskFalloff::kCutoff is 1/512, so 16 bits is ~7
       bits finer than anything the coverage profile promises. */
    std::vector<uint16_t> edgeBuf;
    auto par = [&](int count, auto &&fn) {
        developParallelBands(count, size_t(w) * size_t(h), fn);
    };
    /* Mask-level Edge: grow/shrink the mask every submask has been folded into. Kept as a
       lambda because it has to run on EVERY exit path -- including the one where the fold
       prefix already covered every component, which is exactly the path a drag of this
       slider takes (no component signature changes, so nothing is re-folded). */
    const double scopeEdgePx = double(scopeEdge) * pixelScale;
    auto applyScopeEdge = [&]() {
        if (std::fabs(scopeEdgePx) < MaskEdge::kMinRadius) return;
        QElapsedTimer morphProbe;
        if (stats) morphProbe.start();
        edgeBuf.resize(size_t(w) * size_t(h));
        developParallelRows(w, h, [&](int y0, int y1) {
            for (size_t k = size_t(y0) * w, e = size_t(y1) * w; k < e; ++k)
                edgeBuf[k] = MaskEdge::toU16(out[k]);
        });
        MaskEdge::apply(edgeBuf, w, h, scopeEdgePx, par);
        developParallelRows(w, h, [&](int y0, int y1) {
            for (size_t k = size_t(y0) * w, e = size_t(y1) * w; k < e; ++k)
                out[k] = MaskEdge::toFloat(edgeBuf[k]);
        });
        if (stats) { stats->morphMs += morphProbe.elapsed(); ++stats->morphs; }
    };

    /* Mask-level Halo: pull the folded mask's transition band onto the image's real edge
       (and/or damp it there), so a boundary that misses the subject stops spilling the
       adjustment onto a rim of background. Runs AFTER applyScopeEdge on every exit path
       -- Edge is a deliberate uniform offset and Halo then snaps that offset boundary to
       the picture; morphing afterwards would smear the edge Halo just found.

       The guide is the caller's working image, already on this buffer's grid. No guide
       (the veil before a proxy exists, a caller that has none) = no halo pass rather than
       a wrong one. */
    auto applyScopeHalo = [&]() {
        if (!haloGuide || scopeHalo < MaskHalo::kMinAmount) return;
        if (haloGuide->size() != size_t(w) * size_t(h)) return;
        QElapsedTimer haloProbe;
        if (stats) haloProbe.start();
        MaskHalo::apply(out, *haloGuide, w, h, scopeHalo, pixelScale, par);
        if (stats) { stats->haloMs += haloProbe.elapsed(); ++stats->halos; }
    };

    /* Resume the fold from the deepest still-valid prefix (see the fold-prefix cache
       above), so only the submask the user is actually editing -- and anything after
       it -- is looked at at all. */
    QVector<QByteArray> sigs;
    sigs.reserve(components.size());
    for (const MaskComponent &mc : components) sigs.append(maskComponentSignature(mc));
    const QString foldKey = fPath + "|" + QString::number(w) + "x" + QString::number(h)
                          + "@" + QString::number(degrees);
    int firstDirty = 0;
    int start = 0;
    int foldSlot = -1;              // entry we resumed from; preferred for the re-store
    std::shared_ptr<const std::vector<float>> startPrefix;   // what `out` was seeded from
    {
        QMutexLocker lk(&g_maskFoldMutex);
        auto it = g_maskFoldCache.constFind(foldKey);
        if (it != g_maskFoldCache.constEnd()) {
            const QVector<MaskFoldEntry> &entries = it.value();
            for (int ei = 0; ei < entries.size(); ++ei) {
                const MaskFoldEntry &e = entries.at(ei);
                int fd = 0;
                while (fd < sigs.size() && fd < e.sigs.size()
                       && e.sigs[fd] == sigs[fd]) ++fd;
                const bool usable = e.prefix && e.prefixCount <= fd
                                    && e.prefixCount <= sigs.size()
                                    && e.prefix->size() == out.size();
                /* Take whichever entry resumes DEEPEST; firstDirty must come from that
                   same entry, since it is what bounds the next cut point. */
                if (usable && (foldSlot < 0 || e.prefixCount > start)) {
                    foldSlot = ei;
                    start = e.prefixCount;
                    startPrefix = e.prefix;
                    firstDirty = fd;
                }
                else if (foldSlot < 0 && fd > firstDirty) firstDirty = fd;
            }
            if (startPrefix) out = *startPrefix;   // copy: the fold below mutates it
        }
    }
    /* Where to cut the NEXT prefix: at the first component that changed, so a drag on the
       same submask resumes here every tick. Always leave at least one out, or a tick
       where nothing changed would cache everything and the next edit could not
       resume. */
    int newK = firstDirty;
    if (newK >= components.size()) newK = qMax(start, components.size() - 1);
    newK = qBound(start, newK, components.size());

    const bool swap = (degrees == 90 || degrees == 270);
    const double Wo = swap ? h : w, Ho = swap ? w : h;   // output (oriented) pixel dimensions

    /* Content-range components (Luminance/Color Range) sample this display-referred reference of the
       developed base; built + registered on the GUI thread (MW::ensureRangeRef). Absent (not yet
       built) => range components yield 0, mirroring a brush with no auto-mask guide. */
    std::shared_ptr<const RangeMask::RangeRef> ref;
    for (const MaskComponent &m : components)
        if (m.tool == int(MaskTool::ColorRange) || m.tool == int(MaskTool::LuminanceRange)) {
            ref = RangeMask::getRef(fPath);
            break;
        }

    /* AI subject mask samples this fixed saliency map (built + registered by MW::ensureSubjectMask).
       Absent (not yet built) => subject components yield 0, like an unbuilt RangeRef. */
    /* Subject AND Background both sample the U^2-Net saliency (Background = inverted Subject). */
    std::shared_ptr<const SubjectMask::SubjectRef> subjRef;
    for (const MaskComponent &m : components)
        if (m.tool == int(MaskTool::Subject) || m.tool == int(MaskTool::Background)) {
            subjRef = SubjectMask::getRef(fPath); break;
        }

    std::shared_ptr<const SkyMask::SkyRef> skyRef;
    for (const MaskComponent &m : components)
        if (m.tool == int(MaskTool::Sky)) { skyRef = SkyMask::getRef(fPath); break; }

    std::shared_ptr<const DepthMask::DepthRef> depthRef;
    for (const MaskComponent &m : components)
        if (m.tool == int(MaskTool::Depth)) { depthRef = DepthMask::getRef(fPath); break; }

    /* Reference readiness for the components THIS call looks at ([start..n)). Those below
       `start` came out of a prefix that was only cached when its own were ready, so the
       property is inductive. */
    bool refsHereReady = true;
    QVector<CompDesc> comps;
    comps.reserve(components.size() - start);
    /* comps index at which the components stop belonging to the next cached prefix.
       Components are SKIPPED when their reference is missing, so this cannot be derived
       from the component index -- it has to be recorded as the loop passes newK. */
    int splitIdx = -1;
    bool prefixRefsReady = true;
    for (int ci = start; ci < components.size(); ++ci) {
        if (ci == newK) { splitIdx = comps.size(); prefixRefsReady = refsHereReady; }
        const MaskComponent &m = components.at(ci);
        /* Edge, in this buffer's pixels. Below kMinRadius it is dropped to 0 so a
           sub-threshold nudge stays on the cheap inline path instead of paying a
           materialize for a no-op. The two parametric tools ignore it here -- they take
           it through parseMaskComp and stay analytic. */
        const double edgePx = double(m.edge) * pixelScale;
        const double morphPx = (std::fabs(edgePx) >= MaskEdge::kMinRadius) ? edgePx : 0.0;
        if (m.tool == 2) {                                // Brush: rasterize (cached) strokes
            CompDesc d;
            d.isBrush = true;
            d.brush = brushRasterCached(m.paramsJson, w, h, degrees, fPath,
                                        &refsHereReady, stats);
            d.op = m.op;
            d.inverted = m.inverted;
            d.edgePx = morphPx;
            comps.append(d);
        }
        else if (m.tool == int(MaskTool::ColorRange) || m.tool == int(MaskTool::LuminanceRange)) {
            if (!ref || !ref->valid()) {              // reference not ready -> no effect
                refsHereReady = false;
                continue;
            }
            const QJsonObject o = QJsonDocument::fromJson(m.paramsJson.toUtf8()).object();
            CompDesc d;
            d.isRange   = true;
            d.rangeTool = m.tool;
            d.op        = m.op;
            d.inverted  = m.inverted;
            d.feather   = m.feather;
            d.edgePx    = morphPx;
            if (m.tool == int(MaskTool::LuminanceRange)) {
                d.rlo = o.value("lo").toDouble(0.0);
                d.rhi = o.value("hi").toDouble(1.0);
            }
            else {
                d.hueLo = o.value("hueLo").toDouble(20.0);
                d.hueHi = o.value("hueHi").toDouble(20.0);
                d.satLo = o.value("satLo").toDouble(25.0) / 100.0;
                d.satHi = o.value("satHi").toDouble(25.0) / 100.0;
                const QJsonArray sa = o.value("samples").toArray();
                for (const QJsonValue &sv : sa) {
                    const QJsonArray c = sv.toArray();
                    if (c.size() < 3) continue;
                    d.samples.push_back(RangeMask::toHueSat(float(c[0].toDouble()),
                                                            float(c[1].toDouble()),
                                                            float(c[2].toDouble())));
                }
            }
            comps.append(d);
        }
        else if (m.tool == int(MaskTool::Subject) || m.tool == int(MaskTool::Background)) {
            if (!subjRef || !subjRef->valid()) {      // saliency not ready -> no effect
                refsHereReady = false;
                continue;
            }
            CompDesc d;
            d.isSubject = true;
            d.subjectBaseInvert = (m.tool == int(MaskTool::Background));   // background = 1 - subject
            d.op        = m.op;
            d.inverted  = m.inverted;
            d.feather   = m.feather;
            d.edgePx    = morphPx;
            comps.append(d);
        }
        else if (m.tool == int(MaskTool::Sky)) {
            if (!skyRef || !skyRef->valid()) {            // sky not ready -> no effect
                refsHereReady = false;
                continue;
            }
            CompDesc d;
            d.isSky     = true;
            d.op        = m.op;
            d.inverted  = m.inverted;
            d.feather   = m.feather;
            d.edgePx    = morphPx;
            comps.append(d);
        }
        else if (m.tool == int(MaskTool::Depth)) {
            if (!depthRef || !depthRef->valid()) {      // depth not ready -> no effect
                refsHereReady = false;
                continue;
            }
            const QJsonObject o = QJsonDocument::fromJson(m.paramsJson.toUtf8()).object();
            CompDesc d;
            d.isDepth   = true;
            d.op        = m.op;
            d.inverted  = m.inverted;
            d.feather   = m.feather;
            d.edgePx    = morphPx;
            d.rlo       = o.value("lo").toDouble(0.0);      // depth band [lo,hi], 0=near..1=far
            d.rhi       = o.value("hi").toDouble(0.5);
            comps.append(d);
        }
        else if (m.tool == int(MaskTool::Object)) {
            /* Per-brush ref (keyed path+brush), built by MW::ensureObjectMask. Absent (not yet
               decoded, or this brush empty) => no effect, like an unbuilt SubjectRef. */
            auto objRef = ObjectMask::getRef(objectRefKey(fPath, m.paramsJson));
            if (!objRef || !objRef->valid()) {            // object not ready -> no effect
                refsHereReady = false;
                continue;
            }
            CompDesc d;
            d.isObject  = true;
            d.objRef    = objRef;
            d.op        = m.op;
            d.inverted  = m.inverted;
            d.feather   = m.feather;
            d.edgePx    = morphPx;
            comps.append(d);
        }
        else {
            const MaskComp g = parseMaskComp(m, Wo, Ho, edgePx);
            if (!g.valid) continue;
            CompDesc d;
            d.param = g;
            d.op = g.op;
            comps.append(d);
        }
    }
    if (splitIdx < 0) { splitIdx = comps.size(); prefixRefsReady = refsHereReady; }
    if (refsReady && !refsHereReady) *refsReady = false;
    /* Nothing new to fold -> the prefix (or zeros). The mask-level Edge still applies:
       dragging THAT slider changes no component signature, so this is the path it takes
       on every tick. */
    if (comps.isEmpty()) { applyScopeEdge(); applyScopeHalo(); return out; }
    const RangeMask::RangeRef *refp = ref ? ref.get() : nullptr;
    const SubjectMask::SubjectRef *subjp = subjRef ? subjRef.get() : nullptr;
    const SkyMask::SkyRef *skyp = skyRef ? skyRef.get() : nullptr;
    const DepthMask::DepthRef *depthp = depthRef ? depthRef.get() : nullptr;

    const double invW = 1.0/w, invH = 1.0/h;
    /* c0..c1 is a half-open range into comps, so the fold can run in two passes with
       the prefix captured between them. m is seeded from the buffer, NOT from 0: on a
       resumed call that is the cached prefix, and on a cold one the buffer is zeros --
       exactly what the fold used to start from, so the maths is unchanged either way. */
    auto rows = [&](int c0, int c1, int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const double wny = (y + 0.5) * invH;
            float *row = out.data() + size_t(y) * w;
            for (int x = 0; x < w; ++x) {
                const size_t k = size_t(y) * w + x;
                const double wnx = (x + 0.5) * invW;
                double onx, ony;
                switch (degrees) {           // work-normalized -> output-normalized (CW rotation)
                    case 90:  onx = 1.0 - wny; ony = wnx;       break;
                    case 180: onx = 1.0 - wnx; ony = 1.0 - wny; break;
                    case 270: onx = wny;       ony = 1.0 - wnx; break;
                    default:  onx = wnx;       ony = wny;       break;
                }
                float m = row[x];
                for (int di = c0; di < c1; ++di) {
                    const CompDesc &d = comps.at(di);
                    const float c = evalCompAt(d, onx, ony, k, refp, subjp, skyp, depthp,
                                               Wo, Ho);
                    /* Sequential fold: Subtract removes, Intersect keeps overlap,
                       Add unions. */
                    if      (d.op == int(MaskOp::Subtract))  m *= (1.0f - c);
                    else if (d.op == int(MaskOp::Intersect)) m *= c;
                    else                                     m = qMax(m, c);
                }
                row[x] = m;
            }
        }
    };

    /* Subtract only the raster time THIS call accrued: stats are accumulated across
       calls (the veil makes two), so using the running total drove setup negative. */
    if (stats) { stats->setupMs += buildProbe.restart()
                                   - (stats->rasterMs - rasterMsOnEntry);
                 stats->comps += comps.size(); }

    /*
        Fold [c0,c1). Components with NO Edge keep the original interleaved fold verbatim,
        so a mask nobody has run the slider on costs exactly what it cost before.

        A component WITH an Edge cannot be evaluated inline -- moving a boundary needs
        neighbours, and the fold is a per-pixel scalar loop -- so it is materialized into
        the shared scratch, morphed, and folded from there on its own. Doing them one at
        a time keeps the peak at ONE scratch buffer rather than one per submask.
    */
    auto foldEdgeComp = [&](const CompDesc &d) {
        QElapsedTimer morphProbe;
        if (stats) morphProbe.start();
        edgeBuf.resize(size_t(w) * size_t(h));
        developParallelRows(w, h, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                const double wny = (y + 0.5) * invH;
                for (int x = 0; x < w; ++x) {
                    const double wnx = (x + 0.5) * invW;
                    double onx, ony;
                    switch (degrees) {       // work-normalized -> output-normalized
                        case 90:  onx = 1.0 - wny; ony = wnx;       break;
                        case 180: onx = 1.0 - wnx; ony = 1.0 - wny; break;
                        case 270: onx = wny;       ony = 1.0 - wnx; break;
                        default:  onx = wnx;       ony = wny;       break;
                    }
                    const size_t k = size_t(y) * w + x;
                    edgeBuf[k] = MaskEdge::toU16(
                        evalCompAt(d, onx, ony, k, refp, subjp, skyp, depthp, Wo, Ho));
                }
            }
        });
        MaskEdge::apply(edgeBuf, w, h, d.edgePx, par);
        developParallelRows(w, h, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                float *row = out.data() + size_t(y) * w;
                const uint16_t *src = edgeBuf.data() + size_t(y) * w;
                for (int x = 0; x < w; ++x) {
                    const float c = MaskEdge::toFloat(src[x]);
                    if      (d.op == int(MaskOp::Subtract))  row[x] *= (1.0f - c);
                    else if (d.op == int(MaskOp::Intersect)) row[x] *= c;
                    else                                     row[x] = qMax(row[x], c);
                }
            }
        });
        if (stats) { stats->morphMs += morphProbe.elapsed(); ++stats->morphs; }
    };
    auto foldSegments = [&](int c0, int c1) {
        int i = c0;
        while (i < c1) {
            if (comps.at(i).edgePx != 0.0) { foldEdgeComp(comps.at(i)); ++i; continue; }
            int j = i;
            while (j < c1 && comps.at(j).edgePx == 0.0) ++j;
            developParallelRows(w, h, [&](int y0, int y1){ rows(i, j, y0, y1); });
            i = j;
        }
    };

    /* Fold the part that belongs to the next prefix, snapshot it, then fold the rest. */
    foldSegments(0, splitIdx);

    if (prefixRefsReady) {
        /* When nothing new was folded, `out` is still exactly the buffer we were seeded
           with -- re-share it instead of copying w*h*4 bytes. */
        auto snapshot = (splitIdx == 0 && startPrefix)
                            ? startPrefix
                            : std::make_shared<const std::vector<float>>(out);
        QMutexLocker lk(&g_maskFoldMutex);
        if (g_maskFoldCache.size() > kMaskFoldKeys) g_maskFoldCache.clear();
        QVector<MaskFoldEntry> &entries = g_maskFoldCache[foldKey];
        /* Write back over the entry we resumed from -- this caller's own slot.
           Overwriting a different caller's is exactly the thrash the vector prevents.

           foldSlot is only a HINT: it was chosen under an EARLIER acquisition of this
           mutex, and between then and now the cache can have been cleared -- by
           maskFoldCacheClear() from the GUI thread's ensureRangeRef, by the key cap just
           above, or by another render. Re-validate it against the vector as it stands or
           entries[slot] indexes off the end. That is a SEGV, not a wrong pixel, and
           tests/tsan/run_tsan_develop.sh crashed on it within a minute. */
        int slot = (foldSlot >= 0 && foldSlot < entries.size()) ? foldSlot : -1;
        if (slot < 0) {
            if (entries.size() < kMaskFoldEntriesPerKey) {
                entries.append(MaskFoldEntry());
                slot = entries.size() - 1;
            }
            else {   // evict the shallowest: it is saving the least work
                slot = 0;
                for (int ei = 1; ei < entries.size(); ++ei)
                    if (entries.at(ei).prefixCount < entries.at(slot).prefixCount)
                        slot = ei;
            }
        }
        MaskFoldEntry &e = entries[slot];
        e.sigs = sigs;
        e.prefixCount = newK;
        e.prefix = std::move(snapshot);
    }

    foldSegments(splitIdx, comps.size());

    /* AFTER the snapshot, never before: the prefix is a PARTIAL fold, and morphing it
       would hand every later resume a shape that is not a prefix of anything. */
    applyScopeEdge();
    applyScopeHalo();
    if (stats) stats->foldMs += buildProbe.elapsed();
    return out;
}

/* developComposite for a full scope stack: composite every enabled scope in scene-linear (each
   developed from the original and blended by its mask), then apply orientation / proxy scaling
   exactly as developComposite does. Falls back to the single-pass developComposite when there are
   no non-Global scopes. */
/*
    Run the Fill Replace heals over img in place, preserving its bit depth.

    Both heal engines are 8-bit: LaMa/MI-GAN are 8-bit models, and the exemplar clone path
    works in ARGB32. Handing them a 16-bit export image would flatten the WHOLE frame to 8
    bits over a few small heals. So for a deep image the heal runs on an 8-bit copy and
    only the pixels it actually CHANGED are written back, promoted to 16 bits. Healed
    pixels are synthesized, so 8-bit precision there costs nothing; every untouched pixel
    keeps its full 16-bit value.
*/
/* The heal engines hold per-process model/session state and were only ever driven by ONE
   background render at a time. Now the interactive proxy render has its own pool
   alongside the settle render's, so two can overlap; serialise the heal rather than
   audit LaMa and MI-GAN for reentrancy. Spots are rare, so without them the lock is
   uncontended. */
QMutex g_spotHealMutex;

void applySpots(QImage &img, const QVector<FillSpot> &spots, const QString &fPath)
{
    QMutexLocker healLock(&g_spotHealMutex);
    auto heal = [&spots, &fPath](QImage &target) {
        if (G::useLamaSpotFill)
            LamaFill::apply(target, spots, fPath);   // fPath keys the pinned sources
        else
            MiganFill::apply(target, spots);
    };

    const bool deep = img.format() == QImage::Format_RGBX64 ||
                      img.format() == QImage::Format_RGBA64 ||
                      img.format() == QImage::Format_RGBA64_Premultiplied;
    if (!deep) { heal(img); return; }

    const QImage before = img.convertToFormat(QImage::Format_ARGB32);
    QImage after = before;
    after.detach();                       // heal() mutates in place; keep `before` intact
    heal(after);
    after = after.convertToFormat(QImage::Format_ARGB32);
    if (after.size() != before.size()) {   // engine resized: shouldn't happen, take as-is
        img = after;
        return;
    }

    /* Write back only the changed pixels, 8-bit -> 16-bit by the exact 257x scale
       (255*257 == 65535, so full-scale maps to full-scale). */
    const int W = img.width(), H = img.height();
    for (int y = 0; y < H; ++y) {
        const QRgb *b = reinterpret_cast<const QRgb*>(before.constScanLine(y));
        const QRgb *a = reinterpret_cast<const QRgb*>(after.constScanLine(y));
        quint16 *d = reinterpret_cast<quint16*>(img.scanLine(y));
        for (int x = 0; x < W; ++x) {
            if (a[x] == b[x]) continue;
            d[x * 4 + 0] = quint16(qRed(a[x])   * 257);
            d[x * 4 + 1] = quint16(qGreen(a[x]) * 257);
            d[x * 4 + 2] = quint16(qBlue(a[x])  * 257);
        }
    }
}

/*
    THE DETAIL PANEL'S 1:1 PREVIEW.

    A request to render one small patch of the image at FULL RESOLUTION alongside the
    interactive proxy, for the square window at the head of the Detail section. Passed
    into developCompositeStack because that is where the scopes' mask buffers already
    exist for this tick; the patch is rendered from them rather than rebuilding anything.

    WHY IT CANNOT JUST CROP THE PROXY. Sharpening's radius is in ABSOLUTE pixels, so it is
    the one op the proxy renders wrongly by design (Develop/sharpen.h): at proxy scale the
    effective sigma hits its floor and the slider moves acutance ~5% instead of ~70%.
    Cropping the proxy would magnify that wrong answer. The patch is therefore cut from
    the FULL-RES base with renderScale 1.0 -- the same pixels the settle render produces,
    for a few hundred px square instead of 50 MP.

    THE ONE APPROXIMATION: a masked scope's coverage is sampled from the mask this tick
    already rasterized at PROXY resolution, not rebuilt at full res. Coverage is a smooth
    field, so magnifying it costs a slightly softer mask EDGE inside the patch; rebuilding
    a 50 MP mask per tick to sharpen a 170 px window would cost orders of magnitude more
    than the render it feeds. Documented in notes/Documentation.txt.
*/
struct DetailRoiJob {
    bool wanted = false;
    double nx = 0.5, ny = 0.5;          // sample point, normalized, ORIENTED frame
    int size = 0;                       // patch side, full-res px
    const WorkingImage *fullSrc = nullptr;   // full-res base to cut the patch from
    QImage out;                         // the render's product
};

/* Cut the ROI out of the full-res base. renderScale is forced to 1.0 -- that is the whole
   point of the exercise, and copyMetadata would otherwise carry the base's own value. */
static WorkingImage cutDetailRoi(const WorkingImage &full, const DetailRoi::Rect &r)
{
    WorkingImage roi;
    if (r.isEmpty() || !full.isValid()) return roi;
    roi.width = r.w;
    roi.height = r.h;
    copyMetadata(roi, full);
    roi.renderScale = 1.0f;
    roi.rgb.resize(size_t(r.w) * size_t(r.h) * 3);
    for (int y = 0; y < r.h; ++y) {
        const float *srcRow = full.rgb.data() + (size_t(y + r.y) * full.width + r.x) * 3;
        float *dstRow = roi.rgb.data() + size_t(y) * r.w * 3;
        std::copy(srcRow, srcRow + size_t(r.w) * 3, dstRow);
    }
    return roi;
}

/* A scope's coverage over the ROI, sampled from its proxy-resolution buffer (see the
   approximation note above). Nearest-neighbour would alias the mask edge into a visible
   staircase inside a magnified patch, so this is bilinear. */
static std::shared_ptr<const std::vector<float>>
sampleMaskRoi(const std::vector<float> &mask, int mw, int mh,
              const DetailRoi::Rect &r, int fullW, int fullH)
{
    auto out = std::make_shared<std::vector<float>>(size_t(r.w) * size_t(r.h), 1.0f);
    if (mw <= 0 || mh <= 0 || fullW <= 0 || fullH <= 0) return out;
    if (mask.size() != size_t(mw) * size_t(mh)) return out;
    const double sx = double(mw) / double(fullW);
    const double sy = double(mh) / double(fullH);
    float *o = out->data();
    for (int y = 0; y < r.h; ++y) {
        const double my = std::clamp((y + r.y + 0.5) * sy - 0.5, 0.0, double(mh - 1));
        const int y0 = int(my), y1 = std::min(y0 + 1, mh - 1);
        const float fy = float(my - y0);
        for (int x = 0; x < r.w; ++x) {
            const double mx = std::clamp((x + r.x + 0.5) * sx - 0.5, 0.0, double(mw - 1));
            const int x0 = int(mx), x1 = std::min(x0 + 1, mw - 1);
            const float fx = float(mx - x0);
            const float a = mask[size_t(y0) * mw + x0], b = mask[size_t(y0) * mw + x1];
            const float c = mask[size_t(y1) * mw + x0], d = mask[size_t(y1) * mw + x1];
            o[size_t(y) * r.w + x] = (a + (b - a) * fx)
                                   + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
        }
    }
    return out;
}

/* Render the patch. scopes carry THIS tick's proxy masks (empty for a global-only stack);
   maskW/maskH are the proxy dimensions those masks were rasterized at. */
static QImage renderDetailRoiImage(const DetailRoiJob &job,
                                   const DevelopProperties::StackRenderJob &stackJob,
                                   const std::vector<WorkingImageCache::StackScope> &scopes,
                                   int maskW, int maskH, int degrees)
{
    const WorkingImage &full = *job.fullSrc;
    const DetailRoi::Rect r =
        DetailRoi::sourceRoi(job.nx, job.ny, full.width, full.height, degrees, job.size);
    WorkingImage roi = cutDetailRoi(full, r);
    if (!roi.isValid()) return QImage();

    std::vector<WorkingImageCache::StackScope> roiScopes;
    roiScopes.reserve(scopes.size());
    for (const WorkingImageCache::StackScope &s : scopes) {
        WorkingImageCache::StackScope rs;
        rs.params = s.params;
        if (s.mask) rs.mask = sampleMaskRoi(*s.mask, maskW, maskH, r,
                                            full.width, full.height);
        roiScopes.push_back(std::move(rs));
    }

    QImage out;
    if (!WorkingImageCache::renderStack(roi, stackJob.global, roiScopes, out))
        return QImage();
    /* Orientation only: geometry (crop / warp) is deliberately not applied -- see the
       header note in Develop/detailroi.h. Smooth, not fast: the patch is tiny and it is
       being examined at 1:1, which is exactly where a nearest-neighbour rotate shows. */
    if (degrees != 0 && !out.isNull()) {
        QTransform trans;
        trans.rotate(degrees);
        out = out.transformed(trans, Qt::SmoothTransformation);
    }
    return out;
}

/* cache (optional) is the interactive proxy's per-scope intermediate store -- see
   Develop/developstackcache.h. Only the GUI-thread proxy path passes one; the off-thread
   settle render and the small verification renders pass nullptr and recompute everything,
   which is also what keeps the cache lock-free. */
/* Defined below, next to the other stack predicates; needed here for the mask keys. */
bool scopeMaskDependsOnBase(const DevelopProperties::StackRenderJob::Scope &L);

QImage developCompositeStack(const WorkingImage &src, const DevelopProperties::StackRenderJob &job,
                             int degrees, bool fullRes, int fullW, int fullH, const QString &fPath,
                             WorkingImageCache::RenderTimings *timings = nullptr,
                             WorkingImageCache::OutDepth depth =
                                 WorkingImageCache::OutDepth::Eight,
                             WorkingImageCache::Space space =
                                 WorkingImageCache::Space::sRGB,
                             DevelopStackCache *cache = nullptr,
                             QSize *displaySize = nullptr,
                             MaskBuildStats *maskStats = nullptr,
                             const QByteArray &baseKey = QByteArray(),
                             DetailRoiJob *roiJob = nullptr)
{
    /* An interactive (proxy) render is normally left at PROXY resolution -- the loupe
       stretches it (ScaledPixmapItem) instead of this function allocating and filling a
       full-dimension QImage on every drag tick. *displaySize reports the size it stands
       in for.

       EXCEPTION: a non-identity geometry. Crop/straighten/warp is applied at the bottom
       of this function on the FULL-output-dimension image precisely so the cropped
       result has the SAME dimensions as the settle render's; cropping the proxy instead
       would round to a slightly different size and make the arriving settle re-fit the
       view. So the upscale stays there. */
    const bool upscale = fullRes || !job.geometry.isIdentity();
    if (displaySize && !upscale) *displaySize = QSize(fullW, fullH);

    QImage out;
    if (job.scopes.isEmpty()) {             // just Global -> the fast single-pass path
        /* An unmasked image still lands here on EVERY interactive tick, and render()'s
           working copy is proxy-sized: allocating and releasing it measured 62 ms per
           tick on a 6.7 MP proxy, against 1 ms to fill it. Lend it the same scratch the
           stack path uses -- the two branches are mutually exclusive, and assignReusing
           resizes if the geometry differs. Held locally so the buffer outlives the call
           even if the GUI thread clears the cache mid-render. */
        std::shared_ptr<WorkingImage> scratchHold;
        if (cache) scratchHold = cache->accScratch();
        out = developComposite(src, job.global, degrees, fullRes, fullW, fullH, timings,
                               depth, space, upscale, scratchHold.get());
        /* Global-only stack: the patch has no masks to sample, just the base params. */
        if (roiJob && roiJob->wanted && roiJob->fullSrc)
            roiJob->out = renderDetailRoiImage(*roiJob, job, {}, src.width, src.height,
                                               degrees);
    }
    else {
        QElapsedTimer probe;
        if (timings) probe.start();
        const size_t nScopes = size_t(job.scopes.size());
        if (cache) cache->matchScopeCount(nScopes);

        /* Signature every scope FIRST, while the cache still describes the previous tick,
           so firstDirty compares like with like -- putMask below overwrites the keys. */
        std::vector<QByteArray> maskKeys(nScopes), paramsKeys(nScopes);
        size_t first = 0;
        if (cache) {
            for (size_t i = 0; i < nScopes; ++i) {
                const DevelopProperties::StackRenderJob::Scope &L = job.scopes.at(int(i));
                maskKeys[i] = maskComponentsSignature(L.components, L.maskEdge,
                                                      L.maskHalo);
                /* A content-range mask thresholds a reference derived from the developed
                   base, so its coverage moves when the base does even though no component
                   changed -- fold the base signature in for those scopes and ONLY those.
                   Doing it for every scope is what made a Global drag rebuild a radial
                   mask on every tick. See scopeMaskDependsOnBase. */
                if (scopeMaskDependsOnBase(L)) {
                    maskKeys[i] += '#';
                    maskKeys[i] += baseKey;
                }
                paramsKeys[i] = QJsonDocument(EditStack::paramsToJson(L.params))
                                    .toJson(QJsonDocument::Compact);
            }
            first = cache->firstDirty(maskKeys, paramsKeys);
        }
        /* Read BEFORE the loop below overwrites the stored keys: a cached layer survives
           a mask-only edit, but not a params edit, of the scope we resume at. */
        const size_t hotAt = nScopes ? qMin(first, nScopes - 1) : 0;
        const bool hotParamsSame =
            cache && nScopes && cache->paramsKeyAt(hotAt) == paramsKeys[hotAt];

        /* One guide for the whole tick, and only when some scope actually asks for it --
           a pass over the source is not free and most stacks carry no halo. */
        std::vector<float> haloGuide;
        for (int i = 0; i < job.scopes.size(); ++i) {
            if (job.scopes.at(i).maskHalo >= MaskHalo::kMinAmount) {
                buildHaloGuide(src, haloGuide);
                break;
            }
        }

        std::vector<WorkingImageCache::StackScope> sl;
        sl.reserve(nScopes);
        for (size_t i = 0; i < nScopes; ++i) {
            const DevelopProperties::StackRenderJob::Scope &L = job.scopes.at(int(i));
            WorkingImageCache::StackScope s;
            s.params = L.params;
            if (!L.components.isEmpty()) {       // empty masks => global scope, no buffer
                /* A mask drag changes ONE scope's components; every other scope's buffer
                   is identical to last tick's, so only the changed one is rasterized. */
                s.mask = cache ? cache->mask(i, maskKeys[i]) : nullptr;
                if (!s.mask) {
                    bool refsReady = true;
                    s.mask = std::make_shared<const std::vector<float>>(
                        buildMaskBuffer(L.components, src.width, src.height, degrees,
                                        fPath, &refsReady, maskStats,
                                        double(src.renderScale), L.maskEdge,
                                        L.maskHalo,
                                        haloGuide.empty() ? nullptr : &haloGuide));
                    if (timings) ++timings->maskScopes;
                    /* A buffer built without one of its references is a placeholder --
                       cache it and nothing would ever retire it (the components do not
                       change when the reference lands). */
                    if (cache) {
                        if (refsReady) cache->putMask(i, maskKeys[i], s.mask);
                        else           cache->dropMask(i);
                    }
                }
            }
            if (cache) cache->setParamsKey(i, paramsKeys[i]);
            sl.push_back(std::move(s));
        }
        if (timings) timings->maskBuildMs = probe.restart();

        /* Resume from the developed intermediates of the scope being edited, so a
           mask-only drag costs one blend instead of re-developing the whole stack. The
           layer is reusable only when this scope's PARAMS are also unchanged. */
        WorkingImageCache::StackResume resume;
        WorkingImageCache::StackResume *resumeP = nullptr;
        if (cache && nScopes) {
            /* The prefix IS the developed base, so it is keyed on the base explicitly --
               unlike the masks, which are not a function of it. */
            const DevelopStackCache::Hot hot = cache->hotAt(hotAt, baseKey);  // under lock
            if (hot.prefix) {
                resume.start  = hotAt;
                resume.prefix = hot.prefix;
                if (hotParamsSame) resume.layer = hot.layer;
            }
            resume.capture = hotAt;      // re-arm for the next tick of this same drag
            /* Reusable storage for the tick's three proxy-sized intermediates, so the
               render refills buffers instead of allocating and releasing ~38 MB each
               time -- which measured 87 ms of a 121 ms tick. outScratch() hands back
               whichever of the layer pair `hot.layer` is not serving, so renderStack
               never writes a buffer the cache is still handing out as const. */
            resume.accScratch = cache->accScratch();
            resume.layScratch = cache->layScratch();
            resume.outScratch = cache->outScratch();
            resume.preScratch = cache->prefixScratch();
            resumeP = &resume;
        }
        if (!WorkingImageCache::renderStack(src, job.global, sl, out, timings, depth,
                                           space, resumeP))
            return QImage();
        /* setHot REPLACES the previous tick's prefix/layer, so the shared_ptrs it drops
           free two proxy-resolution WorkingImages here. That is not bookkeeping -- it is
           a large free landing between two timed stages -- so it gets its own number
           rather than hiding in the compositor's unattributed remainder. */
        if (cache && resumeP) {
            if (timings) probe.restart();
            cache->setHot(resume.capture, baseKey, resume.outPrefix, resume.outLayer);
            resume.prefix.reset();
            resume.layer.reset();
            if (timings) timings->setHotMs = probe.elapsed();
        }
        /* The patch, while sl still holds this tick's masks. Before the rotate below so
           it reads the same state the composite did. */
        if (roiJob && roiJob->wanted && roiJob->fullSrc)
            roiJob->out = renderDetailRoiImage(*roiJob, job, sl, src.width, src.height,
                                               degrees);
        if (timings) probe.restart();           // renderStack timed its own stages
        if (degrees != 0) {
            QTransform trans;
            trans.rotate(degrees);
            out = out.transformed(trans, fullRes ? Qt::SmoothTransformation : Qt::FastTransformation);
        }
        if (upscale && !fullRes && (out.width() != fullW || out.height() != fullH))
            out = out.scaled(fullW, fullH, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (timings) timings->orientScaleMs = probe.elapsed();
    }

    /* Fill Replace heals BEFORE geometry, on the developed oriented full frame, so heals
       stay glued to content when later cropped/straightened. The replace engine
       (lamafill.cpp) heals Spot/Fill kinds by exemplar clone -- no model needed -- and
       Object kinds (or clone fallbacks) with LaMa; the model path no-ops warn-if-absent.
       Flip G::useLamaSpotFill off (global.cpp) to revert to the MI-GAN engine. */
    if (!out.isNull() && !job.spots.isEmpty()) applySpots(out, job.spots, fPath);

    /* Geometry (crop / straighten / warp) is applied LAST -- after the develop ops + EXIF
       orientation -- on the full-output-dimension image, so the crop dims match for the
       proxy and the full-res render. Callers pass identity geometry while the crop tool
       is being edited (the loupe then shows the full frame for the overlay). */
    if (!out.isNull() && !job.geometry.isIdentity())
        out = CropTransform::applyGeometry(out, job.geometry);
    return out;
}

/* True if this scope's mask COVERAGE depends on the global (base) params.

   Only the content-range tools do: they threshold a display-referred reference built
   from the developed base (MW::ensureRangeRef), so a base change can move their coverage
   without moving any component. Every other reference a tool samples -- the AI fields,
   the object coverages, the brush auto-mask guide -- is registered per PATH and is never
   rebuilt when the base moves (see the guards in ensureSubjectMask / ensureSkyMask /
   ensureDepthMask / ensureObjectMask / ImageView::ensureAutoGuide), so those masks are
   base-independent by construction rather than by luck. This is what lets the stack cache
   keep a radial or brush mask across a Global slider drag instead of rebuilding it on
   every tick -- see DevelopStackCache's VALIDITY note. */
bool scopeMaskDependsOnBase(const DevelopProperties::StackRenderJob::Scope &L)
{
    for (const MaskComponent &m : L.components)
        if (m.tool == int(MaskTool::ColorRange) || m.tool == int(MaskTool::LuminanceRange))
            return true;
    return false;
}

/* True if any enabled scope carries a content-range mask (Luminance/Color Range) -- these need the
   display-referred base reference (MW::ensureRangeRef) built before the composite. */
bool stackHasRangeMask(const DevelopProperties::StackRenderJob &job)
{
    for (const DevelopProperties::StackRenderJob::Scope &L : job.scopes)
        if (scopeMaskDependsOnBase(L)) return true;
    return false;
}

/* True if any enabled scope carries an AI Subject mask -- these need the saliency map
   (MW::ensureSubjectMask) built before the composite. */
bool stackHasSubjectMask(const DevelopProperties::StackRenderJob &job)
{
    /* Background reuses the subject saliency (inverted), so it needs the ref built too. */
    for (const DevelopProperties::StackRenderJob::Scope &L : job.scopes)
        for (const MaskComponent &m : L.components)
            if (m.tool == int(MaskTool::Subject) || m.tool == int(MaskTool::Background)) return true;
    return false;
}

/* True if any enabled scope carries an AI Sky mask -- needs the sky coverage (MW::ensureSkyMask). */
bool stackHasSkyMask(const DevelopProperties::StackRenderJob &job)
{
    for (const DevelopProperties::StackRenderJob::Scope &L : job.scopes)
        for (const MaskComponent &m : L.components)
            if (m.tool == int(MaskTool::Sky)) return true;
    return false;
}

/* True if any enabled scope carries an AI Depth Range mask -- needs the depth field (ensureDepthMask). */
bool stackHasDepthMask(const DevelopProperties::StackRenderJob &job)
{
    for (const DevelopProperties::StackRenderJob::Scope &L : job.scopes)
        for (const MaskComponent &m : L.components)
            if (m.tool == int(MaskTool::Depth)) return true;
    return false;
}

/* True if any enabled scope carries an AI Object mask -- each needs its brush decoded into an
   ObjectRef (MW::ensureObjectMask) before the composite. */
bool stackHasObjectMask(const DevelopProperties::StackRenderJob &job)
{
    for (const DevelopProperties::StackRenderJob::Scope &L : job.scopes)
        for (const MaskComponent &m : L.components)
            if (m.tool == int(MaskTool::Object)) return true;
    return false;
}

/* True if any enabled scope carries a Brush mask with an auto-mask stroke -- it needs the
   colour guide registered (ImageView::ensureAutoGuide) before the composite, else the
   stroke rasterizes unconfined. */
bool stackHasAutoMaskBrush(const DevelopProperties::StackRenderJob &job)
{
    for (const DevelopProperties::StackRenderJob::Scope &L : job.scopes)
        for (const MaskComponent &m : L.components) {
            if (m.tool != int(MaskTool::Brush)) continue;
            const QJsonArray strokes = QJsonDocument::fromJson(m.paramsJson.toUtf8())
                                           .object().value("strokes").toArray();
            for (const QJsonValue &sv : strokes) {
                const QJsonObject so = sv.toObject();
                if (so.value("autoMask").toBool(false)) return true;
            }
        }
    return false;
}
} // namespace

void MW::developParamsChange()
{
/*
    A Develop dock slider changed (DevelopProperties::paramsChanged). A fast drag fires this
    many times per second, and each re-render is a real cost, so we COALESCE: render the
    screen-resolution proxy at most once per event-loop turn (the 0ms single-shot collapses a
    burst of ticks into one render) and (re)arm a settle timer so the crisp full-resolution
    render runs only once the slider stops moving. The full-res render runs OFF the GUI thread
    (renderDevelopFullResAsync) so it never freezes the drag; developParamsGen is bumped here so a
    render that finishes after the params move on is discarded as stale.
*/
    if (G::isLogger) G::log("MW::developParamsChange");

    ++developParamsGen;
    if (!developProxyRenderTimer->isActive()) developProxyRenderTimer->start(0);
    developFullResTimer->start(kDevelopSettleMs);
}

void MW::ensureRangeRef(const QString &fPath, const WorkingImage &work,
                        const EditParams &base, int degrees)
{
/*
    Build (once per image + base params) the display-referred RGB reference the content-range
    masks measure against, and register it by path so the loupe overlay and the off-thread render
    sample the identical map. It is the developed GLOBAL scope only (Global params + OutputTransform +
    EXIF orientation), capped in size -- range selection does not need full resolution, and base-
    only keeps a range mask from feeding back on its own selection. Cheap no-op when already
    current (keyed on path + a base-params signature), so range-slider drags and colour samples
    never rebuild it -- they only re-threshold the cached reference.
*/
    if (G::isLogger) G::log("MW::ensureRangeRef");
    const QByteArray key =
        QJsonDocument(EditStack::paramsToJson(base)).toJson(QJsonDocument::Compact);
    if (fPath == developRangeRefPath && key == developRangeRefBaseKey && RangeMask::getRef(fPath))
        return;                                       // already current

    /* A rebuild MOVES what every content-range component resolves to, without changing
       any component's signature -- the one thing the fold-prefix cache cannot see. */
    maskFoldCacheClear();

    const WorkingImage small = WorkingImageCache::downscaled(work, 1024);
    int fw = small.width, fh = small.height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    const QImage img = developComposite(small, base, degrees, /*fullRes*/true, fw, fh);
    if (img.isNull()) return;
    const QImage rgb = img.convertToFormat(QImage::Format_ARGB32);

    auto r = std::make_shared<RangeMask::RangeRef>();
    r->w = rgb.width(); r->h = rgb.height();
    r->rgb.resize(size_t(r->w) * size_t(r->h) * 3);
    for (int y = 0; y < r->h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
        float *dst = r->rgb.data() + size_t(y) * size_t(r->w) * 3;
        for (int x = 0; x < r->w; ++x) {
            const QRgb p = line[x];
            dst[x*3+0] = float(qRed(p))   / 255.0f;
            dst[x*3+1] = float(qGreen(p)) / 255.0f;
            dst[x*3+2] = float(qBlue(p))  / 255.0f;
        }
    }
    RangeMask::putRef(fPath, r);
    developRangeRefPath = fPath;
    developRangeRefBaseKey = key;
}

void MW::ensureSubjectMask(const QString &fPath, const WorkingImage &work,
                           const EditParams &base, int degrees)
{
/*
    Build (once per image) the U^2-Net saliency map the "Select Subject" mask samples, and register
    it by path so the loupe overlay and the off-thread render sample the identical coverage. The
    model sees the developed GLOBAL scope (downscaled, output-oriented) -- the same reference the range
    masks use -- so the saliency lines up with what the user sees. Cached by path only: subject
    detection does not depend on the develop sliders, so slider drags never re-run inference (a cheap
    no-op once the map exists). Inference is synchronous on the GUI thread (~200-400ms, one-shot per
    image on the user's add/select action) with a busy cursor.
*/
    if (G::isLogger) G::log("MW::ensureSubjectMask");
    if (fPath == developSubjectRefPath && SubjectMask::getRef(fPath)) return;   // already current

    /* Lazily load u2net.onnx (from the executable dir, next to focus_point_model.onnx). */
    if (!subjectPredictor) {
        const QString modelPath =
            QDir(QCoreApplication::applicationDirPath()).filePath("u2net.onnx");
        subjectPredictor = new SubjectPredictor(modelPath, 320);
        if (!subjectPredictor->isLoaded())
            qWarning("Select Subject: u2net.onnx not found or failed to load at %s",
                     modelPath.toUtf8().constData());
    }
    if (!subjectPredictor->isLoaded()) return;

    const WorkingImage small = WorkingImageCache::downscaled(work, 1024);
    int fw = small.width, fh = small.height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    const QImage img = developComposite(small, base, degrees, /*fullRes*/true, fw, fh);
    if (img.isNull()) return;

    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    auto r = std::make_shared<SubjectMask::SubjectRef>();
    const bool ok = subjectPredictor->predict(img, r->cov, r->w, r->h);
    QGuiApplication::restoreOverrideCursor();
    if (!ok || !r->valid()) return;

    SubjectMask::putRef(fPath, r);
    developSubjectRefPath = fPath;
}

void MW::ensureSkyMask(const QString &fPath, const WorkingImage &work,
                       const EditParams &base, int degrees)
{
/*
    Sky twin of ensureSubjectMask: build (once per image) the sky coverage the "Select Sky" mask
    samples and register it by path, from the developed GLOBAL scope (downscaled, output-oriented) so
    it lines up with what the user sees. Cached by path only; synchronous on the GUI thread with a
    busy cursor. Lazily loads skyseg.onnx.
*/
    if (G::isLogger) G::log("MW::ensureSkyMask");
    if (fPath == developSkyRefPath && SkyMask::getRef(fPath)) return;      // already current

    if (!skyPredictor) {
        const QString modelPath =
            QDir(QCoreApplication::applicationDirPath()).filePath("skyseg.onnx");
        skyPredictor = new SkyPredictor(modelPath, 320);
        if (!skyPredictor->isLoaded())
            qWarning("Select Sky: skyseg.onnx not found or failed to load at %s",
                     modelPath.toUtf8().constData());
    }
    if (!skyPredictor->isLoaded()) return;

    const WorkingImage small = WorkingImageCache::downscaled(work, 1024);
    int fw = small.width, fh = small.height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    const QImage img = developComposite(small, base, degrees, /*fullRes*/true, fw, fh);
    if (img.isNull()) return;

    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    auto r = std::make_shared<SkyMask::SkyRef>();
    const bool ok = skyPredictor->predict(img, r->cov, r->w, r->h);
    QGuiApplication::restoreOverrideCursor();
    if (!ok || !r->valid()) return;

    SkyMask::putRef(fPath, r);
    developSkyRefPath = fPath;
}

void MW::ensureDepthMask(const QString &fPath, const WorkingImage &work,
                         const EditParams &base, int degrees)
{
/*
    Depth twin of ensureSkyMask: build (once per image) the MiDaS depth field the "Depth Range" mask
    bands over, from the developed GLOBAL scope (downscaled, output-oriented). Cached by path only;
    synchronous on the GUI thread with a busy cursor. Lazily loads midas.onnx.
*/
    if (G::isLogger) G::log("MW::ensureDepthMask");
    if (fPath == developDepthRefPath && DepthMask::getRef(fPath)) return;      // already current

    if (!depthPredictor) {
        const QString modelPath =
            QDir(QCoreApplication::applicationDirPath()).filePath("midas.onnx");
        depthPredictor = new DepthPredictor(modelPath, 256);
        if (!depthPredictor->isLoaded())
            qWarning("Depth Range: midas.onnx not found or failed to load at %s",
                     modelPath.toUtf8().constData());
    }
    if (!depthPredictor->isLoaded()) return;

    const WorkingImage small = WorkingImageCache::downscaled(work, 1024);
    int fw = small.width, fh = small.height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    const QImage img = developComposite(small, base, degrees, /*fullRes*/true, fw, fh);
    if (img.isNull()) return;

    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    auto r = std::make_shared<DepthMask::DepthRef>();
    const bool ok = depthPredictor->predict(img, r->depth, r->w, r->h);
    QGuiApplication::restoreOverrideCursor();
    if (!ok || !r->valid()) return;

    DepthMask::putRef(fPath, r);
    developDepthRefPath = fPath;
}

namespace {
/* Rasterize the object PERIMETER brush from paramsJson into a W*H coverage (row-major,
   0/1, output-oriented), then fill the enclosed region -- the silhouette handed to SAM 2
   as a dense prompt. CONTRACT (the perimeter brush UI emits this): {"size":N,"strokes":
   [{"pts":[x,y,...],"size":N,"erase":bool},...]} with x,y output-normalized 0..1. The
   strokes are the traced boundary; fillEnclosed decides closure + fills the interior.
   Returns false (no coverage) unless the perimeter forms a CLOSED loop -- an open trace
   is not a selection yet (mirrors the preview's amber/green signal). Uses the SAME
   BrushStamp rasterize + ObjectMask::fillEnclosed as the overlay so the two agree. */
bool parseObjectBrush(const QString &paramsJson, int W, int H,
                     std::vector<float> &cov, std::vector<float> &band)
{
    if (paramsJson.isEmpty() || W <= 0 || H <= 0) return false;
    const QJsonObject o = QJsonDocument::fromJson(paramsJson.toUtf8()).object();
    const QJsonArray strokes = o.value("strokes").toArray();
    if (strokes.isEmpty()) return false;

    /* Rasterize the perimeter strokes (output-oriented, so degrees = 0). Each stroke is a
       solid dab run (feather 0, flow 100); erase strokes remove from the wall.
       BrushStamp::rasterize reads size/feather/flow/erase per stroke (like Brush). */
    std::vector<float> perim(size_t(W) * size_t(H), 0.0f), scratch;
    BrushStamp::rasterize(strokes, perim.data(), scratch, W, H, /*degrees*/0);

    /* band = the thickened painted wall, handed on as SAM's "unknown" region: the trace
       straddles the boundary, so claiming it as foreground paints the dabs into the mask. */
    std::vector<float> fill;
    const bool closed = ObjectMask::fillEnclosed(perim, W, H, ObjectMask::bridgePx(W, H), fill,
                                                 /*minAreaFrac*/0.0008, &band);
    if (!closed) return false;                 // open perimeter -> no selection yet
    cov = std::move(fill);
    return true;
}

/*
    The traced object's extent in OUTPUT-NORMALIZED coords, padded for context -- the region worth
    encoding at high resolution. Computed straight from the stroke JSON (points expanded by the
    brush radius) rather than by rasterizing, so it is available BEFORE the guide size is chosen.
    fw/fh are the oriented frame dims (any scale; only the aspect is used), because a brush size is
    a percentage of the LONG edge and therefore a different fraction of each axis.
    Returns the full frame if the strokes carry no usable points.
*/
QRectF objectBrushRoi(const QString &paramsJson, int fw, int fh)
{
    const QRectF whole(0.0, 0.0, 1.0, 1.0);
    if (paramsJson.isEmpty() || fw <= 0 || fh <= 0) return whole;
    const QJsonArray strokes = QJsonDocument::fromJson(paramsJson.toUtf8())
                                   .object().value("strokes").toArray();
    const double longE = std::max(fw, fh);
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (const QJsonValue &sv : strokes) {
        const QJsonObject so = sv.toObject();
        const QJsonArray pts = so.value("pts").toArray();
        if (pts.size() < 2) continue;
        /* Dab radius is size/200 of the long edge -- a different normalized amount per axis. */
        const double rN = std::clamp(so.value("size").toDouble(20), 0.0, 100.0) / 200.0;
        const double rx = rN * longE / fw, ry = rN * longE / fh;
        for (int i = 0; i*2 + 1 < pts.size(); ++i) {
            const double px = pts.at(i*2).toDouble(), py = pts.at(i*2 + 1).toDouble();
            x0 = std::min(x0, px - rx); x1 = std::max(x1, px + rx);
            y0 = std::min(y0, py - ry); y1 = std::max(y1, py + ry);
        }
    }
    if (x1 <= x0 || y1 <= y0) return whole;

    /* Context margin: SAM needs to see past the object, and a soft fringe (fur, hair) extends
       beyond the trace. 12% of the ROI's long side, applied equally in PIXELS on both axes. */
    const double marginPx = 0.12 * std::max((x1 - x0) * fw, (y1 - y0) * fh);
    x0 -= marginPx / fw; x1 += marginPx / fw;
    y0 -= marginPx / fh; y1 += marginPx / fh;

    /* Quantize OUTWARD to a coarse grid so extending a trace by a few pixels keeps landing inside
       the already-encoded region -- see ensureObjectEncoder's containment test. Re-encoding costs
       ~1s, and it must not happen on every stroke. */
    const double q = 1.0 / 16.0;
    x0 = std::floor(x0 / q) * q; y0 = std::floor(y0 / q) * q;
    x1 = std::ceil (x1 / q) * q; y1 = std::ceil (y1 / q) * q;
    return QRectF(QPointF(std::max(0.0, x0), std::max(0.0, y0)),
                  QPointF(std::min(1.0, x1), std::min(1.0, y1)));
}

/* Extract `crop` (pixels within a gw*gh map) from src into dst. Used to express the brush prompt
   in the encoded crop's frame. An empty src yields an empty dst (the band is optional). */
void cropCoverage(const std::vector<float> &src, int gw, int gh, const QRect &crop,
                  std::vector<float> &dst)
{
    dst.clear();
    if (src.size() != size_t(gw) * size_t(gh) || crop.isEmpty()) return;
    const int cw = crop.width(), ch = crop.height();
    dst.resize(size_t(cw) * size_t(ch));
    for (int y = 0; y < ch; ++y) {
        const float *sp = src.data() + size_t(y + crop.y()) * gw + crop.x();
        std::copy(sp, sp + cw, dst.data() + size_t(y) * cw);
    }
}
} // namespace

void MW::ensureObjectMask(const QString &fPath, const WorkingImage &work,
                          const EditParams &base, int degrees, const QString &paramsJson)
{
/*
    Build the SAM 2 "Object Mask" coverage for one brush component. TWO-PHASE (unlike the other AI
    masks): the heavy encoder runs ONCE per image (cached in objectMaskPredictor, keyed by path --
    the ~1s cost), then the light decoder runs PER brush edit (~40ms), refining the painted-and-
    filled stroke to the object edge. The result is registered under objectRefKey(path, brush) so it
    is a no-op once decoded and several object masks per image coexist. Synchronous on the GUI thread
    with a busy cursor. Lazily loads sam2_encoder/decoder.onnx (the decoder MUST be the fixed-shape
    export -- see ObjectMaskPredictor). A component with no brush yet just warms the encoder.
*/
    if (G::isLogger) G::log("MW::ensureObjectMask");
    const QString refKey = objectRefKey(fPath, paramsJson);
    if (ObjectMask::getRef(refKey)) return;                 // this brush already decoded

    /* The traced region -- what gets encoded, and at what resolution. Only the aspect of the
       oriented frame is needed here, so the un-downscaled work dims will do. */
    int fw = work.width, fh = work.height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    const QRectF roi = objectBrushRoi(paramsJson, fw, fh);

    /* Phase 1: lazily load the predictor + encode the ROI (cached; re-encoded only when the
       trace outgrows the encoded region). */
    int gw, gh; QRect crop;
    if (!ensureObjectEncoder(fPath, work, base, degrees, roi, gw, gh, crop)) return;

    /* Phase 2: decode the brush stroke. No stroke yet -> encoder is warmed, nothing to register. */
    std::vector<float> fillCov, bandCov;
    if (!parseObjectBrush(paramsJson, gw, gh, fillCov, bandCov)) return;

    /* The prompt must be expressed in the CROP's frame, since that is what was encoded. */
    std::vector<float> fillCrop, bandCrop;
    cropCoverage(fillCov, gw, gh, crop, fillCrop);
    cropCoverage(bandCov, gw, gh, crop, bandCrop);

    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    auto r = std::make_shared<ObjectMask::ObjectRef>();
    const bool ok = objectMaskPredictor->refine(fillCrop, bandCrop, crop.width(), crop.height(),
                                                r->cov, r->w, r->h);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) return;

    /* Store crop-local: the ref carries the crop's normalized extent and reads 0 outside it, so a
       tightly-traced object keeps its full decoded resolution instead of being flattened into a
       whole-frame map (see ObjectMask::ObjectRef). */
    r->x0 = double(crop.x())     / gw;   r->x1 = double(crop.x() + crop.width())  / gw;
    r->y0 = double(crop.y())     / gh;   r->y1 = double(crop.y() + crop.height()) / gh;
    if (!r->valid()) return;

    ObjectMask::putRef(refKey, r);
}

bool MW::ensureObjectEncoder(const QString &fPath, const WorkingImage &work,
                             const EditParams &base, int degrees, const QRectF &normRoi,
                             int &gw, int &gh, QRect &crop)
{
/*
    Phase 1 shared by the Object Mask and the Brush "AI" auto-mask: lazily load the SAM 2 encoder +
    decoder (next to u2net.onnx in the executable dir) and encode the developed base, caching
    image_embed + high_res_feats in objectMaskPredictor. ~1s CPU. Outputs the full oriented guide
    dims (gw,gh -- the space the brush rasterizes in) and the sub-rect of it that was encoded.
    Returns false if the model is missing or the encode failed.

    RESOLUTION. The encoder input is a fixed 1024^2 whatever it is fed, so encoding the WHOLE frame
    spends that budget mostly on background: an object covering a third of the frame gets ~340 px,
    and SAM's 256^2 mask head then resolves it at ~85. That is why the old whole-frame path produced
    blobby edges on fine structure -- the detail was never representable, no refinement downstream
    could recover it. So the guide is scaled such that the caller's ROI lands at ~1024 px on its long
    side and only the ROI is encoded, capped at kMaxGuideLong to bound the develop + matting cost.
    Encoding cost is UNCHANGED (still one 1024^2 forward); only the crop's detail goes up.

    CACHING. Re-encoding is the ~1s step, so it must not fire per stroke. The cached encode is reused
    whenever the requested ROI still fits inside it and is not wildly smaller (which would mean the
    user erased back to a much tighter trace and is now paying for resolution they no longer need).
    objectBrushRoi's outward quantization is what makes a growing trace keep hitting this path.
*/
    if (!objectMaskPredictor) {
        const QDir dir(QCoreApplication::applicationDirPath());
        objectMaskPredictor = new ObjectMaskPredictor(dir.filePath("sam2_encoder.onnx"),
                                                      dir.filePath("sam2_decoder.onnx"), 1024);
        if (!objectMaskPredictor->isLoaded())
            qWarning("Object Mask: sam2_encoder/decoder.onnx not found or failed to load in %s",
                     dir.absolutePath().toUtf8().constData());
    }
    if (!objectMaskPredictor->isLoaded()) return false;

    const QRectF roi = normRoi.isEmpty() ? QRectF(0, 0, 1, 1) : normRoi.intersected(QRectF(0, 0, 1, 1));
    if (roi.isEmpty()) return false;

    /* Reuse the cached encode when it still covers the request (see CACHING above). */
    const bool sameImage = (developObjectImagePath == fPath) && objectMaskPredictor->hasImage();
    const bool covers    = sameImage && developObjectRoi.contains(roi);
    const bool tooCoarse = covers && (developObjectRoi.width() * developObjectRoi.height()
                                      > 2.5 * roi.width() * roi.height());
    if (covers && !tooCoarse) {
        gw = developObjectGuideW; gh = developObjectGuideH; crop = developObjectCrop;
        return true;
    }

    /* Guide long edge that puts the ROI at ~kEncode px on its long side. gw/gh scale linearly with
       it, so one closed-form step is exact. */
    constexpr int kEncode = 1024;            // the encoder's own input size
    constexpr int kMaxGuideLong = 4096;      // bounds the develop + guided-filter cost
    int fw = work.width, fh = work.height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    if (fw <= 0 || fh <= 0) return false;
    const double frameLong = std::max(fw, fh);
    const double q = std::max(roi.width() * fw, roi.height() * fh) / frameLong;   // ROI long / frame long
    const int nativeLong = int(std::max(work.width, work.height));
    const int longEdge = std::clamp(q > 1e-6 ? int(std::lround(kEncode / q)) : kEncode,
                                    kEncode, std::min(kMaxGuideLong, std::max(nativeLong, kEncode)));

    const WorkingImage small = WorkingImageCache::downscaled(work, longEdge);
    gw = small.width; gh = small.height;
    if (degrees == 90 || degrees == 270) std::swap(gw, gh);
    if (gw <= 0 || gh <= 0) return false;

    /* ROI -> guide pixels, rounded OUTWARD so nothing the user traced falls outside. */
    QRect r;
    r.setCoords(int(std::floor(roi.left()  * gw)),     int(std::floor(roi.top()    * gh)),
                int(std::ceil (roi.right() * gw)) - 1, int(std::ceil (roi.bottom() * gh)) - 1);
    crop = r.intersected(QRect(0, 0, gw, gh));
    if (crop.width() < 8 || crop.height() < 8) return false;

    const QImage img = developComposite(small, base, degrees, /*fullRes*/true, gw, gh);
    if (img.isNull()) return false;
    QGuiApplication::setOverrideCursor(Qt::BusyCursor);
    const bool okEnc = objectMaskPredictor->setImage(crop == QRect(0, 0, gw, gh) ? img
                                                                                 : img.copy(crop));
    QGuiApplication::restoreOverrideCursor();
    if (!okEnc) return false;

    developObjectImagePath = fPath;
    developObjectRoi = roi;
    developObjectGuideW = gw; developObjectGuideH = gh;
    developObjectCrop = crop;
    return true;
}

void MW::onAiMaskEditBegin(int tool, int /*op*/, bool /*inverted*/,
                           const QString &paramsJson, double /*feather*/)
{
/*
    A mask tool became active in the dock. For an AI tool (Subject/Background/Sky/Depth), build its
    coverage now (and repaint) so the loupe tint appears immediately on add/select -- the render path
    only builds the ref when a non-identity scope carries the mask, so a just-added mask on an
    unadjusted scope would otherwise show nothing. Cached by path, so re-selecting is a no-op.
*/
    const bool needsSubject = (tool == int(MaskTool::Subject) || tool == int(MaskTool::Background));
    const bool isSky        = (tool == int(MaskTool::Sky));
    const bool isDepth      = (tool == int(MaskTool::Depth));
    const bool isObject     = (tool == int(MaskTool::Object));
    if (!needsSubject && !isSky && !isDepth && !isObject) return;
    const QString fPath = dm->currentFilePath;
    if (fPath.isEmpty()) return;
    auto work = WorkingImageCache::instance().get(fPath);
    if (!work) return;
    const auto mj = developProperties->stackJob();
    const int degrees = work->sceneReferred ? developOrientationDegrees(*work, fPath) : 0;
    if (needsSubject)   ensureSubjectMask(fPath, *work, mj.global, degrees);   // Background = inverted subject
    else if (isSky)     ensureSkyMask(fPath, *work, mj.global, degrees);
    else if (isDepth)   ensureDepthMask(fPath, *work, mj.global, degrees);
    else if (isObject)  ensureObjectMask(fPath, *work, mj.global, degrees, paramsJson);  // warms encoder; decodes if a stroke exists
    imageView->viewport()->update();   // heal the tint now the ref exists
}

void MW::updateMaskOverlayTint()
{
/*
    While a submask is being defined, the loupe shows the WHOLE mask (all the scope's
    submasks composited) as a single-colour coverage veil, under the active submask's
    handles. The veil shows the OUTCOME: the pending submask is composited with the op the
    held modifier is previewing (Opt = Subtract, Shift+Opt = Intersect), so holding Opt
    makes the veil retreat where that submask would be removed. That is why the overlay
    needs only ONE colour (G::maskOverlayColor) -- the op is read off the result, not
    off a colour key. Rebuild when the selection, geometry or previewed op changes (wired to
    maskEditBegin/End + paramsChanged), or clear it when nothing is being edited. The
    composite reuses the render-path buildMaskBuffer, so the veil is pixel-consistent with
    the developed result.
*/
    if (G::isLogger) G::log("MW::updateMaskOverlayTint");
    /* Probe: accumulate into the members the next [DevTime] render line reports and
       resets, so one tick's veil cost -- and how often it ran -- is visible. Started
       after the early-outs below would be wrong: an early-out IS the cheap case, and
       the count is what shows the veil running more than once per render. */
    QElapsedTimer tintProbe;
    if (G::isReportDevelopTime) { tintProbe.start(); ++developTintCount; }
    const auto tintProbeStop = qScopeGuard([this, &tintProbe] {
        if (G::isReportDevelopTime) developTintMs += tintProbe.elapsed();
    });

    /* Mid-stroke on an ADD submask the veil is not drawn at all -- ImageView stands the
       live brush preview in for it (drawMaskOverlay's brushStroking gate) -- so a rebuild
       on every live tick is invisible work on top of that tick's develop render. The
       release re-emits paramsChanged, which rebuilds it from the finished stroke -- and
       ImageView keeps the stand-in up until that rebuilt veil arrives
       (ImageView::maskBrushVeilStale, cleared by setScopeMaskTint), so the coverage does
       not blink back to the pre-stroke veil at mouse-up.
       A SUBTRACT/INTERSECT stroke is the opposite case and must pay for the rebuild: its
       local preview paints coverage in the overlay colour exactly where the mask is being
       REMOVED, which reads as adding. Keeping the real veil live there shows the outcome
       -- the veil retreating under the brush -- matching the develop render. */
    if (imageView->maskStrokeInFlight()
        && developProperties->pendingMaskOp() == int(MaskOp::Add)) return;
    if (!developProperties->maskOverlayActive()) { imageView->clearScopeMaskTint(); return; }
    /* HIDDEN -> do not build it. drawForeground will not paint the veil while it is
       hidden ("O" off, or an adjustment slider pushed it out of the way), so every
       millisecond spent compositing one is wasted -- and at the proxy resolution this
       builds at, that was the largest single item on the GUI thread during a brush drag.
       The stored QImage is deliberately left alone rather than cleared: it is not drawn,
       and re-showing rebuilds it (MW listens to maskTintVisibilityChanged), so there is
       no flash of an empty overlay in between. */
    if (!imageView->maskTintVisible()) return;

    const QString fPath = dm->currentFilePath;
    if (fPath.isEmpty() || currentIsVideo()) { imageView->clearScopeMaskTint(); return; }
    auto work = WorkingImageCache::instance().get(fPath);
    if (!work) { imageView->clearScopeMaskTint(); return; }
    const QVector<MaskComponent> components = developProperties->activeScopeComponents();
    if (components.isEmpty()) { imageView->clearScopeMaskTint(); return; }

    /* DISABLED submasks (the submask list's row checkbox, MaskComponent::enabled) are
       skipped by the render (DevelopProperties::stackJob drops them before the job is
       built), so the veil has to skip them too -- it shows the OUTCOME, and coverage the
       develop result does not have is a lie. buildMaskBuffer composites whatever vector
       it is handed and never reads the flag, so the filtering happens here.

       The in-progress (uncommitted) submask is a real component in `components`, but its
       op is still momentary -- it lives in DevelopProperties::pendingMaskOp() (driven by
       the held modifier) and is only written into the component on commit. Substitute it
       here so the veil composites the OUTCOME of what Return / the commit button would
       do. Its index is against `components`, so it is remapped onto the filtered vector
       (and drops out entirely if the pending submask is itself unchecked -- it then
       contributes nothing, so there is no footprint ring or op chip to show for it). */
    const int pendSrcIdx = developProperties->pendingMaskIndex();
    QVector<MaskComponent> masks;
    masks.reserve(components.size());
    int pendIdx = -1;
    for (int i = 0; i < components.size(); ++i) {
        if (!components.at(i).enabled) continue;
        if (i == pendSrcIdx) pendIdx = masks.size();
        masks.append(components.at(i));
    }
    if (masks.isEmpty()) { imageView->clearScopeMaskTint(); return; }
    const bool hasPending = (pendIdx >= 0);
    if (hasPending) masks[pendIdx].op = developProperties->pendingMaskOp();

    const int degrees = work->sceneReferred ? developOrientationDegrees(*work, fPath) : 0;
    const EditParams base = developProperties->stackJob().global;

    /* Build any content/AI references the scope's tools sample (cached; no-op if already
       present), so the composite is non-zero even for a just-added mask on an unadjusted
       scope. Driven off the FILTERED list: a disabled submask is not composited, so
       building its (costly, AI) reference would be waste -- re-checking it rebuilds on
       the next tick. */
    bool needRange = false, needSubject = false, needSky = false, needDepth = false;
    for (const MaskComponent &m : masks) {
        if      (m.tool == int(MaskTool::ColorRange) || m.tool == int(MaskTool::LuminanceRange)) needRange = true;
        else if (m.tool == int(MaskTool::Subject)    || m.tool == int(MaskTool::Background))     needSubject = true;
        else if (m.tool == int(MaskTool::Sky))        needSky = true;
        else if (m.tool == int(MaskTool::Depth))      needDepth = true;
    }
    if (needRange)   ensureRangeRef(fPath, *work, base, degrees);
    if (needSubject) ensureSubjectMask(fPath, *work, base, degrees);
    if (needSky)     ensureSkyMask(fPath, *work, base, degrees);
    if (needDepth)   ensureDepthMask(fPath, *work, base, degrees);
    /* Object components are per-brush, so build each one from its own component params. */
    for (const MaskComponent &m : masks)
        if (m.tool == int(MaskTool::Object))
            ensureObjectMask(fPath, *work, base, degrees, m.paramsJson);

    /* Composite at a capped resolution (geometry is normalized, so any size is faithful) -- the tint
       is smooth-scaled onto the image, so a couple of MP is ample and keeps live drags cheap. */
    int mw = work->width, mh = work->height;
    if (mw <= 0 || mh <= 0) { imageView->clearScopeMaskTint(); return; }
    int bw, bh;
    /* This buffer's long edge over the FULL-RESOLUTION long edge -- what turns an Edge
       slider value (full-res pixels) into pixels of this buffer. */
    double edgeScale = 1.0;
    if (developProxy && developProxyPath == fPath && developProxy->isValid()) {
        /* Build at the PROXY's exact dimensions, not an independent cap. The
           per-component brush rasters are cached by (paramsJson, dims, degrees), so
           matching the render's dimensions means the veil reuses the buffers the render
           just built, instead of rasterizing a near-identical second set at 1600px --
           which was why `tint` cost about as much as `mask` again on every tick. */
        bw = developProxy->width;
        bh = developProxy->height;
        /* renderScale is COMPOUNDING -- it is measured against the ORIGINAL full res, not
           against `work` -- so take the proxy's own value rather than deriving bw/mw, or
           the veil's Edge would disagree with the render's whenever `work` is itself
           capped. */
        edgeScale = double(developProxy->renderScale);
    }
    else {
        const int cap = 1600;
        const double sc = qMin(1.0, double(cap) / qMax(mw, mh));
        bw = qMax(1, int(mw * sc));
        bh = qMax(1, int(mh * sc));
        edgeScale = sc * double(work->renderScale);
    }
    /* The veil has to show the SAME mask the render composites, so it needs the same halo
       guide -- on ITS grid. That is only free on the proxy branch above, where bw/bh are
       the proxy's own dimensions. On the capped fallback (no proxy for this file yet)
       there is no working image at bw/bh to build a guide from, so the veil draws the
       unrefined mask for the moment before the proxy lands rather than a guide resampled
       from the wrong grid. */
    std::vector<float> veilGuide;
    const float veilHalo = developProperties->activeScopeMaskHalo();
    if (veilHalo >= MaskHalo::kMinAmount && developProxy
        && developProxyPath == fPath && developProxy->isValid()
        && developProxy->width == bw && developProxy->height == bh)
        buildHaloGuide(*developProxy, veilGuide);

    MaskBuildStats tintStats;
    const std::vector<float> buf =
        buildMaskBuffer(masks, bw, bh, degrees, fPath, nullptr,
                        G::isReportDevelopTime ? &tintStats : nullptr,
                        edgeScale, developProperties->activeScopeMaskEdge(),
                        veilHalo, veilGuide.empty() ? nullptr : &veilGuide);

    /* RESULT VEIL: the whole-mask composite as a flat coverage tint in the one overlay
       colour, alpha by coverage. This is the TRUE resulting mask -- Subtract holes stay
       holes -- so the veil alone answers both "what does this scope affect?" and (with a
       modifier held) "what would this op do?". */
    QImage tint(bw, bh, QImage::Format_ARGB32_Premultiplied);
    const int maxA = 150;             // full-coverage alpha
    const QColor ovc = G::maskOverlayColor;
    const int ovR = ovc.red(), ovG = ovc.green(), ovB = ovc.blue();
    /* Row-parallel, and the scanline base is taken ONCE: this runs at the proxy's full
       resolution (that is what lets it share the render's brush rasters), so it is a
       2 MP per-pixel loop on the GUI thread during a drag -- it was measured at ~46 ms
       of the veil's 87. QImage::scanLine() is the detaching accessor and races if called
       from several threads, hence bits()/bytesPerLine() hoisted out. */
    uchar *const tintBits0 = tint.bits();
    const qsizetype tintBpl0 = tint.bytesPerLine();
    developParallelRows(bw, bh, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            QRgb *row = reinterpret_cast<QRgb*>(tintBits0 + qsizetype(y) * tintBpl0);
            const float *mrow = buf.data() + size_t(y) * bw;
            for (int x = 0; x < bw; ++x) {
                const int a = int(qBound(0.0f, mrow[x], 1.0f) * maxA + 0.5f);
                row[x] = a ? qRgba(ovR * a / 255, ovG * a / 255, ovB * a / 255, a) : 0;
            }
        }
    });

    /* PENDING FOOTPRINT: a thin ring around the submask being defined, painted only where
       the result is ABSENT. Without it a Subtract (or an Intersect) whose footprint lies
       outside the mask would change nothing on screen, leaving the user unable to see
       where the submask actually is. The veil WINS everywhere it covers, so coverage that
       survives into the result reads as veil, not as an edge -- which also keeps a noisy
       content submask (Color/Luminance Range), whose per-pixel "edges" are everywhere,
       from fogging the subject. */
    if (hasPending) {
        const float gateT = 0.12f;   // result present -> the veil wins, skip the ring
        const float T = 0.5f;
        MaskComponent c = masks[pendIdx];
        c.op = int(MaskOp::Add);                 // ring the footprint, not the sign
        /* The submask's OWN footprint, so the mask-level Edge is deliberately NOT passed:
           the ring must track the submask being dragged, not the assembled mask. The
           submask's own Edge travels inside `c` and does apply. */
        const std::vector<float> cov =
            buildMaskBuffer({c}, bw, bh, degrees, fPath, nullptr,
                            G::isReportDevelopTime ? &tintStats : nullptr,
                            edgeScale, 0.0f);
        const int a = 235, thick = 1;
        /* Row-parallel: each band writes only its own scanlines and reads cov (const),
           so the y+-thick neighbour lookups cross band boundaries safely. The scanline
           base is taken ONCE here: QImage::scanLine() is the detaching (non-const)
           accessor, and calling it from several threads races on detach_no. */
        uchar *const tintBits = tint.bits();
        const qsizetype tintBpl = tint.bytesPerLine();
        developParallelRows(bw, bh, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                QRgb *row = reinterpret_cast<QRgb*>(tintBits + qsizetype(y) * tintBpl);
                for (int x = 0; x < bw; ++x) {
                    const size_t idx = size_t(y) * bw + x;
                    if (cov[idx] < T || buf[idx] > gateT) continue;   // absent/in result
                    bool edge = false;
                    for (int dy = -thick; dy <= thick && !edge; ++dy)
                        for (int dx = -thick; dx <= thick; ++dx) {
                            const int nx = x + dx, ny = y + dy;
                            if (nx < 0 || ny < 0 || nx >= bw || ny >= bh ||
                                cov[size_t(ny) * bw + nx] < T) { edge = true; break; }
                        }
                    if (!edge) continue;
                    row[x] = qRgba(ovR * a / 255, ovG * a / 255, ovB * a / 255, a);
                }
            }
        });
    }
    if (degrees != 0) tint = tint.transformed(QTransform().rotate(degrees));
    imageView->setScopeMaskTint(tint);

    /* Op indicator: tell ImageView which submask is being defined and which op the
       veil previews, so the on-canvas chip can name it (see ImageView::setMaskLegend). The
       modifier hint is suppressed on the first submask -- nothing to combine with. */
    if (G::isReportDevelopTime)
        qDebug().noquote() << "[DevTime] tint " << bw << "x" << bh
                           << " raster" << tintStats.rasterMs
                           << tintStats.strokes << "strokes"
                           << " setup" << tintStats.setupMs
                           << " fold" << tintStats.foldMs << tintStats.comps << "comps"
                           << " morph" << tintStats.morphMs << tintStats.morphs;

    if (hasPending)
        imageView->setMaskLegend(DevelopProperties::maskToolName(masks[pendIdx].tool),
                                 developProperties->pendingMaskOp(), pendIdx > 0);
    else
        imageView->setMaskLegend(QString(), -1, false);
}

void MW::updateSharpenMaskPreview()
{
/*
    SHARPENING MASK PREVIEW, as Lightroom's Alt-drag does it. Sharpening's Masking slider
    gates the effect by local edge strength; which pixels survive that gate cannot be seen
    in the result without hunting around at 1:1. Holding Opt while dragging Masking
    replaces the photo with the gate itself in GRAYSCALE -- white is sharpened, black is
    protected -- so the slider can be set by looking at the whole frame. It lives only for
    the duration of the drag (see DevelopProperties::sharpenMaskPreviewActive), which is
    what lets it be this loud.

    BUILT FROM THE FRAME ON SCREEN, not from the render. The true gate lives inside
    Develop::Sharpen, several stages deep in a worker-thread stack render, and plumbing a
    buffer back out of it would cost a pipeline seam and land a tick late. The displayed
    frame is the same image one op later, so the same blur + Sobel + Sharpen::edgeGate over
    its luma reproduces the gate closely enough to set the slider by -- and it is always in
    step with what the user is looking at. Two known approximations, neither material at the
    job this does: the frame carries sharpening (and grain, if any) that the real gate is
    measured before, and its perceptual luma is sRGB-encoded gray rather than the
    pipeline's own encode.

    The result is in DISPLAYED space (post-geometry, oriented) because its source is, so
    ImageView draws it straight over the pixmap -- the one Develop overlay that must NOT go
    through maskNormToItem.
*/
    if (G::isLogger) G::log("MW::updateSharpenMaskPreview");
    if (!imageView) return;
    if (!developProperties || !developProperties->sharpenMaskPreviewActive()
        || developFrame.isNull() || !dm || developFramePath != dm->currentFilePath) {
        imageView->clearSharpenMaskImage();
        return;
    }

    const QImage frame = developFrame;
    const int w = frame.width(), h = frame.height();
    if (w < 3 || h < 3) { imageView->clearSharpenMaskImage(); return; }

    /* The gate's knee is expressed per FULL-RESOLUTION pixel, so it scales with this
       frame's render scale exactly as Develop::Sharpen scales it for a proxy (the one
       scale-dependent op -- see Develop/sharpen.h). */
    float scale = 1.0f;
    if (auto work = WorkingImageCache::instance().get(developFramePath)) {
        const int degrees = work->sceneReferred
                                ? developOrientationDegrees(*work, developFramePath) : 0;
        int fw = work->width, fh = work->height;
        if (degrees == 90 || degrees == 270) std::swap(fw, fh);
        const int fullEdge = qMax(fw, fh);
        if (fullEdge > 0) scale = float(qMax(w, h)) / float(fullEdge);
    }
    scale = qBound(0.05f, scale, 1.0f);

    const EditParams p = developProperties->editParams();
    const float masking = qBound(0.0f, p.sharpenMasking, 1.0f);
    const float sigma   = Sharpen::effectiveSigma(p.sharpenRadius, scale);

    /* Perceptual luma of the displayed frame, blurred exactly as the op blurs it, then
       the 1/8-normalised Sobel pair the gate reads (see Develop::Sharpen for why the
       gradient is taken on the BLURRED luma and why the Sobel gain is divided out). */
    QImage gray = frame.convertToFormat(QImage::Format_Grayscale8);
    cv::Mat g8(h, w, CV_8UC1, gray.bits(), size_t(gray.bytesPerLine()));
    cv::Mat yp;
    g8.convertTo(yp, CV_32F, 1.0 / 255.0);
    cv::Mat base;
    cv::GaussianBlur(yp, base, cv::Size(0, 0), sigma);
    constexpr double kSobelNorm = 1.0 / 8.0;
    cv::Mat gx, gy, grad;
    cv::Sobel(base, gx, CV_32F, 1, 0, 3, kSobelNorm);
    cv::Sobel(base, gy, CV_32F, 0, 1, 3, kSobelNorm);
    cv::magnitude(gx, gy, grad);

    /* The gate straight out as 8-bit gray: 255 = full sharpening, 0 = fully protected.
       Grayscale8 rather than a tint, so the preview IS the mask (Lightroom's reading) and
       the intermediate greys show the soft roll-off rather than hiding it. */
    QImage mask(w, h, QImage::Format_Grayscale8);
    const float *gp = grad.ptr<float>();
    uchar *const bits = mask.bits();
    const qsizetype bpl = mask.bytesPerLine();
    developParallelRows(w, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            uchar *row = bits + qsizetype(y) * bpl;
            const float *grow = gp + size_t(y) * w;
            for (int x = 0; x < w; ++x) {
                const float gate = Sharpen::edgeGate(grow[x], masking, scale);
                row[x] = uchar(qBound(0.0f, gate, 1.0f) * 255.0f + 0.5f);
            }
        }
    });
    imageView->setSharpenMaskImage(mask);
}

void MW::syncPendingMaskOp()
{
/*
    The op modifiers are MOMENTARY: Opt previews Subtract, Shift+Opt previews Intersect,
    neither is Add, and nothing is written into the submask until it is committed. Read
    the live modifier state and push it to DevelopProperties, which relabels the button
    and re-composites the veil.

    Opt is skipped while a brush/object stroke is in flight, where it already means "erase
    from this stroke" -- the one rule that keeps the two meanings apart: Opt takes away
    from the STROKE while painting, from the MASK otherwise.
*/
    if (!developProperties || !developProperties->isMaskPanelOpen()) return;
    /* An ERASE stroke owns Opt, so the previewed op drops back to Add for its duration
       (the button and chip say "Update", not "Subtract", while erasing). A plain paint
       stroke does not -- including an Opt stroke on an empty submask, which paints the
       area the user wants SUBTRACTED and must keep previewing that. */
    if (imageView && imageView->maskStrokeIsErase()) {
        developProperties->setPendingMaskOp(int(MaskOp::Add));
        return;
    }
    developProperties->setPendingMaskOp(DevelopProperties::maskOpFromModifiers());
}

void MW::renderDevelopPreview(bool fullRes)
{
/*
    Re-render the current image through Develop + OutputTransform and push the result straight
    to the loupe, using the WorkingImage-cache hot path: keep the cached pre-develop scene-
    linear image and re-run only the cheap develop stage -- no decode, no demosaic, no file read.

    fullRes=false (interactive drag) renders a screen-resolution PROXY (cached per path) and
    upscales it to the displayed dimensions, so a 50MP RAW costs a few MP of work per tick.
    fullRes=true (drag settled) renders the full image once for a crisp result.

    PHASE 1 (Develop ops): this is a live, in-session preview. The edit is NOT yet persisted
    or written back to the image cache, so navigating away and back shows the un-developed
    image. Per-image persistence (sidecar) and the scope/mask stack land in later phases.
    See notes/Documentation.txt "Scope & masking model".
*/
    if (G::isLogger) G::log("MW::renderDevelopPreview");

    const QString fPath = dm->currentFilePath;
    if (fPath.isEmpty()) return;
    if (currentIsVideo()) return;               // Develop operates on stills, not videos

    auto mj = developProperties->stackJob();   // full scope stack (independent of active scope)
    /* While the crop tool is active, suppress only the CROP (the overlay sets it, applied on commit)
       but KEEP the warp/straighten: before Rectify there is none (full frame); after Rectify the
       stored quad warps the frame so the crop overlay can be placed on the corrected canvas. */
    if (developCropEditing && !developCropShowResult) {
        mj.geometry.cropX = 0.0; mj.geometry.cropY = 0.0;
        mj.geometry.cropW = 1.0; mj.geometry.cropH = 1.0;
    }

    /* Reuse the cached pre-develop (full-res) WorkingImage. RAW always caches it at decode;
       non-raw caches it the first time it is developed. */
    auto work = WorkingImageCache::instance().get(fPath);
    const bool wantRaw = isFileRaw(fPath) && G::useRaw;
    /* Develop needs the SCENE-LINEAR raw WorkingImage, not the tone-mapped 8-bit display image. It
       may be absent (evicted from the small WorkingImageCache during Preview read-ahead) OR a prior
       develop-miss may have cached a display-referred one under this path. Decode it OFF the GUI
       thread (ensureDevelopWork) and bail -- doing it inline froze the first slider drag by ~1s on a
       50MP RAW. The completion re-renders; until then the loupe keeps showing the current image.
       developWorkTriedPath guards a display-referred format (no scene-linear result possible): after
       one async attempt we fall through and render the display-referred fallback below, not loop. */
    if (wantRaw && (!work || !work->sceneReferred) && developWorkTriedPath != fPath) {
        ensureDevelopWork(fPath);
        return;
    }
    if (!work) {
        /* Non-raw (JPG/TIFF/HEIC), or the raw re-decode failed: build the pre-develop WorkingImage
           from the decoded display image. Correct for display-referred files; a last resort for raw. */
        if (!icd->contains(fPath)) return;          // not decoded yet; nothing to preview
        const QImage src = icd->imCache.value(fPath);
        if (src.isNull()) return;
        auto built = std::make_shared<WorkingImage>();
        InputTransform input;
        if (!input.FromImage(src, *built)) return;
        WorkingImageCache::instance().put(fPath, built);
        work = built;
    }

    /* Global image for the render: the raw-DENOISED WorkingImage when the Global scope's "Denoise raw"
       is set and ready, else the clean cached image. The heavy denoise runs on settle (see
       renderDevelopFullResAsync -> ensureRawDenoise), so a slider tick never blocks on it. */
    const std::shared_ptr<const WorkingImage> base = developRawDenoisedBase(fPath, mj.global, work);

    QElapsedTimer probe;
    if (G::isReportDevelopTime) probe.start();
    qint64 tProxy = 0;

    /* Pick the render source: full-res for the settled render, else the screen-resolution proxy
       (built once per image, sized a little over the loupe viewport so a modest zoom still
       looks reasonable mid-drag). */
    const WorkingImage *srcImg = base.get();
    if (!fullRes) {
        if (developProxyPath != fPath || !developProxy) {
            const QSize vp = imageView->viewport()->size();
            const int target = qMax(800, qMax(vp.width(), vp.height()) * 3 / 2);
            developProxy = std::make_shared<WorkingImage>(
                WorkingImageCache::downscaled(*base, target));
            developProxyPath = fPath;
            developStackCache.clear();     // new pixels: cached masks are the wrong size
            if (G::isReportDevelopTime) tProxy = probe.restart();
        }
        srcImg = developProxy.get();
    }

    /* Orientation must be computed here (GUI thread): it reads the sort/filter model. */
    const int degrees = work->sceneReferred ? developOrientationDegrees(*work, fPath) : 0;
    int fw = work->width, fh = work->height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);

    /* Content-range masks need the display-referred base reference in place before the composite
       (built from the FULL-res work so it is proxy-independent; cached, so this is a no-op unless
       the base params or image changed). */
    if (stackHasRangeMask(mj)) ensureRangeRef(fPath, *work, mj.global, degrees);
    if (stackHasSubjectMask(mj)) ensureSubjectMask(fPath, *work, mj.global, degrees);
    if (stackHasSkyMask(mj)) ensureSkyMask(fPath, *work, mj.global, degrees);
    if (stackHasDepthMask(mj)) ensureDepthMask(fPath, *work, mj.global, degrees);
    if (stackHasObjectMask(mj))
        for (const DevelopProperties::StackRenderJob::Scope &L : mj.scopes)
            for (const MaskComponent &m : L.components)
                if (m.tool == int(MaskTool::Object))
                    ensureObjectMask(fPath, *work, mj.global, degrees, m.paramsJson);
    /* Brush luminance auto-mask: guarantee the guide is registered (built from the loupe,
       the same one the overlay samples) so proxy and settle confine identically -- else a
       guideless proxy paints the full brush and the settle snaps it to the band. */
    if (stackHasAutoMaskBrush(mj)) imageView->ensureAutoGuide();

    /* ---- Hand the composite to the worker ----
       Everything above had to happen here: the orientation reads the sort/filter model,
       ensureAutoGuide reads the loupe pixmap, and the ensure*Mask builders run inference
       and touch MW state. Everything below is pure pixel work over a const WorkingImage
       and value-copied params, so it runs off the GUI thread -- which is the whole point:
       the brush CURSOR is painted by ImageView on this thread, so any millisecond spent
       compositing here is a millisecond the cursor cannot move. */
    if (developProxyInFlight) {
        /* One render at a time. Note the request rather than dropping it: the completion
           re-arms, so the newest state always gets rendered. Relying on developParamsGen
           instead would silently lose the crop/warp callers, which re-render without
           bumping it.

           THIS MUST STAY ABOVE THE CACHE RESET BELOW. It used to sit after it, so every
           mouse-move that arrived mid-render still called reset() -- wiping the per-scope
           masks out from under the worker that was using them, then returning here. A
           drag delivers an event about every 8 ms against a ~120 ms render, so the cache
           was cleared a dozen times per render and NO mask ever survived: [DevTime] read
           `cacheKept` (the reset that tick was a no-op) while still rebuilding the mask
           every tick, and the discarded buffer's last reference died in the compositor,
           which is what COMPOSEOTHER was measuring. */
        developProxyPending = true;
        return;
    }
    developProxyInFlight = true;

    /* Per-scope mask cache, PROXY ONLY: the full-res path renders once on settle, and
       its buffers are ~w*h*4 bytes, so it recomputes rather than caching. reset() drops
       every slot when the image, the proxy size or the orientation changes. The GLOBAL
       params are NOT part of that key: only a content-range mask's coverage depends on
       them, and that dependency is carried per scope in the mask key instead (see
       scopeMaskDependsOnBase). The base signature is still needed here, for the hot
       prefix/layer, which ARE a function of the base. */
    DevelopStackCache *cache = nullptr;
    bool cacheSurvived = false;             // [DevTime] probe only
    QByteArray baseKey;
    if (!fullRes) {
        baseKey = QJsonDocument(EditStack::paramsToJson(mj.global))
                      .toJson(QJsonDocument::Compact);
        cacheSurvived = developStackCache.reset(fPath, srcImg->width, srcImg->height,
                                                degrees);
        cache = &developStackCache;
    }

    if (G::isReportDevelopTime) {         // reset the dab counters for this tick
        BrushStamp::dabStats().dabs.store(0, std::memory_order_relaxed);
        BrushStamp::dabStats().pixels.store(0, std::memory_order_relaxed);
    }


    /* Captured by value / shared_ptr so the worker cannot be left holding a dangling
       reference if the GUI thread moves on: `proxySrc` keeps the pixels alive even if
       developProxy is reset, and mj/fPath are copies. */
    const std::shared_ptr<const WorkingImage> proxySrc =
        fullRes ? base : std::static_pointer_cast<const WorkingImage>(developProxy);
    const quint64 reqGen = ++developProxyReqGen;
    /* Bump only when the geometry actually MOVES, so a frame superseded by later requests
       that share its geometry is still safe to show (see developProxyGeomGen). */
    auto sameGeom = [](const Geometry &a, const Geometry &b) {
        if (a.cropX != b.cropX || a.cropY != b.cropY ||
            a.cropW != b.cropW || a.cropH != b.cropH ||
            a.straighten != b.straighten || a.hasWarp != b.hasWarp ||
            a.show != b.show) return false;
        if (a.hasWarp)
            for (int i = 0; i < 8; ++i) if (a.quad[i] != b.quad[i]) return false;
        return true;
    };
    if (!developProxyHaveLastGeom || !sameGeom(mj.geometry, developProxyLastGeom)) {
        developProxyLastGeom = mj.geometry;
        developProxyHaveLastGeom = true;
        ++developProxyGeomGen;
    }
    const quint64 geomGen = developProxyGeomGen;
    /* Pair the frame with whether it actually depicts the STORED recipe. Captured here,
       on the GUI thread with the job, because the view overrides it reflects (History
       hover, Transform eye, Replace eye) can change while the render is in flight. */
    const bool faithful = developProperties && developProperties->renderMatchesStoredRecipe();
    /* Only a faithful frame can ever be cached, so only a faithful one pays for the hash. */
    const QByteArray recipe = faithful ? developRecipeKey(fPath) : QByteArray();
    /* Arm the full-resolution settle render for the callers that re-render WITHOUT going
       through developParamsChange -- crop, warp, level and the mask tools. Without this
       developFullFrame is never refreshed for a geometry edit, so it stays at the recipe
       in force before the crop (the provider now declines it, which costs the devPreview
       tier its sensor-resolution source). Restarting an already-armed timer is what the
       slider path does too, so a burst still settles into ONE render. */
    if (!fullRes && faithful && developFullResTimer)
        developFullResTimer->start(kDevelopSettleMs);
    const bool wantTime = G::isReportDevelopTime;
    const qint64 tProxyCap = tProxy;

    /* The Detail panel's 1:1 patch, rendered with this tick so it is live during a drag
       (the whole reason it exists -- see the DetailRoiJob note). The full-res base is
       carried by the same shared_ptr the settle render uses, so the worker cannot outlive
       the pixels it reads. Skipped entirely when the panel is closed or no point has been
       picked, which is the common case. */
    DetailRoiJob roiJob;
    std::shared_ptr<const WorkingImage> roiHold;
    if (!fullRes && developProperties && developProperties->detailPreviewWanted()) {
        const int size = developProperties->detailRoiSize();
        const QPointF n = developProperties->detailPoint();
        if (size > 0) {
            roiHold = base;                     // keeps the full-res pixels alive
            roiJob.wanted = true;
            roiJob.nx = n.x();
            roiJob.ny = n.y();
            roiJob.size = size;
            roiJob.fullSrc = roiHold.get();
        }
    }

    developProxyPool->start([this, proxySrc, mj, degrees, fullRes, fw, fh, fPath, cache,
                            reqGen, geomGen, wantTime, tProxyCap, cacheSurvived,
                            baseKey, faithful, recipe, roiJob, roiHold]() mutable {
        QElapsedTimer wt;
        wt.start();
        WorkingImageCache::RenderTimings rt;
        rt.cacheSurvived = cacheSurvived;
        MaskBuildStats ms;
        QSize displaySize;
        QImage out = developCompositeStack(*proxySrc, mj, degrees, fullRes, fw, fh, fPath,
                                           wantTime ? &rt : nullptr,
                                           WorkingImageCache::OutDepth::Eight,
                                           WorkingImageCache::Space::sRGB, cache,
                                           &displaySize,
                                           wantTime ? &ms : nullptr, baseKey,
                                           roiJob.wanted ? &roiJob : nullptr);
        const QImage roiOut = roiJob.out;
        const qint64 tRender = wt.restart();
        const Geometry appliedGeom = mj.geometry;
        const QSize orientedSize(fw, fh);
        QMetaObject::invokeMethod(this, [this, out, displaySize, fPath, fullRes, reqGen,
                                         geomGen, rt, ms, tProxyCap, tRender, wantTime,
                                         proxySrc, appliedGeom, orientedSize, faithful,
                                         recipe, roiOut] {
            developProxyInFlight = false;
            /* Show it if we are still on this image AND its geometry still applies.
               Being SUPERSEDED is no longer a reason to drop it: a drag delivers events
               far faster than a render completes, so all but the last frame is
               superseded, and dropping them left the loupe frozen until the drag stopped.
               They are only tens of ms old and arrive in order, so showing them is a
               smooth stream rather than a stale one. What must NOT be shown late is a
               frame whose geometry has since changed -- the crop tool alternates
               geometry-applied and geometry-suppressed renders and flashing the wrong one
               reads as a glitch -- and developProxyGeomGen is what catches exactly that.
               The re-arm below still renders the newest state either way. */
            const bool current = (dm && fPath == dm->currentFilePath)
                                 && geomGen == developProxyGeomGen;
            /* Now that every frame is shown, the GUI-thread cost of showing one is on the
               hot path -- QPixmap::fromImage at proxy resolution plus the scope sample --
               so it gets probed like every other stage rather than assumed cheap. */
            qint64 msPreview = 0, msScopes = 0;
            if (current && !out.isNull()) {
                QElapsedTimer pv;
                if (wantTime) pv.start();
                /* Pair the geometry with the pixels it produced, BEFORE they are shown:
                   the overlays map mask coordinates (pre-geometry) onto this frame, so a
                   stale pairing would draw them in the wrong place for one tick. */
                imageView->setDevelopGeometry(appliedGeom, orientedSize);
                imageView->setDevelopPreview(out, displaySize);
                /* Keep the frame: the sharpening mask preview is derived from the pixels
                   on screen, so it must be rebuilt from each one -- a Masking drag with
                   Opt held is exactly the case that has to stay live. */
                developFrame = out;
                developFramePath = fPath;
                developFrameFaithful = faithful;
                developFrameRecipe = recipe;
                /* The 1:1 patch, if this tick rendered one. Pushed with the frame it was
                   rendered alongside, so the preview and the loupe never show different
                   slider values. */
                if (!roiOut.isNull()) developProperties->setDetailRoiImage(roiOut);
                updateSharpenMaskPreview();
                if (wantTime) msPreview = pv.restart();
                updateDevelopScopes(out, /*verifyVsPreview*/fullRes);
                if (wantTime) msScopes = pv.elapsed();
            }
            if (developProxyPending) {
                developProxyPending = false;
                developProxyRenderTimer->start(0);
            }
            if (wantTime) {
                qDebug().noquote() << "[DevTime]" << (fullRes ? "full " : "proxy")
                                   << proxySrc->width << "x" << proxySrc->height
                                   << " proxyBuild" << tProxyCap
                                   << " mask" << rt.maskBuildMs
                                   << "(" << rt.maskScopes << "scopes,"
                                   << BrushStamp::dabStats().dabs
                                          .load(std::memory_order_relaxed)
                                   << "dabs,"
                                   << BrushStamp::dabStats().pixels
                                          .load(std::memory_order_relaxed) / 1000000
                                   << "Mpx) [raster" << ms.rasterMs
                                   << ms.strokes << "strokes"
                                   << " setup" << ms.setupMs
                                   << " fold" << ms.foldMs << ms.comps << "comps"
                                   << " morph" << ms.morphMs << ms.morphs << "]"
                                   << " srcCopy" << rt.copyMs
                                   << " develop" << rt.developMs
                                   << "[copy" << rt.stackCopyMs
                                   << " apply" << rt.stackApplyMs
                                   << " blend" << rt.stackBlendMs
                                   << " free" << rt.stackFreeMs
                                   << " OTHER" << (rt.developMs - rt.stackCopyMs
                                                   - rt.stackApplyMs - rt.stackBlendMs
                                                   - rt.stackFreeMs) << "]"
                                   << " outFree" << rt.outFreeMs
                                   << " resume" << rt.resumeStart
                                   << "/" << rt.stackScopes
                                   << (rt.cacheSurvived ? " cacheKept" : " cacheWiped")
                                   << (rt.hadPrefix ? " prefix" : " noPrefix")
                                   << (rt.hadLayer ? "+layer" : "")
                                   << " toImage" << rt.toImageMs
                                   << " orient+scale" << rt.orientScaleMs
                                   << " setHot" << rt.setHotMs
                                   << " worker" << tRender
                                   << " COMPOSEOTHER" << (tRender - rt.maskBuildMs
                                                          - rt.copyMs - rt.developMs
                                                          - rt.toImageMs - rt.outFreeMs
                                                          - rt.orientScaleMs
                                                          - rt.setHotMs)
                                   << " applied" << (current ? 1 : 0)
                                   << " superseded" << (reqGen != developProxyReqGen)
                                   << " preview" << msPreview
                                   << " scopes" << msScopes
                                   << " tint" << developTintMs << "x" << developTintCount
                                   << "ms";
                developTintMs = 0;
                developTintCount = 0;
            }
        });
    });
}

int MW::developOrientationDegrees(const WorkingImage &work, const QString &fPath) const
{
/*
    EXIF rotation (degrees) to apply to a scene-referred (RAW) render so it matches the loupe.
    The RAW decoder hands back a SENSOR-NATIVE (unrotated) WorkingImage and ImageDecoder::run()
    rotates the display image afterwards via rotate(); a non-raw work is built from the already-
    rotated display image, so display-referred renders need no rotation. Reads the sort/filter
    model, so it MUST run on the GUI thread. (PHASE 1: mirrors ImageDecoder::rotate(); fold into a
    shared helper when the edit pipeline is unified.)
*/
    Q_UNUSED(work)
    int degrees = 0;
    const int sfRow = dm->proxyRowFromPath(fPath);
    if (sfRow >= 0 && sfRow < dm->sf->rowCount()) {
        const int orientation =
            dm->sf->index(sfRow, G::OrientationColumn).data().toInt();
        const int rotationDegrees =
            dm->sf->index(sfRow, G::RotationDegreesColumn).data().toInt();
        if (orientation == 3)      degrees = rotationDegrees + 180;
        else if (orientation == 6) degrees = rotationDegrees + 90;
        else if (orientation == 8) degrees = rotationDegrees + 270;
        else                       degrees = rotationDegrees;
        if (degrees > 360) degrees -= 360;
    }
    return degrees;
}

bool MW::currentIsVideo() const
{
    if (!dm || !dm->sf || dm->currentSfRow < 0 || dm->currentSfRow >= dm->sf->rowCount())
        return false;
    return dm->sf->index(dm->currentSfRow, G::VideoColumn).data().toBool();
}

bool MW::currentDevelopEditsVisible() const
{
    /* Develop edits render only in Develop mode. Preview mode is fast, as-shot review: it shows the
       embedded preview / decoded image WITHOUT the saved develop recipe. */
    if (G::operationMode != G::OperationMode::Develop) return false;
    if (!developProperties || developProperties->currentIsIdentity()) return false;
    /* A RAW file's edits are calibrated for the demosaiced render, so they show only in raw mode
       (in preview mode the loupe shows the untouched embedded JPG). A non-RAW file (JPG/TIFF/PNG)
       IS the developable image, so its edits show regardless of useRaw. */
    return G::useRaw || !isFileRaw(dm->currentFilePath);
}

bool MW::isFileRaw(const QString &fPath) const
{
    if (!metadata) return false;
    const QString ext = QFileInfo(fPath).suffix().toLower();
    return metadata->hasJpg.contains(ext);
}

void MW::applyDevelopPreviewIfEdited()
{
/*
    Called right after the current image's loupe pixmap is shown. If the image has saved Develop
    edits that should be visible (currentDevelopEditsVisible), render them over the loupe using the
    existing coalesced proxy + async settle pipeline; otherwise the normal decoded image is left.
    Safe to call more than once per image (the render path is keyed on dm->currentFilePath and
    coalesced).
*/
    if (G::isLogger) G::log("MW::applyDevelopPreviewIfEdited");
    /* A new image is on the loupe: re-evaluate the chip (the previous image's decode or
       settle may still be flagged, and this image's may not have started yet). */
    updateDevelopRenderingHint();
    if (currentIsVideo()) return;               // Develop operates on stills, not videos
    /* An edited image (raw on) renders its saved params, and that render refreshes the scopes via
       setDevelopPreview. Otherwise nothing overlays the loupe, so refresh the scopes here from the
       decoded image actually shown (valid in preview mode too). */
    if (!currentDevelopEditsVisible()) {
        updateDevelopScopes(icd->imCache.value(dm->currentFilePath));
        return;
    }
    developParamsChange();   // schedule the proxy + full-res settle render of the saved params
}

void MW::renderDevelopFullResAsync()
{
/*
    Render the crisp full-resolution preview OFF the GUI thread (it is ~1.3s on a 50MP RAW and run
    synchronously would freeze the drag -- and the freeze itself fakes "settle" pauses that re-fire
    this timer, so it would run repeatedly mid-drag). developRenderPool drives one render at a
    time; the result is marshalled back to the GUI thread (onDevelopFullResReady) and applied only
    if still current. Inputs that need the GUI thread (current path, edit params, orientation) are
    captured here; the WorkingImage is const and held alive by a shared_ptr for the worker.
*/
    if (G::isLogger) G::log("MW::renderDevelopFullResAsync");
    if (developFullResInFlight) return;   // one at a time; onDevelopFullResReady re-arms if needed

    const QString fPath = dm->currentFilePath;
    if (fPath.isEmpty()) return;
    if (currentIsVideo()) return;               // Develop operates on stills, not videos
    auto work = WorkingImageCache::instance().get(fPath);
    if (!work) return;                    // proxy render builds/caches it first; nothing to do yet

    auto mj = developProperties->stackJob();          // full stack, captured on the GUI thread
    const bool cropSuppressed = developCropEditing && !developCropShowResult;
    if (cropSuppressed) {                 // suppress crop, keep warp (see renderDevelopPreview)
        mj.geometry.cropX = 0.0; mj.geometry.cropY = 0.0;
        mj.geometry.cropW = 1.0; mj.geometry.cropH = 1.0;
    }
    /* Pair the frame with whether it actually depicts the STORED recipe, captured here on
       the GUI thread with the job because the view overrides it reflects can change while
       the render is in flight (same reason as the proxy path). Suppressing the crop for an
       interactive crop drag also makes the frame the wrong SHAPE for the stored recipe, so
       it cannot be cached either. Only a faithful frame may become a devPreview. */
    const bool faithful = developProperties && developProperties->renderMatchesStoredRecipe()
                          && !cropSuppressed;
    /* The recipe this render depicts, captured with the job (see MW::developFrameRecipe).
       gen alone cannot stand in for it: the crop / warp / level callers re-render without
       bumping developParamsGen, so a gen match does not mean the recipe has not moved. */
    const QByteArray recipe = faithful ? developRecipeKey(fPath) : QByteArray();
    const int degrees = work->sceneReferred ? developOrientationDegrees(*work, fPath) : 0;
    const quint64 gen = developParamsGen;

    /* Ensure the raw-DENOISED base is ready before the crisp full-res render. If "Denoise raw" is
       set but the denoised image for the current amounts isn't cached yet, compute it off-thread
       and bail -- ensureRawDenoise() repaints and re-arms this render when it lands. */
    const std::shared_ptr<const WorkingImage> base = developRawDenoisedBase(fPath, mj.global, work);
    if (base == work &&
        (mj.global.denoiseLuma > 0.0f || mj.global.denoiseChroma > 0.0f) &&
        G::decodeRawEngine == G::DecodeRawEngine::winnowDecodeRawEngine) {
        /* Run PMRID when the RECIPE wants it: EditParams::denoiseRaw, falling back to the
           Auto run preference when the image says nothing. This used to be "Auto run OR a
           PMRID base is already cached", the second half being how a manual-mode amount
           change stayed denoised instead of reverting to clean -- the recipe answers that
           now (the checkbox pins denoiseRaw = 1), and the old OR would render a
           deliberately UN-denoised image denoised whenever a base happened to be cached
           for it. An amount change is still only a cheap re-blend: ensureRawDenoise
           reuses developPmridFull, which is amount-independent. */
        if (mj.global.wantsDenoiseRaw(G::autoRunDenoise)) {
            ensureRawDenoise(fPath, mj.global, work, currentImageIso());
            return;   // wait for the (re)blended base; PMRID re-arms this render
        }
    }
    /* On the Apple engine PMRID can't run (no CFA), so "Denoise raw" is inert -- don't bail here
       (there is no denoised base coming); fall through and render the clean base at full res. */

    /* Ensure the content-range reference is registered before the background render samples it
       (GUI thread; cached, so normally a no-op after the proxy render already built it). */
    if (stackHasRangeMask(mj)) ensureRangeRef(fPath, *work, mj.global, degrees);
    if (stackHasSubjectMask(mj)) ensureSubjectMask(fPath, *work, mj.global, degrees);
    if (stackHasSkyMask(mj)) ensureSkyMask(fPath, *work, mj.global, degrees);
    if (stackHasDepthMask(mj)) ensureDepthMask(fPath, *work, mj.global, degrees);
    if (stackHasObjectMask(mj))
        for (const DevelopProperties::StackRenderJob::Scope &L : mj.scopes)
            for (const MaskComponent &m : L.components)
                if (m.tool == int(MaskTool::Object))
                    ensureObjectMask(fPath, *work, mj.global, degrees, m.paramsJson);
    /* Register the brush luminance auto-mask guide (GUI thread) before dispatching the
       worker, so the off-thread render confines the stroke the same as the proxy did. */
    if (stackHasAutoMaskBrush(mj)) imageView->ensureAutoGuide();

    developFullResInFlight = true;
    updateDevelopRenderingHint();
    std::shared_ptr<const WorkingImage> src = base;   // denoised base when set, else clean; kept alive
    std::shared_ptr<const WorkingImage> clean = work; // un-denoised base for verify
    developRenderPool->start([this, src, clean, mj, degrees, fPath, gen, faithful,
                              recipe]() {
        QElapsedTimer t;
        WorkingImageCache::RenderTimings rt;
        const bool probe = G::isReportDevelopTime;
        if (probe) t.start();
        const QImage out = developCompositeStack(*src, mj, degrees, /*fullRes*/true, 0, 0, fPath,
                                                 probe ? &rt : nullptr);
        const qint64 ms = probe ? t.elapsed() : 0;

        /* Cheap pixel-change verification (developVerifyMaxAbs): at a small fixed size,
           render the CLEAN base with identity params (the un-developed original) vs the
           actual base (incl. raw denoise) with the recipe -- geometry suppressed on both
           so the sizes match -- and measure the pixel difference. maxAbs==0 proves the
           develop produced NOTHING visible over the original (the failure class
           behind the denoise + Apple-demosaic bugs). ~256px, sub-ms. */
        int vMaxAbs = -1;
        double vMeanAbs = -1.0;
        {
            DevelopProperties::StackRenderJob idJob;  // identity: default base, no scopes
            DevelopProperties::StackRenderJob edJob = mj;
            idJob.geometry = Geometry();
            edJob.geometry = Geometry();  // isolate recipe/denoise from crop/warp
            /* Downscale once; reuse the clean baseline when no raw denoise is active
               (src==clean), so the common case pays a single O(N) downscale, not two. */
            const WorkingImage baseSmall = WorkingImageCache::downscaled(*src, 256);
            WorkingImage cleanSmallStore;
            const WorkingImage *cleanSmall = &baseSmall;
            if (src != clean) { cleanSmallStore = WorkingImageCache::downscaled(*clean, 256);
                                cleanSmall = &cleanSmallStore; }
            const QImage baseImg = developCompositeStack(*cleanSmall, idJob, degrees, false, 0, 0, fPath);
            const QImage editImg = developCompositeStack(baseSmall,   edJob, degrees, false, 0, 0, fPath);
            if (!baseImg.isNull() && !editImg.isNull() && baseImg.size() == editImg.size()) {
                const QImage a = baseImg.convertToFormat(QImage::Format_RGB888);
                const QImage b = editImg.convertToFormat(QImage::Format_RGB888);
                quint64 acc = 0, n = 0;
                int mx = 0;
                const int wb = a.width() * 3;
                for (int r = 0; r < a.height(); ++r) {
                    const uchar *pa = a.constScanLine(r);
                    const uchar *pb = b.constScanLine(r);
                    for (int i = 0; i < wb; ++i) {
                        const int d = qAbs(int(pa[i]) - int(pb[i]));
                        acc += quint64(d);
                        if (d > mx) mx = d;
                        ++n;
                    }
                }
                vMaxAbs = mx;
                vMeanAbs = n ? double(acc) / double(n) : -1.0;
            }
        }
        const bool vRecipeIdentity = mj.global.isIdentity() && mj.scopes.isEmpty();
        const bool vGeometryActive = !mj.geometry.isIdentity();

        QMetaObject::invokeMethod(this, [this, out, fPath, gen, ms, rt, faithful, recipe,
                                         vMaxAbs, vMeanAbs, vRecipeIdentity, vGeometryActive]() {
            if (G::isReportDevelopTime)
                qDebug().noquote() << "[DevTime] full(async)" << out.width() << "x" << out.height()
                                   << " total" << ms
                                   << " (mask" << rt.maskBuildMs << "(" << rt.maskScopes
                                   << "scopes) copy" << rt.copyMs
                                   << " develop" << rt.developMs
                                   << "[copy" << rt.stackCopyMs
                                   << " apply" << rt.stackApplyMs
                                   << " blend" << rt.stackBlendMs
                                   << " free" << rt.stackFreeMs
                                   << " OTHER" << (rt.developMs - rt.stackCopyMs
                                                   - rt.stackApplyMs - rt.stackBlendMs
                                                   - rt.stackFreeMs) << "]"
                                   << " outFree" << rt.outFreeMs
                                   << " toImage" << rt.toImageMs
                                   << " orient+scale" << rt.orientScaleMs << ")ms"
                                   << " develop=[denoise" << rt.denoiseMs << " point" << rt.pointMs
                                   << " texture" << rt.textureMs << " clarity" << rt.clarityMs
                                   << " dehaze" << rt.dehazeMs
                                   << " vignette" << rt.vignetteMs
                                   << " sharpen" << rt.sharpenMs
                                   << " grain" << rt.grainMs << "]";
            if (dm && fPath == dm->currentFilePath) {
                developVerifyMaxAbs = vMaxAbs;
                developVerifyMeanAbs = vMeanAbs;
                developVerifyRecipeIdentity = vRecipeIdentity;
                developVerifyGeometryActive = vGeometryActive;
                developVerifyPath = fPath;
            }
            onDevelopFullResReady(out, fPath, gen, faithful, recipe);
        });
    });
}

void MW::onDetailPointPicked(double nx, double ny)
{
/*
    The loupe's Detail picker landed. Disarm (auto-dismiss, like the WB dropper), store
    the point, mark it on the image, and re-render so the patch appears immediately rather
    than at the next slider move.
*/
    if (G::isLogger) G::log("MW::onDetailPointPicked");
    if (!developProperties) return;
    developProperties->cancelDetailPick();
    developProperties->setDetailPoint(QPointF(nx, ny));   // emits detailRoiNeeded
}

void MW::onDetailRoiNeeded()
{
/*
    The Detail preview needs a fresh patch: the point moved, or the panel just opened.
    The patch is rendered as part of the normal proxy tick (see the DetailRoiJob note),
    so this only has to ask for a tick -- deliberately WITHOUT bumping developParamsGen,
    because nothing about the recipe changed and a bump would discard an in-flight settle
    render.
*/
    if (G::isLogger) G::log("MW::onDetailRoiNeeded");
    if (!developProperties) return;
    if (imageView) {
        if (developProperties->detailHasPoint())
            imageView->setDetailPoint(developProperties->detailPoint());
        else imageView->clearDetailPoint();
    }
    if (G::operationMode != G::OperationMode::Develop) return;
    if (!developProperties->detailPreviewWanted()) return;   // nothing to render
    if (!developProxyRenderTimer->isActive()) developProxyRenderTimer->start(0);
}

void MW::onDetailPointNudged(int dx, int dy)
{
/*
    A drag inside the preview, in image pixels of the ORIENTED frame. Resolved here rather
    than in the dock because the oriented frame needs developOrientationDegrees, which
    reads the sort/filter model (GUI-thread state the dock does not touch).
*/
    if (!developProperties) return;
    const QString fPath = dm ? dm->currentFilePath : QString();
    if (fPath.isEmpty()) return;
    auto work = WorkingImageCache::instance().get(fPath);
    if (!work || !work->isValid()) return;
    const int degrees = work->sceneReferred ? developOrientationDegrees(*work, fPath) : 0;
    int fw = work->width, fh = work->height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    if (fw <= 0 || fh <= 0) return;
    const QPointF p = developProperties->detailPoint();
    developProperties->setDetailPoint(QPointF(p.x() + double(dx) / fw,
                                              p.y() + double(dy) / fh));
}

void MW::resetDetailPoint()
{
/*
    A new image: drop the picked point (it meant a place in the PREVIOUS picture) and put
    the preview back to its "pick a location" prompt. Called from the image-change path
    alongside the other per-image Develop view state.
*/
    if (!developProperties) return;
    developProperties->cancelDetailPick();
    developProperties->clearDetailPoint();
    developProperties->setDetailRoiImage(QImage());
    developProperties->setDetailMessage(tr("Pick a location in the image"));
    if (imageView) imageView->clearDetailPoint();
}

void MW::pushDevelopGeometryToView()
{
/*
    Pair the loupe's overlays with the geometry the frame on screen carries. Reads the
    CURRENT stack job, so callers must only use it where the shown frame is known to be
    up to date with it (the render completions, which already gate on that). Mirrors
    renderDevelopPreview's crop suppression: while the crop tool is open the render shows
    the full frame, so the overlays must map through the same suppressed geometry.
*/
    if (!imageView || !developProperties || !dm) return;
    const QString fPath = dm->currentFilePath;
    if (fPath.isEmpty()) { imageView->setDevelopGeometry(Geometry(), QSize()); return; }
    auto work = WorkingImageCache::instance().get(fPath);
    if (!work) { imageView->setDevelopGeometry(Geometry(), QSize()); return; }

    Geometry g = developProperties->stackJob().geometry;
    if (developCropEditing && !developCropShowResult) {
        g.cropX = 0.0; g.cropY = 0.0; g.cropW = 1.0; g.cropH = 1.0;
    }
    const int degrees = work->sceneReferred ? developOrientationDegrees(*work, fPath) : 0;
    int fw = work->width, fh = work->height;
    if (degrees == 90 || degrees == 270) std::swap(fw, fh);
    imageView->setDevelopGeometry(g, QSize(fw, fh));
}

void MW::updateDevelopRenderingHint()
{
/*
    Raise or clear the loupe's "still rendering" chip. It tracks only the SLOW stages --
    the off-thread scene-linear RAW decode an image switch needs (ensureDevelopWork) and
    the full-resolution settle render -- because those are the ones that leave an interim
    frame on screen for long enough to be mistaken for the final one. The interactive
    proxy tick is sub-second and is the normal editing state, so it says nothing.

    NOTE this was the deliberate ZERO-MEMORY answer to image-switch latency: show what is
    already decoded, and label it, rather than pre-decoding neighbours. A proxy cache or
    neighbour read-ahead would make the switch instant but costs 46 MB per cached proxy
    (and a full scene-linear WorkingImage is 12 bytes/pixel -- ~600 MB on a 50 MP file).
    That trade was weighed and deferred; see notes/Documentation.txt.

    The develop-preview cache changes what is on screen during that wait but not this
    trade: it holds a JPEG on DISK, not a decoded proxy in memory, and only for images the
    user has actually edited. The decode still takes just as long.
*/
    if (!imageView) return;
    if (G::operationMode != G::OperationMode::Develop) {
        imageView->clearRenderingHint();
        return;
    }
    const QString fPath = dm ? dm->currentFilePath : QString();
    if (fPath.isEmpty()) { imageView->clearRenderingHint(); return; }

    /* Distinguish the two placeholders. Showing a cached develop preview already looks
       like the finished picture, so the chip has to say the pixels are provisional --
       otherwise the later swap to the real render reads as an unexplained flicker. */
    if (developWorkInFlight == fPath) {
        imageView->setRenderingHint(developInterimIsDevPreview
                                        ? tr("Cached preview - decoding raw")
                                        : tr("Decoding raw"));
    }
    else if (developFullResInFlight)  imageView->setRenderingHint(tr("Rendering full"));
    else {
        imageView->clearRenderingHint();
        developInterimIsDevPreview = false;   // the real render is on screen now
    }
}

void MW::onDevelopFullResReady(const QImage &out, const QString &fPath, quint64 gen,
                               bool faithful, const QByteArray &recipe)
{
/*
    GUI-thread completion for a background full-res render. Apply the image only if it is still
    current (same image shown, no slider change since it launched); otherwise discard it. If newer
    params arrived while it ran, re-arm the settle timer so the latest settled value still gets a
    crisp render -- this also covers the case where the timer fired and was skipped because a render
    was already in flight.

    The frame is also RETAINED (developFullFrame) so the devPreview provider can encode the
    devPreview tier at sensor resolution instead of from the screen-resolution proxy, together
    with the RECIPE it was rendered from. The currency test here only says the frame may be
    SHOWN now; it says nothing about later, because the next edit does not replace this frame
    -- only the next completed render does. The recipe is what lets the provider tell a
    current frame from one the user has since edited past. Retaining costs one full-res QImage
    per edited image -- the same buffer that was just handed to the loupe.
*/
    if (G::isLogger) G::log("MW::onDevelopFullResReady");
    developFullResInFlight = false;
    updateDevelopRenderingHint();

    const bool currentImage = (fPath == dm->currentFilePath);
    if (!out.isNull() && currentImage && gen == developParamsGen) {
        /* gen unchanged => the recipe (and its geometry) is still the one this render
           used, so the current state is the right pairing for the overlays. */
        pushDevelopGeometryToView();
        imageView->setDevelopPreview(out);
        updateDevelopScopes(out);
        developFullFrame = out;
        developFullFramePath = fPath;
        developFullFrameFaithful = faithful;
        developFullFrameRecipe = recipe;
    }

    if (currentImage && gen != developParamsGen)
        developFullResTimer->start(kDevelopSettleMs);
}

/* Cache key for the raw-denoised base: image path + the two Global "Denoise raw" amounts + ISO. */
static QString rawDenoiseKey(const QString &fPath, const EditParams &base, int iso)
{
    return QString("%1|dnL=%2|dnC=%3|iso=%4")
        .arg(fPath).arg(base.denoiseLuma).arg(base.denoiseChroma).arg(iso);
}

/* Cache key for the FULL-strength PMRID base (amount-independent -- the model runs once per image;
   the two amounts only scale the blend). */
static QString pmridBaseKey(const QString &fPath, int iso)
{
    return QString("%1|iso=%2").arg(fPath).arg(iso);
}

int MW::currentImageIso() const
{
    if (!dm || !dm->sf) return 0;
    return dm->sf->index(dm->currentSfRow, G::ISOColumn).data().toInt();
}

bool MW::rawDenoiseCleanBaseOk(const std::shared_ptr<const WorkingImage> &w)
{
/*
    May w be used as the CLEAN side of the "Denoise raw" blend? The denoised side always
    comes out of the raw decode as camera-native scene-linear pixels (RawColor::
    ToCameraNative), and Develop::BlendRawDenoise adds a difference of the two -- so the
    clean side has to be in that same colour state.

    The WorkingImageCache is keyed by path alone and, despite what its header promises,
    does not hold only camera-native raw decodes: Preview mode and the display-referred
    paths (ImageDecoder::applyDevelop, MW::ensureWorkingImageNow) build a WorkingImage
    from the 8-bit decoded QImage (InputTransform::FromImage -- working space, white
    balanced, display-referred) and put THAT under the raw's path. Blending one of those
    against a camera-native PMRID base tints the whole image, because the sensor's green
    sits far above its red/blue until the white balance runs: the difference is dominated
    by green and the render lands on a magenta (or green) cast.

    So an entry that fails this test is not fixed up here -- it is dropped, and the decode
    produces a fresh clean base of its own (outClean), which then replaces it in the
    cache.
*/
    return w && w->isValid() && w->sceneReferred &&
           w->space == ColorSpaceMath::ColorSpace::CameraNative;
}

std::shared_ptr<const WorkingImage> MW::developRawDenoisedBase(
    const QString &fPath, const EditParams &base,
    const std::shared_ptr<const WorkingImage> &clean)
{
/*
    The base image the develop render should start from. "Denoise raw" is a Global-only, RAW-only
    global op: when it is set and the denoised WorkingImage for the current amounts is cached, that
    is the base; otherwise the clean cached image is used (and ensureRawDenoise() computes the
    denoised one off-thread on settle). Pure lookup -- never runs the model.
*/
    if (!clean || (base.denoiseLuma <= 0.0f && base.denoiseChroma <= 0.0f))
        return clean;
    /* PMRID is Winnow-engine only; on the Apple engine "Denoise raw" is inert, so serve
       the clean base even if a Winnow-denoised base is still cached (the key is engine-
       independent, so this is what makes a Winnow->Apple switch actually update). */
    if (G::decodeRawEngine != G::DecodeRawEngine::winnowDecodeRawEngine)
        return clean;
    const QString key = rawDenoiseKey(fPath, base, currentImageIso());
    if (developDenoisedKey == key && developDenoised) return developDenoised;
    return clean;   // not ready yet -> serve clean; ensureRawDenoise() fills it in
}

void MW::ensureRawDenoise(const QString &fPath, const EditParams &base,
                          const std::shared_ptr<const WorkingImage> &clean, int iso)
{
/*
    Build the raw-denoised base off the GUI thread (developRenderPool). PMRID runs
    PRE-demosaic, so the denoised base comes from DECODING the raw with the denoiser on
    (ImageDecoder::decodeRawWorking, in-house/Winnow engine only) -- NOT from an
    already-demosaiced image. That one decode ALSO returns the CLEAN base (outClean,
    demosaiced from the same mosaic before PMRID), so a single UnpackCfa yields both bases
    the blend needs instead of a second full decode; the clean base is published to
    WorkingImageCache so the rest of the develop pipeline reuses it. The heavy decode runs
    ONCE per image and is cached (developPmridFull, keyed path+iso); the two Global amounts
    only scale a cheap luma/chroma blend (Develop::BlendRawDenoise), so a slider drag
    re-blends without re-running the model. Callable on image select (clean == null): the
    worker decodes both bases and progress shows from the start. Coalesced -- one job.
*/
    if (base.denoiseLuma <= 0.0f && base.denoiseChroma <= 0.0f) return;
    /* PMRID needs the CFA mosaic -> Winnow engine only. On the Apple engine "Denoise raw"
       is inert; bail so a slider drag doesn't trigger a full re-decode that denoises
       nothing. */
    if (G::decodeRawEngine != G::DecodeRawEngine::winnowDecodeRawEngine) return;
    /* Can't work here (no inference backend / no pmrid.onnx, or this sensor's CFA). Bail
       BEFORE the decode: the build case is known up front, and for the sensor case an
       auto-run retry on every settle would be a full raw re-decode each time for a base
       that comes back identical to clean. See reportRawDenoiseUnavailable. */
    if (!rawDenoiseAvailable(fPath)) return;
    const QString key = rawDenoiseKey(fPath, base, iso);
    if (developDenoisedKey == key && developDenoised) return;   // already current
    /* ONE job at a time. Concurrent jobs (one per drag value) finish out of order and a
       stale early result can win the cache, so the lookup never matches the slider.
       Serialize: while a job runs, skip new requests; the completion re-renders, and the
       render's gate re-triggers this for the latest params -- so it converges on where
       the sliders end up. */
    if (!developDenoiseInFlightKey.isEmpty()) return;
    developDenoiseInFlightKey = key;

    /* Build the raw metadata (rawInfo + ISO) on the GUI thread for the off-thread decode;
       the worker then consults only this copy (no DataModel access). Reuse the cached
       full-strength PMRID base for this image if we already have it (amount change). */
    const QString pkey = pmridBaseKey(fPath, iso);
    ImageMetadata m;
    m.fPath = fPath;
    m.ISONum = iso;
    // model drives the PMRID calibration
    m.model = dm->sf->index(dm->currentSfRow, G::CameraModelColumn).data().toString();
    dm->fPathRawInfoGet(fPath, m.rawInfo);
    std::shared_ptr<const WorkingImage> pmridCached =
        (developPmridKey == pkey && developPmridFull) ? developPmridFull : nullptr;

    /* Clean base if the caller had one (may be null on select) -- but only when it is in
       the colour state the PMRID base will be in. See rawDenoiseCleanBaseOk. */
    std::shared_ptr<const WorkingImage> src = rawDenoiseCleanBaseOk(clean) ? clean : nullptr;
    const EditParams b = base;
    developRenderPool->start([this, src, b, key, pkey, fPath, m, pmridCached]() {
        /* Resolve both bases. The PMRID base and the clean base come from ONE decode
           (shared UnpackCfa) when either is missing; a pure amount change (both cached)
           skips the decode and just re-blends. clean is taken from the caller, else
           WorkingImageCache, else the decode's outClean. */
        std::shared_ptr<const WorkingImage> pmrid = pmridCached;
        std::shared_ptr<const WorkingImage> cached = WorkingImageCache::instance().get(fPath);
        const bool cachedOk = rawDenoiseCleanBaseOk(cached);
        std::shared_ptr<const WorkingImage> cleanBase = src ? src
                                                            : (cachedOk ? cached : nullptr);
        // freshClean is published to WorkingImageCache if the decode produced it
        std::shared_ptr<const WorkingImage> freshClean;
        /* Diagnostic: the (k,b) tier PMRID used for THIS decode (captured right after the
           decode so a concurrent run can't overwrite the global; read-ahead is off in
           Develop anyway). Only meaningful when a decode actually ran (capturedRes). */
        PMRID::Resolution decodeRes;
        bool capturedRes = false;
        /* Did PMRID actually run? A no-op model (built without ONNX Runtime, pmrid.onnx
           missing, non-Bayer CFA) still returns a valid base -- one that is pixel-identical
           to the clean one. Publishing it would show "Denoised" and enabled amount sliders
           over an unchanged image, so the result is dropped instead (unavailable below). */
        bool denoiseApplied = false;
        if (!pmrid || !cleanBase) {
            /* Reveal the progress row (EMPTY) as the decode starts; PMRID then fills it
               per tile. Must not updateProgress(0,1) here -- FromStart would paint the
               whole bar (item 0 of 1 = full width), leaving it stuck at 100%. */
            QMetaObject::invokeMethod(this, [this]() {
                progress->showRow(progressRawDenoiseRow, true);
            });
            auto prog = [this](int done, int total) {
                QMetaObject::invokeMethod(this, [this, done, total]() {
                    progress->updateProgress(progressRawDenoiseRow, done, total);
                });
            };
            ImageDecoder dec(0, dm, metadata);
            /* Ask for the clean base only when we don't already have it, so the common
               amount-change decode (clean cached, PMRID evicted) skips a demosaic. */
            std::shared_ptr<const WorkingImage> decodedClean;
            auto decodedPmrid =
                dec.decodeRawWorking(m, true, prog, cleanBase ? nullptr : &decodedClean,
                                     &denoiseApplied);
            if (decodedPmrid) {
                if (!pmrid) pmrid = decodedPmrid;
                if (!cleanBase && decodedClean) {
                    cleanBase = decodedClean;
                    freshClean = decodedClean;
                }
                decodeRes = PMRID::LastResolution();
                capturedRes = true;
            }
        }
        else {
            /* Both bases cached -> no decode, so this is a pure re-blend of a PMRID base
               that already proved itself when it was built. */
            denoiseApplied = true;
        }
        /* No in-house decoder (lossless ARW -> Apple) or the decode failed. */
        if (!pmrid || !cleanBase) {
            QMetaObject::invokeMethod(this, [this]() {
                developDenoiseInFlightKey.clear();
                progress->clearProgress(progressRawDenoiseRow);  // hide the row
            });
            return;
        }
        /* Decoded, but the denoiser did nothing. Keep the freshly-decoded CLEAN base (it is
           good, and the render needs it) but publish no denoised base, so the render stays
           on clean and the dock reports "Denoise", not "Denoised". */
        if (!denoiseApplied) {
            QMetaObject::invokeMethod(this, [this, freshClean, fPath]() {
                developDenoiseInFlightKey.clear();
                progress->clearProgress(progressRawDenoiseRow);
                if (!dm || fPath != dm->currentFilePath) return;
                if (freshClean && !rawDenoiseCleanBaseOk(
                                      WorkingImageCache::instance().get(fPath)))
                    WorkingImageCache::instance().put(fPath, freshClean);
                reportRawDenoiseUnavailable(fPath);
            });
            return;
        }
        /* Blend clean <-> PMRID by the luma/chroma amounts (cheap, per-settle). */
        auto blended = std::make_shared<WorkingImage>();
        Develop::BlendRawDenoise(*cleanBase, *pmrid, b.denoiseLuma, b.denoiseChroma, *blended);
        std::shared_ptr<const WorkingImage> result = blended;

        QMetaObject::invokeMethod(this, [this, result, pmrid, freshClean, key, pkey, fPath,
                                         decodeRes, capturedRes]() {
            // job finished; allow the next (latest) one
            developDenoiseInFlightKey.clear();
            progress->clearProgress(progressRawDenoiseRow);   // hide the progress row
            if (!dm || fPath != dm->currentFilePath) return;  // navigated away; drop it
            /* Engine switched to Apple mid-decode: this Winnow result is stale -- drop
               it so it can't re-publish a Winnow base the Apple render would ignore. */
            if (G::decodeRawEngine != G::DecodeRawEngine::winnowDecodeRawEngine) return;
            /* Publish the freshly-decoded clean base so renderDevelopPreview /
               ensureDevelopWork reuse it, not trigger another scene-linear decode. It
               also REPLACES a display-referred entry left under this path by Preview
               mode -- that entry is what the blend had to reject to get here, and
               leaving it would send the next denoise straight back to it. */
            if (freshClean &&
                !rawDenoiseCleanBaseOk(WorkingImageCache::instance().get(fPath)))
                WorkingImageCache::instance().put(fPath, freshClean);
            developPmridFull = pmrid;      // cache the full base for other amounts
            developPmridKey = pkey;
            if (capturedRes) {             // noise-model snapshot for this base (diag)
                developPmridResSource = decodeRes.source;
                developPmridResK = decodeRes.k;
                developPmridResB = decodeRes.b;
                developPmridResHadNP = decodeRes.hadNoiseProfile;
            }
            developDenoised = result;
            developDenoisedKey = key;
            if (developProperties)
                developProperties->updateDenoiseRunState(true);   // -> "Denoised"
            developProxy.reset();          // rebuild proxy from the denoised base
            developProxyPath.clear();
            developStackCache.clear();     // new base pixels: every intermediate is stale
            ++developParamsGen;            // discard any stale in-flight full-res
            renderDevelopPreview(false);   // repaint the proxy now
            developFullResTimer->start(kDevelopSettleMs);   // and a crisp full-res
        });
    });
}

void MW::reportRawDenoiseUnavailable(const QString &fPath)
{
/*
    PMRID decoded but changed nothing. Two different causes, and they must be remembered
    differently or the next settle launches the same fruitless full raw re-decode:

      o the model cannot run AT ALL -- built without ONNX Runtime (CMake
        WINNOW_ENABLE_ORT=OFF -> the OrtBackend stub) or pmrid.onnx is not beside the
        binary. Permanent for the session: rawDenoiseUnavailable blocks every later
        attempt, for every image.
      o the model is fine but THIS sensor is not a Bayer phase PMRID handles (X-Trans).
        Per image: remember the path only, so the next raw still gets its chance.

    Either way the dock greys the whole denoise group and shows the reason in its place
    (DevelopProperties::updateDenoiseAvailability -> RawPanel::setDenoiseAvailable), rather
    than leaving live-looking controls that accept a click and change nothing.
*/
    if (G::isLogger) G::log("MW::reportRawDenoiseUnavailable");
    if (PMRID::IsAvailable()) rawDenoiseUnsupported.insert(fPath);
    else                      rawDenoiseUnavailable = true;
    if (developProperties) {
        developProperties->updateDenoiseRunState(false);
        developProperties->updateDenoiseAvailability();   // grey the group + say why
    }
}

bool MW::rawDenoiseAvailable(const QString &fPath, QString *reason) const
{
/*
    Can "Denoise raw" do anything for this image? Answered for the DOCK, so it is cheap
    (PMRID::IsSupportedBuild does not load the model) and returns the reason to show in
    place of the greyed controls. Two ways to know it cannot: the build itself (no
    inference backend / no pmrid.onnx -- known before anything runs), and a sensor whose
    CFA PMRID left untouched (only knowable after a decode, remembered per path by
    reportRawDenoiseUnavailable).
*/
    if (!PMRID::IsSupportedBuild()) {
        if (reason)
            *reason = tr("Not available in this build (needs ONNX Runtime and pmrid.onnx).");
        return false;
    }
    if (rawDenoiseUnavailable) {     // present, but the model would not load / run
        if (reason) *reason = tr("The denoise model could not be loaded.");
        return false;
    }
    if (rawDenoiseUnsupported.contains(fPath)) {
        if (reason) *reason = tr("Not available for this sensor (Bayer only).");
        return false;
    }
    if (reason) reason->clear();
    return true;
}

void MW::onAutoRunDenoiseToggled(bool on)
{
    if (G::isLogger) G::log("MW::onAutoRunDenoiseToggled");
    G::autoRunDenoise = on;
    settings->setValue("Develop/autoRunDenoise", on);
    /* Turning auto ON behaves "as currently done": run the denoise for the current image
       now so it updates without waiting for the next param change. NOT for an image whose
       recipe pins denoiseRaw OFF -- the preference is the fallback for images that said
       nothing, and it must not override one that did. */
    if (!on) return;
    const bool pinnedOff = developProperties &&
                           developProperties->stackJob().global.denoiseRaw == 0;
    if (!pinnedOff) runRawDenoiseNow();
}

void MW::runRawDenoiseNow()
{
    if (G::isLogger) G::log("MW::runRawDenoiseNow");
    if (!G::useRaw || !developProperties || !dm) return;
    const QString fPath = dm->currentFilePath;
    if (fPath.isEmpty()) return;
    /* PMRID is Winnow-engine only; inert on Apple. */
    if (G::decodeRawEngine != G::DecodeRawEngine::winnowDecodeRawEngine) return;
    /* Known not to work here: undo the tick and grey the group rather than leave a checked
       box over an image that will never change (ensureRawDenoise would just bail). Normally
       unreachable -- the controls are already disabled by then. */
    if (!rawDenoiseAvailable(fPath)) {
        reportRawDenoiseUnavailable(fPath);
        return;
    }
    const auto mj = developProperties->stackJob();
    ensureRawDenoise(fPath, mj.global, WorkingImageCache::instance().get(fPath),
                     currentImageIso());
}

void MW::clearRawDenoiseNow()
{
    if (G::isLogger) G::log("MW::clearRawDenoiseNow");
    /* "Denoise" unchecked: drop BOTH the blended and the full-strength PMRID base so the
       render falls back to clean and the amount sliders disable again. Reset the checkbox
       label and re-render. (With Auto run on, the next auto trigger recomputes it.) */
    developDenoised.reset();
    developDenoisedKey.clear();
    developPmridFull.reset();
    developPmridKey.clear();
    if (developProperties) developProperties->updateDenoiseRunState(false);
    developParamsChange();
}

bool MW::rawDenoiseReadyForCurrent()
{
    /* True once the full-strength PMRID base for the current image is cached -- i.e. the
       heavy denoise has completed. Amount-independent (the base is keyed path+iso; the
       sliders only scale the blend), so it stays true across denoise-amount changes. It
       drives both the "Denoise"/"Denoised" checkbox and the amount-slider enabled. */
    if (!dm) return false;
    const QString fPath = dm->currentFilePath;
    if (fPath.isEmpty() || !developPmridFull) return false;
    if (G::decodeRawEngine != G::DecodeRawEngine::winnowDecodeRawEngine) return false;
    return developPmridKey == pmridBaseKey(fPath, currentImageIso());
}

void MW::onDemosaicProgress(const QString &fPath, int done, int total)
{
    /* Relayed from an ImageCache decoder thread. Show the "Demosaic" status-bar row only
       for the CURRENT image's Winnow raw demosaic while Auto-run denoise is off (with it
       on, the "Denoise raw" path shows its own row). Cleared when the current image
       finishes caching (setCached, wired in createImageCache). */
    if (G::autoRunDenoise) return;
    if (G::operationMode != G::OperationMode::Develop || !G::useRaw) return;
    if (G::decodeRawEngine != G::DecodeRawEngine::winnowDecodeRawEngine) return;
    if (!dm || fPath != dm->currentFilePath) return;
    progress->showRow(progressDemosaicRow, true);
    progress->updateProgress(progressDemosaicRow, done, total);
}

void MW::ensureDevelopWork(const QString &fPath)
{
/*
    Decode the current image's SCENE-LINEAR pre-develop WorkingImage OFF the GUI thread when it is
    missing from WorkingImageCache (evicted, or only a display-referred one cached), then re-render.
    decodeIndependent caches the scene-linear WorkingImage as a side effect. Coalesced -- one decode
    in flight. This replaces the old synchronous re-decode in renderDevelopPreview that blocked the
    first slider drag ~1s on a 50MP RAW; the slider stays live and the develop preview appears once
    the decode lands (the loupe shows the current image meanwhile).
*/
    if (fPath.isEmpty()) return;
    if (developWorkInFlight == fPath) return;               // already decoding this image
    developWorkInFlight = fPath;
    updateDevelopRenderingHint();        // label the interim frame while decoding

    ImageMetadata m = dm->imMetadata(fPath);                // GUI thread: read the datamodel
    if (m.fPath.isEmpty()) m.fPath = fPath;
    /* ImageDecoder::load() selects the in-house RAW decoder off m.ext in independent
       mode; without it the scene-linear decode is skipped and the base falls back to
       the display-referred embedded JPG (wrong size + not scene-linear), breaking the
       "Denoise raw" blend. imMetadata() now populates ext, so this is a defensive
       fallback for the rare empty-struct return (invalid row). */
    if (m.ext.isEmpty()) m.ext = QFileInfo(fPath).suffix().toLower();
    const quint64 gen = developParamsGen;
    /* Show the Winnow raw demosaic progress on this develop-open decode -- the "Denoise
       raw" path (ensureRawDenoise) shows its own row and runs only when Auto run is on,
       so this covers the manual (Auto run off) case. Raw + in-house engine only. */
    const bool showDemosaic = !G::autoRunDenoise && G::useRaw && isFileRaw(fPath)
        && G::decodeRawEngine == G::DecodeRawEngine::winnowDecodeRawEngine;

    developRenderPool->start([this, fPath, m, gen, showDemosaic]() mutable {
        ImageDecoder dec(0, dm, metadata);
        std::function<void(int, int)> prog;
        if (showDemosaic) {
            QMetaObject::invokeMethod(this, [this]() {
                progress->showRow(progressDemosaicRow, true);
            });
            prog = [this](int done, int total) {
                QMetaObject::invokeMethod(this, [this, done, total]() {
                    progress->updateProgress(progressDemosaicRow, done, total);
                });
            };
        }
        QImage img;
        dec.decodeIndependent(img, metadata, m, prog);   // caches the scene-linear work
        QMetaObject::invokeMethod(this, [this, fPath, gen, showDemosaic]() {
            if (showDemosaic) progress->clearProgress(progressDemosaicRow);
            developWorkInFlight.clear();
            updateDevelopRenderingHint();
            if (!dm || fPath != dm->currentFilePath) return;   // navigated away; drop it
            /* Success (scene-linear work now cached) -> clear the marker so a future eviction can
               re-decode. Otherwise (display-referred format, e.g. lossless ARW) -> mark it so the
               re-render below uses the display fallback instead of looping the async decode. */
            auto w = WorkingImageCache::instance().get(fPath);
            if (w && w->sceneReferred) developWorkTriedPath.clear();
            else                        developWorkTriedPath = fPath;
            renderDevelopPreview(false);                       // render the proxy (scene-linear or fallback)
            if (gen == developParamsGen)                       // no newer edit arrived; settle a crisp one
                developFullResTimer->start(kDevelopSettleMs);
        });
    });
}

void MW::ensureWorkingImageNow(const QString &fPath)
{
/*
    Build the pre-develop WorkingImage for fPath NOW, for a Develop action that needs the
    pixels (and the colour characterisation) before the image has been edited: the WB
    dropper and Auto white balance. Without this an untouched DISPLAY-REFERRED file has no
    cache entry at all -- ImageDecoder::applyDevelop skips identity params and
    applyDevelopPreviewIfEdited skips an unedited image -- so the dropper reported "not
    ready to sample yet" on every freshly opened JPEG.

    Display-referred (JPG/TIFF/HEIC, or raw in preview mode): built HERE, synchronously,
    from the already-decoded image, so DevelopProperties::ensureWorkingImage can use it on
    return. That is one InputTransform pass over the full image -- the same cost the first
    slider move pays -- and it runs only on an explicit click.

    Raw needing the SCENE-LINEAR image: that is a decode, so hand off to the async
    ensureDevelopWork and let the caller report "not ready". In practice raw already has
    it: the raw decode path caches it unconditionally.
*/
    if (G::isLogger) G::log("MW::ensureWorkingImageNow");
    if (fPath.isEmpty() || !icd) return;

    auto work = WorkingImageCache::instance().get(fPath);
    const bool wantRaw = isFileRaw(fPath) && G::useRaw;
    if (wantRaw && (!work || !work->sceneReferred) && developWorkTriedPath != fPath) {
        ensureDevelopWork(fPath);
        return;
    }
    if (work && work->isValid()) return;

    if (!icd->contains(fPath)) return;      // not decoded yet; nothing to build from
    const QImage src = icd->imCache.value(fPath);
    if (src.isNull()) return;
    auto built = std::make_shared<WorkingImage>();
    InputTransform input;
    if (!input.FromImage(src, *built)) return;
    WorkingImageCache::instance().put(fPath, built);
}

/* Cheap, size-agnostic pixel-difference between two QImages: sample both on a normalised
   grid (no scaling of the large image) and return the max and mean per-pixel max-channel
   difference in 0..255. Used for the develop render verifications. maxAbs == 0 =>
   visually identical. */
static void imageGridDiff(const QImage &a, const QImage &b, int grid, int &maxAbs, double &meanAbs)
{
    maxAbs = -1;
    meanAbs = -1.0;
    if (a.isNull() || b.isNull() || grid < 1) return;
    quint64 acc = 0;
    int mx = 0, cnt = 0;
    for (int gy = 0; gy < grid; ++gy) {
        const double fy = (gy + 0.5) / grid;
        const int ay = qMin(a.height() - 1, int(fy * a.height()));
        const int by = qMin(b.height() - 1, int(fy * b.height()));
        for (int gx = 0; gx < grid; ++gx) {
            const double fx = (gx + 0.5) / grid;
            const QRgb ca = a.pixel(qMin(a.width() - 1, int(fx * a.width())), ay);
            const QRgb cb = b.pixel(qMin(b.width() - 1, int(fx * b.width())), by);
            const int d = qMax(qMax(qAbs(qRed(ca) - qRed(cb)), qAbs(qGreen(ca) - qGreen(cb))),
                               qAbs(qBlue(ca) - qBlue(cb)));
            acc += quint64(d);
            if (d > mx) mx = d;
            ++cnt;
        }
    }
    maxAbs = mx;
    meanAbs = cnt ? double(acc) / cnt : -1.0;
}

void MW::updateDevelopScopes(const QImage &shown, bool verifyVsPreview)
{
/*
    Rebuild the Develop scopes (histogram + vectorscope) from the image currently shown. Called
    after each develop preview render (the post-render `out`) and, for an unedited image, from the
    decoded image. One strided sample pass at a fixed budget fills both scopes, so the cost is the
    same on a 50MP RAW as on the small proxy. Cheap no-op while the scopes are hidden; a null image
    clears them. Stays on the GUI thread (the sample budget keeps it sub-millisecond); revisit only
    if a probe shows otherwise.
*/
    if (G::isLogger) G::log("MW::updateDevelopScopes");
    developShownImage = shown;   // cache for the cursor readout (implicitly shared; free)

    /* Verify the Develop display differs from the Preview (embedded) image captured on
       mode entry -- confirms the decode change (demosaic) and/or edits actually altered
       pixels. Grid-sampled, so it is cheap even on a 50MP `shown` (no scaling). Runs for
       edited AND unedited displays.

       SKIPPED on an interactive proxy tick (verifyVsPreview=false): it is a diagnostic
       about the IMAGE, not about the drag, and 4096 QImage::pixel() calls per tick is
       pure overhead on the hot path. The settle render re-measures it. */
    if (verifyVsPreview && dm && !shown.isNull() && !developVerifyPreviewBaseline.isNull()
        && developVerifyPreviewBaselinePath == dm->currentFilePath) {
        // 64x64 grid: negligible per drag tick
        imageGridDiff(developVerifyPreviewBaseline, shown, 64,
                      developVerifyVsPreviewMaxAbs, developVerifyVsPreviewMeanAbs);
        developVerifyVsPreviewPath = dm->currentFilePath;
    }

    /* Two consumers of the one sample: the scopes strip, and the Curves panel's plot
       (which draws the histogram behind the curve). The strip can be hidden while the
       Curves panel is open, so take the sample if EITHER wants it. */
    const bool wantStrip = scopesView && developScopesVisible;
    const bool wantCurve = developProperties && developProperties->wantsScopeData();
    if (!wantStrip && !wantCurve) return;               // neither: skip the sample cost
    auto clearScopes = [this, wantStrip, wantCurve] {
        if (wantStrip) scopesView->clear();
        if (wantCurve) developProperties->clearScopeData();
    };
    if (shown.isNull()) { clearScopes(); return; }

    /* Sample the shown image IN PLACE. RGB888 gets its own branch because that is what
       OutputTransform::ToImage produces, i.e. what every develop render hands us: routing
       it through convertToFormat allocated and converted the WHOLE frame (~20 MB at a
       6.7 MP proxy) just to read ~180k strided samples, on the GUI thread, on every tick.
       The 32-bit branch stays for the other callers. */
    const QImage &im = shown;
    const bool rgb888 = im.format() == QImage::Format_RGB888;
    QImage im32;
    if (!rgb888 && im.format() != QImage::Format_RGB32 &&
        im.format() != QImage::Format_ARGB32 &&
        im.format() != QImage::Format_ARGB32_Premultiplied)
        im32 = im.convertToFormat(QImage::Format_RGB32);
    const QImage &src = im32.isNull() ? im : im32;

    const int W = src.width();
    const int H = src.height();
    if (W < 1 || H < 1) { clearScopes(); return; }

    constexpr qint64 budget = 180000;
    const int step = qMax(1, static_cast<int>(std::sqrt(static_cast<double>(W) * H / budget)));

    ScopeData d;
    d.clear();
    for (int y = 0; y < H; y += step) {
        /* constScanLine: never detaches (src may share `shown`'s buffer), so a 50MP
           full-res `out` is sampled in place rather than deep-copied. */
        const uchar *raw = src.constScanLine(y);
        const QRgb *line = rgb888 ? nullptr : reinterpret_cast<const QRgb*>(raw);
        for (int x = 0; x < W; x += step) {
            int r, g, b;
            if (rgb888) { r = raw[x*3+0]; g = raw[x*3+1]; b = raw[x*3+2]; }
            else        { const QRgb p = line[x]; r = qRed(p); g = qGreen(p); b = qBlue(p); }
            d.hist[0][r]++;
            d.hist[1][g]++;
            d.hist[2][b]++;
            /* BT.709 luma, integer (coeffs * 256 = 54,183,19). */
            const int luma = (r * 54 + g * 183 + b * 19) >> 8;
            d.hist[3][luma & 0xff]++;
            /* BT.601 Cb/Cr around 128 (coeffs * 256), quantised to VN bins for the vectorscope. */
            const int cb = qBound(0, 128 + ((-43 * r - 85 * g + 128 * b) >> 8), 255);
            const int cr = qBound(0, 128 + ((128 * r - 107 * g - 21 * b) >> 8), 255);
            d.vec[(cr * ScopeData::VN) >> 8][(cb * ScopeData::VN) >> 8]++;
        }
    }
    if (wantStrip) scopesView->setData(d);
    if (wantCurve) developProperties->setScopeData(d);
}

void MW::toggleDevelopScopes()
{
/*
    Develop action-row toggle: show/hide the scopes strip and persist the choice.
*/
    if (G::isLogger) G::log("MW::toggleDevelopScopes");
    setDevelopScopesVisible(!developScopesVisible);
}

void MW::setDevelopScopesVisible(bool isVisible)
{
/*
    Show/hide the scopes strip and persist the choice. When shown, repopulate from the
    image currently displayed -- re-render the developed preview if the image has edits
    (so the scope matches what is on screen), else sample the decoded image
    (updateDevelopScopes no-ops while the strip is hidden, so the data is stale by then).
*/
    if (G::isLogger) G::log("MW::setDevelopScopesVisible");
    developScopesVisible = isVisible;
    if (scopesView) scopesView->setVisible(developScopesVisible);
    if (developScopesBtn) developScopesBtn->setActive(developScopesVisible);
    settings->setValue("Develop/scopesVisible", developScopesVisible);
    if (developScopesVisible) {
        if (currentDevelopEditsVisible())
            developParamsChange();
        else
            updateDevelopScopes(icd->imCache.value(dm->currentFilePath));
    }
}

void MW::setDevelopScopesChoice(int choice)
{
/*
    Apply a scopes-strip choice made in the strip's right-click menu or the Develop menu's
    Scopes submenu:

        Both / HistogramOnly / VectorscopeOnly  ->  show the strip with that layout
        kDevelopScopesHidden                    ->  hide the strip, keeping its layout

    The single-scope layouts expand to fill the strip. Both the layout and the visibility
    are persisted, so the strip comes back the way it was left in the next session, and
    the layout is kept while hidden so the action-row button restores the same scopes.
*/
    if (G::isLogger) G::log("MW::setDevelopScopesChoice");
    if (!scopesView) return;

    if (choice == kDevelopScopesHidden) {
        setDevelopScopesVisible(false);
        return;
    }
    if (choice < ScopesView::Both || choice > ScopesView::VectorscopeOnly) return;

    developScopesLayout = choice;
    scopesView->setScopeLayout(static_cast<ScopesView::ScopeLayout>(developScopesLayout));
    settings->setValue("Develop/scopesLayout", developScopesLayout);
    if (!developScopesVisible) setDevelopScopesVisible(true);
}

void MW::syncDevelopScopesMenu()
{
/*
    Tick the item matching the strip's current state as the menu opens. The actions are an
    exclusive QActionGroup, so checking one unchecks the rest.
*/
    if (G::isLogger) G::log("MW::syncDevelopScopesMenu");
    QAction *a = developScopesHideAction;
    if (developScopesVisible) {
        switch (developScopesLayout) {
        case ScopesView::HistogramOnly:   a = developScopesHistAction; break;
        case ScopesView::VectorscopeOnly: a = developScopesVectAction; break;
        default:                          a = developScopesBothAction; break;
        }
    }
    if (a) a->setChecked(true);
}

void MW::showDevelopScopesMenu(QMenu *menu, QPoint globalPos)
{
/*
    Right-click in the scopes strip. Every scope shows the SAME scopes-layout section
    (both scopes / histogram only / vectorscope only / hidden), so it is appended here
    rather than built in each scope: the scope under the cursor builds its own items
    first (VectorscopeView's zoom + skin line; HistogramView has none yet, and is the
    place to add histogram-only entries), hands the part-built menu up through
    ScopesView::menuRequested, and this adds the layout section -- after a separator when
    the scope contributed anything -- and execs.

    The menu belongs to the emitting scope (a stack QMenu), so it must be shown before
    returning and nothing here may outlive the call. The actions are MW's, and a QMenu
    does not own actions added to it, so they survive the menu's destruction.
*/
    if (G::isLogger) G::log("MW::showDevelopScopesMenu");
    if (!menu || !developScopesBothAction) return;
    syncDevelopScopesMenu();
    if (!menu->isEmpty()) menu->addSeparator();
    menu->addAction(developScopesBothAction);
    menu->addAction(developScopesHistAction);
    menu->addAction(developScopesVectAction);
    menu->addSeparator();
    menu->addAction(developScopesHideAction);
    menu->exec(globalPos);
}

void MW::closeDevelopScopes()
{
/*
    The scopes strip's own [X] (top right corner): hide the strip, exactly as the Develop
    action-row button does. The layout is left alone, so "G" or the button brings the
    strip back showing the same scope(s) it had.
*/
    if (G::isLogger) G::log("MW::closeDevelopScopes");
    setDevelopScopesVisible(false);
}

void MW::toggleDevelopTransform()
{
/*
    Develop action-row toggle (also bound to "R"): show/hide the Transform
    crop/perspective panel and persist the choice. When shown it brings the Develop
    dock forward so the panel is visible.
*/
    if (G::isLogger) G::log("MW::toggleDevelopTransform");
    /* Transform/Crop only exists in Develop mode. In Preview mode tell the user how to switch and
       leave the panel closed. */
    if (G::operationMode != G::OperationMode::Develop) {
        if (G::popup) G::popup->showPopup("Transform/Crop is only available in Develop Mode, "
                                          "which can be set in the status bar or the shortcut \"D\".",
                                          3000);
        return;
    }
    /* Transform and Spot are MUTUALLY EXCLUSIVE: both own the loupe (crop overlay vs
       spot brush), so opening Transform disarms Spot. Done before the panel is shown so
       the spot tool has released the canvas by the time the crop overlay starts. */
    if (!developTransformVisible && developProperties &&
        developProperties->isSpotActive())
        developProperties->onSpotToolToggled(false);

    developTransformVisible = !developTransformVisible;
    if (transformPanel) transformPanel->setVisible(developTransformVisible);
    if (developTransformAction) developTransformAction->setChecked(developTransformVisible);
    if (developTransformBtn) developTransformBtn->setActive(developTransformVisible);
    settings->setValue("Develop/transformVisible", developTransformVisible);
    if (developTransformVisible && developDock) {
        developDock->setVisible(true);
        developDock->raise();
    }
    /* The crop tool appears whenever the Transform panel is shown (R / action-row
       button), and commits + clears when it is hidden. */
    if (imageView && transformPanel) {
        if (developTransformVisible) enterDevelopCrop();
        else                         exitDevelopCrop();
    }
}

void MW::toggleDevelopReplace()
{
/*
    Action-row spot button (also "S" in Develop mode): arm/disarm the Fill
    Replace (spot/fill/object heal) tool. DevelopProperties owns the armed state; the
    ReplacePanel's visibility tracks it via spotActiveChanged (see createDevelopDock), so
    Escape on the loupe closes the panel through the same path.

    Transform and Spot are MUTUALLY EXCLUSIVE (both drive the loupe), so arming Spot
    closes an open Transform session. toggleDevelopTransform is re-used rather than
    duplicating its hide path, so the crop COMMITS exactly as it does when the user
    closes the panel with R / the action-row button (Esc is the only cancel).
*/
    if (G::isLogger) G::log("MW::toggleDevelopReplace");
    if (G::operationMode != G::OperationMode::Develop) {
        if (G::popup) G::popup->showPopup("Fill Replace is only available in Develop Mode, "
                                          "which can be set in the status bar or the shortcut \"D\".",
                                          3000);
        return;
    }
    if (!developProperties) return;
    const bool arming = !developProperties->isSpotActive();
    if (arming && developTransformVisible) toggleDevelopTransform();
    developProperties->onSpotToolToggled(arming);
}

void MW::toggleDevelopWbSampler()
{
/*
    "W" in Develop mode (and the Develop menu): arm/disarm the Basic panel's white-
    balance dropper -- click a neutral to balance from it, Opt/Alt-click skin to correct
    the skin colour. DevelopProperties owns the armed state, so this just flips it and
    the dock button / cursor / loupe follow.

    W is claimed by an ACTIVE Transform session for Warp (developShortcutIntercept rule
    1a, which runs before the developShortcuts table), so this is only reached when no
    Transform is up -- tool-local beats mode-local, the same ranking S and the brush keys
    already follow.
*/
    if (G::isLogger) G::log("MW::toggleDevelopWbSampler");
    if (G::operationMode != G::OperationMode::Develop) {
        if (G::popup)
            G::popup->showPopup("The white balance sampler is only available in Develop "
                                "Mode, which can be set in the status bar or the "
                                "shortcut \"D\".", 3000);
        return;
    }
    if (developProperties) developProperties->toggleWbDropper();
}

void MW::toggleMaskOverlay()
{
/*
    "O" (Develop mode): hide/show the current scope's mask overlay tint (the red coverage
    visualisation) so the user can see the developed image without it while still editing.
    The visibility state lives in ImageView (per mask-edit session); this just flips it.

    There is nothing to hide or show when no mask is on display -- the Global scope has no
    mask -- so say so rather than let the click (or "O") appear to do nothing.
*/
    if (G::isLogger) G::log("MW::toggleMaskOverlay");
    if (!imageView) return;
    if (!imageView->maskTintAvailable()) {
        if (G::popup)
            G::popup->showPopup("The overlay tint shows a mask's coverage. The Global "
                                "scope has no mask, so there is nothing to tint. Select "
                                "a mask scope (or add a mask) first.", 3000);
        return;
    }
    imageView->toggleMaskTint();
}

void MW::refreshDevelopMaskTintBtn()
{
/*
    The action-row tint button is a colour swatch rather than a glyph: it is filled with
    the overlay colour in force (G::maskOverlayColor, picked from the Mask panel swatches)
    so the row always shows which colour the veil speaks. Called on launch and whenever
    the tint is toggled ("O", the scope menu, a slider auto-hide) or recoloured.

    The swatch is painted 12x12 inside the 16x16 button so BarBtn::setActive's blue border
    has room to read; dimmed to G::iconOpacity when the tint is off, matching the other
    action-row buttons.

    With no mask on display (the Global scope) there is nothing to toggle, so the swatch is
    dimmed further and hollowed out. The button stays ENABLED: its right-click menu (overlay
    colour, grayscale background) is meant to be reachable without a mask, and a disabled
    QToolButton would swallow that too -- the left-click explains itself with a popup.
*/
    if (G::isLogger) G::log("MW::refreshDevelopMaskTintBtn");
    if (!developMaskTintBtn) return;
    const bool available = imageView ? imageView->maskTintAvailable() : false;
    const bool shown = available && imageView->maskTintVisible();

    const int side = 12;
    const qreal dpr = developMaskTintBtn->devicePixelRatioF();
    QPixmap pm(QSize(side, side) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(available ? (shown ? 1.0 : G::iconOpacity) : G::iconOpacity * 0.5);
    p.setBrush(available ? QBrush(G::maskOverlayColor) : QBrush(Qt::NoBrush));
    p.setPen(QPen(available ? QColor(48, 48, 48) : G::maskOverlayColor, 1));
    p.drawRoundedRect(QRectF(0.5, 0.5, side - 1, side - 1), 2, 2);
    p.end();

    developMaskTintBtn->setIcon(QIcon(pm));
    developMaskTintBtn->setActive(shown);
}

void MW::showDevelopMaskTintMenu(const QPoint &pos)
{
/*
    Right-click on the action-row tint button: the overlay's two APPEARANCE settings --
    which colour the veil speaks and whether the image under it is desaturated -- without
    having to open the Mask panel to reach the same chips. Left-click stays the toggle.

    Both settings are pushed through DevelopProperties (setMaskOverlayColour /
    setMaskOverlayGrayscale), which persists them, repaints the Mask panel chips and asks
    for the right redraw, so this menu and the panel can never disagree. The colours come
    from MaskPanel::overlayColours() for the same reason.
*/
    if (G::isLogger) G::log("MW::showDevelopMaskTintMenu");
    if (!developMaskTintBtn || !developProperties) return;

    QMenu menu(developMaskTintBtn);
    const QVector<QColor> &colours = MaskPanel::overlayColours();
    const QStringList &names = MaskPanel::overlayColourNames();
    for (int i = 0; i < colours.size(); ++i) {
        const QColor c = colours.at(i);
        QPixmap chip(16, 16);
        chip.fill(c);
        QAction *a = menu.addAction(QIcon(chip),
                                    i < names.size() ? names.at(i) : c.name());
        a->setCheckable(true);
        a->setChecked(c.rgb() == G::maskOverlayColor.rgb());
        /* No repaint of the button here: setMaskOverlayColour emits
           maskOverlayRefreshRequested, which refreshDevelopMaskTintBtn is wired to. */
        connect(a, &QAction::triggered, this, [this, c]{
            developProperties->setMaskOverlayColour(c);
        });
    }
    menu.addSeparator();
    QAction *gray = menu.addAction(tr("Grayscale background"));
    gray->setCheckable(true);
    gray->setChecked(G::maskOverlayGrayscale);
    gray->setToolTip(tr("Show the image in grayscale while the overlay is on,\n"
                        "so the overlay colour is easier to see (view only)"));
    connect(gray, &QAction::triggered, this, [this](bool on){
        developProperties->setMaskOverlayGrayscale(on);
    });

    menu.exec(developMaskTintBtn->mapToGlobal(pos));
}

void MW::developNewScope()
{
/*
    "N" (Develop mode): add a scope to the current image's edit stack. DevelopProperties
    owns the flow (name dialog, append, select the new scope) and no-ops without a current
    image; this just brings the dock forward so the new scope's tree is visible.
*/
    if (G::isLogger) G::log("MW::developNewScope");
    if (!developProperties) return;
    if (developDock) { developDock->setVisible(true); developDock->raise(); }
    developProperties->newScope();
}

void MW::developSavePreset()
{
/*
    Cmd+Shift+N (Develop mode only): save the current image's develop state as a named,
    reusable preset. DevelopProperties owns the flow (checklist dialog + QSettings write)
    and no-ops with a message when there is no current image or it has no edits.
*/
    if (G::isLogger) G::log("MW::developSavePreset");
    if (!developProperties) return;
    developProperties->saveDevelopPreset();
}

void MW::developCopySettings()
{
/*
    Cmd+Opt+C (Develop mode only): copy the ticked develop settings from this image to the
    develop clipboard -- Lightroom's Copy Settings. DevelopProperties owns the flow (the
    Save Preset checklist in Copy mode, then the clipboard write) and reports its own
    messages when there is no image or it has no edits.
*/
    if (G::isLogger) G::log("MW::developCopySettings");
    if (!developProperties) return;
    developProperties->copyDevelopSettings();
}

void MW::developPasteSettings()
{
/*
    Cmd+Opt+V (Develop mode only): merge the develop clipboard onto the current image, on
    the active scope, as one history step -- Lightroom's Paste Settings. Says so when
    nothing has been copied yet.
*/
    if (G::isLogger) G::log("MW::developPasteSettings");
    if (!developProperties) return;
    developProperties->pasteDevelopSettings();
}

void MW::developAddToMask()
{
/*
    "M" (Develop mode): pop the Add/Subtract mask-tool menu at the cursor for the active
    scope, the same menu the tree's [+] mask row shows. DevelopProperties handles the Global
    scope case (it cannot be masked) with its own message.
*/
    if (G::isLogger) G::log("MW::developAddToMask");
    if (!developProperties) return;
    if (developDock) { developDock->setVisible(true); developDock->raise(); }
    developProperties->showMaskMenu();
}

bool MW::prepareExport(QStringList &targets)
{
/*
    Shared entry work for every export path: flush anything that has not reached the
    sidecars yet, build the list of selected images, and lazily create the exporter and
    the preset store. Returns false (with a popup) when there is nothing to export.

    The flush matters. A develop edit made across a multi-image selection is BATCHED
    (DevelopProperties::kPropagateMs), and the exporter reads each image's stored stack --
    so without flushing first, an export fired straight after a slider drag would write
    the pre-edit recipe for every image except the current one.
*/
    if (G::isLogger) G::log("MW::prepareExport");

    if (developProperties) developProperties->flushPropagation();
    if (developProperties) developProperties->flushAll();

    targets.clear();
    if (dm && dm->selectionModel) {
        const QModelIndexList rows = dm->selectionModel->selectedRows();
        for (const QModelIndex &idx : rows) {
            const QString fPath = idx.data(G::PathRole).toString();
            if (!fPath.isEmpty()) targets << fPath;
        }
    }
    if (targets.isEmpty() && !dm->currentFilePath.isEmpty())
        targets << dm->currentFilePath;

    if (targets.isEmpty()) {
        if (G::popup) G::popup->showPopup("No images selected to export", 1500);
        return false;
    }

    if (!exportPresets) exportPresets = new ExportPresets(settings, this);
    if (!imageExporter) imageExporter = new ImageExporter(dm, metadata, this);
    return true;
}

void MW::developPixelSource(const QString &fPath, bool want16Bit,
                            OutputTransform::Space space,
                            std::function<void(bool, const QImage &)> done)
{
/*
    Render one image's FULL develop recipe for the exporter.

    This is renderDevelopFullResAsync's staging, reused: the GUI thread owns the datamodel
    reads, the stack capture, the orientation and the mask prerequisites (they are
    synchronous and keep path-keyed caches that the live edit session also uses), while
    the decode and the composite -- the seconds-long parts -- run on developRenderPool. It
    is written as a chain rather than a blocking call so the exporter's batch never holds
    the GUI thread across an image.

    Unlike the preview path this uses stackJobFor(fPath): every image gets its OWN stored
    recipe, which is the whole point of a batch export.
*/
    if (G::isLogger) G::log("MW::developPixelSource");

    if (fPath.isEmpty() || !developProperties) { done(false, QImage()); return; }

    const auto depth = want16Bit ? WorkingImageCache::OutDepth::Sixteen
                                 : WorkingImageCache::OutDepth::Eight;
    /* The develop render ENCODES into the chosen space, so the image handed back is
       tagged with it below -- ImageExporter::save() then writes that tag through instead
       of converting. */
    const QColorSpace outSpace = OutputTransform::ColorSpaceOf(space);

    /* GUI thread: this image's stored recipe, and the metadata the decoder needs. */
    const DevelopProperties::StackRenderJob mj = developProperties->stackJobFor(fPath);
    ImageMetadata m = dm->imMetadata(fPath);
    if (m.fPath.isEmpty()) m.fPath = fPath;
    if (m.ext.isEmpty()) m.ext = QFileInfo(fPath).suffix().toLower();

    /* "Denoise raw" is a property of the BASE, applied before the composite. The
       interactive path gets there through developRawDenoisedBase / ensureRawDenoise, and
       NEITHER is reachable from here -- both are keyed to the image on screen. Without the
       block below an export, and a devPreview written by the builder, would come out CLEAN
       while the loupe showed a denoised render, and the devPreview would carry a recipe key
       asserting the two match.

       Gated on the GUI thread: rawDenoiseAvailable reads session state (the unsupported-
       sensor set). Only the heavy PMRID decode goes to the worker.
       GATED ON THE RECIPE, not on the amounts alone. EditParams defaults denoiseLuma to
       0.75 and denoiseChroma to 1.0 -- NON-ZERO -- so "amounts > 0" is true for every raw
       including an unedited one, and testing only that would run PMRID on every image the
       exporter and the preview builder touch. EditParams::denoiseRaw carries the stored
       intent (falling back to the Auto run preference when the image says nothing), so
       this reads the same answer the interactive render does. */
    const bool wantDenoise =
        mj.global.wantsDenoiseRaw(G::autoRunDenoise) &&
        (mj.global.denoiseLuma > 0.0f || mj.global.denoiseChroma > 0.0f) &&
        G::decodeRawEngine == G::DecodeRawEngine::winnowDecodeRawEngine &&
        rawDenoiseAvailable(fPath);
    /* Reuse the live session's full-strength PMRID base when it belongs to THIS image, so
       exporting the image being edited does not re-run the model. It is amount-independent
       (keyed path+iso), so it is valid whatever the stored amounts are. Read here rather
       than on the worker because developPmridFull/developPmridKey are GUI-thread state. */
    std::shared_ptr<const WorkingImage> pmridCached;
    if (wantDenoise && developPmridFull && developPmridKey == pmridBaseKey(fPath, m.ISONum))
        pmridCached = developPmridFull;

    /* Step 1 (worker): make sure the scene-linear WorkingImage exists. decodeIndependent
       caches it as a side effect, so an image already visited is a cache hit. */
    developRenderPool->start([this, fPath, m, mj, depth, space, outSpace, wantDenoise,
                              pmridCached, done]() mutable {
        auto work = WorkingImageCache::instance().get(fPath);
        /* A cached base is only usable here if it IS the sensor image. The cache is keyed
           by path alone and Preview mode fills it from the embedded JPEG (a DNG's is
           typically 1024 px), so reusing it would render this image's export -- or its
           devPreview, the thing the loupe shows at 100% -- from a thumbnail. Compare
           against the CFA active area and decode again when the entry is nowhere near it;
           half the long edge separates a preview from a sensor image (which lands on the
           active area or the slightly smaller default crop) without being brittle. */
        if (work && G::useRaw && m.rawInfo.isRaw && m.rawInfo.width > 0) {
            const int sensorEdge = qMax(m.rawInfo.width, m.rawInfo.height);
            if (qMax(work->width, work->height) * 2 < sensorEdge) work.reset();
        }
        bool decodedHere = false;
        if (!work) {
            ImageDecoder dec(0, dm, metadata);
            QImage img;
            dec.decodeIndependent(img, metadata, m, nullptr);
            work = WorkingImageCache::instance().get(fPath);
            decodedHere = true;
            if (!work && !img.isNull()) {
                /* Display-referred format (or the raw decode failed): build the working
                   image from the decoded 8-bit image, as renderDevelopPreview does. */
                auto built = std::make_shared<WorkingImage>();
                InputTransform input;
                if (input.FromImage(img, *built)) {
                    WorkingImageCache::instance().put(fPath, built);
                    work = built;
                }
            }
        }
        if (!work) {
            QMetaObject::invokeMethod(this, [done]() { done(false, QImage()); });
            return;
        }

        /* Raw denoise -> the base the COMPOSITE starts from. PMRID is pre-demosaic, so the
           denoised base comes from re-decoding the mosaic with the denoiser on and blending
           toward the clean base by the two stored amounts -- the same two steps
           ensureRawDenoise performs interactively, minus its caching, which is keyed to the
           current image and would be wrong to disturb from a batch.

           Only the composite gets it. Orientation and the mask prerequisites below stay on
           the CLEAN base, exactly as renderDevelopFullResAsync splits src from work, so a
           mask selects the same pixels whether or not denoise is on.

           A PMRID that cannot run (built without ONNX Runtime, pmrid.onnx absent, non-Bayer
           sensor) still returns a valid base -- one identical to the clean one -- so the
           blend is skipped on applied == false rather than mixing in an unchanged image.
           Nothing is recorded as unavailable here: reportRawDenoiseUnavailable drives the
           dock, and this render is not the user's edit session. */
        std::shared_ptr<const WorkingImage> src = work;
        if (wantDenoise) {
            std::shared_ptr<const WorkingImage> pmrid = pmridCached;
            bool applied = (pmrid != nullptr);
            if (!pmrid) {
                ImageDecoder dec(0, dm, metadata);
                pmrid = dec.decodeRawWorking(m, /*denoiseRaw*/true, nullptr, nullptr,
                                             &applied);
            }
            if (pmrid && applied) {
                auto blended = std::make_shared<WorkingImage>();
                Develop::BlendRawDenoise(*work, *pmrid, mj.global.denoiseLuma,
                                         mj.global.denoiseChroma, *blended);
                src = blended;
            }
        }

        /* Step 2 (GUI thread): orientation + the mask prerequisites, which must not run
           concurrently with the live session's use of the same path-keyed caches. */
        QMetaObject::invokeMethod(this, [this, fPath, work, src, mj, depth, space, outSpace,
                                         decodedHere, done]() {
            const int degrees = work->sceneReferred
                                    ? developOrientationDegrees(*work, fPath) : 0;

            if (stackHasRangeMask(mj))   ensureRangeRef(fPath, *work, mj.global, degrees);
            if (stackHasSubjectMask(mj)) ensureSubjectMask(fPath, *work, mj.global, degrees);
            if (stackHasSkyMask(mj))     ensureSkyMask(fPath, *work, mj.global, degrees);
            if (stackHasDepthMask(mj))   ensureDepthMask(fPath, *work, mj.global, degrees);
            if (stackHasObjectMask(mj))
                for (const DevelopProperties::StackRenderJob::Scope &L : mj.scopes)
                    for (const MaskComponent &c : L.components)
                        if (c.tool == int(MaskTool::Object))
                            ensureObjectMask(fPath, *work, mj.global, degrees, c.paramsJson);

            /* Step 3 (worker): the composite itself. src is the raw-denoised base when
               "Denoise raw" is set, else the clean one. work is kept alive alongside it
               because the mask fields registered above are keyed to those pixels. */
            developRenderPool->start([this, fPath, work, src, mj, degrees, depth, space,
                                      outSpace, decodedHere, done]() {
                QImage out = developCompositeStack(*src, mj, degrees,
                                                   /*fullRes*/true, 0, 0, fPath,
                                                   nullptr, depth, space);
                /* Tag at the export boundary, not inside the composite: the geometry and
                   spot stages round-trip through OpenCV / fresh QImages, which would drop
                   a tag set any earlier. */
                if (!out.isNull()) out.setColorSpace(outSpace);
                /* Leave the interactive cache as we found it: a long batch would
                   otherwise evict the image the user is actively editing (the cache is
                   byte-budgeted and this walks every selected image). */
                if (decodedHere && fPath != dm->currentFilePath)
                    WorkingImageCache::instance().remove(fPath);
                QMetaObject::invokeMethod(this, [done, out]() {
                    done(!out.isNull(), out);
                });
            });
        });
    });
}

void MW::previewPixelSource(const QString &fPath,
                            std::function<void(bool, const QImage &)> done)
{
/*
    The plain (no develop recipe) pixel source behind File > Save Preview as: the same
    Pixmap decode the old SaveAsDlg used, so that command's output is unchanged apart
    from now going through the shared exporter -- which gives it the naming, resizing,
    ICC tag and metadata copy it never had.
*/
    if (G::isLogger) G::log("MW::previewPixelSource");

    Pixmap pixmap(this, dm, metadata);
    QImage image;
    QString path = fPath;                    // Pixmap::load takes a non-const reference
    const bool ok = pixmap.load(path, image, "MW::previewPixelSource");
    done(ok && !image.isNull(), image);
}

void MW::onExportFinished(const ImageExporter::Result &result, bool addToFolderView)
{
/*
    Report the outcome, and surface the new files if they landed in a folder that is
    currently loaded -- otherwise an export into the folder you are looking at appears to
    have done nothing until a manual refresh.
*/
    if (G::isLogger) G::log("MW::onExportFinished");

    QString msg;
    if (result.aborted)
        msg = "Export cancelled -- " + QString::number(result.written.count()) + " written";
    else
        msg = "Exported " + QString::number(result.written.count()) + " image" +
              (result.written.count() == 1 ? "" : "s");
    if (!result.skipped.isEmpty())
        msg += ", " + QString::number(result.skipped.count()) + " skipped";
    if (!result.failed.isEmpty())
        msg += ", " + QString::number(result.failed.count()) + " failed";
    if (G::popup) G::popup->showPopup(msg, 2500);

    if (result.written.isEmpty()) return;

    bool loadedFolderTouched = false;
    for (const QString &folder : result.folders)
        if (dm && dm->isFolderLoaded(folder)) loadedFolderTouched = true;

    if (loadedFolderTouched) {
        if (addToFolderView) insertFiles(result.written);
        else refresh();
    }
    fsTree->updateCount();
    bookmarks->updateCount();
}

void MW::developExport()
{
/*
    "X" (Develop mode): export the selected images with their develop recipes applied.
    Opens the shared export dialog with the DEVELOP pixel source; the dialog collects the
    settings and drives ImageExporter, which stays alive after the dialog closes only long
    enough to finish (the dialog stays open for the duration).
*/
    if (G::isLogger) G::log("MW::developExport");

    QStringList targets;
    if (!prepareExport(targets)) return;

    imageExporter->setPixelSource(
        [this](const QString &fPath, ImageExporter::Done done) {
            const ExportSettings &es = imageExporter->activeSettings();
            developPixelSource(fPath, es.wants16Bit(),
                               ImageExporter::renderSpace(es.space), done);
        });

    ExportDlg dlg(imageExporter, exportPresets, targets, dm->currentFilePath,
                  filenameTemplates, ExportDlg::Mode::Develop, this);
    QMetaObject::Connection c = connect(imageExporter, &ImageExporter::finished, this,
        [this](const ImageExporter::Result &r) {
            onExportFinished(r, imageExporter->activeSettings().addToFolderView);
        });
    dlg.exec();
    disconnect(c);
    if (exportPresets) exportPresets->writeLast(dlg.settings());
}

void MW::developExportWithPreset(const QString &presetName)
{
/*
    Develop > Export with preset > <name>: run a saved export preset over the selection
    with NO dialog. An export preset is complete (it carries its destination), so there is
    nothing left to ask -- progress and the result go through G::popup instead.
*/
    if (G::isLogger) G::log("MW::developExportWithPreset");

    QStringList targets;
    if (!prepareExport(targets)) return;
    if (!exportPresets->contains(presetName)) {
        if (G::popup) G::popup->showPopup("Export preset not found: " + presetName, 2000);
        return;
    }

    const ExportSettings s = exportPresets->read(presetName);
    if (s.dest == ExportSettings::ChosenFolder && s.folderPath.isEmpty()) {
        if (G::popup)
            G::popup->showPopup("The export preset \"" + presetName +
                                "\" has no destination folder.", 2500);
        return;
    }

    imageExporter->setPixelSource(
        [this](const QString &fPath, ImageExporter::Done done) {
            const ExportSettings &es = imageExporter->activeSettings();
            developPixelSource(fPath, es.wants16Bit(),
                               ImageExporter::renderSpace(es.space), done);
        });

    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(targets.count());
    G::popup->showPopup("Exporting " + QString::number(targets.count()) +
                        " images with \"" + presetName + "\"", 0, true, 1);

    /* Single-shot connections: the exporter outlives this call, so both are dropped in
       the finished handler (which needs its own handle, hence the shared holder). */
    auto conns = std::make_shared<QVector<QMetaObject::Connection>>();
    conns->append(connect(imageExporter, &ImageExporter::progress, this,
        [](int done, int) { if (G::popup) G::popup->setProgress(done); }));
    conns->append(connect(imageExporter, &ImageExporter::finished, this,
        [this, conns](const ImageExporter::Result &r) {
            if (G::popup) { G::popup->setProgressVisible(false); G::popup->reset(); }
            const bool addToView = imageExporter->activeSettings().addToFolderView;
            for (const QMetaObject::Connection &c : *conns) disconnect(c);
            onExportFinished(r, addToView);
        }));

    imageExporter->run(targets, s);
}

void MW::buildDevelopExportPresetMenu()
{
/*
    Rebuild Develop > Export with preset from the preset store, the same way
    embelExportMenu is rebuilt from the embellish templates. Disabled (and empty) until
    the user has saved a preset.
*/
    if (!developExportPresetMenu) return;
    developExportPresetMenu->clear();

    if (!exportPresets) exportPresets = new ExportPresets(settings, this);
    const QStringList names = exportPresets->names();
    developExportPresetMenu->setEnabled(!names.isEmpty());
    for (const QString &name : names) {
        QAction *a = developExportPresetMenu->addAction(name);
        connect(a, &QAction::triggered, this,
                [this, name]() { developExportWithPreset(name); });
    }
}

void MW::enterDevelopCrop()
{
/*
    Begin editing the crop. Show the FULL developed frame (developCropEditing suppresses the stored
    geometry in the render) so the overlay can be positioned over the whole image, then start the
    crop overlay from the stored crop rectangle.
*/
    if (G::isLogger) G::log("MW::enterDevelopCrop");
    if (!imageView || !transformPanel || developCropEditing) return;
    developCropEditing = true;
    developCropShowResult = false;               // start in editing (overlay), not result-preview
    /* Snapshot pre-session geometry so Esc (cancelDevelopTransform) can revert it. */
    developCropGeometryBackup = developProperties->currentGeometry();
    transformPanel->setPreviewShown(false);      // eye reflects "editing", not "showing result"
    renderDevelopPreview(false);     // full frame (geometry suppressed); refits if it was cropped
    const Geometry g = developProperties->currentGeometry();
    imageView->beginCropEdit(transformPanel->aspectRatio(), transformPanel->isAspectLocked(),
                             transformPanel->isAspectFlipped(),
                             QRectF(g.cropX, g.cropY, g.cropW, g.cropH));
    transformPanel->setLevelAngle(g.straighten);         // show the stored angle in the Level field
    setDevelopTransformMode(transformPanel->mode());     // arm the panel's current mode's tool
}

void MW::exitDevelopCrop()
{
/*
    Commit the crop: write the overlay's rectangle into the image's EditStack geometry (persists via
    the sidecar) and re-render with the geometry applied, so the loupe shows the cropped result.
*/
    if (G::isLogger) G::log("MW::exitDevelopCrop");
    if (!imageView || !developCropEditing) return;
    /* When result-preview is on, the overlay was already committed + dropped at the toggle, so only
       read/commit the overlay crop when we are still in editing mode. */
    if (!developCropShowResult) {
        const QRectF crop = imageView->cropRect();
        imageView->endCropEdit();
        if (developProperties) {
            Geometry g = developProperties->currentGeometry();
            g.cropX = crop.x(); g.cropY = crop.y(); g.cropW = crop.width(); g.cropH = crop.height();
            developProperties->setCurrentGeometry(g);
        }
    }
    developCropEditing = false;
    developCropShowResult = false;
    renderDevelopPreview(false);     // geometry applied -> cropped result
}

void MW::cancelDevelopTransform()
{
/*
    Esc while the Transform panel is open: cancel the session instead of committing it.
    Discard every crop/straighten/warp change made since the panel opened by restoring the
    geometry snapshot taken in enterDevelopCrop, drop the crop overlay WITHOUT reading its
    rectangle, and hide the panel with the same visibility bookkeeping as
    toggleDevelopTransform's hide path (minus the commit). Counterpart to exitDevelopCrop.
*/
    if (G::isLogger) G::log("MW::cancelDevelopTransform");
    if (!developTransformVisible || !developCropEditing) return;

    if (imageView) imageView->endCropEdit();     // drop overlay; do NOT commit cropRect()
    if (developProperties)
        developProperties->setCurrentGeometry(developCropGeometryBackup);   // revert
    developCropEditing = false;
    developCropShowResult = false;

    /* Hide the panel (toggleDevelopTransform's hide branch, without exitDevelopCrop). */
    developTransformVisible = false;
    if (transformPanel) transformPanel->setVisible(false);
    if (developTransformAction) developTransformAction->setChecked(false);
    if (developTransformBtn) developTransformBtn->setActive(false);
    settings->setValue("Develop/transformVisible", false);

    renderDevelopPreview(false);     // restored geometry applied -> pre-session result
}

void MW::rectifyDevelopCrop()
{
/*
    Commit the 4-point warp (Rectify button). Store the traced quad + the suggested (largest
    inscribed) crop into the image's geometry, then re-render: because the crop tool is still active
    the render KEEPS the warp but suppresses the crop, so the loupe shows the CORRECTED canvas and
    the crop overlay is restarted on it for the user to set the final crop. Non-destructive: the
    warp is a stored parameter, not baked pixels.
*/
    if (G::isLogger) G::log("MW::rectifyDevelopCrop");
    if (!imageView || !developProperties || !developCropEditing) return;
    if (!imageView->cropIsWarp()) return;

    const QRectF suggested = imageView->computeRectifyCrop();
    if (!suggested.isValid()) return;            // degenerate quad: leave the overlay as-is

    Geometry g = developProperties->currentGeometry();
    g.hasWarp = true;
    imageView->cropQuad(g.quad);                 // traced corners (oriented-image normalized space)
    g.cropX = suggested.x();     g.cropY = suggested.y();
    g.cropW = suggested.width(); g.cropH = suggested.height();
    developProperties->setCurrentGeometry(g);

    renderDevelopPreview(false);                 // warp kept, crop suppressed -> corrected canvas
    /* Restart the crop overlay (rectangle mode) on the corrected canvas at the suggested crop, and
       return the panel toggle to Crop so the UI matches the rectangle overlay. */
    imageView->beginCropEdit(transformPanel->aspectRatio(), transformPanel->isAspectLocked(),
                             transformPanel->isAspectFlipped(),
                             suggested);
    transformPanel->setMode(TransformPanel::CropMode);
}

void MW::applyDevelopLevel(double deltaDeg)
{
/*
    A level line was drawn on the (straighten-applied) frame. Add its angle to the stored straighten,
    auto-fit the crop to the rotation wedges (analytic largest inscribed rect, using the developed
    frame's dimensions), then re-render the straightened canvas and restart the crop overlay.
*/
    if (G::isLogger) G::log("MW::applyDevelopLevel");
    if (!imageView || !developProperties || !developCropEditing) return;
    if (deltaDeg == 0.0) return;

    const QString fPath = dm->currentFilePath;
    auto work = WorkingImageCache::instance().get(fPath);
    if (!work) return;
    int fw = work->width, fh = work->height;
    if (work->sceneReferred) {
        const int deg = developOrientationDegrees(*work, fPath);
        if (deg == 90 || deg == 270) std::swap(fw, fh);
    }

    Geometry g = developProperties->currentGeometry();
    g.straighten = qBound(-45.0, g.straighten + deltaDeg, 45.0);
    /* Auto-crop to the wedges (analytic; only meaningful without a warp -- with a warp the crop is
       in post-warp space, so leave it and let the user re-crop). */
    if (!g.hasWarp) {
        const QRectF c = CropTransform::straightenCropNorm(fw, fh, g.straighten);
        g.cropX = c.x(); g.cropY = c.y(); g.cropW = c.width(); g.cropH = c.height();
    }
    developProperties->setCurrentGeometry(g);

    renderDevelopPreview(false);                 // straighten kept, crop suppressed -> level canvas
    imageView->beginCropEdit(transformPanel->aspectRatio(), transformPanel->isAspectLocked(),
                             transformPanel->isAspectFlipped(),
                             QRectF(g.cropX, g.cropY, g.cropW, g.cropH));
    transformPanel->setLevelAngle(g.straighten); // reflect the new angle in the Level field
}

void MW::setDevelopTransformMode(int mode)
{
/*
    The Transform panel's Crop / Level / Warp toggle changed: arm the matching ImageView tool on the
    live crop overlay. Crop restarts the rectangle overlay from the stored crop; Level arms the
    draw-a-level tool; Warp seeds the 4-point quad for corner dragging. Only meaningful while the
    crop editor is active (the panel is only visible then).
*/
    if (G::isLogger) G::log("MW::setDevelopTransformMode");
    if (!imageView || !developProperties || !developCropEditing) return;
    switch (mode) {
    case TransformPanel::CropMode: {
        const Geometry g = developProperties->currentGeometry();
        imageView->beginCropEdit(transformPanel->aspectRatio(), transformPanel->isAspectLocked(),
                                 transformPanel->isAspectFlipped(),
                                 QRectF(g.cropX, g.cropY, g.cropW, g.cropH));
        break;
    }
    case TransformPanel::LevelMode:
        imageView->beginLevel();
        break;
    case TransformPanel::WarpMode:
        imageView->beginWarp();
        break;
    }
}

void MW::setDevelopLevelAngle(double degrees)
{
/*
    An absolute straighten angle was typed into the Level field. Convert it to a delta and reuse the
    drawn-level path so the crop auto-fits and the canvas re-renders identically.
*/
    if (G::isLogger) G::log("MW::setDevelopLevelAngle");
    if (!imageView || !developProperties || !developCropEditing) return;
    const Geometry g = developProperties->currentGeometry();
    const double target = qBound(-45.0, degrees, 45.0);
    if (target == g.straighten) return;
    applyDevelopLevel(target - g.straighten);
}

void MW::resetDevelopTransformMode(int mode)
{
/*
    A per-row Transform reset ([R] on a mode line): clear just that mode's contribution to the
    geometry, leave the others, then re-render and restart the crop overlay on the result.
*/
    if (G::isLogger) G::log("MW::resetDevelopTransformMode");
    if (!imageView || !developProperties || !developCropEditing) return;
    Geometry g = developProperties->currentGeometry();
    switch (mode) {
    case TransformPanel::CropMode:
        /* Reset the aspect to "As shot" (free) first, else beginCropEdit below re-fits
           the full frame to the locked aspect and the reset appears to do nothing. */
        transformPanel->setAspectAsShot();
        g.cropX = 0.0; g.cropY = 0.0; g.cropW = 1.0; g.cropH = 1.0;
        break;
    case TransformPanel::LevelMode:
        g.straighten = 0.0;
        transformPanel->setLevelAngle(0.0);
        break;
    case TransformPanel::WarpMode: {
        g.hasWarp = false;
        const double identity[8] = {0,0, 1,0, 1,1, 0,1};
        for (int k = 0; k < 8; ++k) g.quad[k] = identity[k];
        break;
    }
    }
    developProperties->setCurrentGeometry(g);
    developCropShowResult = false;
    transformPanel->setPreviewShown(false);
    renderDevelopPreview(false);
    imageView->beginCropEdit(transformPanel->aspectRatio(), transformPanel->isAspectLocked(),
                             transformPanel->isAspectFlipped(),
                             QRectF(g.cropX, g.cropY, g.cropW, g.cropH));
    /* beginCropEdit re-arms the crop cursor; restore the row's own tool so resetting the
       Level/Warp row keeps its cursor instead of dropping back to the crop tool. */
    setDevelopTransformMode(mode);
}

void MW::onImageCursorPos(double xFraction, double yFraction)
{
/*
    The loupe cursor is at (xFraction, yFraction) of the displayed image. Sample that one pixel
    from the cached shown QImage (O(1)) and drive the scopes' readout marker. Cheap no-op while
    the scopes are hidden or no image is shown.
*/
    if (!scopesView || !developScopesVisible) return;
    if (developShownImage.isNull()) { scopesView->clearMarker(); return; }

    const int x = qBound(0, static_cast<int>(xFraction * developShownImage.width()),
                         developShownImage.width() - 1);
    const int y = qBound(0, static_cast<int>(yFraction * developShownImage.height()),
                         developShownImage.height() - 1);
    const QRgb p = developShownImage.pixel(x, y);
    scopesView->setMarker(qRed(p), qGreen(p), qBlue(p));
}

bool MW::isValidPath(QString &path)
{
    if (G::isLogger) G::log("MW::isValidPath");
    QDir checkPath(path);
    if (!checkPath.exists() || !checkPath.isReadable()) {
        return false;
    }
    return true;
}

void MW::removeBookmark()
{
    if (G::isLogger)
        G::log("MW::removeBookmark", QApplication::focusWidget()->objectName());
    if (QApplication::focusWidget() == bookmarks) {
        bookmarks->removeBookmark();
        bookmarks->saveBookmarks(settings);
        return;
    }
}

void MW::refreshBookmarks()
{
/*
    This is run from the bookmarks (fav) panel context menu to update the image count for each
    bookmark folder.
*/
    if (G::isLogger) G::log("MW::refreshBookmarks");
    bookmarks->updateCount();
}

void MW::updateState()
{
/*
    Called when program starting or when a workspace is invoked. Based on the condition
    of actions sets the visibility of all window components.
*/
    if (G::isLogger) G::log("MW::updateState");
    // set flag so
    isUpdatingState = true;
    //setWindowsTitleBarVisibility();   // problem with full screen toggling
    // setCentralView has to precede setting visibility to docks
    setCentralView();
    setMenuBarVisibility();
    setStatusBarVisibility();
    setCacheStatusVisibility();
    setFolderDockVisibility();
    setFavDockVisibility();
    setFilterDockVisibility();
    setCatalogDockVisibility();
    setMetadataDockVisibility();
    setEmbelDockVisibility();
    setDevelopDockVisibility();
    setHistoryDockVisibility();     // follows Develop (set just above)
    setPresetsDockVisibility();     // ditto
    setThumbDockVisibity();
    // setShootingInfoVisibility();
    updateStatusBar();
    //setActualDevicePixelRation();
    isUpdatingState = false;
    //reportState();
}

void MW::refreshFolders()
{
/*
    Update image counts in FSTree and Bookmarks.

    Called by FSTree and Bookmarks buttons, Menu File > Refresh folders
    and MW::openUsbFolder
*/
    refresh();
    return;

    if (G::isLogger) G::log("MW::refreshFolders");
    bool showImageCount = fsTree->isShowImageCount();
    fsTree->refreshModel();
    fsTree->updateCount();
    return;

    // make folder panel visible and set focus
    folderDock->raise();
    folderDockVisibleAction->setChecked(true);

    // set sort forward (not reverse)
    if (sortReverseAction->isChecked()) {
        sortReverseAction->setChecked(false);
        sortChange("MW::refreshFolders");
        reverseSortBtn->setIcon(QIcon(":/images/icon16/A-Z.png"));
    }

    // do not embellish
    embelProperties->invokeFromAction(embelTemplatesActions.at(0));
}

void MW::newEmbelTemplate()
{
    if (G::isLogger) G::log("MW::newEmbelTemplate");
    embelDock->setVisible(true);
    embelDock->raise();
    embelDockVisibleAction->setChecked(true);
    embelProperties->newTemplate();
    loupeDisplay("MW::newEmbelTemplate");
}

void MW::changeInfoOverlay()
{
    if (G::isLogger) G::log("MW::tokenEditor");

    if (!infoVisibleAction->isChecked()) {
        infoVisibleAction->setChecked(true);
        imageView->infoOverlay->setVisible(infoVisibleAction->isChecked());
    }
    imageView->changeInfoOverlay();
    embelProperties->updateMetadataTemplateList();
}

void MW::exportEmbelFromAction(QAction *embelExportAction)
{
/*
    Called from main menu Embellish > Export > Export action.  The embellish editor
    may not be active, but the embellish template has been chosen by the action.
*/
    if (G::isLogger) G::log("MW::exportEmbelFromAction");

    QStringList picks;
    dm->getSelectionOrPicks(picks);

    if (picks.size() == 0)  {
        QMessageBox::information(this,
            "Oops", "There are no picks or selected images to export.    ",
            QMessageBox::Ok);
        return;
    }

    EmbelExport embelExport(metadata, dm, icd, embelProperties);
    connect(this, &MW::abortEmbelExport, &embelExport, &EmbelExport::abortEmbelExport);

//    embelExport.exportRemoteFiles(embelExportAction->text(), picks);

    embelProperties->setCurrentTemplate(embelExportAction->text());
    G::isProcessingExportedImages = true;
    bool isRemote = true;
    embelExport.exportImages(picks, isRemote);
    embelProperties->doNotEmbellish();
    G::isProcessingExportedImages = false;
    bookmarks->updateCount();
}

void MW::exportEmbel()
{
/*
    Called when embellish editor is active and an embellish template has been selected.
*/
    if (G::isLogger) G::log("MW::exportEmbel");

    QStringList picks;
    dm->getSelectionOrPicks(picks);

    if (picks.size() == 0)  {
        QMessageBox::information(this,
            "Oops", "There are no picks or selected images to export.    ",
            QMessageBox::Ok);
        return;
    }

    EmbelExport embelExport(metadata, dm, icd, embelProperties);
    connect(this, &MW::abortEmbelExport, &embelExport, &EmbelExport::abortEmbelExport);

    embelExport.exportImages(picks);
}

void MW::ingest()
{
/*
    Copies images from a source location (usually a camera media card) to one or more
    destinations.  Ingestion comprises of three components:

    1.  MW::ingest()

        This function keeps track of the prevSourceFolder and baseFolderDescription during
        subsequent calls.  It uses this to control the behavior of the IngestDlg when it is
        called.  When IngestDlg closes persistent ingest data is saved in settings.  If the
        isBackgroundIngest flag == true then a backgroundIngest instantiation of Ingest is
        created and run.

    2.  IngestDlg

        When this dialog is invoked the files that have been picked are copied to a primary
        destination folder, and optionally, to a backup location. This process is known as
        ingestion: selecting and copying images from a camera card to a computer. If backround
        ingesting is selected then the dialog closes and the user can continue working while
        the ingest happens in the background in another thread, using all the IngestDlg
        settings.

        The destination folder can be selected/created manually or automatically.  If
        automatic, a root folder is selected/created, a path to the destination folder
        is defined using tokens, and the destination folder description defined.  The
        picked images can be renamed during this process.

        Files are copied to a destination based on building a file path consisting of:

              Root Folder                   (rootFolderPath)
            + Path to base folder           (fromRootToBaseFolder) source pathTemplateString
            + Global Folder Description       (baseFolderDescription)
            + File Name                     (fileBaseName)     source filenameTemplateString
            + File Suffix                   (fileSuffix)

            ie E:/2018/201802/2018-02-08 Rory birthday/2018-02-08_0001.NEF

            rootFolderPath:         ie "E:/" where all the images are located
            fromRootToBaseFolder:   ie "2018/201802/2018-02-08" from the path template
            baseFolderDescription:  ie " Rory birthday" a description appended to the
                                        pathToBaseFolder
            fileBaseName:           ie "2018-02-08_0001" from the filename template
            fileSuffix:             ie ".NEF"

            folderPath:             ie "E:/2018/201802/2018-02-08 Rory birthday/"
                = rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
                = the copy to destination

            The strings fromRootToBaseFolder and the fileBaseName can be tokenized in
            TokenDlg, allowing the user to automate the construction of the entire destination
            file path.

    3.  Ingest (class)

        Ingest duplicates the actual ingest process that runs in the IngestDlg, but runs in a
        separate thread after the IngestDlg closes.  Progress is updated in progressBar at the
        extreme left in the status bar.
*/

    if (G::isLogger) G::log("MW::ingest");

    // flush any unsaved per-image Develop edits so their sidecars are current before they are copied
    if (developProperties) developProperties->flushAll();

    // check if background ingest in progress
    if (G::isRunningBackgroundIngest) {
        QString msg =
                "There is a background ingest in progress.  When it<br>"
                "has completed the progress bar on the left side of<br>"
                "the status bar will disappear and you can make another<br>"
                "ingest."
                ;
        G::popup->showPopup(msg, 5000);
        return;
    }

    static QString prevSourceFolder = "";
    static QString baseFolderDescription = "";
    /*
    qDebug() << "MW::ingest"
             << "prevSourceFolder" << prevSourceFolder
             << "currentViewDirPath" << dm->currentPrimaryFolderPath
             << "baseFolderDescription" << baseFolderDescription
                ;  //*/

    // what we want ie dm->currentPrimaryFolderPath
    if (prevSourceFolder != dm->primaryFolderPath()) baseFolderDescription = "";

    QString folderPath;        // req'd by backgroundIngest
    QString folderPath2;       // req'd by backgroundIngest
    bool combinedIncludeJpg;   // req'd by backgroundIngest
    int seqStart = 1;          // req'd by backgroundIngest

    if (dm->isAnyPick()) {
        ingestDlg = new IngestDlg(this,
                                  combineRawJpg,
                                  combinedIncludeJpg,
                                  autoEjectUsb,
                                  integrityCheck,
                                  isBackgroundIngest,
                                  isBackgroundIngestBeep,
                                  ingestIncludeXmpSidecar,
                                  backupIngest,
                                  gotoIngestFolder,
                                  seqStart,
                                  metadata,
                                  dm,
                                  ingestRootFolder,
                                  ingestRootFolder2,
                                  manualFolderPath,
                                  manualFolderPath2,
                                  folderPath,
                                  folderPath2,
                                  baseFolderDescription,
                                  pathTemplates,
                                  filenameTemplates,
                                  pathTemplateSelected,
                                  pathTemplateSelected2,
                                  filenameTemplateSelected,
                                  ingestDescriptionCompleter,
                                  autoIngestFolderPath,
                                  G::css);

        /* Ingest copies the sidecars off the card, so any Develop edit still sitting in
           the 2s debounce has to be on disk first or the ingested copy misses it. Covers
           the modal ingest below and the background ingest started further down. */
        FileOps::flushPendingEdits();

        bool okToIngest = ingestDlg->exec();
        // do not delete ingestDlg: scrambles QMap objects for some reason

        // update ingest history folders
        // get rid of "/" at end of path for history (in file menu)
        QString historyPath = folderPath.left(folderPath.length() - 1);
        addIngestHistoryFolder(historyPath);

        // save ingest history folders
        settings->beginGroup("IngestHistoryFolders");
        settings->remove("");
        for (int i = 0; i < ingestHistoryFolders->count(); i++) {
            settings->setValue("ingestHistoryFolder" + QString::number(i+1),
                              ingestHistoryFolders->at(i));
        }
        settings->endGroup();

        // save ingest description completer list
        settings->beginGroup("IngestDescriptionCompleter");
        for (const auto& i : ingestDescriptionCompleter) {
            settings->setValue(i, "");
        }
        settings->endGroup();

        // save ingest settings
        settings->setValue("autoIngestFolderPath", autoIngestFolderPath);
        settings->setValue("autoEjectUSB", autoEjectUsb);
        settings->setValue("integrityCheck", integrityCheck);
        settings->setValue("isBackgroundIngest", isBackgroundIngest);
        settings->setValue("isBackgroundIngestBeep", isBackgroundIngestBeep);
        settings->setValue("ingestIncludeXmpSidecar", ingestIncludeXmpSidecar);
        settings->setValue("backupIngest", backupIngest);
        settings->setValue("gotoIngestFolder", gotoIngestFolder);
        settings->setValue("ingestRootFolder", ingestRootFolder);
        settings->setValue("ingestRootFolder2", ingestRootFolder2);
        settings->setValue("pathTemplateSelected", pathTemplateSelected);
        settings->setValue("pathTemplateSelected2", pathTemplateSelected2);
        settings->setValue("manualFolderPath", manualFolderPath);
        settings->setValue("manualFolderPath2", manualFolderPath2);
        settings->setValue("filenameTemplateSelected", filenameTemplateSelected);
        settings->setValue("ingestCount", G::ingestCount);
        settings->setValue("ingestLastSeqDate", G::ingestLastSeqDate);

        // save path templates
        settings->beginGroup("PathTokens");
        settings->remove("");
        QMapIterator<QString, QString> pathIter(pathTemplates);
        while (pathIter.hasNext()) {
            pathIter.next();
            settings->setValue(pathIter.key(), pathIter.value());
        }
        settings->endGroup();

        // save filename templates
        settings->beginGroup("FileNameTokens");
        settings->remove("");
        QMapIterator<QString, QString> filenameIter(filenameTemplates);
        while (filenameIter.hasNext()) {
            filenameIter.next();
            settings->setValue(filenameIter.key(), filenameIter.value());
        }
        settings->endGroup();

        if (!okToIngest) {
            QString msg = "Not okay to ingest.";
            G::issue("Warning", msg, "MW::ingest");
            return;
        }

        // start background ingest
        if (isBackgroundIngest) {
            backgroundIngest = new Ingest(this,
                                          combineRawJpg,
                                          combinedIncludeJpg,
                                          integrityCheck,
                                          ingestIncludeXmpSidecar,
                                          backupIngest,
                                          seqStart,
                                          metadata,
                                          dm,
                                          folderPath,
                                          folderPath2,
                                          filenameTemplates,
                                          filenameTemplateSelected);

            connect(backgroundIngest, &Ingest::updateProgress, this, &MW::setProgress);
            connect(backgroundIngest, &Ingest::ingestFinished, this, &MW::ingestFinished);
            connect(backgroundIngest, &Ingest::rptIngestErrors, this, &MW::rptIngestErrors);
            backgroundIngest->commence();
            G::isRunningBackgroundIngest = true;
        }

        prevSourceFolder = dm->primaryFolderPath();    // rgh what we want?
        /*
        qDebug() << "MW::ingest"
                 << "gotoIngestFolder =" << gotoIngestFolder
                 << "isBackgroundIngest =" << isBackgroundIngest
                 << "lastIngestLocation =" << lastIngestLocation
                    ;//*/

        /* Auto-eject the source memory card.  Eject only applies to a foreground ingest
           (the dialog disables and clears the eject option for background ingests), and
           the copy has finished by the time exec() returns, so it is safe to eject here. */
        QString ingestSourcePath = dm->primaryFolderPath();

        // if background ingesting do not jump to the ingest destination folder
        if (gotoIngestFolder && !isBackgroundIngest) {
            if (autoEjectUsb) ejectUsb(ingestSourcePath);
            fsTree->select(lastIngestLocation);
            return;
        }

        // set the ingested flags, clear the pick flags and update pickLog
        setIngested();

        updateStatus(true, "", "MW::ingest");

        if (autoEjectUsb && !isBackgroundIngest) ejectUsb(ingestSourcePath);
    }
    else {
        QMessageBox::information(this,
        "Oops", "There are no picks to ingest.    ", QMessageBox::Ok);
    }
}

void MW::rptIngestErrors(QStringList failedToCopy, QStringList integrityFailure)
{
    if (G::isLogger) G::log("MW::rptIngestErrors");

    IngestErrors ingestErrors(failedToCopy, integrityFailure, this);
    ingestErrors.exec();
}

void MW::ejectUsb(QString path)
{
/*
    If the datamodel includes images on the drive to be ejected, attempts to read
    subsequent files will cause a crash. This is avoided by stopping any further activity
    in the metaReadThread and imageCacheThread, preventing any file reading attempts to a
    non-existent drive.
*/
    if (G::isLogger) G::log("MW::ejectUsb");

    /* Normalize to the drive root: path may be any folder on the card (eg a DCIM
       subfolder or the ingest source), but isEjectable/eject operate on the mount root. */
    QStorageInfo ejectDrive(path);
    QString rootPath = ejectDrive.rootPath();

    // does datamodel include any image on the ejection drive
    bool ejectDriveIsCurrent = false;
    for (QString path : dm->folderList) {
        QStorageInfo currDrive(path);
        if (currDrive.rootPath() == ejectDrive.rootPath()) {
            ejectDriveIsCurrent = true;
            break;
        }
    }

    if (ejectDriveIsCurrent) {
        stop("MW::ejectUSB");
        // reset("MW::ejectUSB");
        fsTree->selectionModel()->clearSelection();
    }

    // get the drive name ie WIN "D:" or MAC "Untitled"
    QString driveName = Utilities::getDriveName(rootPath);

    // confirm this is an ejectable drive
    if (UsbUtil::isEjectable(rootPath)) {
        // eject USB drive
        if (UsbUtil::eject(rootPath)) {
            // drive was ejected
            G::popup->showPopup("Ejected drive " + driveName, 2000);
            bookmarks->updateCount();
            #ifdef Q_OS_WIN
            fsTree->refreshModel();
            #endif
        }
        // drive ejection failed
        else
            G::popup->showPopup("Failed to eject drive " + driveName, 2000);
    }
    // drive not ejectable
    else {
        G::popup->showPopup("Drive " + driveName
              + " is not removable and cannot be ejected", 2000);
    }
}

void MW::ejectUsbFromContextMenu()
{
    if (G::isLogger) G::log("MW::ejectUsbFromContextMenu");
    ejectUsb(mouseOverFolderPath);
}

void MW::infoViewChanged(QStandardItem* item)
{
/*
    This slot is called when there is a data change in InfoView.

    If the change was a result of a new image selection then ignore.

    If the metadata in the tags section of the InfoView panel has been editied (title,
    creator, copyright, email or url) then all selected image tag(s) are updated to the
    new value(s) in the data model. If xmp edits are enabled the new tag(s) are embedded
    in the image metadata, either internally or as a sidecar when ingesting. If raw+jpg
    are combined then the raw file rows are also updated in the data model.
*/
    if (!G::useInfoView) return;
    if (G::isLogger) G::log("MW::infoViewChanged");
    // if new folder is invalid no relevent data has changed
     if (G::useInfoView) if (infoView->ignoreDataChange) return;

    QModelIndex par = item->index().parent();
     if (G::useInfoView) if (par != infoView->tagInfoIdx) return;

    QString tagValue = item->data(Qt::DisplayRole).toString();
    QModelIndexList selection = dm->selectionModel->selectedRows();
    int row = item->index().row();
    QModelIndex tagIdx = infoView->ok->index(row, 0, par);
    QString tagName = tagIdx.data().toString();

    QHash<QString,int> col;
    col["Title"] = G::TitleColumn;
    col["Creator"] = G::CreatorColumn;
    col["Copyright"] = G::CopyrightColumn;
    col["Email"] = G::EmailColumn;
    col["Url"] = G::UrlColumn;

    // list of file paths to send to Metadata::writeMetadata
    QStringList paths;

    QString src = "MW::metadataChanged";
    for (int i = 0; i < selection.count(); ++i) {
        int sfRow = selection.at(i).row();
        // build list of files to send to Metadata::writeMetadata
        paths << dm->sf->index(sfRow, G::PathColumn).data().toString();
        // update data model
        QModelIndex dmIdx = dm->sf->mapToSource(dm->sf->index(sfRow, col[tagName]));
        emit setValSf(sfRow, col[tagName], tagValue, dm->instance, src);
        // emit setValueDm(dmIdx, tagValue, dm->instance, src, Qt::EditRole, Qt::AlignLeft);
        // check if combined raw+jpg and also set the tag item for the hidden raw file
        if (combineRawJpg) {
            /* Is this part of a raw+jpg pair? The pairing roles live on column 0,
               so these were previously read off dmIdx -- which is column
               col[tagName] -- and came back invalid every time, silently
               disabling tag propagation to the hidden raw file. */
            int dmRow = dmIdx.row();
            int rawRow = dm->isDupJpg(dmRow) ? dm->dupOtherRow(dmRow) : -1;
            if (rawRow >= 0) {
                // set tag item for raw file row as well
                emit setValDm(rawRow, col[tagName], tagValue, dm->instance, src,
                              Qt::EditRole);
            }
        }
    }

    // update shooting info
    QModelIndex idx = dm->currentSfIdx;  //thumbView->currentIndex();
    QString fPath = dm->currentFilePath;  //thumbView->getCurrentFilePath();
    QString sel = infoString->getCurrentInfoTemplate();
    QString info = infoString->parseTokenString(infoString->infoTemplates[sel],
                                        fPath, idx);
    qDebug() << "MW::tokenEditor  info =" << info;
    // imageView->updateShootingInfo(info);
    imageView->setShootingInfo(info);
}

void MW::getSubfolders(QString fPath)
{
    if (G::isLogger) G::log("MW::getSubfolders");
    subfolders = new QStringList;
    subfolders->append(fPath);
    QDirIterator iterator(fPath, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        fPath = iterator.filePath();
        if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
            subfolders->append(fPath);
        }
    }
}

void MW::addNewBookmarkFromContextMenu()
{
    if (G::isLogger) G::log("MW::addNewBookmarkFromContextMenu");
    addBookmark(mouseOverFolderPath);
}

void MW::addBookmark(QString path)
{
    if (G::isLogger) G::log("MW::addBookmark");
    bookmarks->bookmarkPaths.insert(path);
    bookmarks->reloadBookmarks();
    bookmarks->saveBookmarks(settings);
}

void MW::openFolder()
{
    if (G::isLogger) G::log("MW::openFolder");
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Folder"),
         "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath == "") return;
    fsTree->select(dirPath);
}

void MW::gotoFolder()
{
/*
    Go > Go to Folder... (Ctrl+Shift+G).  Type or paste a folder path and Winnow selects
    it in the Folders dock, exactly as if it had been clicked there.  Unlike Open
    Folder..., which has to be navigated, this is the fast route to a path the user
    already knows (or has on the clipboard), and it can include subfolders.

    The OK button is enabled only once the text names an existing folder; the reason it is
    not is shown inline beneath the field rather than as a popup after the fact.
*/
    if (G::isLogger) G::log("MW::gotoFolder");

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Go to Folder"));

    QLabel *label = new QLabel(tr("Folder path:"), &dlg);
    QLineEdit *pathEdit = new QLineEdit(&dlg);
    pathEdit->setMinimumWidth(QFontMetrics(dlg.font())
                              .boundingRect("/Users/xxxxxxxxxx/Pictures/xxxxxxxxxxxxxxx").width());
    pathEdit->setClearButtonEnabled(true);
    pathEdit->setPlaceholderText(QDir::homePath());

    /* Inline completion over folders only, so the path can be typed a segment at a time. */
    QFileSystemModel *completerModel = new QFileSystemModel(&dlg);
    completerModel->setRootPath(QDir::rootPath());
    completerModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QCompleter *completer = new QCompleter(completerModel, &dlg);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    pathEdit->setCompleter(completer);

    QPushButton *browseBtn = new QPushButton(tr("Browse..."), &dlg);
    QCheckBox *recurseCB = new QCheckBox(tr("Include subfolders"), &dlg);
    QLabel *reason = new QLabel(&dlg);
    reason->setObjectName("gotoFolderReason");

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Go"));
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);

    QGridLayout *layout = new QGridLayout(&dlg);
    layout->addWidget(label, 0, 0);
    layout->addWidget(pathEdit, 0, 1);
    layout->addWidget(browseBtn, 0, 2);
    layout->addWidget(recurseCB, 1, 1);
    layout->addWidget(reason, 2, 1, 1, 2);
    layout->addWidget(buttons, 3, 0, 1, 3);

    /* Seed with the loaded folder so the field is a starting point, not a blank. */
    if (dm->folderList.count() == 1) pathEdit->setText(dm->folderList.at(0));

    auto expand = [](QString path) {
        path = path.trimmed();
        /* Drag/drop and copy-as-pathname often deliver a quoted or file:// form. */
        if (path.startsWith('"') && path.endsWith('"') && path.length() > 1)
            path = path.mid(1, path.length() - 2);
        if (path.startsWith("file://")) path = QUrl(path).toLocalFile();
        if (path == "~") path = QDir::homePath();
        else if (path.startsWith("~/")) path = QDir::homePath() + path.mid(1);
        return path;
    };

    auto validate = [&] {
        const QString path = expand(pathEdit->text());
        bool ok = false;
        /* An empty field needs no reason -- the label already says what goes here. */
        if (path.isEmpty()) reason->clear();
        else if (QFileInfo(path).isFile()) reason->setText(tr("That is a file, not a folder"));
        else if (!QDir(path).exists()) reason->setText(tr("Folder not found"));
        else {
            reason->clear();
            ok = true;
        }
        buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
    };
    connect(pathEdit, &QLineEdit::textChanged, &dlg, validate);
    validate();

    connect(browseBtn, &QPushButton::clicked, &dlg, [&] {
        QString start = expand(pathEdit->text());
        if (!QDir(start).exists()) start = QDir::homePath();
        QString dirPath = QFileDialog::getExistingDirectory(&dlg, tr("Go to Folder"), start,
             QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dirPath.isEmpty()) pathEdit->setText(dirPath);
    });

    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    pathEdit->setFocus();
    pathEdit->selectAll();
    if (dlg.exec() != QDialog::Accepted) return;

    const QString dirPath = expand(pathEdit->text());
    if (dirPath.isEmpty()) return;

    /* The Folders dock is where the selection lands, so show it if it is hidden -
       otherwise the folder loads with no visible sign of where it came from. */
    if (!folderDock->isVisible()) showFolderDock();
    fsTree->select(dirPath, recurseCB->isChecked() ? "Recurse" : "None", "MW::gotoFolder");
}

void MW::openUsbFolder()
{
/*
    A list of available USB drives are listed in a dialog for the user.  Show all subfolders
    is set and all images on the USB drive are loaded.
*/
    if (G::isLogger) G::log("MW::openUsbFolder");
    struct  UsbInfo {
        QString rootPath;
        QString name;
        QString description;
    };
    UsbInfo usbInfo;

    QMap<QString, UsbInfo> usbMap;
    QStringList usbDrives;
    int n = 0;

    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (UsbUtil::isMemCardWithDCIM(storage.rootPath())) {
            QString dcimPath = storage.rootPath() + "/DCIM";
            usbInfo.rootPath = storage.rootPath();
            usbInfo.name = storage.name();
            QString count = QString::number(n) + ". ";
            if (usbInfo.name.length() > 0)
                usbInfo.description = count + usbInfo.name + " (" + usbInfo.rootPath + ")";
            else
                usbInfo.description = count + usbInfo.rootPath;
            usbMap.insert(usbInfo.description, usbInfo);

            usbDrives << usbInfo.description;
            n++;
        }
    }

    QString drive;

    // show usb drives in a dialog
    if (usbDrives.length() > 1) {
        loadUsbDlg = new LoadUsbDlg(this, usbDrives, drive);
        loadUsbDlg->exec();
    }
    else if (usbDrives.length() == 1) {
        drive = usbDrives.at(0);
    }
    else if (usbDrives.length() == 0) {
        G::popup->showPopup("No USB Drives available");
    }

    refresh();
    bookmarks->selectionModel()->clear();
    QString fPath = usbMap[drive].rootPath;
    if (isFolderValid(fPath, true, false)) {
        fsTree->select(fPath, "USBDrive", "MW::openUSBFolder");
    }
    else {
        setWindowTitle(winnowWithVersion);
        setCentralMessage("Unable to access " + fPath);
    }
}

void MW::revealLogFile()
{
    if (G::isLogger) G::log("MW::revealLogFile");

    // message dialog explaining process to user
    QMessageBox msgBox;
    int msgBoxWidth = 300;
    msgBox.setWindowTitle("Email Log File to Winnow Developer (Rory)");
    msgBox.setText("If you continue two windows will be opened: \n"
                   "\n1. File explorer or finder showing the file 'WinnowLog.txt'. "
                   "\n2. A selection of your email clients."
                   "\n\nPlease select an email client.  It will open and "
                   "the 'to:' and 'subject: will be filled.  The body will have some "
                   "instructions to add 'WinnowLog.txt' as an attachment."
                  );
    msgBox.setInformativeText("Do you want continue?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStyleSheet(G::css);
    QSpacerItem* horizontalSpacer = new QSpacerItem(msgBoxWidth, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = static_cast<QGridLayout*>(msgBox.layout());
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    int ret = msgBox.exec();
    resetFocus();
    if (ret == QMessageBox::Cancel) return;

    // open explorer/finder at WinnowLog.txt location
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dirPath += "/Log";
    revealInFileBrowser(dirPath);

    // open email to send to Rory
    QString to = "winnowimageviewer@outlook.com";
    QString subject = "Winnow log file";
    QString body = "Please add the file 'WinnowLog.txt' that Winnow has revealed in "
                   "Explorer/Finder to this email as an attachment.  Also, please add some "
                   "text explaining the issue.  \n\nThanks very much.  \nRory";
    QDesktopServices::openUrl(QUrl("mailto:" + to + "?subject=" + subject + "&body=" + body, QUrl::TolerantMode));

}

void MW::revealWinnets()
{
    if (G::isLogger) G::log("MW::revealWinnets");
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dirPath += "/Winnets";
    revealInFileBrowser(dirPath);
}

void MW::revealFile()
{
    if (G::isLogger) G::log("MW::revealFile");
    QString fPath = dm->sf->index(dm->currentSfRow, 0).data(G::PathRole).toString();
    revealInFileBrowser(fPath);
}

void MW::revealFileFromContext()
{
    if (G::isLogger) G::log("MW::revealFileFromContext");
    revealInFileBrowser(mouseOverFolderPath);
}

void MW::revealInFileBrowser(QString path)
{
/*
    See http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
    for details
*/
    if (G::isLogger) G::log("MW::revealInFileBrowser");
//    QString path = thumbView->getCurrentFilename();
//    QString path = (mouseOverFolder == "") ? currentViewDir : mouseOverFolder;
    QFileInfo info(path);
#if defined(Q_OS_WIN)
    QStringList args;
    if (!info.isDir())
        args << "/select,";
    args << QDir::toNativeSeparators(path);
    if (QProcess::startDetached("explorer", args))
        return;
#elif defined(Q_OS_MAC)
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \"" + path + "\"";
    args << "-e";
    args << "end tell";
    if (!QProcess::execute("/usr/bin/osascript", args))
        return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir()? path : info.path()));
}

void MW::collapseAllFolders()
{
    if (G::isLogger) G::log("MW::collapseAllFolders");
    fsTree->collapseAll();
    updateCollapseFoldersAction();
}

void MW::updatePickDependentActions()
{
/*
    Enable actions that require at least one picked image.  Called from the pick-toggle
    paths (which change picks outside of selection events) and after ingest clears picks.
*/
    if (G::isLogger) G::log("MW::updatePickDependentActions");
    bool isAnyPick = dm->isAnyPick();
    const QString reason = "no images are picked";
    for (QAction *a : {ingestAction, nextPickAction, prevPickAction}) {
        a->setEnabled(isAnyPick);
        a->setProperty("disabledReason", reason);
    }
}

bool MW::ownsShortcut(const QKeySequence &seq)
{
/*
    Returns true if any MW action (enabled or disabled) owns the shortcut seq.  Used by the
    eventFilter to suppress MW's window shortcuts when keyboard focus is in a modeless dialog
    parented to MW (see "MODELESS DIALOG SHORTCUT GUARD").
*/
    if (seq.isEmpty()) return false;
    const QList<QAction *> acts = actions();
    for (QAction *a : acts)
        if (a->shortcuts().contains(seq)) return true;
    return false;
}

QAction *MW::disabledActionForShortcut(const QKeySequence &seq)
{
/*
    Returns a disabled MW action whose shortcut matches seq, or nullptr.  If any enabled
    action owns the same shortcut, returns nullptr (that action would have handled the key,
    so there is nothing to explain).  Used to give the user feedback when a disabled
    shortcut is pressed (see eventFilter).
*/
    if (seq.isEmpty()) return nullptr;
    QAction *disabledMatch = nullptr;
    const QList<QAction *> acts = actions();
    for (QAction *a : acts) {
        if (a->shortcuts().contains(seq)) {
            if (a->isEnabled()) return nullptr;
            disabledMatch = a;
        }
    }
    return disabledMatch;
}

QString MW::actionDisabledReason(QAction *a)
{
/*
    Short explanation of why action a is currently disabled.  The reason is stored on the
    action as the "disabledReason" dynamic property when it is gated in
    enableSelectionDependentMenus() / updatePickDependentActions(), so this function does not
    duplicate the gating conditions.
*/
    QString name = a->text();
    name.remove('&');
    QString why = a->property("disabledReason").toString();
    if (why.isEmpty()) why = "not available right now";
    return name + " — " + why + ".";
}

void MW::updateCollapseFoldersAction()
{
/*
    Enable "Collapse all folders" only when something in the folders tree is expanded.
    When the tree is already collapsed the action has nothing to do, so it is disabled.
    Driven by the FSTree expanded/collapsed signals (see createFSTree).
*/
    if (G::isLogger) G::log("MW::updateCollapseFoldersAction");
    if (collapseFoldersAction == nullptr) return;
    collapseFoldersAction->setEnabled(fsTree->isAnyExpanded());
}

void MW::openInFinder()
{
    if (G::isLogger) G::log("MW::openInFinder");
    takeCentralWidget();
    setCentralWidget(imageView);
}

void MW::openInExplorer()
{
    if (G::isLogger) G::log("MW::openInExplorer");
    QString path = "C:/exampleDir/example.txt";

    QStringList args;

    args << "/select," << QDir::toNativeSeparators(path);

    QProcess *process = new QProcess(this);
    process->start("explorer.exe", args);
}

bool MW::isFolderValid(QString dirPath, bool report, bool isRemembered)
{
    if (G::isLogger) G::log("MW::isFolderValid", dirPath);
    QString msg;
    QDir testDir(dirPath);

    if (dirPath.length() == 0) {
        if (report) {
            if (isRemembered)
                msg = "The last folder from your previous session is unavailable";
            else
                if (testDir.exists())
                    msg = "No images available in this folder";
                else
                    msg = "The folder (" + dirPath + ") does not exist or is not available";

            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    if (!testDir.exists()) {
        if (report) {
            if (isRemembered)
                msg = "The last folder from your previous session (" + dirPath + ") does not exist or is not available";
            else
                msg = "The folder (" + dirPath + ") does not exist or is not available";

            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    // check if unmounted USB drive
    if (!testDir.isReadable()) {
        if (report) {
            msg = "The folder " + Utilities::enquote(dirPath) + " is not readable.\n\nPerhaps it was a USB drive that is not currently mounted or that has been ejected, \nor you may not have permission to view this folder.";
            statusLabel->setText("");
            setCentralMessage(msg);
        }
        return false;
    }

    return true;
}

void MW::generateMeanStack()
{
    if (G::isLogger) G::log("MW::generateMeanStack");
    QStringList selection;
    if (!dm->getSelectionOrPicks(selection)) return;
    meanStack = new Stack(selection, dm, metadata, icd);
    connect(this, &MW::abortStackOperation, meanStack, &Stack::stop);
    QString fPath = meanStack->mean();
    if (fPath != "") {
        int dmRow = dm->insert(fPath);
        int sfRow = dm->rowFromPath(fPath);
        qDebug() << "MW::generateMeanStack" << sfRow << dmRow << fPath;
        // metadataCacheThread->loadIcon(sfRow);
        sel->setCurrentPath(fPath);
        // update FSTree image count
        fsTree->refreshModel();
        bookmarks->updateCount();

        if (thumbView && thumbView->isVisible()) {
            thumbView->refreshIcons("MW::generateMeanStack");
            thumbView->scrollToRow(dm->currentSfRow, "MW::generateMeanStack");
        }
        if (gridView && gridView->isVisible()) {
            gridView->refreshIcons("MW::generateMeanStack");
            gridView->scrollToRow(dm->currentSfRow, "MW::generateMeanStack");
        }
    }
}

void MW::reportHueCount()
{
    if (G::isLogger) G::log("MW::reportHueCount");

    QStringList selection;
    if (!dm->getSelectionOrPicks(selection)) return;
    ColorAnalysis hueReport;
    connect(this, &MW::abortHueReport, &hueReport, &ColorAnalysis::abortHueReport);
    hueReport.process(selection);
}

void MW::mediaReadSpeed()
{
    if (G::isLogger) G::log("MW::mediaReadSpeed");

    /*
        Benchmark every mounted drive by reading a random sample of real image
        files from it (Performance::sampleReadSpeed), discarding reads that come
        back at RAM speed as OS-cache hits, and report the result as a table.
        Random sampling + outlier rejection removes the cache bias without any
        special commands (no purge, no sudo) or datamodel coupling.
    */

    /* Collect the user-visible drives: on macOS the boot volume and anything   */
    /* under /Volumes (matching the FSTree); on Windows every ready drive.       */
    struct Drive { QString label; QString path; };
    QList<Drive> drives;
    for (const QStorageInfo &si : QStorageInfo::mountedVolumes()) {
        if (!si.isValid() || !si.isReady()) continue;
#ifndef Q_OS_WIN
        QString root = si.rootPath();
        if (root != "/" && !root.startsWith("/Volumes/")) continue;  // skip synthetic mounts
#else
        if (si.isReadOnly()) continue;
        QString root = si.rootPath();
#endif
        QString label = si.displayName();
        if (label.isEmpty()) label = root;
        /* Scanning "/" wastes the budget in system folders that hold no large  */
        /* images; sample the user's home (Pictures, Downloads, …) instead.     */
        QString scanPath = (root == "/") ? QDir::homePath() : root;
        drives.append({label, scanPath});
    }
    if (drives.isEmpty()) {
        G::popup->showPopup("No mounted drives found.", 2000);
        return;
    }

    /* Image name filters (e.g. "*.jpg", "*.nef") from the supported formats. */
    QStringList nameFilters;
    for (const QString &ext : metadata->supportedFormats)
        nameFilters << "*." + ext;

    /* Benchmark each drive (synchronous; keep the UI alive with a popup). */
    QString rows;
    for (const Drive &d : std::as_const(drives)) {
        G::popup->showPopup("Testing read speed: " + d.label + " …", 60000);
        qApp->processEvents();
        Performance::ReadSpeedResult r =
            Performance::sampleReadSpeed(d.path, nameFilters);
        QString cell = r.error.isEmpty()
                           ? "<td align=right>" + QString::number(r.mbPerSec, 'f', 0) + "</td>"
                           : "<td align=right><i>" + r.error + "</i></td>";
        rows += "<tr><td>" + d.label.toHtmlEscaped() + "</td>" + cell + "</tr>";
    }
    G::popup->reset();

    QString html =
        "<table cellspacing='0' cellpadding='6' "
        "style='border-collapse:collapse'>"
        "<tr><th align='left'>Drive</th><th align='right'>MB/sec</th></tr>"
        + rows + "</table>";

    QMessageBox box(this);
    box.setWindowTitle("Media read speed");
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.setStyleSheet(G::css);
    box.exec();
}

void MW::findDuplicates()
{
    QString srcFun = "MW::findDuplicate";
    if (G::isLogger) G::log(srcFun);
    FindDuplicatesDlg *findDuplicatesDlg = new FindDuplicatesDlg(nullptr, dm, metadata);
    findDuplicatesDlg->setStyleSheet(G::css);
    // minimize dialog size fitting contents
    // findDuplicatesDlg->resize(100, 100);
    if (findDuplicatesDlg->exec()) {
        qDebug() << srcFun << "accepted";
        // add true to compare filter
        buildFilters->updateCategory(BuildFilters::CompareEdit, BuildFilters::NoAfterAction);
        filterChange(srcFun);
    }
}

void MW::help()
{
    if (G::isLogger) G::log("MW::help");
    HtmlWindow *w = new HtmlWindow("Winnow - Help",
                                   ":/Docs/winnowhelp.html",
                                   QSize(900, 750), geometry(), this);
    openWindows.append(w);
}

void MW::helpShortcuts()
{
    if (G::isLogger) G::log("MW::helpShortcuts");
    QScrollArea *helpShortcuts = new QScrollArea;
    helpShortcuts->setAttribute(Qt::WA_DeleteOnClose);  // frees + nulls the QPointers on close
    Ui::shortcutsForm ui;
    ui.setupUi(helpShortcuts);

    styleShortcutsWindow(helpShortcuts);                // font scaling + fit columns to text
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(helpShortcuts->winId(), G::backgroundColor);
    #endif
    openWindows.append(helpShortcuts);
    shortcutsWindows.append(helpShortcuts);             // tracked for live font-size updates
    helpShortcuts->show();
}

void MW::styleShortcutsWindow(QScrollArea *w)
{
/*
    Applies the current app font (G::fontSize) to the Shortcuts help window and
    sizes its columns so no text is clipped, growing the window to fit (capped at
    the screen). Used both when the window is created and when the font size
    changes while it is open, so it scales dynamically like the HtmlWindow help
    pages.
*/
    if (G::isLogger) G::log("MW::styleShortcutsWindow");
    QTreeWidget *tree = w->findChild<QTreeWidget*>("treeWidget");
    if (!tree) return;

    // Theme/background, and the font-size for the header and non-styled rows.
    if (w->widget()) w->widget()->setStyleSheet(G::css);
    tree->setStyleSheet(G::css);

    // uic bakes a fixed-size bold QFont onto the category-header items, and a
    // per-item font overrides the stylesheet — so re-apply the scaled size to
    // every item, preserving each item's bold flag.
    int px = static_cast<int>(G::strFontSize.toInt() * G::ptToPx);
    if (px < 6) px = 6;
    QTreeWidgetItemIterator it(tree);
    while (*it) {
        for (int col = 0; col < tree->columnCount(); ++col) {
            QFont f = (*it)->font(col);
            f.setPixelSize(px);
            (*it)->setFont(col, f);
        }
        ++it;
    }
    tree->expandAll();

    // Size each column to the larger of its scaled design width and the width
    // needed to show its longest cell / header label (so nothing overflows),
    // then size the window to fit all columns. setStretchLastSection(false)
    // keeps the widths we set instead of stretching the last column to fill.
    tree->header()->setStretchLastSection(false);
    const qreal scale = G::strFontSize.toInt() / 12.0;     // design widths assume 12pt default
    const int designW[3] = {300, 250, 250};
    int columnsW = 0;
    for (int c = 0; c < tree->columnCount(); ++c) {
        tree->resizeColumnToContents(c);                   // fit content + header label
        int colW = qMax(qRound(designW[qMin(c, 2)] * scale), tree->columnWidth(c));
        tree->setColumnWidth(c, colW);
        columnsW += colW;
    }

    QScreen *scr = w->screen() ? w->screen() : QGuiApplication::primaryScreen();
    if (!scr) return;
    const QRect avail = scr->availableGeometry();

    int chrome = 2 * (tree->frameWidth() + w->frameWidth())
               + tree->style()->pixelMetric(QStyle::PM_ScrollBarExtent);   // reserve v-scrollbar
    int targetW = qMin(columnsW + chrome, avail.width());
    int targetH = qMin(w->height(), avail.height());
    w->resize(targetW, targetH);

    // Nudge back into view if the resize pushed the window off-screen (it grows
    // from the top-left, so the right/bottom edge can spill past the screen).
    // Uses frameGeometry so window-manager decorations are accounted for.
    QRect fg = w->frameGeometry();
    QPoint pos = w->pos();
    if (fg.right()  > avail.right())  pos.rx() -= fg.right()  - avail.right();
    if (fg.bottom() > avail.bottom()) pos.ry() -= fg.bottom() - avail.bottom();
    if (pos.x() < avail.left()) pos.setX(avail.left());
    if (pos.y() < avail.top())  pos.setY(avail.top());
    if (pos != w->pos()) w->move(pos);
}

void MW::helpWelcome()
{
    if (G::isLogger) G::log("MW::helpWelcome");
    centralLayout->setCurrentIndex(StartTab);
}

void MW::toggleRory()   // shortcut = "Shift+Ctrl+Alt+."
{
    G::isRory = !G::isRory;
    rory();
}

void MW::rory()
{
    if (pref != nullptr) pref->rory();
    if (G::isRory) {
        G::showCacheProgress = true;
        setCacheProgressEnabled(true);
        refreshAfterImageCacheSizeChange();
    }
    else {
        G::showCacheProgress = false;
        setCacheProgressEnabled(false);
        refreshAfterImageCacheSizeChange();
    }

    qDebug() << "MW::rory" << G::isRory;
}

// End MW
