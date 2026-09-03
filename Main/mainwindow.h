#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include <QtConcurrent>
// #include <QThread>
//#include <QDesktopWidget>         // qt6.2
#include "sstream"
#include "stresstest.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

#if defined(Q_OS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#include "CoreGraphics/CoreGraphics.h"
//#include "AppKit/NSSharingService.h"
#endif

#include "Main/widgetcss.h"
#include "Main/winnow_version.h"   // generated from CMake project(VERSION ...) into build/<preset>/Main

#include "appdlg.h"
#include "addthumbnailsdlg.h"
#include "Datamodel/datamodel.h"
#include "Datamodel/selection.h"
#include "Datamodel/filters.h"
#include "Datamodel/buildfilters.h"
#include "File/bookmarks.h"
#include "File/fstree.h"
#include "PropertyEditor/propertyeditor.h"
#include "PropertyEditor/preferences.h"
#include "Views/compareImages.h"
#include "Views/iconview.h"
#include "Views/tableview.h"
#include "Views/imageview.h"
#include "Views/videoview.h"
#include "Views/infoview.h"
#include "Views/catalogview.h"
#include "Views/findpanel.h"
#include "Views/catalogscoperow.h"
#include "Main/catalogscanner.h"
#include "Dialogs/catalogrootsdlg.h"
#include "Views/infostring.h"
#include "Metadata/metadata.h"
#include "Main/dockwidget.h"
#include "Embellish/Properties/embelproperties.h"
#include "Develop/Properties/developproperties.h"
#include "Develop/workingimage.h"
#include "Develop/developstackcache.h"   // MW::developStackCache (interactive proxy)
#include "Develop/Scopes/scopesview.h"
#include "Develop/Transform/transformpanel.h"
#include "Develop/Replace/replacepanel.h"
#include "Develop/History/historyview.h"
#include "Develop/Presets/presetsview.h"
#include "Develop/Properties/scopeheader.h"
#include "Develop/Properties/scopeheaderlab.h"   // experimental, G::useScopeHeaderLab
#include "Develop/Properties/rawpanel.h"          // lab UI raw-decode strip
#include "Develop/Properties/maskpanel.h"         // lab UI mask-editing strip
#include "Embellish/embelexport.h"
#include "Embellish/embel.h"
#include "Export/imageexporter.h"
#include "Export/exportpresets.h"

#include "Cache/cachedata.h"
#include "Cache/metaread.h"
#include "Cache/imagecache.h"
#include "Cache/framedecoder.h"

#ifdef Q_OS_WIN
#endif

#include "File/ingest.h"
#include "ingestdlg.h"
#include "exportdlg.h"
#include "aboutdlg.h"
#include "findduplicatesdlg.h"
//#include "selectionorpicksdlg.h"
#include "Image/thumb.h"
#include "preferencesdlg.h"
#include "updateAppDlg.h"
#include "workspacedlg.h"
#include "zoomdlg.h"
#include "loadusbdlg.h"
#include "erasememcardimagesdlg.h"
#include "Utilities/utilities.h"
#include "Utilities/renamefile.h"
#include "Utilities/usbutil.h"
#include "Utilities/inputdlg.h"
#include "Utilities/htmlwindow.h"
#include "Utilities/reportdialog.h"
#include "progress.h"

#include "Utilities/coloranalysis.h"
#include "Utilities/dirwatcher.h"
#include "Image/stack.h"        // used by meanStack
// #include "ImageStack/stackcontroller.h"

// #include "FocusStack/fsrunner.h"    // temp class
#include "FocusStack/fs.h"

#include <QSoundEffect>

#include "Utilities/performance.h"
#include "ui_shortcutsform.h"
#include "ui_welcome.h"
#include "ui_message.h"
#include "ui_test12.h"

#include "Metadata/ExifTool.h"

#include "Utilities/focuspointtrainer.h"
// #include "Utilities/focuspredictor.h"

//#include "Image/tiffhandler.h";
//#include "Lib/zlib/zlib.h"

// #include "Log/log.h"

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#include "Main/winnativeeventfilter.h"
#endif

#ifdef Q_OS_MAC
//#include "Utilities/mac.h"
#endif

class QNetworkAccessManager;
class QNetworkReply;

class MW : public QMainWindow
{
    Q_OBJECT

    friend class Preferences;       // mw;
    friend class PropertyEditor;    // mw;
    friend class IconView;          // mw2
    friend class EmbelProperties;   // mw3
    friend class DevelopProperties;
    friend class InfoString;        // mw4
    friend class TableView;         // mw5

public:
    MW(const QString args, QWidget *parent = nullptr);

    // Headless self-test used by the smoke test layer (tests/). Opens folderPath,
    // lets it load for settleMs, then exits the app 0 if images loaded, else 2.
    void runSelfTest(const QString &folderPath, int settleMs);

    // End-to-end metadata read used by the metadata test layer (tests/). Reads
    // filePath through the full Metadata pipeline and exits 0 if make/model and
    // dimensions parsed (and match WINNOW_METATEST_MAKE/MODEL if set), else 2.
    /* Headless DEVELOP stress driver for ThreadSanitizer: simulates a brush drag on a
       multi-submask scope while switching images, toggling the veil and re-selecting the
       folder, so the worker-thread proxy render collides with the GUI thread's use of the
       same caches. See tests/tsan/run_tsan_develop.sh. */
    void runDevelopStressTest(const QString &folderPath, int durationMs);
    void runMetaTest(const QString &filePath);

    // Headless soak used by the soak test layer (tests/soak). Bounces between
    // folders (reloading each and ping-ponging through its images) for
    // durationMs, seeded for reproducibility, then drives an orderly teardown so
    // AddressSanitizer/LeakSanitizer can report at exit. Probes footprint each
    // bounce. Run under mac-asan (leaks) or mac-tsan (races).
    void runSoakTest(const QStringList &folders, int durationMs,
                     int msPerImage, uint seed);

    /*
    alpha, beta, gamma, delta, epsilon, zeta, eta, theta, iota, kappa, lambda, mu, nu,
    xi, omicron, pi, rho, sigma, tau, upsilon, phi, chi, psi, and omega.
    */
    QString versionNumber = WINNOW_VERSION ;   // single-sourced from CMakeLists project(VERSION ...)
    QString compileDate = "Compiled: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");
    QString version = "Version: " + versionNumber;
    QString winnowWithVersion = "Winnow " + versionNumber;
    QString website = "Website: "
//            "<a href=\"http://165.227.46.158/winnow/winnow.html\">"
            "<a href=\"winnow.ca\">"
            "<span style=\" text-decoration: underline; color:#e5e5e5;\">"
            "Winnow main page</span></a>.  "
            "Includes links to download and video tutorials.</p></body></html>";

    bool isShiftOnOpen;               // used when opening if shift key pressed
    QString args;                     // opening args

    /* Version tag for saveState()/restoreState(). BUMP whenever the set of docks changes,
       and add the new dock to the table in MW::placeDocksAddedSince. The version is what
       tells a restore which docks did not exist when the state was saved: those docks are
       missing from it, and left to Qt they are dumped loose into whatever area they happen
       to be in (which is how developDock once ended up as orphaned, flickering empty tab
       bars). MW::restoreWindowState restores the state at its own version and then places
       the newer docks, so a dock addition MIGRATES the user's layout rather than discarding
       it. v1: added developDock. v2: added historyDock. v3: added presetsDock.
       v4: added catalogDock. */
    static constexpr int winnowStateVersion = 4;

    // debugging flags
    bool ignoreSelectionChange = false;
    bool isStartupArgs = false;
    bool hideEmbellish = false;

    QTextStream rpt;

    // QSettings
    QSettings *settings;
    QString iniPath;
    QMap<QString, QAction *> actionKeys;
    /* Develop mode local shortcuts: Qt::Key (bare, no modifiers) -> the action to run
       instead of whatever the global shortcut table binds that key to. */
    QHash<int, QAction *> developShortcuts;

    QMap<QString, QString> pathTemplates;
    QMap<QString, QString> filenameTemplates;

    QStringList *subfolders;
    QStringList *recentFolders;
    QStringList *ingestHistoryFolders;
    QStringList ingestDescriptionCompleter;

    G::App externalApp;                  // external application name / executable path
    QList<G::App> externalApps;          // list of external apps
    QVector<QString> xAppShortcut = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

    struct WorkspaceData {
        QString name;           // used for menu
        // State
        QByteArray geometry;
        QByteArray state;
        /* Which dock set the state above was saved with (MW::winnowStateVersion). The
           blob itself is written unversioned so an older workspace still restores; this
           says which docks did not exist when it was saved, so MW::placeDocksAddedSince
           can put the newer ones where they belong instead of leaving them stranded. */
        int stateVersion = 0;
        int screenNumber;
        QScreen *screen;
        QRect geometryRect;
        bool isFullScreen;
        bool isMaximised;
        // Visibility
        bool isWindowTitleBarVisible;
        bool isMenuBarVisible;
        bool isStatusBarVisible;
        bool isFolderDockVisible;
        bool isFavDockVisible;
        bool isFilterDockVisible;
        bool isCatalogDockVisible;
        bool isMetadataDockVisible;
        bool isEmbelDockVisible;
        bool isDevelopDockVisible;
        bool isHistoryDockVisible;
        bool isPresetsDockVisible;
        bool isThumbDockVisible;
        bool isImageDockVisible;
        // View
        bool isLoupeDisplay;
        bool isGridDisplay;
        bool isTableDisplay;
        bool isCompareDisplay;
        // Thumbview
        int thumbSpacing;
        int thumbPadding;
        int thumbWidth;
        int thumbHeight;
        int labelFontSize;
        bool showThumbLabels;
        // GridView
        int thumbSpacingGrid;
        int thumbPaddingGrid;
        int thumbWidthGrid;
        int thumbHeightGrid;
        int labelFontSizeGrid;
        bool showThumbLabelsGrid;
        QString labelChoice;
        // ImageView
        bool isImageInfoVisible;
        bool isEmbelDisplay;
        // Processes
        bool isColorManage;
        int sortColumn;
        bool isReverseSort;
    };
    WorkspaceData ws;   // hold values for workspace n, current workspace
    WorkspaceData *w;
    QList<WorkspaceData> *workspaces;

    /* The default workspace (Ctrl+Shift+W).  If the user has updated it to the current
       layout (Manage Workspaces) then isCustomDefaultWorkspace is true and defaultWs
       holds the saved layout, otherwise the built-in Winnow layout is used. */
    WorkspaceData defaultWs;
    bool isCustomDefaultWorkspace = false;

    // recoverGeometry info
    struct RecoverGeometry {
        QRect frameGeometry;
        QRect normalGeometry;
        qint32 screenNumber;
        quint8 maximized;
        quint8 fullScreen;
    } recover;

    // full screen behavior
    struct FullScreenDocks {
        bool isFolders = false;
        bool isFavs = false;
        bool isFilters = false;
        bool isCatalog = false;
        bool isMetadata = false;
        bool isDevelop = false;
        bool isHistory = false;
        bool isPresets = false;
        bool isEmbellish = false;
        bool isThumbs = true;
        bool isStatusBar = true;
    } fullScreenDocks;

    bool wasFullSpaceOnDiffScreen = false;

    // persistant data

    /*
    Some persistent values are required for class creation and for actions
    that are mutually dependent - so which one do we do first?  Save the
    values in struct mwd and use later.
     */
    WorkspaceData mwd;  // hold the persistent start values from QSettings

    int folderMaxWidth = 600;       // not in preferences or QSetting

    // general
    int lastPrefPage;
    // bool mouseClickScroll;       // positionAtCenter scrolling when mouse click?
    // int displayHorizontalPixels; // move to global
    // int displayVerticalPixels;   // move to global
    bool checkIfUpdate = true;          // automatic check for a newer Winnow at startup
    QString updateSkipVersion;          // version the user chose to skip (suppresses startup dialog)
    bool turnOffEmbellish = true;
    bool deleteWarning = true;
    bool isFirstImageSelected = true;
    bool isLogAllToFileForDebugging = false;

    // appearance
    bool isImageInfoVisible;
    bool isRatingBadgeVisible;
    bool isIconNumberVisible;
    int infoOverlayFontSize = 20;
    int classificationBadgeInImageDiameter;
    int classificationBadgeSizeFactor;
    int iconNumberSize;

    // files
    QString folderAndFileChangePath = "";
    bool rememberLastDir;
    QString lastDir;
    QString lastFileIfCrash;
//    bool inclSubfolders;
    int maxRecentFolders = 20;
    int maxIngestHistoryFolders = 20;

    // ingest
    QString ingestRootFolder;
    QString ingestRootFolder2;
    int pathTemplateSelected;
    int pathTemplateSelected2;
    int filenameTemplateSelected;
    QString manualFolderPath;
    QString manualFolderPath2;
    bool combineRawJpg;
    bool autoIngestFolderPath;
    bool autoEjectUsb;
    bool integrityCheck;

    bool isBackgroundIngest;
    bool isBackgroundIngestBeep;
    bool ingestIncludeXmpSidecar;
    bool backupIngest;
    bool gotoIngestFolder;
    QString lastIngestLocation;     // used when exit ingest to show is gotoIngestFolder == true

    // preference dialog
    bool isSoloPrefDlg;

    // preferences: slideshow
    int slideShowDelay;
    bool isSlideShowRandom;
    bool isSlideShowWrap = true;
    QStack<QString> *slideshowRandomHistoryStack;

    // preferences: cache
    int cacheBarProgressWidth;

    // first use
    bool isFirstTimeTableViewVisible = true;

    // tooltip for tabs in docked and tabified panels
    // int prevTabIndex= -1;

    QDir currRootDir;
    int instance;

    enum centralWidgetTabs {
        LoupeTab,       // 0
        VideoTab,       // 1
        CompareTab,     // 2
        TableTab,       // 3
        GridTab,        // 4
        StartTab,       // 5
        MessageTab,     // 6
        BlankTab,       // 7
        EmbelTab        // 8    rgh req'd? for coord help?
    };

    enum Tog {
        toggle,
        off,
        on
    };

    // mode change
    QString prevMode;
    int scrollRow;              // the row to scroll to when change mode
//    QModelIndex currDmIdx;   // the datamodel index for the current proxy index
    bool modeChangeJustHappened;
    bool gridDisplayFirstOpen = true;
    bool justUpdatedBestFit;
    int sortColumn = G::NameColumn;
    bool isReverseSort = false;
    bool prevIsReverseSort = false;
    bool preferencesHasFocus = false;
    bool workspaceChanged = false;

    bool remoteAccess = false;
    bool showImageCount = true;
    bool copyOp;
    bool isDragDrop;
    QString dragDropFilePath;
    QString dragDropFolderPath;
    int maxThumbSpaceHeight;
    QString pickMemSize;
    WidgetCSS widgetCSS;

    void test();                // for debugging
    void mergeProjectFiles();   // to send to Gemini

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void changeEvent(QEvent *event) override;

signals:
    void updateCurrent(QModelIndex sfIdx, int instance);
    void setValDm(int dmRow, int dmCol, QVariant value, int instance, QString src = "MW",
                    int role = Qt::EditRole);
    void setValSf(int sfRow, int sfCol, QVariant value, int instance, QString src = "MW",
                    int role = Qt::EditRole);
    void setValuePath(QString fPath, int col, QVariant value, int instance, int role);
    void startImageCache();
    void initializeImageCache();
    void setAutoMaxMB(bool autoSize, ImageCache::AutoStrategy strategy);
    void setMaxMB(quint64 mb);
    void setImageCachePosition(QString, QString);
    void imageCacheFilterChange(QString, QString);
    void imageCacheColorManageChange();
    void resizeMW(QRect mainWindowRect, QRect centralWidgetRect);
    // void closeZoomDlg();        // not being used
    void metadataLoaded();
    void aSyncGo(int);          // rgh req'd?
    void needToShow();          // not being used
    void abortMDCache();        // not being used
    void stopImageCache();
    void abortMetaRead();
    void abortImageCache();
    void abortBuildFilters();   // not being used
    void abortFrameDecoder();   // not being used
    void abortEmbelExport();
    void abortHueReport();
    void abortStackOperation();
    void abortFocusStack();
    void fusionFinished(bool ok, const QString &outputImagePath,
                        const QString &depthMapPath);

public slots:
//    void prevSessionWindowLocation(QWindow::Visibility visibility);
    void showMouseCursor();
    void hideMouseCursor();
    void whenActivated(Qt::ApplicationState state);
    void appStateChange(Qt::ApplicationState state);
    void handleStartupArgs(const QString &msg);
    /* Load a catalog search result -- images from any number of folders as one set.
       append = false REPLACES what is loaded (the same reset as a folder change);
       append = true adds to it, the way ctrl-clicking a second folder does. */
    void loadCatalogResults(const QStringList &paths, bool append = false,
                            const CatalogQuery &query = CatalogQuery());
    /* Start / stop the background scan over catalogRoots. */
    void startCatalogScan();
    void stopCatalogScan();
    /* Open the Catalogued Folders editor -- which folders are indexed in the background.
       Created on first use and kept, so it can stay open while a scan runs. Reachable
       from the Catalog panel and from Preferences > Catalog. */
    void manageCatalogRoots();
    /* Drop the develop caches that belong to the images being replaced. Shared by
       folderSelectionChange and loadCatalogResults. */
    void resetDevelopCachesForNewFolder();

    void folderSelectionChange(QString folderPath = "",
                               G::FolderOp op = G::FolderOp::Add,
                               bool resetDataModel = true, bool recurse = false);
    // void folderSelectionChange(QString folderPath = "", QString op = "Add",
    //                            bool resetDataModel = true, bool recurse = false);
    void fileSelectionChange(QModelIndex current, QModelIndex, bool clearSelection = true, QString src = "");
    void folderAndFileSelectionChange(QString fPath, QString src = "");
    void currentFolderDeletedExternally(QString path);
    void refresh();
    void updateImageCount();
    void stop(QString src = "");
    bool reset(QString src = "");
    void waitUntilMetadataLoaded(int ms, QString src = "");
    void nullFiltration();
    void updateFilterMenu(QString source = "");
    void filterLastDay();
    void searchTextEdit();
    void handleDrop(QString fPath);
    // void handleDrop(QDropEvent *event);
    // void handleDrop(const QMimeData *mimeData);
    void sortIndicatorChanged(int column, Qt::SortOrder sortOrder);
    void setProgress(int value);
    void setStatus(QString state);
    void updateStatus(bool keepBase = true, QString s = "", QString source = "");
    void updateSidecarStatus(QString fPath);
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void setThumbDockHeight();  // signal from thumbView
    void setThumbDockFeatures(Qt::DockWidgetArea area);
    void setThumbDockFloatFeatures(bool isFloat);
    void resortImageCache();
    void setCentralMessage(QString message);
    void slideShow();
    void slideShowResetDelay();
    void slideShowResetSequence();
    void slideshowHelpMsg();
    void rptIngestErrors(QStringList failedToCopy, QStringList integrityFailure);
    void invokeCurrentWorkspace();
    void invokeWorkspaceFromAction(QAction *workAction);
    void invokeWorkspace(const WorkspaceData &w);
    void invokeRecentFolder(QAction *recentFolderActions);
    void invokeIngestHistoryFolder(QAction *ingestHistoryFolderActions);
    void snapshotWorkspace(WorkspaceData &wsd);
    void manageWorkspaces();
    void deleteWorkspace(int n);
    void renameWorkspace(int n, QString name);
    void reassignWorkspace(int n);
    void defaultWorkspace();
    /* Restore the main window dock layout, migrating a state saved by a build with fewer
       docks instead of throwing it away.  See MW::winnowStateVersion. */
    bool restoreWindowState(const QByteArray &state);
    void placeDocksAddedSince(int stateVersion);
    void builtInDefaultWorkspace();
    void updateDefaultWorkspace();
    void loadDefaultWorkspace();
    void saveDefaultWorkspace();
    QString reportWorkspaces();
    void reportWorkspaceNum(int n);
    void reportWorkspace(WorkspaceData &ws, QString src = "");
    void loadWorkspaces();
    void saveWorkspaces();
    void readWorkspaceSettings(WorkspaceData &wsd);
    void writeWorkspaceSettings(const WorkspaceData &wsd);
    void matFromQImage(QString fPath, ImageMetadata m, cv::Mat &mat);

private slots:
    void focusChange(QWidget *previous, QWidget *current);
    void resetFocus();
    void checkForUpdate(bool silent = false);
    void onUpdateCheckReply(QNetworkReply *reply);
    void downloadAndOpenUpdate(const QString &dmgUrl);
    void setShowImageCount();
    void about();
    void helpThumbViewStatusBarSymbols();
    void ingest();
    void exportEmbelFromAction(QAction *embelExportAction);
    void exportEmbel();
    void enableSelectionDependentMenus();
    /* True when any loaded folder is the develop preview cache, which is browsable but
       read-only (see Cache/devpreviewcache.h). Gates every menu action that writes. */
    bool isPreviewCacheFolderLoaded() const;
    void enableStatusBarBtns();
    void renameEjectUsbMenu(QString path);
    void renameAddBookmarkAction(QString path);
    void renameRemoveBookmarkAction(QString path);
    void renamePasteFilesAction(QString folderName);
    void renameDeleteFolderAction(QString folderName);
    void renameCopyFolderPathAction(QString folderName);
    void renameRevealFileAction(QString folderName);
    void ejectUsb(QString path);
    void ejectUsbFromContextMenu();
    void renameEraseMemCardFromContextMenu(QString path);
    void refreshViewsOnCacheChange(QString fPath, bool isCached, QString src);
    void searchTextEdit2();
//    void searchTextEdit();

    int ratingLogCount();
    void recoverRatingLog();
    void clearRatingLog();
    void updateRatingLog(QString fPath, QString pickStatus);
    void setRating();

    int colorClassLogCount();
    void recoverColorClassLog();
    void clearColorClassLog();
    void updateColorClassLog(QString fPath, QString pickStatus);
    void setColorClass();
    void setColorClassForRow(int sfRow, QString colorClass);

    void setRotation(int degrees);
    /* A Develop dock slider changed: schedule a coalesced proxy re-render now and (re)arm the
       full-resolution settle render. Connected to DevelopProperties::paramsChanged. */
    void developParamsChange();
    /* Re-render the current image through Develop + OutputTransform and push it to the loupe.
       fullRes=false renders the screen-resolution proxy (interactive drag); fullRes=true
       renders the full image (drag settled). The WorkingImage-cache hot path: no decode. */
    void renderDevelopPreview(bool fullRes);
    /* Launch the full-resolution settle render on developRenderPool (off the GUI thread) so a
       large RAW does not freeze the drag. At most one runs at a time; the result is applied via
       onDevelopFullResReady only if still current. */
    void renderDevelopFullResAsync();
    /* Detail panel 1:1 preview (see the DetailRoiJob note in mainwindow.cpp): the loupe
       picker landed, the point moved / the panel opened, a drag inside the preview, and
       the per-image reset. The point is VIEW state -- no EditParams, no sidecar. */
    void onDetailPointPicked(double nx, double ny);
    void onDetailRoiNeeded();
    void onDetailPointNudged(int dx, int dy);
    void resetDetailPoint();
    /* After the current image's loupe pixmap is shown, render its saved Develop edits over it (if
       any). No-op for an unedited image. Reuses the coalesced proxy + async settle pipeline. */
    void applyDevelopPreviewIfEdited();
    /* True when the current image has Develop edits that should be shown. A RAW file's edits show
       only in raw mode (useRaw): in preview mode the loupe shows the untouched embedded JPG. A
       non-RAW file (e.g. a JPG) is always the developable image, so its edits show regardless. */
    bool currentDevelopEditsVisible() const;
    /* True if fPath is a raw file (a format that carries an embedded jpg preview). Used by the
       Develop panel to show the raw-only rows (Edit source, Demosaic, Denoise raw). */
    bool isFileRaw(const QString &fPath) const;
    /* True when the current selection is a video. The Develop module operates on decoded still
       frames, so every Develop entry point (dock load + preview/full-res render) is gated on this to
       avoid trying to develop a video. */
    bool currentIsVideo() const;
    /* GUI-thread completion for a background full-res render: apply the image if its params/image
       are still current, otherwise discard, then re-arm if newer params arrived while it ran. */
    void onDevelopFullResReady(const QImage &out, const QString &fPath, quint64 gen,
                               bool faithful, const QByteArray &recipe);
    /* Global image the develop render pipeline should start from: the raw-DENOISED WorkingImage when
       the Global scope has "Denoise raw" (denoiseLuma/denoiseChroma) set and it is ready, else the
       clean cached WorkingImage. Pure lookup (no work); the async compute is ensureRawDenoise(). */
    std::shared_ptr<const WorkingImage> developRawDenoisedBase(
        const QString &fPath, const EditParams &base,
        const std::shared_ptr<const WorkingImage> &clean);
    /* May w be the CLEAN side of the "Denoise raw" blend? Only a camera-native,
       scene-linear WorkingImage may: the WorkingImageCache also holds display-referred
       entries under a raw's path (Preview mode), and blending one of those against the
       camera-native PMRID base casts the image magenta/green. See the definition. */
    static bool rawDenoiseCleanBaseOk(const std::shared_ptr<const WorkingImage> &w);
    /* Compute the raw-denoised base for the current Global params OFF the GUI thread (developRenderPool),
       cache it (developDenoised), then repaint. Coalesced (one in flight); no-op if already current or
       already computing this key. Called from the settle path so a drag does not spawn many DNN runs. */
    void ensureRawDenoise(const QString &fPath, const EditParams &base,
                          const std::shared_ptr<const WorkingImage> &clean, int iso);
    /* Dock "auto run denoise" checkbox handler: store G::autoRunDenoise (+ persist);
       turning it ON runs the denoise for the current image immediately. */
    void onAutoRunDenoiseToggled(bool on);
    /* Dock "Denoise" checkbox handlers: runRawDenoiseNow forces ensureRawDenoise for the
       current image + Global params regardless of G::autoRunDenoise; clearRawDenoiseNow
       drops the denoised base and re-renders clean. rawDenoiseReadyForCurrent reports
       whether a denoised base is cached for the current image + params (drives the
       "Denoise"/"Denoised" checkbox state). */
    void runRawDenoiseNow();
    void clearRawDenoiseNow();
    bool rawDenoiseReadyForCurrent();
    /* PMRID ran but changed nothing (no ORT in this build / no pmrid.onnx, or a non-Bayer
       sensor). Reset the "Denoise" checkbox + amount sliders instead of reporting a
       "Denoised" state that never happened, tell the user once, and remember it so later
       settles do not re-decode the raw for the same null result. */
    void reportRawDenoiseUnavailable(const QString &fPath);
    /* Can "Denoise raw" do anything for fPath (build has an inference backend + pmrid.onnx,
       and this sensor's CFA is one PMRID handles)? Cheap -- for the dock, which greys the
       denoise group and shows *reason in its place. */
    bool rawDenoiseAvailable(const QString &fPath, QString *reason = nullptr) const;
    /* Show the status-bar "Demosaic" progress row while the CURRENT image's Winnow raw
       demosaic decodes with Auto-run denoise off (relayed from ImageCache). */
    void onDemosaicProgress(const QString &fPath, int done, int total);
    /* Ensure the current image's scene-linear (pre-develop) WorkingImage is cached, decoding it OFF
       the GUI thread when it is missing (evicted / a display-referred one cached), then re-render.
       Coalesced (one decode in flight). Replaces the old synchronous re-decode inside
       renderDevelopPreview that froze the first slider drag by ~1s on a 50MP RAW. */
    void ensureDevelopWork(const QString &fPath);
    /* On-demand version, for a Develop action that needs the pre-develop
       WorkingImage BEFORE any edit exists (WB dropper / Auto white balance). A
       display-referred file is built here and now -- synchronously, so the caller
       can use it on return; raw falls back to the async ensureDevelopWork. See
       DevelopProperties::ensureWorkingImage. */
    void ensureWorkingImageNow(const QString &fPath);
    /* ISO of the current image (sort/filter model, GUI thread) for the denoise model conditioning. */
    int currentImageIso() const;
    /* EXIF rotation (degrees) to apply to a scene-referred render so it matches the loupe. Reads
       the sort/filter model, so it MUST run on the GUI thread. */
    int developOrientationDegrees(const WorkingImage &work, const QString &fPath) const;
    /* Refresh the Develop scopes (histogram + vectorscope) from the image currently shown: the
       develop preview after a render, else the decoded image. One strided sample pass feeds both
       scopes; no-op (cheap) while the scopes are hidden. A null image clears the scopes. */
    /* verifyVsPreview=false on an interactive proxy tick: skip the grid-sampled
       "Develop differs from Preview" diagnostic, which the settle render re-measures. */
    void updateDevelopScopes(const QImage &shown, bool verifyVsPreview = true);
    /* Raise/clear the loupe's "still rendering" chip from the two SLOW develop stages
       (the off-thread raw decode and the full-res settle). See the impl for why image
       switch is answered this way rather than with a proxy cache. */
    void updateDevelopRenderingHint();
    /* Tell the loupe which geometry (crop/straighten/warp) the frame it is showing was
       rendered with, so the mask / spot overlays -- whose coordinates are all in the
       geometry stage's INPUT space -- can map onto it. Called with each applied render;
       cheap when nothing changed. */
    void pushDevelopGeometryToView();
    /* Show/hide the Develop scopes strip (Develop action-row toggle); persists the
       choice. */
    void toggleDevelopScopes();
    /* Apply a scopes-strip visibility, repopulating the scopes when it is shown;
       persists. */
    void setDevelopScopesVisible(bool isVisible);
    /* Apply a scopes-strip choice picked from the scopes context menu or the Develop
       menu's Scopes submenu: both scopes / histogram only / vectorscope only / hidden.
       Persists the layout and the visibility. */
    void setDevelopScopesChoice(int choice);
    /* Right-click in the scopes strip: `menu` arrives part-built with whatever items the
       scope under the cursor added; append the shared scopes-layout section and show it
       at globalPos. The menu is owned by the emitting scope (stack), so this must not
       keep it. */
    void showDevelopScopesMenu(QMenu *menu, QPoint globalPos);
    /* Tick the Scopes submenu item matching the strip's current state (aboutToShow). */
    void syncDevelopScopesMenu();
    /* The scopes strip's own [X]: hide the strip, keeping its layout; persists. */
    void closeDevelopScopes();
    /* Show/hide the Develop Transform panel (action-row toggle / "R"); persists. */
    void toggleDevelopTransform();
    /* Show/hide the Fill Replace panel (action-row spot button / "S" in Develop mode):
       visible == the replace (spot/fill/object) tool is armed on the loupe. */
    void toggleDevelopReplace();
    /* "W" in Develop mode: arm/disarm the Basic panel's white-balance dropper. Transform
       claims W for Warp while it is active (arbiter rule 1a), so this only runs when no
       Transform session is up. */
    void toggleDevelopWbSampler();
    void toggleMaskOverlay();     // "O": hide/show the active mask overlay tint
    /* Repaint the action-row tint button: its swatch is filled with the current overlay
       colour and it carries the blue active border while the tint is shown. */
    void refreshDevelopMaskTintBtn();
    /* Right-click on that button: pick the overlay colour / toggle the grayscale
       background without opening the Mask panel. pos is in button coordinates. */
    void showDevelopMaskTintMenu(const QPoint &pos);
    void developNewScope();       // "N": add a scope to the current image's stack
    void developAddToMask();        // "M": pop the submask type menu
    void developExport();         // "X": export the developed selection (opens ExportDlg)
    /* Develop > Export with preset > <name>: run the named export preset over the
       selection with no dialog, reporting through G::popup. */
    void developExportWithPreset(const QString &presetName);
    void buildDevelopExportPresetMenu();     // rebuild it from the preset store
    void developSavePreset();     // Cmd+Shift+N: save develop state as a preset
    /* Cmd+Opt+C / Cmd+Opt+V: copy the ticked develop settings to the develop clipboard
       and merge them onto another image (Lightroom's Copy / Paste Settings). */
    void developCopySettings();
    void developPasteSettings();
    /* Enter/exit the crop editor: enter shows the full frame + overlay (geometry suppressed); exit
       commits the crop into the image's EditStack geometry and re-renders the cropped result. */
    void enterDevelopCrop();
    void exitDevelopCrop();
    /* Esc while the Transform panel is open: cancel the session -- discard every
       crop/straighten/warp change made since it opened (restore the enter-time geometry
       snapshot), drop the overlay WITHOUT committing, and hide the panel. The counterpart
       to toggleDevelopTransform's commit-on-hide. */
    void cancelDevelopTransform();
    /* Rectify the 4-point warp: store the quad + suggested crop into the image's geometry, then
       re-render so the corrected canvas shows with the crop overlay on it (two-phase warp). */
    void rectifyDevelopCrop();
    /* Apply a drawn level line: add deltaDeg to the straighten, auto-fit the crop to the rotation
       wedges, re-render the straightened canvas and restart the crop overlay. */
    void applyDevelopLevel(double deltaDeg);
    /* Transform panel mode toggle (TransformPanel::Mode): arm the matching ImageView crop tool. */
    void setDevelopTransformMode(int mode);
    /* Level field: set the straighten to an absolute angle (typed into the panel). */
    void setDevelopLevelAngle(double degrees);
    /* Per-row Transform reset: clear just the crop / straighten / warp (TransformPanel::Mode). */
    void resetDevelopTransformMode(int mode);
    /* Loupe cursor moved to a normalized position over the displayed image: sample that pixel and
       drive the scopes' readout marker (no-op while the scopes are hidden). */
    void onImageCursorPos(double xFraction, double yFraction);
    void infoViewChanged(QStandardItem* item);
//    void filterLastDay();
    void filterDockTabMousePress();
    void syncActionsWithFilters();
    void filterChange(QString source = "");
    void quickFilter();
    void quickFilterComplete();
    bool isSortFilter();
    void invertFilters();
    void setFilterSolo();
    void toggleReject();
    void uncheckAllFilters();
    void clearAllFilters();
    void sortChangeFromAction();
    void sortReverse();
    void sortChange(QString src = "Action");
    void updateSortColumn(int sortColumn);
    void toggleSortDirectionClick();
    void toggleSortDirection(Tog n = toggle);
    void toggleIncludeSidecarsClick();
    void toggleIncludeSidecars(Tog n = toggle);
    void toggleModifyImagesClick();
    void toggleModifyImages();
    void toggleColorManageClick();
    void toggleColorManage(Tog n = toggle);
    void toggleUseRawClick();
    void toggleUseRaw(Tog n = toggle);
    void togglePanToFocusClick();
    void togglePanToFocus(Tog n = toggle);
    void allPreferences();
    void infoViewPreferences();
    void cachePreferences();
    void preferences(QString text = "");
    void externalAppManager();
    void toggleFullScreen();
    void escapeFullScreen();
    void loupeDisplay(QString src = "");
    void gridDisplay();
    void tableDisplay();
    void compareDisplay();
    void newEmbelTemplate();
    void embelTemplateChange(int id);
    void toggleZoomDlg();
    void zoomOut();
    void zoomIn();
    void zoomToFit();
    void mouseSideKeyPress(int direction);
    void keyRight(/*Qt::KeyboardModifiers modifer = Qt::NoModifier*/);
    void keyLeft();
    void keyUp();
    void keyDown();
    void keyPageUp();
    void keyPageDown();
    void keyHome();
    void keyEnd();
    void keyScrollDown();
    void keyScrollUp();
    void keyScrollPageDown();
    void keyScrollPageUp();
    void keyScrollHome();
    void keyScrollEnd();
    void keyScrollCurrent();
    void scrollToCurrentRowIfNotVisible();
    void jump();
    void zoomToggle();
    // status functions
    // void updateStatus(bool keepBase = true, QString s = "", QString source = "");
    void clearStatus();
    void showPopUp(QString msg, int duration, bool isAutosize = false,
                   float opacity = 0.75, Qt::Alignment alignment = Qt::AlignHCenter);
    // caching status functions
    void setThreadRunStatusInactive();
    void setCacheStatusVisibility();
    void setCacheRunningLightsWidth();
    QString getImageCacheRunningTip(bool isAuto, quint64 maxMB);
    void updateMetadataThreadRunStatus(bool isRun, bool success = true);
    void updateImageCachingThreadRunStatus(bool isRun, bool showCacheLabel);
    void updateImageCacheStatus(int instruction, bool isAutoSize,
                                quint64 currMB, quint64 maxMB, int tFirst, int tLast,
                                QString source);
    // caching
    void folderChanged(bool aborted);
    /*  Push one row's edited values back to the catalog, after its sidecar has
        been written. See MW::updateCatalogForRow. */
    void updateCatalogForRow(int dmRow);

    void folderChangeCompleted();
    void buildFiltersWhenModelReady(int forInstance, int attempt = 0);
    void onMemoryOverrun(quint64 footprintMB, quint64 capMB);
    void updateChange(int sfRow, bool isCurrent = true, QString src = "");

    void updateIconRange(QString src = "");
    void reloadIconChunk();             // re-dispatch MetaRead after a JIT chunk resize
    /* An image's develop edits were flushed: refresh its grid thumbnail from the new
       cached preview, or forget it so the loader re-reads the camera thumb. */
    void devPreviewUpdated(const QString &fPath, const QImage &thumb);
    /* Cached screen-res develop preview for the loupe placeholder, or null on a miss. */
    QImage devPreview(const QString &fPath);
    /* devPreviewKey of the recipe currently in force for fPath (empty when unedited).
       Captured when a develop render is launched and compared when its frame is about to
       be cached as a preview -- see developFrameRecipe. */
    QByteArray developRecipeKey(const QString &fPath);
    void thumbHasScrolled();
    void gridHasScrolled();
    void tableHasScrolled();
    void loadEntireMetadataCache(QString source);
    void updateAllFilters();
    void addNewBookmarkFromContextMenu();
    void reportMetadata();
    void checkDirState(const QString &dirPath);
    void bookmarkClicked(QTreeWidgetItem *item, int col);
    void setRatingBadgeVisibility();
    void setIconNumberVisibility();
    void setShootingInfoVisibility();
//    void selectTokenString();
    void changeInfoOverlay();
//    void toggleThumbWrap();
    void setIngested();
//    QStringList getSelectionOrPicks();
    void togglePick();
    void togglePickMouseOverItem(QModelIndex idx);
    void togglePickUnlessRejected();
    int pickLogCount();
    void recoverPickLog();
    void clearPickLog();
    void updatePickLog(QString fPath, QString pickStatus);
    void pushPick(QString fPath, QString status = "true");
    void popPick();
    void updatePickFromHistory(QString fPath, QString status);
    void updateClassification();
    void refreshFolders();
    void setFontSize(int fontPixelSize);
    void setBackgroundShade(int shade);
    void setInfoFontSize();
    void setClassificationBadgeImageDiam(int d);
    void setClassificationBadgeSizeFactor(int d);
    void setIconNumberSize(int d);
    void setPrefPage(int page);
    void setDisplayResolution();
    void getDisplayProfile();
    void updateDisplayResolution();
    void setCombineRawJpg();
    void nextSlide();
    void prevRandomSlide();
    void refreshAfterImageCacheSizeChange();
    void selectAllThumbs();
    void removeBookmark();
    void refreshBookmarks();
    void rotateLeft();
    void rotateRight();
    void showHiddenFiles();
    void thumbsEnlarge();
    void thumbsShrink();
//    void setDockFitThumbs();

    void runExternalApp();
    void cleanupSender();
    void externalAppError(QProcess::ProcessError);

    void addIngestHistoryFolder(QString fPath);
    void ingestFinished();

    void setIgnoreAddThumbnailsDlg(bool ignore);
    void setBackupModifiedFiles(bool isBackup);

    void setCentralView();

    void setThumbDockVisibity();
    void setFolderDockVisibility();
    void setFavDockVisibility();
    void setFilterDockVisibility();
    void setCatalogDockVisibility();
    void setMetadataDockVisibility();
    void setEmbelDockVisibility();
    void setDevelopDockVisibility();
    void setHistoryDockVisibility();
    void setPresetsDockVisibility();
    void setMetadataDockFixedSize();    // rgh finish or remove

    void focusOnDock(DockWidget *dockWidget);
    void closeThumbDock();
    void closeEmbelDock();
    void closeDevelopDock();
    void closeHistoryDock();
    void closePresetsDock();
    void closeFolderDock();
    void closeFavDock();
    void closeFilterDock();
    void closeCatalogDock();
    void closeMetadataDock();
    void showThumbDock();
    void showEmbelDock();
    void showDevelopDock();
    void showHistoryDock();
    void showPresetsDock();
    void showFolderDock();
    void showFavDock();
    void showFilterDock();
    void showCatalogDock();
    /*  THE ONE PLACE G::scope CHANGES. Every entry point -- either Catalog row,
        the Find dock's Folders|Catalog buttons, F2/Shift+F2, and selecting a
        folder -- routes here, and this pushes the result back to all of them so
        they cannot disagree. src is for the log only. */
    void setScope(G::Scope s, QString src = "");
    void updateCatalogScopeRows();   // push the catalogued count onto both rows
    void showMetadataDock();

    void setMenuBarVisibility();
    void setStatusBarVisibility();

    void reportWorkspaceState();
    void reportState(QString title);

    void openFolder();
    void gotoFolder();
    void openUsbFolder();
    void saveAsFile();
    void revealWinnets();
    void revealLogFile();
    void revealFile();
    void revealFileFromContext();
    void revealInFileBrowser(QString path);
    void openInFinder();
    void openInExplorer();
    void collapseAllFolders();
    void updateCollapseFoldersAction();
    void updatePickDependentActions();
    bool ownsShortcut(const QKeySequence &seq);
    QAction *disabledActionForShortcut(const QKeySequence &seq);
    QString actionDisabledReason(QAction *a);

    void newWorkspace();
    QString fixDupWorkspaceName(QString name);

    void help();
    void helpShortcuts();
    void styleShortcutsWindow(QScrollArea *w);  // (re)apply font scaling + fit columns
    void helpWelcome();

private:
//    QApplication *app;

    /* ---- Export helpers ------------------------------------------------------------
       Plain methods, deliberately NOT slots: moc cannot parse a std::function parameter
       (it reads "void(bool, const QImage&)" as a return type) and silently emits a broken
       signature for the whole class.

       prepareExport flushes pending develop edits to the sidecars, builds the selection
       and lazily creates the exporter + preset store; it returns false when there is
       nothing to export. The two *PixelSource methods are the ImageExporter::PixelSource
       implementations -- develop renders the full stored recipe (staged across the GUI
       thread and developRenderPool, like renderDevelopFullResAsync, so a batch never
       freezes the UI), preview hands over the plain browse decode. onExportFinished is
       the shared completion for every export entry point. */
    bool prepareExport(QStringList &targets);
    void developPixelSource(const QString &fPath, bool want16Bit,
                            OutputTransform::Space space,
                            std::function<void(bool, const QImage &)> done);
    void previewPixelSource(const QString &fPath,
                            std::function<void(bool, const QImage &)> done);
    void onExportFinished(const ImageExporter::Result &result, bool addToFolderView);

    QMenuBar *thumbsMenuBar;
    QMenu *fileMenu;
    QMenu *openWithMenu;
    QMenu *recentFoldersMenu;
    QMenu *ingestMenu;
    QMenu *ingestHistoryFoldersMenu;
    QMenu *editMenu;
    QMenu *ratingsMenu;
    QMenu *labelsMenu;
    QMenu *goMenu;
    QMenu *filterMenu;
    QMenu *sortMenu;

    QMenu *utilitiesMenu;
    QMenu *utilMenu;
        QMenu *developMenu;
        QMenu *embelMenu;
            QMenu *embelExportMenu;
            QMenu *embelExportOverrideMenu;
        QMenu *focusStackMenu;
    QMenu *viewMenu;
       QMenu *zoomSubMenu;
    QMenu *windowMenu;
        QMenu *workspaceMenu;
    QMenu *helpMenu;
        QMenu *logMenu;
        QMenu *helpDiagnosticsMenu;
            QMenu *testMenu;

    QMenu *viewSubMenu;
    QMenu *imageFileSubMenu;
    QMenu *MirroringSubMenu;
    QMenu *transformSubMenu;

    QList<QAction *> *mainContextActions;

    // File menu
    QAction *openAction;
    QAction *refreshCurrentAction;
    QAction *openUsbAction;
    QAction *revealFileAction;
    QAction *saveAsFileAction;
    QAction *revealFileActionFromContext;
    QAction *copyFolderPathFromContextAction;
    QAction *copyImagePathFromContextAction;
    QAction *openWithMenuAction;
        QAction *manageAppAction;
        QList<QAction *> appActions;
    QAction *recentFoldersAction;
        QList<QAction *> recentFolderActions;
    QAction *ingestHistoryFoldersAction;
        QList<QAction *> ingestHistoryFolderActions;
    // QAction *subFoldersAction;
    QAction *collapseFoldersAction = nullptr;
    QAction *addBookmarkActionFromContext;
    QAction *removeBookmarkAction;
    QAction *refreshBookmarkAction;
    QAction *ingestAction;
    QAction *ejectActionFromContextMenu;
    QAction *eraseUsbAction;
    QAction *eraseUsbActionFromContextMenu;
    QAction *colorManageAction;
    QAction *includeSidecarsAction;
    QAction *combineRawJpgAction;
    QAction *refreshFoldersAction;
    QAction *renameAction;
    QAction *showImageCountAction;
    QAction *runDropletAction;
    QAction *reportMetadataAction;
    QAction *findDuplicatesAction;
    QAction *mediaReadSpeedAction;
    QAction *reportHueCountAction;
    QAction *meanStackAction;
    QAction *focusStackAction;
    QAction *copyFocusStackWinnetPathAction;
    QAction *exitAction;

    // Open with menu in File menu

    // Edit menu
    QAction *selectAllAction;
    QAction *invertSelectionAction;
    QAction *copyFilesAction;
    QAction *copyImageAction;
    QAction *deleteImagesAction;
    QAction *deleteAction1;
    QAction *deleteActiveFolderAction;
    QAction *deleteBookmarkFolderAction;
    QAction *deleteFSTreeFolderAction;
    QAction *shareFilesAction;
    QAction *rejectAction;
    QAction *pickAction;                // shortcut "`" or "P"
    QAction *pickMouseOverAction;       // shortcut mouse forward/back buttons
    QAction *popPickHistoryAction;
    QAction *pickUnlessRejectedAction;
    QAction *filterPickAction;
    QAction *rotateLeftAction;
    QAction *rotateRightAction;
    QAction *prefAction;
    QAction *prefInfoAction;
    QAction *oldPrefAction;

    // Go Menu
    QAction *keyRightAction;
    QAction *keyLeftAction;
    QAction *keyUpAction;
    QAction *keyDownAction;
    QAction *keyPageUpAction;
    QAction *keyPageDownAction;
    QAction *keyHomeAction;
    QAction *keyEndAction;

    QAction *keyRightAddToSelectionAction;
    QAction *keyLeftAddToSelectionAction;
    QAction *keyUpAddToSelectionAction;
    QAction *keyDownAddToSelectionAction;
    QAction *keyPageUpAddToSelectionAction;
    QAction *keyPageDownAddToSelectionAction;
    QAction *keyHomeAddToSelectionAction;
    QAction *keyEndAddToSelectionAction;

    QAction *jumpAction;                    // shortcut "J", "="
    QAction *gotoFolderAction;              // shortcut "Ctrl+Shift+G"

    QAction *keyScrollLeftAction;
    QAction *keyScrollRightAction;
    QAction *keyScrollDownAction;
    QAction *keyScrollUpAction;
    QAction *keyScrollPageDownAction;
    QAction *keyScrollPageUpAction;
    QAction *keyScrollHomeAction;
    QAction *keyScrollEndAction;
    QAction *keyScrollCurrentAction;

    QAction *nextPickAction;
    QAction *prevPickAction;
    QAction *tokenTemplateEditorAction;
    QAction *rate0Action;
    QAction *rate1Action;
    QAction *rate2Action;
    QAction *rate3Action;
    QAction *rate4Action;
    QAction *rate5Action;
    QAction *label0Action;
    QAction *label1Action;
    QAction *label2Action;
    QAction *label3Action;
    QAction *label4Action;
    QAction *label5Action;

    // Filters

    QAction *filterSoloAction;
    QAction *filterUpdateAction;
    QAction *clearAllFiltersAction;
    QAction *expandAllFiltersAction;
    QAction *collapseAllFiltersAction;
    QAction *filterSearchAction;
    QAction *filterRating1Action;
    QAction *filterRating2Action;
    QAction *filterRating3Action;
    QAction *filterRating4Action;
    QAction *filterRating5Action;
    QAction *filterRedAction;
    QAction *filterYellowAction;
    QAction *filterGreenAction;
    QAction *filterBlueAction;
    QAction *filterPurpleAction;
    QAction *filterInvertAction;
    QAction *filterLastDayAction;

    // Sort Menu
    QActionGroup *sortGroupAction;
    QAction *sortFileNameAction;
    QAction *sortPickAction;
    QAction *sortLabelAction;
    QAction *sortRatingAction;
    QAction *sortFileTypeAction;
    QAction *sortFileSizeAction;
    QAction *sortCreateAction;
    QAction *sortModifyAction;
    QAction *sortCreatorAction;
    QAction *sortMegaPixelsAction;
    QAction *sortDimensionsAction;
    QAction *sortApertureAction;
    QAction *sortShutterSpeedAction;
    QAction *sortISOAction;
    QAction *sortModelAction;
    QAction *sortLensAction;
    QAction *sortFocalLengthAction;
    QAction *sortTitleAction;

    // Develop
    QAction *developAction;
    QAction *operationModeAction;   // D shortcut: enter Develop mode
    /* Original / Developed: which of an image's two pictures is shown outside Develop. */
    QActionGroup *previewSourceGroup = nullptr;
    QAction *previewSourceOriginalAction = nullptr;
    QAction *previewSourceDevelopedAction = nullptr;
    QAction *togglePreviewSourceAction = nullptr;   // Y shortcut
    QAction *buildDevPreviewsAction = nullptr;
    QAction *clearDevPreviewCacheAction = nullptr;

    /* Develop mode local shortcuts (see loadDevelopShortcuts).  These actions carry NO
       QKeySequence -- their keys live in developShortcuts and are dispatched by
       developShortcutIntercept, so S/X can also stay bound to Slideshow/Reject
       globally. */
    QAction *developNewScopeAction;     // N
    QAction *developAddToMaskAction;      // M
    QAction *developSpotAction;         // S
    QAction *developWbSamplerAction = nullptr;    // W (Transform owns W while it is up)
    QAction *developExportAction;       // X
    QAction *developSavePresetAction = nullptr;   // Cmd+Shift+N (real, mode-gated)
    QAction *developCopySettingsAction = nullptr;  // Cmd+Opt+C (real, mode-gated)
    QAction *developPasteSettingsAction = nullptr; // Cmd+Opt+V (real, mode-gated)
    /* Title-bar toggle buttons that carry the blue "active" border while their panel /
       tool is on (kept in sync from the toggle handlers). */
    BarBtn *developScopesBtn = nullptr;
    BarBtn *developTransformBtn = nullptr;
    BarBtn *developSpotBtn = nullptr;
    BarBtn *developMaskTintBtn = nullptr;  // mask overlay tint on/off (O)
    BarBtn *developPresetBtn = nullptr;    // shows/raises the Presets dock (P)
    /* Scopes-strip "choice" = a ScopesView::ScopeLayout, or this sentinel for hidden. */
    static constexpr int kDevelopScopesHidden = -1;
    /* Scopes strip choice: one exclusive checkable action per state. They fill both the
       Develop menu's Scopes submenu and the strip's right-click menu (no shortcut). */
    QActionGroup *developScopesGroup = nullptr;
    QAction *developScopesBothAction = nullptr;
    QAction *developScopesHistAction = nullptr;
    QAction *developScopesVectAction = nullptr;
    QAction *developScopesHideAction = nullptr;
    // developTransformAction (R) and toggleMaskOverlayAction (O) live with the docks

    // Embellish
    QAction *embelNewTemplateAction;
    QAction *embelExportAction;
    QAction *embelSaveTemplateAction;
    QAction *embelReadTemplateAction;
    QAction *embelManageTilesAction;
    QAction *embelManageGraphicsAction;
    QAction *embelRevealWinnetsAction;
    QActionGroup *embelExportGroupAction;
    QList<QAction *> embelTemplatesActions;

    // View Menu
    QActionGroup *centralGroupAction;
    QAction *asGridAction;
    QAction *asTableAction;
    QAction *asLoupeAction;
    QAction *asCompareAction;
    QAction *slideShowAction;
    QAction *randomImageAction; // req'd by slideshow
    QAction *fullScreenAction;
    QAction *escapeFullScreenAction;
    QAction *ratingBadgeVisibleAction;
    QAction *iconNumberVisibleAction;
    QAction *infoVisibleAction;
    QAction *infoSelectAction;
    QActionGroup *iconGroupAction;
    QAction *zoomToAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomToggleAction;
    QActionGroup *zoomGroupAction;
    QAction *panFocusToggleAction;
    QAction *thumbsEnlargeAction;
    QAction *thumbsShrinkAction;
    QAction *sortReverseAction;
    QAction *colorManageToggeAction;

    // Window Menu
    QAction *defaultWorkspaceAction;
    QAction *newWorkspaceAction;
    QAction *manageWorkspaceAction;
    QList<QAction *> workspaceActions;

    QAction *folderDockVisibleAction;
    QAction *favDockVisibleAction;
    QAction *filterDockVisibleAction;
    QAction *catalogDockVisibleAction;
    QAction *metadataDockVisibleAction;
    QAction *thumbDockVisibleAction;
    QAction *embelDockVisibleAction;
    QAction *developDockVisibleAction;
    QAction *historyDockVisibleAction = nullptr;   // "H": develop-mode local (arbiter)
    QAction *presetsDockVisibleAction = nullptr;   // "P": develop-mode local (arbiter)
    QAction *developTransformAction;    // "R": toggle the Develop Transform (crop) panel
    QAction *toggleMaskOverlayAction;   // "O": hide/show the scope's mask overlay tint
//    QAction *windowTitleBarVisibleAction;
    QAction *menuBarVisibleAction;
    QAction *statusBarVisibleAction;

    QAction *metadataFixedSizeAction;

    // Workspace Menu

    // Help Menu
    QAction *checkForUpdateAction;
    QAction *aboutAction;
    QAction *helpAction;
    QAction *helpShortcutsAction;
    QAction *helpWelcomeAction;
    QAction *helpPerformanceTipsAction;
    QAction *helpFocusStackingTipsAction;
    QAction *helpFilmStripAction;  // to be removed, not used
    QAction *helpRevealLogFileAction;

    // Help Diagnostics Menu
    QAction *diagnosticsAllAction;
    QAction *diagnosticsCurrentAction;
    QAction *diagnosticsDevelopAction;
    QAction *diagnosticsMainAction;
    QAction *diagnosticsSelectionAction;
    QAction *diagnosticsWorkspacesAction;
    QAction *viewLogIssuesAction;
    QAction *viewSessionIssuesAction;
    // QAction *clearIssuesAction;
    QAction *viewLogAction;
    QAction *clearLogAction;
    QAction *mailLogAction;
    QAction *diagnosticsGridViewAction;
    QAction *diagnosticsFSTreeAction;
    QAction *diagnosticsThumbViewAction;
    QAction *diagnosticsImageViewAction;
    QAction *diagnosticsInfoViewAction;
    QAction *diagnosticsTableViewAction;
    QAction *diagnosticsCompareViewAction;
    QAction *diagnosticsMetadataAction;
    QAction *diagnosticsXMPAction;
    QAction *diagnosticsMetadataCacheAction;
    QAction *diagnosticsImageCacheAction;
    QAction *diagnosticsMemoryAction;
    QAction *diagnosticsDataModelAction;
    QAction *diagnosticsDataModelAllRowsAction;
    QAction *diagnosticsEmbellishAction;
    QAction *diagnosticsFiltersAction;
    QAction *diagnosticsFileTreeAction;
    QAction *diagnosticsBookmarksAction;
    QAction *diagnosticsPixmapAction;
    QAction *diagnosticsThumbAction;
    QAction *diagnosticsIngestAction;
    QAction *diagnosticsZoomAction;

    // Testing Menu (under Help Diagnostics Menu)
    QAction *traverseFolderStressTestAction;
    QAction *bounceFoldersStressTestAction;

    // watch current folder in case it is deleted externally
    DirWatcher folderWatcher;
    QString lastFolderDeletedByWinnow = "";

    // Group actions
    QAction *fileGroupAct;
    QAction *ingestGroupAct;
    QAction *editGroupAct;
    QAction *goGroupAct;
    QAction *filterGroupAct;
    QAction *sortGroupAct;
    QAction *utilGroupAct;
    QAction *developGroupAct;
    QAction *embelGroupAct;
    QAction *focusStackGroupAct;
    QAction *viewGroupAct;
    QAction *windowGroupAct;
    QAction *helpGroupAct;

    // Context menus
    QList<QAction *> *fsTreeActions;
    QList<QAction *> *filterActions;

    // InfoView context menu
    QAction *copyInfoTextToClipboardAction;

    // General
    QAction *roryAction;                 // only available via shortcut key "Shift+Ctrl+Alt+."
    QAction *thriftyCacheAction;         // only available via shortcut key "F10"
    QAction *moderateCacheAction;        // only available via shortcut key "F11"
    QAction *greedyCacheAction;          // only available via shortcut key "F12"
    QAction *testAction;                 // only available via shortcut key "Shift+Ctrl+Alt+T"
    QAction *testAction1;                // only available via shortcut key "*"
    QAction *testNewFileFormatAction;    // only available via shortcut key "Shift+Ctrl+Alt+F"

    // Might need
    QAction *pasteFilesAction;
    QAction *pasteImageAction;

    // Not needed
    //    QAction *cutAction;
    //    QAction *copyAction;
    //    QAction *copyToAction;
    //    QAction *moveToAction;
    //    QAction *deleteAction;

    QScrollArea *welcome;       // welcome screen for first time use
    QWidget *messageView;
    QWidget *blankView = new QWidget;
    Ui::message msg;
    QLineEdit *filterBar;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    BarBtn *reverseSortBtn;
    BarBtn *includeSidecarsToggleBtn;
    BarBtn *colorManageToggleBtn;
    BarBtn *useRawBtn;
    QComboBox *previewSourceCombo = nullptr;   // status-bar Original / Developed dropdown
    BarBtn *panToFocusToggleBtn;
    BarBtn *modifyImagesBtn;
    BarBtn *cacheMethodBtn;
    QLabel *filterStatusLabel;
    QLabel *subfolderStatusLabel;
    BarBtn *rawJpgStatusBtn;
    QLabel *slideShowStatusLabel;
    QLabel *cacheStatusLabel;
    Progress *progress = nullptr;    // created in createDataModel, before any row driver runs
    /* Progress rows added at runtime (ImageCache is owned by Progress itself).
       Each is driven with progress->updateProgress(id, ...)/clearProgress(id). */
    int progressMetaReadRow = -1;
    int progressFocusStackRow = -1;
    int progressRawDenoiseRow = -1;
    int progressDemosaicRow = -1;
    int progressDevPreviewRow = -1;
    /* Background catalog scan over the user's designated roots. */
    int progressCatalogRow = -1;
    int statusBarBaseHeight = 0;     // status bar height before Progress; never go below
    QLabel *centralLabel;
    QLabel *statusBarSpacer;
    QLabel *statusBarSpacer1;
    QLabel *metadataThreadRunningLabel = new QLabel;
    QLabel *thumbThreadRunningLabel;
    QLabel *imageThreadRunningLabel;
    QLabel *cacheAmountLabel;
    DockWidget *folderDock;
    DockWidget *favDock;
    DockWidget *filterDock;
    /* NULL with G::useFindDock -- the Catalog is a scope of the Find dock, not a dock of
       its own, so createCatalogDock returns before building it. Initialised here because
       createDocks and placeDocksAddedSince both reach for it before that is decided. */
    DockWidget *catalogDock = nullptr;
    DockWidget *metadataDock;
    DockWidget *thumbDock;
    DockWidget *propertiesDock;
    DockWidget *embelDock;
    DockWidget *developDock;
    /* Develop edit history. Tabbed with developDock and shown/hidden with it (the two are
       one tool), so every developDock visibility change mirrors onto this. */
    DockWidget *historyDock = nullptr;
    /* Develop presets. Also tabbed with developDock and shown/hidden with it -- Develop,
       History and Presets are one tool. */
    DockWidget *presetsDock = nullptr;
    /* The dock's normal features, captured at creation so setDevelopPanelEnabled() can
       strip them (lock float/move) while disabled and restore them when re-enabled. */
    QDockWidget::DockWidgetFeatures developDockFeatures = QDockWidget::NoDockWidgetFeatures;
    /* The tool button row (Scopes, Crop, Spot, Preset, Export) is a sibling of the panel
       inside the dock, so it must be greyed explicitly when the panel is unusable. */
    QWidget *developActionRow = nullptr;
    /* Two separate states: VISIBLE = the Develop tool is in play at all (the user's
       Develop toggle + Develop operation mode); USABLE = the current selection can
       actually be developed. A video cannot, so it stays visible but greyed -- the
       alert row has to remain on screen to say why. */
    void setDevelopPanelEnabled(bool visible, bool usable);
    /* The Develop panel is enabled only when the user's Develop toggle is on AND the
       operation mode is Develop -- Preview mode always greys it out. Call after either
       input changes. */
    void syncDevelopPanelEnabled();
    /* Operation mode (G::operationMode): Preview (fast review) vs Develop (best-quality single
       image). setOperationMode applies a mode and syncs the status-bar dropdown. There is no
       toggle: D enters Develop (operationModeAction), and E / G leave it into the Loupe / Grid
       view (asLoupeAction / asGridAction); the status-bar dropdown goes either way. Behavior
       wiring (suspend read-ahead, raw re-decode on Develop) lives in setOperationMode. */
    void setOperationMode(G::OperationMode mode);
    /* Which of an image's two pictures the grid and the loupe show outside Develop mode.
       Rebuilds the icons and the image cache, because it changes what a cached decode
       MEANS -- the same modelling as toggleUseRaw. A no-op if already set. */
    void setPreviewSource(G::PreviewSource source);

    /* devPreview builder (Main/devpreviewbuilder.cpp). Renders devPreviews for images that
       are NOT open in Develop -- earlier-session edits, multi-image paste targets, and the
       DEFAULT RENDER of any raw with a sensor decoder -- one at a time on
       developPixelSource. Never runs unasked. */
    void buildDevPreviews(const QStringList &paths, const QString &src);
    void buildDevPreviewsForSelection();
    void queueBackgroundDevPreviewBuild();
    void cancelDevPreviewBuild();
    void clearDevPreviewCache();
    /* The key fPath's devPreview should carry: the recipe hash when it has edits, the
       renderer hash (Metadata::defaultRenderKey) for an unedited raw, empty when it should
       have no preview at all. */
    QString devPreviewBuildKey(const QString &fPath) const;
    /* GUI-thread continuation of buildDevPreviews, once the off-thread cache filter has
       said which paths actually need rendering. instance guards against the folder having
       changed while that ran. */
    void startDevPreviewBuild(const QStringList &paths, const QString &src, int instance);
    void devPreviewBuildNext();
    /* expectKey is the key captured when this render was dispatched; the write is dropped
       if the image has moved under it since. */
    void devPreviewStore(const QString &fPath, const QImage &full,
                         const QString &expectKey);
    void devPreviewBuildFinish(const QString &reason = QString());
    void updateDevPreviewBuildProgress();
    QStringList devPreviewBuildQueue;
    QString devPreviewBuildCurrent;      // the render in flight; not abortable
    int devPreviewBuildTotal = 0;        // 0 = no build running
    int devPreviewBuildDone = 0;
    bool devPreviewBuildCancelled = false;
    /* True when the run came from Develop > Build Developed Previews rather than from the
       background preference. Progress is shown the same way either way (the status-bar
       row), but only a run the user ASKED for reports its result in a popup -- a
       background build that announces itself is the noise this row replaced. */
    bool devPreviewBuildFromMenu = false;
    void togglePreviewSource();
    /* Develop mode always shows the developed image, so the Original / Developed control
       is disabled there (and reads Developed). Called by setOperationMode and
       setPreviewSource; owns the combo row and the checked state of the menu pair. */
    void syncPreviewSourceEnabled();
    /* Re-read the thumbnails of developed images because which picture they should show
       has changed. The effective source is (Develop mode ? Developed : G::previewSource),
       so a MODE change moves it too -- not only setPreviewSource. */
    void refreshDevelopThumbs();
    DockTitleBar *folderTitleBar;
    DockTitleBar *favTitleBar;
    DockTitleBar *filterTitleBar;
    DockTitleBar *catalogTitleBar;
    DockTitleBar *metaTitleBar;
    DockTitleBar *embelTitleBar;
    DockTitleBar *developTitleBar;
    DockTitleBar *historyTitleBar = nullptr;
    DockTitleBar *presetsTitleBar = nullptr;
    BarBtn *embelRunBtn;
    FSTree *fsTree;
    BookMarks *bookmarks;
    Filters *filters;
    /* The Catalog dock's body: cross-folder search over the local index. NULL with
       G::useFindDock, which is the default -- createCatalogDock returns before building
       it, and the guards that check it (startCatalogScan, the scanner's finished signal)
       need it to be genuinely null rather than indeterminate. It was the one member of
       this group without an initialiser, and an uninitialised pointer passes `if (p)`. */
    CatalogView *catalogView = nullptr;
    /* Walks the designated roots on its own low-priority thread. */
    CatalogScanner *catalogScanner = nullptr;
    /* Folders the user nominated for cataloguing. In QSettings, NOT in the index
       database: CacheDb::moveAside discards that file without asking, and this is
       user intent rather than derived data. */
    QStringList catalogRoots;
    bool catalogRootsRecurse = true;
    /* The roots editor. Null until first opened; MW owns the list itself, so the dialog
       may come and go without the setting being at risk. */
    CatalogRootsDlg *catalogRootsDlg = nullptr;
    /* The unified Find panel (G::useFindDock). When it exists it OWNS the layout the
       filters tree sits in, catalogDock/catalogView are never created, and the Catalog
       entry points (Shift+F2, Window > Catalog Panel) switch its scope instead of showing
       a second dock. Null when the flag is off, in which case the two original panels are
       built exactly as before. */
    FindPanel *findPanel = nullptr;
    /*  The Catalog scope row above each of the two scope trees. See
        Views/catalogscoperow.h -- two views of one fact, both mirrored by
        MW::setScope. */
    CatalogScopeRow *folderCatalogScopeRow = nullptr;
    CatalogScopeRow *favCatalogScopeRow = nullptr;
    QWidget *centralWidget;
    QGridLayout *compareLayout;
    QStackedLayout *centralLayout;
    DataModel *dm;
    Selection *sel;
    ImageCacheData *icd;
    // IconCacheData *iconCacheData;
    BuildFilters *buildFilters;
//    QItemSelectionModel *selectionModel;
    Metadata *metadata;
    IconView *thumbView;
    IconView *gridView;
    TableView *tableView;
    ImageView *imageView;
    VideoView *videoView;
    EmbelExport *embelExport;
    EmbelProperties *embelProperties;
    DevelopProperties *developProperties = nullptr;
    /* Live Develop scopes (histogram + vectorscope) shown above the property tree in the
       Develop dock. Fed by updateDevelopScopes() from the displayed image / preview render;
       toggled by a button on the Develop action row and persisted
       (Develop/scopesVisible). */
    ScopesView *scopesView = nullptr;
    bool developScopesVisible = true;
    /* Which scopes the strip shows (ScopesView::ScopeLayout, cycled by "G" and persisted
       in Develop/scopesLayout). Held as an int so mainwindow.h needs no scopes
       include. */
    int developScopesLayout = 0;
    /* The History dock's list of develop actions (hover previews a state, a click reverts
       to it). DevelopProperties owns the timeline it views. */
    HistoryView *historyView = nullptr;
    /* The Presets dock's list of saved develop presets (hover previews it applied, a
       click applies it). DevelopProperties owns the store it views. */
    PresetsView *presetsView = nullptr;
    /* Alert rows, directly below the Develop action row: coloured text on the panel
       background, one row per live condition, refreshed from the selection by
       updateDevelopSelectionWarning. Conditions stack, each in its own row; the container
       hides when there is nothing to say.
       TWO SEVERITIES, and they are different things:
         ShowStopper  Develop cannot run at all on this selection (a video). RED. The
                      panel is greyed alongside, so the row explains dead controls.
         Warning      Develop works, but not the way an unwary user assumes: with more
                      than one image selected every edit and every Paste Settings lands
                      on ALL of them (DevelopProperties::flushPropagation), off-screen
                      and invisible. AMBER -- proceed, but know what you are doing.
       Show stoppers sort above warnings, so severity reads from position as well as
       colour (which colour-blind users may not get). */
    enum DevelopAlert { AlertWarning, AlertShowStopper };
    QWidget *developAlertRows = nullptr;
    QVBoxLayout *developAlertRowsLayout = nullptr;
    QList<QLabel *> developAlertLabels;      // pooled rows, surplus hidden not deleted
    void updateDevelopSelectionWarning();
    /* Fill the alert rows, one message per row, most severe first. Empty = hide. */
    void setDevelopAlerts(const QList<QPair<DevelopAlert, QString>> &alerts);
    /* Develop Transform (crop + perspective) panel: a control strip below the scopes
       and above the property tree. Toggled by a button on the Develop action row and
       the "R" shortcut; visibility persists (Develop/transformVisible). */
    TransformPanel *transformPanel = nullptr;
    /* Fill Replace (spot/fill/object heal) strip below the Transform panel. Visible only
       while the replace tool is armed (like the Transform panel / crop tool pairing). */
    ReplacePanel *replacePanel = nullptr;
    bool developTransformVisible = false;
    /* True while the crop tool is being edited: the render pipeline then SUPPRESSES the geometry
       (shows the full frame for the overlay); committing the crop clears it and re-renders cropped. */
    bool developCropEditing = false;
    /* Transform Preview eye "live result toggle" (only meaningful while developCropEditing): true =
       peek at the cropped/warped RESULT (overlay dropped, geometry applied in the render); false =
       normal editing (full frame + overlay). Reset on enter/exit crop. */
    bool developCropShowResult = false;
    /* Snapshot of the image's Geometry taken when the Transform panel opens
       (enterDevelopCrop), so Esc (cancelDevelopTransform) can restore it. Straighten and
       warp are written live during a session, so a cancel must revert them, not just skip
       the pending overlay crop. */
    Geometry developCropGeometryBackup;
    /* The QImage last pushed to the scopes (== what the loupe shows; implicitly shared, so
       holding it is free). Sampled per pixel for the cursor readout marker. */
    QImage developShownImage;
    /* Develop slider-drag preview pipeline. A drag re-renders only the cheap Develop +
       OutputTransform stage from the cached pre-develop WorkingImage. To stay interactive on
       large RAW files the drag renders a screen-resolution PROXY (developProxy, cached per
       path), coalesced so a fast drag collapses to one render; a settle timer then renders the
       full-resolution image once the slider stops. See MW::developParamsChange /
       renderDevelopPreview and notes/Documentation.txt "DEVELOP / IMAGE EDIT". */
    /* The developed frame last shown in the loupe, kept only so a preview that is derived
       from what is ON SCREEN (the sharpening mask preview) can be rebuilt without re-running
       the pipeline. Replaced by every applied proxy / full-res render. */
    QImage developFrame;
    QString developFramePath;
    /* developFrame depicts the STORED recipe (not a History hover, and with the Transform
       and Replace preview eyes on). Only a faithful frame may be cached as a preview. */
    bool developFrameFaithful = false;
    /* THE RECIPE THIS FRAME DEPICTS -- devPreviewKey of the blob captured when the render
       was LAUNCHED, or empty when the frame is not faithful.

       Path + faithful is not enough to say a retained frame may be cached as a preview.
       A frame is replaced only when a render COMPLETES, and a render completes only if
       one was armed and its result was still current when it landed; edit, then leave
       Develop / switch image / quit inside the settle window and the retained frame is
       the PREVIOUS recipe's -- with the same path and the same faithful flag. Writing it
       out stamps old pixels with the new recipe's hash, which is worse than having no
       preview: the key matches, so every staleness check downstream passes and nothing
       ever repairs it. The devPreview provider compares this against the recipe it is
       flushing and declines on a mismatch. */
    QByteArray developFrameRecipe;
    /* The FULL-RESOLUTION settle render, retained so the devPreview tier can be
       encoded at sensor resolution rather than from the screen-resolution proxy in
       developFrame. Set by onDevelopFullResReady under the same currency guard that
       decides the render may be shown. The devPreview provider prefers it and falls back
       to developFrame, which keeps a preview a byproduct of a render that happened
       anyway. */
    QImage developFullFrame;
    QString developFullFramePath;
    bool developFullFrameFaithful = false;
    QByteArray developFullFrameRecipe;      // see developFrameRecipe
    std::shared_ptr<WorkingImage> developProxy;
    QString developProxyPath;
    /* Per-scope intermediates for the PROXY tick, so a mask drag re-rasterizes only the
       scope it is dragging (see Develop/developstackcache.h). GUI thread only -- the
       off-thread settle render passes nullptr and recomputes, which is what keeps this
       lock-free. Dropped with the proxy. */
    DevelopStackCache developStackCache;
    QTimer *developProxyRenderTimer = nullptr;   // coalesces rapid ticks into one proxy render
    QTimer *developFullResTimer = nullptr;       // fires once the drag settles (full-res render)
    /* Full-res settle render runs OFF the GUI thread (it is ~1.3s on a 50MP RAW and would
       otherwise freeze the drag). developRenderPool drives one such render at a time; the result
       is marshalled back to the GUI thread and applied only if still current. developParamsGen is
       bumped on every slider change so a finished render that no longer matches the latest params
       (or image) is discarded. See MW::renderDevelopFullResAsync. */
    static constexpr int kDevelopSettleMs = 160;  // full-res render fires this long after the drag pauses
    QThreadPool *developRenderPool = nullptr;
    quint64 developParamsGen = 0;                 // ++ on every Develop param change (staleness guard)
    bool developFullResInFlight = false;      // a background full-res render is running
    /* Interactive PROXY render, off the GUI thread. The brush cursor and every mask
       overlay are painted by ImageView on the GUI thread, so compositing there capped how
       smoothly the cursor could move no matter how cheap the render got. Its own 1-thread
       pool, separate from developRenderPool so a settled full-res render cannot block an
       interactive tick. developProxyPending is what makes it correct for the callers that
       re-render WITHOUT bumping developParamsGen (crop, warp, level): a request arriving
       mid-flight is remembered and re-armed on completion rather than dropped.
       developProxyReqGen identifies a frame that a newer request has superseded.

       A superseded frame is still SHOWN, as long as the geometry it was rendered with is
       still the geometry in force (developProxyGeomGen). A drag delivers mouse events far
       faster than a render completes, so every frame but the last is superseded -- and
       dropping them all meant the loupe did not update at all until the user paused,
       which is the lag itself. The frames are only tens of ms old and strictly ordered,
       so showing them gives a smooth stream. The ONE thing that must never be shown late
       is a frame whose geometry no longer applies: the crop tool alternates
       geometry-applied and geometry-suppressed renders and flashing the wrong one reads
       as a glitch. That is what the generation counter guards -- geometry, not age. */
    QThreadPool *developProxyPool = nullptr;
    /* One-shot guard for the develop-preview cache sweep, run from
       folderChangeCompleted (see Cache/devpreviewcache.h). */
    /* True when the loupe placeholder currently showing is a cached DEVELOP preview
       rather than the camera's embedded JPG, so the rendering hint can say so. */
    bool developInterimIsDevPreview = false;
    bool devPreviewSweepDone = false;
    /* One-shot guard for the catalog sweep, run from folderChangeCompleted
       (see Cache/catalog.h). */
    bool catalogSweepDone = false;
    bool developProxyInFlight = false;
    bool developProxyPending = false;
    quint64 developProxyReqGen = 0;
    quint64 developProxyGeomGen = 0;      // ++ when a request's geometry differs
    Geometry developProxyLastGeom;        // what developProxyGeomGen counts changes to
    bool developProxyHaveLastGeom = false;

    /* [DevTime] probe only (G::isReportDevelopTime): updateMaskOverlayTint accumulates
       here and renderDevelopPreview reports + resets. The COUNT matters as much as the
       ms -- more than one veil rebuild per render means the tint is not coalesced. */
    qint64 developTintMs = 0;
    int developTintCount = 0;

    /* Develop render verification (cheap, updated on each full-res settle; reported by
       the Develop diagnostics). Renders the base at a small fixed size TWICE -- the clean
       base with identity params vs the actual base (incl. raw denoise) with the recipe
       (geometry suppressed so sizes match) -- and measures the pixel difference. Confirms
       the develop pipeline truly changed pixels: a maxAbs of 0 means the render is pixel-
       identical to the un-developed original (the failure class behind the denoise +
       Apple-demosaic bugs). See renderDevelopFullResAsync. */
    int developVerifyMaxAbs = -1;    // max |edited-original| 0..255; -1 = not run
    double developVerifyMeanAbs = -1.0;   // mean |edited-original| 0..255
    // no non-geometry edits at verify time (so no change expected):
    bool developVerifyRecipeIdentity = true;
    // crop/warp present (changes pixels independently of the recipe):
    bool developVerifyGeometryActive = false;
    QString developVerifyPath;       // image the last recipe verification ran on

    /* Second verification axis -- the Develop DISPLAY vs the Preview (embedded)
       image. The Preview image is captured (downscaled) on entering Develop; each
       develop display (edited OR not, via updateDevelopScopes) is grid-sampled against
       it. This is what confirms the DECODE change (embedded preview -> demosaic) actually
       altered pixels, independent of any edits -- so it populates even for an unedited
       image (unlike the recipe check, which needs an edited settle). */
    // downscaled Preview (embedded) image; captured on Develop entry:
    QImage developVerifyPreviewBaseline;
    QString developVerifyPreviewBaselinePath;
    // max channel diff (0..255) display vs Preview; >0 = changed:
    int developVerifyVsPreviewMaxAbs = -1;
    double developVerifyVsPreviewMeanAbs = -1.0;
    // image the last display-vs-Preview verification ran on:
    QString developVerifyVsPreviewPath;
    /* Interactive "Denoise raw": PMRID is a heavy DNN run PRE-demosaic (Global-only, RAW-only,
       Winnow engine only). ensureRawDenoise re-decodes the raw with PMRID once to build the
       full-strength denoised base (developPmridFull, keyed path+iso), then blends clean<->PMRID by
       the two amounts (luma/chroma split) into developDenoised (keyed path+amounts+iso). So the
       model runs ONCE per image; amount changes only re-blend (cheap), and later exposure/tone
       edits reuse the base. With no denoise the render uses the clean cached image unchanged. See
       developRawDenoisedBase / ensureRawDenoise. */
    std::shared_ptr<const WorkingImage> developDenoised;
    QString developDenoisedKey;                   // "path|dnL|dnC|iso"; empty when clean
    QString developDenoiseInFlightKey;            // key currently being computed (coalesce guard)
    // full-strength PMRID base, reused across amounts
    std::shared_ptr<const WorkingImage> developPmridFull;
    QString developPmridKey;                      // "path|iso" for developPmridFull
    /* PMRID proved it cannot denoise: session-wide when the model itself cannot run (no
       ONNX Runtime / no pmrid.onnx -- nothing on any image will change that), per path
       when only that sensor's CFA is unsupported. Both gate ensureRawDenoise so a failed
       denoise is not retried as a full raw re-decode on every settle. */
    bool rawDenoiseUnavailable = false;
    QSet<QString> rawDenoiseUnsupported;
    /* Diagnostic snapshot of the (k,b) noise-model tier (PMRID::resolveKB) that produced
       developPmridFull -- valid for exactly developPmridKey, so the Develop diagnostics
       report shows it ONLY for the current image's base, never a stale one. Set in
       ensureRawDenoise when PMRID decodes; cleared/reset with developPmridFull. */
    QString developPmridResSource;                // resolveKB tier name; "" = unset
    double  developPmridResK = 0.0;
    double  developPmridResB = 0.0;
    bool    developPmridResHadNP = false;
    QString developWorkInFlight;                  // path whose scene-linear decode is running (ensureDevelopWork coalesce)
    QString developWorkTriedPath;                 // path already async-decoded but with no scene-linear result
                                                  // (display-referred format) -> render the fallback, don't loop
    /* Content-range mask (Luminance/Color Range) reference: a display-referred RGB map of the
       developed GLOBAL scope, registered by path (RangeMask::putRef) and sampled identically by
       the loupe overlay and the render. Global-only so a range mask cannot feed back on its own
       selection. Rebuilt only when the base params or the image change (range-slider ticks and
       colour samples just re-threshold), tracked by path + a base-params signature. */
    void ensureRangeRef(const QString &fPath, const WorkingImage &work,
                        const EditParams &base, int degrees);
    QString developRangeRefPath;
    QByteArray developRangeRefBaseKey;
    /* AI "Select Subject" mask: the U^2-Net saliency map (SubjectMask store) built once per image by
       ensureSubjectMask. Keyed on path only (independent of develop sliders). The predictor loads
       u2net.onnx lazily on first use. */
    void ensureSubjectMask(const QString &fPath, const WorkingImage &work,
                           const EditParams &base, int degrees);
    /* Build the AI coverage + show the loupe tint the moment a Subject/Sky mask is selected -- the
       render path skips identity scopes, so it cannot be relied on to build the ref before the user
       has adjusted a slider. Connected to DevelopProperties::maskEditBegin; no-op for other tools. */
    void onAiMaskEditBegin(int tool, int op, bool inverted, const QString &paramsJson,
                           double feather);
    /* Rebuild (or clear) the whole-mask coverage tint shown in the loupe while a
       submask is being defined: composite the active scope's submasks -- INCLUDING
       the pending one, with the op the modifiers are previewing -- into a
       single-colour veil and hand it to ImageView. Cheap (capped resolution); a
       no-op when nothing is being edited. */
    void updateMaskOverlayTint();
    /* Build (or clear) the SHARPENING mask preview: while Opt is held during a Masking
       drag, show the sharpen edge gate in grayscale over the photo (white sharpened,
       black protected), as Lightroom's Alt-drag does. Derived from the developed frame on
       screen (developFrame), so it costs one blur + Sobel at proxy resolution and needs
       nothing from the render path. */
    void updateSharpenMaskPreview();
    /* Read Opt/Shift+Opt while a submask is being defined and push the previewed op into
       DevelopProperties (Opt = Subtract, Shift+Opt = Intersect, neither = Add). Ignored
       mid-stroke, where Opt means "erase from this stroke". */
    void syncPendingMaskOp();
    QString developSubjectRefPath;
    class SubjectPredictor *subjectPredictor = nullptr;
    /* AI "Select Sky" mask: single-channel sky coverage (SkyMask store) built once per image by
       ensureSkyMask (skyseg.onnx, lazily loaded). Keyed on path only. Twin of the Subject mask. */
    void ensureSkyMask(const QString &fPath, const WorkingImage &work,
                       const EditParams &base, int degrees);
    QString developSkyRefPath;
    class SkyPredictor *skyPredictor = nullptr;
    /* AI "Depth Range" mask: a MiDaS depth field (DepthMask store) built once per image by
       ensureDepthMask (midas.onnx, lazily loaded). Keyed on path only. The mask selects a [near,far]
       band of the field (like Luminance Range over depth). */
    void ensureDepthMask(const QString &fPath, const WorkingImage &work,
                         const EditParams &base, int degrees);
    QString developDepthRefPath;
    class DepthPredictor *depthPredictor = nullptr;
    /* AI "Object Mask" (SAM 2): a PROMPTABLE brush mask. Two-phase, unlike the other AI masks:
       ensureObjectMask encodes the developed base ONCE per image (cached in objectMaskPredictor)
       then decodes the component's brush stroke (paramsJson) into an ObjectRef. Because the coverage
       DEPENDS on the brush, the ObjectRef is keyed by path + brush signature (objectRefKey), so
       several object masks on one image coexist. Lazily loads sam2_encoder/decoder.onnx. */
    void ensureObjectMask(const QString &fPath, const WorkingImage &work,
                          const EditParams &base, int degrees, const QString &paramsJson);
    /* Lazily load the predictor and ensure a SAM 2 encoder embedding covering normRoi
       (output-normalized) is cached (Phase 1). The guide is scaled so
       that ROI resolves at ~1024 px and only the ROI is encoded -- fine structure needs the
       resolution, and the encoder's input is a fixed 1024^2 either way. Outputs the FULL oriented
       guide dims (gw,gh) plus the encoded sub-rect within them. False + nothing cached on failure. */
    bool ensureObjectEncoder(const QString &fPath, const WorkingImage &work,
                             const EditParams &base, int degrees, const QRectF &normRoi,
                             int &gw, int &gh, QRect &crop);
    QString developObjectImagePath;   // path whose encoder embedding is cached in objectMaskPredictor
    /* What that cached embedding actually covers, so ensureObjectEncoder can reuse it instead of
       paying the ~1s encode again: the output-normalized region requested, the full guide dims it
       was computed at, and the crop within them that was fed to the encoder. */
    QRectF developObjectRoi;
    int    developObjectGuideW = 0, developObjectGuideH = 0;
    QRect  developObjectCrop;
    class ObjectMaskPredictor *objectMaskPredictor = nullptr;
    Preferences *pref = nullptr;
    StressTest *stressTest;
    QFrame *embelFrame;
    Embel *embel;
    InfoString *infoString;
    PropertyEditor *propertyEditor;
    QHeaderView *headerView;
    CompareImages *compareImages;
    MetaRead *metaRead;
    Thumb *refreshThumb = nullptr;    // lazily created; refreshes a single icon after in-place replace
    ImageCache *imageCache;
    QThread imageCacheThread;
    InfoView *infoView;
    Ingest *backgroundIngest = nullptr;
    IngestDlg *ingestDlg;
    /* Export: one engine + one preset store shared by the Develop export, the no-dialog
       preset export and File > Save Preview as. Created lazily by prepareExport(). */
    ImageExporter *imageExporter = nullptr;
    ExportPresets *exportPresets = nullptr;
    QMenu         *developExportPresetMenu = nullptr;
    QMenu         *developScopesMenu = nullptr;
    LoadUsbDlg *loadUsbDlg;
    AboutDlg *aboutDlg;
    WorkspaceDlg *workspaceDlg;
    PreferencesDlg *preferencesDlg = nullptr;
    UpdateAppDlg *updateAppDlg;
    QNetworkAccessManager *updateNetManager = nullptr;
    ZoomDlg *zoomDlg = nullptr;
    QTimer *slideShowTimer;
    QTimer *resetTimer;
    QTimer *memoryWatchdog = nullptr;
    void memoryWatchdogTick();
    bool memoryDialogActive = false;
    bool memoryThrottleActive = false;  // ImageCache decode throttle engaged (pressure)
    QWidget *folderDockEmptyWidget;
    QWidget *favDockEmptyWidget;
    QWidget *filterDockEmptyWidget;
    QWidget *metadataDockEmptyWidget;
    QWidget *embelDockEmptyWidget;
    QWidget *thumbDockEmptyWidget;
    QVBoxLayout *imageViewContainer;
    Stack *meanStack;

    QStandardItemModel *imageModel;

    QSoundEffect *pickClick = new QSoundEffect();
    float pickClickvolume = 0.25;

    QHash<QString, bool> stopped;
    bool allIdle() const;

    // pick history
    struct Pick {
        QString path;
        QString status;
    } pick;
    QStack<Pick> *pickStack;

    // focus stack
    QPointer<FS> fsPipeline;   // current running pipeline (null when inactive)
    QThread *fsThread;

    // slideShow counter
    int slideCount = 0;
    bool prevUseImageCache = true;

    // used in visibility and focus setting for docks
    enum {SetFocus, SetVisible} dockOption;

    QString prevScreenName;                 // the monitor being used be winnow
    QPoint prevScreenLoc = QPoint(-1,-1);   // the centroid of Winnow window in monitor
    qreal prevDevicePixelRatio = -1;

    // QList<QWidget*> openWindows;
    QList<QPointer<QWidget>> openWindows;
    QList<QPointer<QScrollArea>> shortcutsWindows;   // open Shortcuts help, for live font updates

    bool ignoreDockResize;
    bool wasThumbDockVisible;
    bool workspaceChange;
    bool isUpdatingState;
    bool embelDockTabActivated;

    bool isFilterChange = false;        // prevent fileSelectionChange

    bool simulateJustInstalled;
    bool isSettings = false;
    // bool isStressTest;
    int stressSecToGoInFolder;
    bool hasGridBeenActivated;
//    bool isSlideshowPaused;
    bool isSlideShowHelpVisible;

    bool isLeftMouseBtnPressed = false;
    bool isMouseDrag = false;

    bool isZoomDlgVisible = false;

    bool ignoreAddThumbnailsDlg = false;

    bool sortMenuUpdateToMatchTable = false;

    QString imageCacheFilePath;

    bool newScrollSignal;           // used for scroll signal delay in case many/sec
    QTimer *metadataCacheScrollTimer;

    int prevCentralView;
    QString mouseOverFolderPath;        // current mouseover folder in folder dock used
                                    // in context menu to eject usb drives

    QString rating = "";
    QString colorClass = "";
    bool isPick;
    bool isReject;

    QFile pickLog;

    QString folderDockTabText;
    QString folderDockTabRichText;
    QString favDockTabText;
    QString filterDockTabText;
    QString catalogDockTabText;
    QString metadataDockTabText;
    QString embelDockTabText;
    QString developDockTabText;
    QString historyDockTabText;
    QString presetsDockTabText;
    QString thumbDockTabText;

    QStringList dockTextNames;
    QList<QTabBar*> dockTabBars;
    //QList<RichTextTabBar*> dockRichTextTabBars;

    void createEmbel();
    void createDocks();
    void createFolderDock();
    void createFavDock();
    void createFilterDock();
    void createCatalogDock();
    void createMetadataDock();
    void createThumbDock();
    void createEmbelDock();
    void createDevelopDock();
    void createHistoryDock();
    void createPresetsDock();
    QTabBar* tabifiedBar();
    bool isDockTabified(QDockWidget *dock);
    QString dockTabToolTip(const QString &tabText);
    void tabBarAssignRichText(QTabBar *richTextTabBar);
    bool tabBarContainsDocks(QTabBar *tabBar);
    bool isSelectedDockTab(QDockWidget *dock);
    void updateDockTabGraphics(QTabBar *tabBar);   // responsive text/graphic dock tab titles
    void scheduleDockTabUpdate();                  // deferred re-eval of all dock tab bars
    void moveDroppedDockLast();  // a dock dropped into a tab group lands last
    QDockWidget* dockForTabText(const QString &tabText);
    QHash<quint64, QString> dockTabTitleByKey;     // QMainWindow tab key -> dock title (learned)
    void folderDockVisibilityChange();
    void embelDockActivated(QDockWidget *dockWidget);
    void embelDockVisibilityChange();
    void developDockVisibilityChange();
    void historyDockVisibilityChange();
    void presetsDockVisibilityChange();
    /* The Develop action-row Preset button reads as a checked toolbutton while the
       Presets dock is the front tab. A TABIFIED dock keeps isVisible() == true when a
       sibling tab is selected, so visibilityChanged alone cannot see a tab switch --
       this is called from there AND from tabifiedDockWidgetActivated /
       showPresetsDock. */
    void updateDevelopPresetBtn();

public:
    // dock collapse/expand area-scoped helpers (used by DockTitleBar context menu)
    QList<DockWidget*> docksInArea(Qt::DockWidgetArea area) const;
    void collapseDocksInArea(Qt::DockWidgetArea area);
    void expandDocksInArea(Qt::DockWidgetArea area);
    void setDockSoloModeForArea(Qt::DockWidgetArea area, bool on);
    bool dockSoloModeForArea(Qt::DockWidgetArea area) const;
    void enforceDockSoloMode(DockWidget *justExpanded);
    void applyDockCollapseState();   // restores per-dock collapsed flag from QSettings
private:
    QHash<Qt::DockWidgetArea, bool> m_dockSoloMode;
    void updateState();
    void deleteViewerImage();
    // actions
    void addMenuSeparator(QWidget *widget);
    void createActions();
    void createFileActions();
    void createIngestActions();
    void createEditActions();
    void createGoActions();
    void createFilterActions();
    void createSortActions();
    void createUtilActions();
    void createViewActions();
    void createWindowActions();
    void createHelpActions();
    void createMiscActions();
    void createBookmarks();
    void createLinearMetaRead();
    void createMetaRead();
    void createCatalogScanner();
    void createImageCache();
    void createCentralWidget();
    void createCompareView();
    void createDataModel();
    void createFilterView();
    void createFSTree();
    void createImageView();
    void createVideoView();
    void createInfoView();
    void createInfoString();
    // menus
    void createMenus();
    void createFileMenu();
    void createIngestMenu();
    void createEditMenu();
    void createGoMenu();
    void createFilterMenu();
    void createSortMenu();
    void createUtilMenu();
    void createViewMenu();
    void createWindowMenu();
    void createHelpMenu();
    void createMainMenu();
    void createMainContextMenu();
    void createFSTreeContextMenu();
    void createBookmarksContextMenu();
    void createFiltersContextMenu();
    void createInfoViewContextMenu();
    void createThumbViewContextMenu();
    void createFocusStackMenu();
    void createEmbellishMenu();

    void createTableView();
    void createThumbView();
    void createGridView();
    void createSelectionModel();
    void createSelection();
    void createStatusBar();
    int availableSpaceForProgressBar();
    void updateProgressBarWidth();
    /* Gate the cache progress rows (ImageCache + MetaRead) as a group, honoring
       the "Show caching progress" preference. */
    void setCacheProgressEnabled(bool on);
    void updateStatusBar();
    void createMessageView();
    void createPreferences();
    void createStressTest();
    void createAppStyle();
    void setupCentralWidget();
    void initialize();
    void migrateOldSettings();
    void setupPlatform();
    void recoverGeometry(const QByteArray &geometry, RecoverGeometry &r);
    void checkRecoveredGeometry(const QRect &availableGeometry, QRect *restoredGeometry,
                               int frameHeight);
    void setfsModelFlags();
    void removeDeprecatedSettings();
    void writeSetting(QString key, QVariant value);
    void writeSettings();
    bool loadSettings();
    void loadShortcuts(bool defaultShortcuts);
    /* Build the Develop mode local shortcut table (key -> action).  Called at the end of
       loadShortcuts, once every action exists. */
    void loadDevelopShortcuts();
    /* Develop mode shortcut arbiter, called from eventFilter on QEvent::ShortcutOverride.
       Returns true when it has consumed the key. */
    bool developShortcutIntercept(QEvent *event);
    bool thumbViewHasFocus() const;     // the selection keys' gate in Develop mode
    /* Grey the Develop menu's mode-local items outside Develop mode (aboutToShow). */
    void syncDevelopMenuEnabled();
    void startLog();
    void closeLog();
    void clearLog();
    void startErrLog();
    void closeErrLog();
    void trimErrLog(QFile &errorLog, int daysToKeep);

    bool isValidPath(QString &path);
    QString getSelectedPath();

    // located in fileoperations.cpp
    void copyFiles();
    void pasteFiles(QString folderPath = "");
    void shareFiles();
    void copyFolderPathFromContext();
    void copyImagePathFromContext();
    void renameSelectedFiles();
    void dmInsert(QStringList pathList);
    void insertFiles(QStringList fPaths);
    void deleteSelectedFiles();
    void deleteFiles(QStringList paths);
    void deleteFolder();
    void deleteAllImageMemCard(QString rootPath, QString name);
    void eraseMemCardImages();
    void eraseMemCardImagesFromContextMenu();

    bool removeDirOp(QString dirToDelete);
    void addBookmark(QString path);
    void populateWorkspace(int n, QString name);
    void syncWorkspaceMenu();
    void syncEmbellishMenu();
    void getSubfolders(QString fPath);
    QString getPosition();
    QString getZoom();
    QString getPicked();
    QString getRawRendered();
    QString getSelectedFileSize();
    double macActualDevicePixelRatio(QPoint loc, QScreen *screen);
    bool isFolderValid(QString fPath, bool report, bool isRemembered = false);
    void addRecentFolder(QString fPath);

    QRect testR;

    qulonglong memoryReqdForPicks();
    qulonglong memoryReqdForSelection();

    FocusPointTrainer *focusPointTrainer = new FocusPointTrainer;

    void fitDiagnostics(QDialog *dlg, QTextBrowser *textBrowser);
    void diagnosticsAll();
    void diagnosticsCurrent();
    QString diagnostics();
    QString developDiagnostics();
    void diagnosticsDevelop();
    void diagnosticsMain();
    void diagnosticsSelection();
    void diagnosticsWorkspaces();
    void diagnosticsGridView();
    void diagnosticsThumbView();
    void diagnosticsImageView();
    void diagnosticsInfoView();
    void diagnosticsTableView();
    void diagnosticsCompareView();
    void diagnosticsMetadata();
    void diagnosticsXMP();
    void diagnosticsMetadataCache();
    void diagnosticsImageCache();
    void diagnosticsMemory();
    void diagnosticsDataModel();
    void diagnosticsDataModelAllRows();
    void diagnosticsEmbellish();
    void diagnosticsErrors();
    void diagnosticsFilters();
    void diagnosticsFSTree();
    void diagnosticsBookmarks();
    void diagnosticsPixmap();
    void diagnosticsThumb();
    void diagnosticsIngest();
    void diagnosticsZoom();
    void diagnosticsReport(QString reportString, QString title = "Winnow Diagnostics");
    void allIssuesReport();
    void sessionIssuesReport();
    void logReport();
    void mailLogs();

    // HTML Help
    void helpPerformanceTips();
    void helpFocusStackingTips();

    void mediaReadSpeed();
    void findDuplicates();
    void reportHueCount();
    void generateMeanStack();

    // Focus Stack Utility
    QString fsMethod;
    bool fsRemoveTemp = true;
    bool fs8bit = true;
    void focusStackFromSelection();
    void groupFocusStacks(QList<QStringList> &groups, const QStringList &paths);
    void generateFocusStack(const QStringList paths, QString method,
                            const bool isLocal);
    void copyFocusStackWinnetPath();

    void scrollImageViewStressTest(int ms, int pauseCount, int msPauseDelay);
    void traverseFolderStressTestFromMenu();
    void traverseFolderStressTest(int msPerImage = 0, double secPerFolder = 0,
                                  bool uturn = true);
    void bounceFoldersStressTestFromMenu();
    void bounceFoldersStressTest(int msPerImage = 0, double secPerFolder = -1);
    template<typename T> void test2(T& io, int x);
    void ingestTest(QWidget *target);
    void testNewFileFormat();       // for debugging

    QString readSvgFileToString(const QString &filePath);

    QElapsedTimer testTime;
    bool testCrash = false;

    QByteArray geo;
    QByteArray sta;
    void toggleRory();
    void rory();

};
#endif // MAINWINDOW_H
