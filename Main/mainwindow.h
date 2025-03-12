#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include <QtConcurrent>
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
#include "Views/infostring.h"
#include "Metadata/metadata.h"
#include "Main/dockwidget.h"
#include "Embellish/Properties/embelproperties.h"
#include "Embellish/embelexport.h"
#include "Embellish/embel.h"

#include "Cache/cachedata.h"
#include "Cache/metaread.h"
#include "Cache/imagecache.h"
#include "Cache/framedecoder.h"

#ifdef Q_OS_WIN
#endif

#include "File/ingest.h"
#include "ingestdlg.h"
#include "saveasdlg.h"
#include "aboutdlg.h"
#include "findduplicatesdlg.h"
//#include "selectionorpicksdlg.h"
#include "Image/thumb.h"
#include "preferencesdlg.h"
#include "updateapp.h"
#include "workspacedlg.h"
#include "zoomdlg.h"
#include "loadusbdlg.h"
#include "erasememcardimagesdlg.h"
#include "Utilities/utilities.h"
#include "Utilities/renamefile.h"
#include "Utilities/usb.h"
#include "Utilities/inputdlg.h"
#include "Utilities/htmlwindow.h"
#include "progressbar.h"

#include "Utilities/coloranalysis.h"
#include "Utilities/dirwatcher.h"
#include "Image/stack.h"
#include <QSoundEffect>

#include "Utilities/performance.h"
#include "ui_helpform.h"
#include "ui_shortcutsform.h"
#include "ui_welcome.h"
#include "ui_message.h"
#include "ui_test12.h"

#include "Metadata/ExifTool.h"
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

class MW : public QMainWindow
{
    Q_OBJECT

    friend class Preferences;       // mw;
    friend class PropertyEditor;    // mw;
    friend class ProgressBar;       // mw1
    friend class IconView;          // mw2
    friend class EmbelProperties;   // mw3
    friend class InfoString;        // mw4
    friend class TableView;         // mw5

public:
    MW(const QString args, QWidget *parent = nullptr);

    /*
    alpha, beta, gamma, delta, epsilon, zeta, eta, theta, iota, kappa, lambda, mu, nu,
    xi, omicron, pi, rho, sigma, tau, upsilon, phi, chi, psi, and omega.
    */
    QString versionNumber = "1.41_gamma" ;

    QString version = "Version: " + versionNumber;
    QString winnowWithVersion = "Winnow " + versionNumber;
    QString website = "Website: "
//            "<a href=\"http://165.227.46.158/winnow/winnow.html\">"
            "<a href=\"winnow.ca\">"
            "<span style=\" text-decoration: underline; color:#e5e5e5;\">"
            "Winnow main page</span></a>.  "
            "Includes links to download and video tutorials.</p></body></html>";

    bool isShiftOnOpen;               // used when opening if shift key pressed
    QString args;                     // opening args  // rgh req'd?

    // debugging flags
    bool ignoreSelectionChange = false;
    bool isStartupArgs = false;
    bool hideEmbellish = false;

    int copyCutCount;   // rgh req'd?
    QTextStream rpt;

    // QSettings
    QSettings *settings;
    QMap<QString, QAction *> actionKeys;

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
        bool isMetadataDockVisible;
        bool isEmbelDockVisible;
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
        QString cacheSizeMethod;
        int sortColumn;
        bool isReverseSort;
    };
    WorkspaceData ws;   // hold values for workspace n, current workspace
    WorkspaceData *w;
    QList<WorkspaceData> *workspaces;

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
        bool isFolders;
        bool isFavs;
        bool isFilters;
        bool isMetadata;
        bool isThumbs;
        bool isStatusBar;
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
    bool checkIfUpdate = true;
    // bool autoAdvance = false;    // move to global
    bool turnOffEmbellish = true;
    bool deleteWarning = true;
    bool isStartingWhileUpdating = true;

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
//    bool updateImageCacheWhenFileSelectionChange = true; // rghcachechange
    QStack<QString> *slideshowRandomHistoryStack;

    // preferences: cache
    QString cacheMethod;
    QString cacheSizeStrategy;
    QString cacheMinSize;
    int cacheSizePercentOfAvailable;
    int cacheMinMB = 2000;
    int cacheMaxMB;
    int cacheDelay = 100;
    bool isShowCacheProgressBar;
    int cacheBarProgressWidth;
    int progressWidthBeforeResizeWindow;
    int cacheWtAhead;
    bool isCachePreview;
    int cachePreviewWidth;
    int cachePreviewHeight;

    // first use
    bool isFirstTimeTableViewVisible = true;

    // tooltip for tabs in docked and tabified panels
//    int prevTabIndex= -1;

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
    int prevSortColumn = G::NameColumn;
    bool isReverseSort = false;
    bool prevIsReverseSort = false;
    bool preferencesHasFocus = false;
    bool workspaceChanged = false;

    bool remoteAccess = false;
    bool showImageCount = true;
    bool isCurrentFolderOkay;
    bool copyOp;
    bool isDragDrop;
    QString dragDropFilePath;
    QString dragDropFolderPath;
    int maxThumbSpaceHeight;
    QString pickMemSize;
    WidgetCSS widgetCSS;
    // rgh make css consistent with G::css
    QString css;                // stylesheet text
    QString css1;               // stylesheet text
    QString cssBase;            // stylesheet text

    void test();                    // for debugging

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
    void setValueDm(QModelIndex dmIdx, QVariant value, int instance, QString src = "MW",
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src = "MW",
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValuePath(QString fPath, int col, QVariant value, int instance, int role);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, bool ok, int fromInstance, QString src);
    void startImageCache();
    void initializeImageCache(int maxMB, int minMB, bool showStatus, int wtAhead);
    void imageCacheChangeParam(int maxMB, int minMB, bool showStatus, int wtAhead);
    void setImageCachePosition(QString, QString);
    void imageCacheFilterChange(QString, QString);
    void imageCacheColorManageChange();
    void resizeMW(QRect mainWindowRect, QRect centralWidgetRect);
    void closeZoomDlg();        // not being used
    void aSyncGo(int);          // rgh req'd?
    void needToShow();          // not being used
    void abortMetaRead();       // not being used
    void abortMDCache();        // not being used
    void abortImageCache();
    void stopImageCache();
    void abortBuildFilters();   // not being used
    void abortFrameDecoder();   // not being used
    void abortEmbelExport();
    void abortHueReport();
    void abortStackOperation();
    void testAddToDM(ImageMetadata m, QString src); // not being used

public slots:
//    void prevSessionWindowLocation(QWindow::Visibility visibility);
    void showMouseCursor();
    void hideMouseCursor();
    void whenActivated(Qt::ApplicationState state);
    void appStateChange(Qt::ApplicationState state);
    void handleStartupArgs(const QString &msg);
    void folderSelectionChange(QString folderPath = "", QString op = "Add",
                               bool resetDataModel = true, bool recurse = false);
    void fileSelectionChange(QModelIndex current, QModelIndex, bool clearSelection = true, QString src = "");
    void folderAndFileSelectionChange(QString fPath, QString src = "");
    void tryLoadImageAgain(QString fPath);
    void currentFolderDeletedExternally(QString path);
    bool stop(QString src = "");
    bool reset(QString src = "");
    void nullFiltration();
    void filterLastDay();
    void searchTextEdit();
    void handleDrop(QString fPath);
    // void handleDrop(QDropEvent *event);
    // void handleDrop(const QMimeData *mimeData);
    void sortIndicatorChanged(int column, Qt::SortOrder sortOrder);
    void setProgress(int value);
    void setStatus(QString state);
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
    QString reportWorkspaces();
    void reportWorkspaceNum(int n);
    void reportWorkspace(WorkspaceData &ws, QString src = "");
    void loadWorkspaces();
    void saveWorkspaces();

private slots:
    void focusChange(QWidget *previous, QWidget *current);
    void resetFocus();
    void checkForUpdate();
    void setShowImageCount();
    void about();
    void helpThumbViewStatusBarSymbols();
    void ingest();
    void exportEmbelFromAction(QAction *embelExportAction);
    void exportEmbel();
    void enableSelectionDependentMenus();
    void enableStatusBarBtns();
    void enableEjectUsbMenu(QString path);
    void renameEjectUsbMenu(QString path);
    void renameAddBookmarkAction(QString path);
    void renameRemoveBookmarkAction(QString path);
    void renamePasteFilesAction(QString folderName);
    void renameDeleteFolderAction(QString folderName);
    void renameCopyFolderPathAction(QString folderName);
    void renameRevealFileAction(QString folderName);
    void ejectUsb(QString path);
    void ejectUsbFromMainMenu();
    void ejectUsbFromContextMenu();
    void renameEraseMemCardFromContextMenu(QString path);
    void renameEmbedThumbsContextMenu();
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

    void setRotation(int degrees);
    void metadataChanged(QStandardItem* item);
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
    void toggleImageCacheStrategy();
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
    void scrollToCurrentRowIfNotVisible();
    void jump();
    void zoomToggle();
    // status functions
    void updateStatus(bool keepBase = true, QString s = "", QString source = "");
    void clearStatus();
    void showPopUp(QString msg, int duration, bool isAutosize = false,
                   float opacity = 0.75, Qt::Alignment alignment = Qt::AlignHCenter);
    // caching status functions
    void setThreadRunStatusInactive();
    void setCacheStatusVisibility();
//    void updateImageCachePosition();  rgh trigger imageCache
    void updateMetadataThreadRunStatus(bool isRun, bool showCacheLabel,
                                       bool success, QString src = "");
    void updateImageCachingThreadRunStatus(bool isRun, bool showCacheLabel);
    void updateImageCacheStatus(QString instruction,
                                float currMB, int maxMB, int tFirst, int tLast,
                                QString source);
    // caching
    void loadFolder(QString folderPath);
    void load(int sfRow, bool isCurrent = true, QString src = "");
    void loadChanged(const QString folderPath, const QString op);
    void loadDone();

    void refreshCurrentAfterReload();
    void updateDefaultIconChunkSize(int size);
    bool updateIconRange(bool sizeChange, QString src = "");
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
    void setImageCacheParameters();
    void setImageCacheMinSize(QString size);
    void setImageCacheSize(QString method);
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
    void setMetadataDockVisibility();
    void setEmbelDockVisibility();
    void setMetadataDockFixedSize();    // rgh finish or remove

    void focusOnDock(DockWidget *dockWidget);
    void toggleThumbDockVisibity();
    void toggleEmbelDockVisibility();
    void toggleFolderDockVisibility();
    void toggleFavDockVisibility();
    void toggleFilterDockVisibility();
    void toggleMetadataDockVisibility();

    void setMenuBarVisibility();
    void setStatusBarVisibility();

    void reportWorkspaceState();
    void reportState(QString title);

    void openFolder();
    void refreshDataModel();
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

    void newWorkspace();
    QString fixDupWorkspaceName(QString name);

    void help();
    void helpShortcuts();
    void helpWelcome();

    void thriftyCache();
    void moderateCache();
    void greedyCache();

private:
//    QApplication *app;
//    friend class ProgressBar;   // mw1


    QMenuBar *thumbsMenuBar;
    QMenu *fileMenu;
    QMenu *openWithMenu;
    QMenu *recentFoldersMenu;
    QMenu *ingestHistoryFoldersMenu;
    QMenu *editMenu;
    QMenu *ratingsMenu;
    QMenu *labelsMenu;
    QMenu *utilitiesMenu;
    QMenu *goMenu;
    QMenu *filterMenu;
    QMenu *sortMenu;
    QMenu *embelMenu;
        QMenu *embelExportMenu;
        QMenu *embelExportOverrideMenu;
    QMenu *viewMenu;
       QMenu *zoomSubMenu;
    QMenu *windowMenu;
        QMenu *workspaceMenu;
    QMenu *helpMenu;
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
    QAction *collapseFoldersAction;
    QAction *addBookmarkActionFromContext;
    QAction *removeBookmarkAction;
    QAction *refreshBookmarkAction;
    QAction *ingestAction;
    QAction *ejectAction;
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
    QAction *visCmpImagesAction;
    QAction *mediaReadSpeedAction;
    QAction *reportHueCountAction;
    QAction *meanStackAction;
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
    QAction *embedThumbnailsAction;
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

    QAction *keyScrollLeftAction;
    QAction *keyScrollRightAction;
    QAction *keyScrollDownAction;
    QAction *keyScrollUpAction;
    QAction *keyScrollPageDownAction;
    QAction *keyScrollPageUpAction;
    QAction *keyScrollHomeAction;
    QAction *keyScrollEndAction;

    QAction *nextPickAction;
    QAction *prevPickAction;
    QAction *searchTextEditAction;
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
    QAction *metadataDockVisibleAction;
    QAction *thumbDockVisibleAction;
    QAction *embelDockVisibleAction;
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
    QAction *helpFilmStripAction;
    QAction *helpRevealLogFileAction;

    // Help Diagnostics Menu
    QAction *diagnosticsAllAction;
    QAction *diagnosticsCurrentAction;
    QAction *diagnosticsMainAction;
    QAction *diagnosticsWorkspacesAction;
    QAction *diagnosticsLogIssuesAction;
    QAction *diagnosticsSessionIssuesAction;
    QAction *diagnosticsGridViewAction;
    QAction *diagnosticsThumbViewAction;
    QAction *diagnosticsImageViewAction;
    QAction *diagnosticsInfoViewAction;
    QAction *diagnosticsTableViewAction;
    QAction *diagnosticsCompareViewAction;
    QAction *diagnosticsMetadataAction;
    QAction *diagnosticsXMPAction;
    QAction *diagnosticsMetadataCacheAction;
    QAction *diagnosticsImageCacheAction;
    QAction *diagnosticsDataModelAction;
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
    QAction *editGroupAct;
    QAction *goGroupAct;
    QAction *filterGroupAct;
    QAction *sortGroupAct;
    QAction *embelGroupAct;
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
    BarBtn *modifyImagesBtn;
    BarBtn *cacheMethodBtn;
    QLabel *filterStatusLabel;
    QLabel *subfolderStatusLabel;
    QLabel *rawJpgStatusLabel;
    QLabel *slideShowStatusLabel;
    QLabel *cacheStatusLabel;
    ProgressBar *cacheProgressBar;
    QLabel *progressLabel;
    QPixmap *progressPixmap;
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
    DockWidget *metadataDock;
    DockWidget *thumbDock;
    DockWidget *propertiesDock;
    DockWidget *embelDock;
    DockTitleBar *folderTitleBar;
    DockTitleBar *favTitleBar;
    DockTitleBar *filterTitleBar;
    DockTitleBar *metaTitleBar;
    DockTitleBar *embelTitleBar;
    BarBtn *embelRunBtn;
    FSTree *fsTree;
    BookMarks *bookmarks;
    Filters *filters;
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
    Preferences *pref = nullptr;
    StressTest *stressTest;
    QFrame *embelFrame;
    Embel *embel;
    InfoString *infoString;
    PropertyEditor *propertyEditor;
    QHeaderView *headerView;
    CompareImages *compareImages;
    MetaRead *metaRead;
    ImageCache *imageCache;
    QThread imageCacheThread;
    FrameDecoder *frameDecoderInGui;
    Thumb *thumb;
    InfoView *infoView;
    Ingest *backgroundIngest = nullptr;
    IngestDlg *ingestDlg;
    SaveAsDlg *saveAsDlg;
    LoadUsbDlg *loadUsbDlg;
    AboutDlg *aboutDlg;
    WorkspaceDlg *workspaceDlg;
    PreferencesDlg *preferencesDlg = nullptr;
    UpdateApp *updateAppDlg;
    ZoomDlg *zoomDlg = nullptr;
    QTimer *slideShowTimer;
    QTimer *resetTimer;
    QWidget *folderDockEmptyWidget;
    QWidget *favDockEmptyWidget;
    QWidget *filterDockEmptyWidget;
    QWidget *metadataDockEmptyWidget;
    QWidget *embelDockEmptyWidget;
    QWidget *thumbDockEmptyWidget;
    QVBoxLayout *imageViewContainer;
    Stack *meanStack;
    QLabel *dragLabel = new QLabel(this);

    QStandardItemModel *imageModel;

    QSoundEffect *pickClick = new QSoundEffect();
    float pickClickvolume = 0.25;

    QHash<QString, bool> stopped;

    // pick history
    struct Pick {
        QString path;
        QString status;
    } pick;
    QStack<Pick> *pickStack;

    // slideShow counter
    int slideCount = 0;

    // used in visibility and focus setting for docks
    enum {SetFocus, SetVisible, SetInvisible} dockToggle;

    QString prevScreenName;                 // the monitor being used be winnow
    QPoint prevScreenLoc = QPoint(-1,-1);   // the centroid of Winnow window in monitor
    qreal prevDevicePixelRatio = -1;

    bool ignoreDockResize;
    bool wasThumbDockVisible;
    bool workspaceChange;
    bool isUpdatingState;
    bool embelDockTabActivated;

    bool isFilterChange = false;        // prevent fileSelectionChange
    bool isRefreshingDM = false;

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

    bool rotationAlertShown = false;

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
    QString metadataDockTabText;
    QString embelDockTabText;
    QString thumbDockTabText;

    QStringList dockTextNames;
    QList<QTabBar*> dockTabBars;
    //QList<RichTextTabBar*> dockRichTextTabBars;

    void createEmbel();
    void createDocks();
    void createFolderDock();
    void createFavDock();
    void createFilterDock();
    void createMetadataDock();
    void createThumbDock();
    void createEmbelDock();
    QTabBar* tabifiedBar();
    bool isDockTabified(QString tabText);
    void tabBarAssignRichText(QTabBar *richTextTabBar);
    bool tabBarContainsDocks(QTabBar *tabBar);
    bool isSelectedDockTab(QString tabText);
    void folderDockVisibilityChange();
    void embelDockActivated(QDockWidget *dockWidget);
    void embelDockVisibilityChange();
    void updateState();
    // bool stop(QString src = "");
//    void stopAndClearAllAfterMetaReadStopped();
    void deleteViewerImage();
    void selectCurrentViewDir();
    // actions
    void addMenuSeparator(QWidget *widget);
    void createActions();
    void createFileActions();
    void createEditActions();
    void createGoActions();
    void createFilterActions();
    void createSortActions();
    void createEmbellishActions();
    void createViewActions();
    void createWindowActions();
    void createHelpActions();
    void createMiscActions();
    void createBookmarks();
    void createFrameDecoder();
    void createLinearMetaRead();
    void createMetaRead();
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
    void createEditMenu();
    void createGoMenu();
    void createFilterMenu();
    void createSortMenu();
    void createEmbellishMenu();
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

    void createTableView();
    void createThumbView();
    void createGridView();
    void createSelectionModel();
    void createSelection();
    void createStatusBar();
    int availableSpaceForProgressBar();
    void updateProgressBarWidth();
    void updateStatusBar();
    void createMessageView();
    void createPreferences();
    void createStressTest();
    void createAppStyle();
    void setupCentralWidget();
    void initialize();
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
    void dmRemove(QStringList pathList);
    void deleteSelectedFiles();
    void deleteFiles(QStringList paths);
    void deleteFolder();
    void deleteAllImageMemCard(QString rootPath, QString name);
    void eraseMemCardImages();
    void eraseMemCardImagesFromContextMenu();

    void showNewImageWarning(QWidget *parent);
    bool removeDirOp(QString dirToDelete);
    void addBookmark(QString path);
    void populateWorkspace(int n, QString name);
    void syncWorkspaceMenu();
    void syncEmbellishMenu();
    void getSubfolders(QString fPath);
    QString getPosition();
    QString getZoom();
    QString getPicked();
    QString getSelectedFileSize();
    double macActualDevicePixelRatio(QPoint loc, QScreen *screen);
    bool isFolderValid(QString fPath, bool report, bool isRemembered = false);
    void addRecentFolder(QString fPath);
    QString embedThumbnails();
    void embedThumbnailsFromAction();
    void chkMissingEmbeddedThumbnails(QString src = "FromLoading");

    QRect testR;

    qulonglong memoryReqdForPicks();
    qulonglong memoryReqdForSelection();

    void diagnosticsAll();
    void diagnosticsCurrent();
    QString diagnostics();
    void diagnosticsMain();
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
    void diagnosticsDataModel();
    void diagnosticsEmbellish();
    void diagnosticsErrors();
    void diagnosticsFilters();
    void diagnosticsFileTree();
    void diagnosticsBookmarks();
    void diagnosticsPixmap();
    void diagnosticsThumb();
    void diagnosticsIngest();
    void diagnosticsZoom();
    void diagnosticsReport(QString reportString, QString title = "Winnow Diagnostics");
    void allIssuesReport();
    void SessionIssuesReport();

    void mediaReadSpeed();
    void findDuplicates();
    void reportHueCount();
    void generateMeanStack();
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

    //    void setCopyCutActions(bool setEnabled);
    //    void setDeleteAction(bool setEnabled);
    //    void setInterfaceEnabled(bool enable);
    //    void setViewerKeyEventsEnabled(bool enabled);

};
#endif // MAINWINDOW_H
