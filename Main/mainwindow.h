#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include <QDesktopWidget>
#include "sstream"
#include <iostream>
#include <iomanip>

#if defined(Q_OS_MAC)
//#include "macScale.h"
#include <ApplicationServices/ApplicationServices.h>
//#include <QtMacExtras>
#include <CoreServices/CoreServices.h>
#include "CoreGraphics/CoreGraphics.h"
#endif

#include "Main/global.h"
#include "Main/widgetcss.h"
#include "appdlg.h"
#include "Datamodel/datamodel.h"
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
#include "Views/infoview.h"
#include "Views/infostring.h"
#include "Metadata/metadata.h"
#include "Main/dockwidget.h"
#include "Embellish/Properties/embelproperties.h"
#include "Embellish/embelexport.h"
#include "Embellish/embel.h"

#include "Cache/cachedata.h"
#include "Cache/mdcache.h"
#include "Cache/imagecache.h"
//#ifdef Q_OS_WIN
#include "Utilities/icc.h"
//#endif

#include "ingestdlg.h"
#include "saveasdlg.h"
#include "aboutdlg.h"
#include "Image/thumb.h"
#include "preferencesdlg.h"
#include "updateapp.h"
#include "workspacedlg.h"
#include "zoomdlg.h"
#include "loadusbdlg.h"
#include "Utilities/utilities.h"
#include "Utilities/usb.h"
#include "Utilities/inputdlg.h"
#include "Utilities/htmlwindow.h"
#include "progressbar.h"

#include "Utilities/coloranalysis.h"

#include "Utilities/performance.h"
#include "ui_helpform.h"
#include "ui_shortcutsform.h"
#include "ui_welcome.h"
#include "ui_message.h"
#include "ui_test12.h"

#include "Metadata/ExifTool.h"
//#include "Image/tiffhandler.h";
//#include "Lib/zlib/zlib.h"

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

#ifdef Q_OS_MAC
#include "Utilities/mac.h"
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

public:
    MW(const QString args, QWidget *parent = nullptr);

    bool isStartupArgs = false;
    bool isReleaseVersion = true;
    bool hideEmbellish = false;
    bool useImageCache = true;

    QString versionNumber = "1.27" ;

    QString version = "Version: " + versionNumber;
    QString winnowWithVersion = "Winnow " + versionNumber;
    QString website = "Website: "
//            "<a href=\"http://165.227.46.158/winnow/winnow.html\">"
            "<a href=\"winnow.ca\">"
            "<span style=\" text-decoration: underline; color:#e5e5e5;\">"
            "Winnow main page</span></a>.  "
            "Includes links to download and video tutorials.</p></body></html>";

    bool isShift;               // used when opening if shift key pressed
    bool ignoreSelectionChange = false;

//    QScreen *currScreen;

    int copyCutCount;   // rgh req'd?
    QTextStream rpt;

    // QSettings
    QSettings *setting;
    QMap<QString, QAction *> actionKeys;

    QMap<QString, QString> pathTemplates;
    QMap<QString, QString> filenameTemplates;

    QStringList *subfolders;
    QStringList *recentFolders;
    QStringList *ingestHistoryFolders;
    QStringList ingestDescriptionCompleter;

    G::Pair externalApp;                  // external application name / executable path
    QList<G::Pair> externalApps;          // list of external apps
    QVector<QString> xAppShortcut = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};

    struct workspaceData {
        QString accelNum;       // for accelerator
        QString name;           // used for menu
        QByteArray geometry;
        QByteArray state;
        bool isFullScreen;
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
        int thumbSpacing;
        int thumbPadding;
        int thumbWidth;
        int thumbHeight;
        int labelFontSize;
        bool showThumbLabels;
        int thumbSpacingGrid;
        int thumbPaddingGrid;
        int thumbWidthGrid;
        int thumbHeightGrid;
        int labelFontSizeGrid;
        bool showThumbLabelsGrid;
        bool isImageInfoVisible;
        bool includeSubfolders;
        bool isLoupeDisplay;
        bool isGridDisplay;
        bool isTableDisplay;
        bool isCompareDisplay;
        bool isEmbelDisplay;
        bool isColorManage;
        QString cacheSizeMethod;
    };
    workspaceData ws;   // hold values for workspace n
    workspaceData *w;
    QList<workspaceData> *workspaces;

    // persistant data

    /*
    Some persistent values are required for class creation and for actions
    that are mutually dependent - so which one do we do first?  Save the
    values in struct mwd and use later.
     */
    workspaceData mwd;  // hold the persistent start values from QSettings

    int folderMaxWidth = 600;       // not in preferences or QSetting

    // general
    int lastPrefPage;
//    bool mouseClickScroll;       // positionAtCenter scrolling when mouse click?
//    int displayHorizontalPixels; // move to global
//    int displayVerticalPixels;   // move to global
    bool checkIfUpdate = true;
    bool autoAdvance = false;
    bool turnOffEmbellish = true;
    bool deleteWarning = true;
    bool isStartingWhileUpdating = true;

    // appearance
    bool isImageInfoVisible;
    bool isRatingBadgeVisible;
    int infoOverlayFontSize = 20;
    int classificationBadgeInImageDiameter;
    int classificationBadgeInThumbDiameter;

    // files
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
    QString cacheSizeMethod;
    QString cacheMinSize;
    int cacheSizePercentOfAvailable;
    int cacheMinMB = 2000;
    int cacheMaxMB;
//    bool isShowCacheStatus;   // replaced with G::showCacheStatus
    int cacheDelay = 100;
    bool isShowCacheThreadActivity;
    int progressWidth;
    int progressWidthBeforeResizeWindow;
    int cacheWtAhead;
    bool isCachePreview;
    int cachePreviewWidth;
    int cachePreviewHeight;

    // full screen behavior
    struct FullScreenDocks {
        bool isFolders;
        bool isFavs;
        bool isFilters;
        bool isMetadata;
        bool isThumbs;
        bool isStatusBar;
    } fullScreenDocks;
    bool isNormalScreen;

    // tooltip for tabs in docked and tabified panels
//    int prevTabIndex= -1;

    QString currentViewDir;

    enum centralWidgetTabs {
        LoupeTab,
        CompareTab,
        TableTab,
        GridTab,
        StartTab,
        MessageTab,
        EmbelTab            // rgh req'd? fo coord help?
    };

    // mode change
    QString prevMode;
    int currentRow;             // the current row in MW::fileSelection
    int scrollRow;              // the row to scroll to when change mode
    QModelIndex currentSfIdx;   // the current proxy index in MW::fileSelection
    QModelIndex currentDmIdx;   // the datamodel index for the current proxy index
    bool allIconsLoaded;
    bool modeChangeJustHappened;
    bool gridDisplayFirstOpen = true;
    bool justUpdatedBestFit;
    int sortColumn = 0;
    int prevSortColumn = 0;
    bool preferencesHasFocus = false;

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

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

public slots:
    void handleStartupArgs(const QString &msg);
    void watchCurrentFolder();
    void folderSelectionChange();
    void fileSelectionChange(QModelIndex current, QModelIndex);
    void folderAndFileSelectionChange(QString fPath);
    void nullFiltration();
    void handleDrop(QString fPath);
//    void handleDrop(QDropEvent *event);
//    void handleDrop(const QMimeData *mimeData);
    void sortIndicatorChanged(int column, Qt::SortOrder sortOrder);
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

signals:
    void setImageCachePosition(QString);
    void resizeMW(QRect mainWindowRect, QRect centralWidgetRect);
    void closeZoomDlg();
    void aSyncGo(int);  // rgh req'd?
    void needToShow();
    void abortEmbelExport();
    void abortHueReport();

private slots:
    void focusChange(QWidget *previous, QWidget *current);
    void checkForUpdate();
    void setShowImageCount();
    void about();
    void ingest();
    void exportEmbel();
    void enableSelectionDependentMenus();
    void enableEjectUsbMenu(QString path);
    void ejectUsb(QString path);
    void ejectUsbFromMainMenu();
    void ejectUsbFromContextMenu();
    void setCachedStatus(QString fPath, bool isCached);
    void searchTextEdit();

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
    void filterLastDay();
    void filterDockVisibilityChange(bool);
    void filterChange(QString source = "");
    void quickFilter();
    void invertFilters();
    void toggleReject();
    void refine();
    void uncheckAllFilters();
    void clearAllFilters();
    void sortChangeFromAction();
    void sortChange(QString src = "Action");
    void updateSortColumn(int sortColumn);
    void reverseSortDirection();
    void toggleColorManage();
    void toggleImageCacheMethod();
    void allPreferences();
    void infoViewPreferences();
    void cachePreferences();
    void preferences(QString text = "");
    void externalAppManager();
    void toggleFullScreen();
    void escapeFullScreen();
    void loupeDisplay();
    void gridDisplay();
    void tableDisplay();
    void compareDisplay();
    void newEmbelTemplate();
    void embelTemplateChange(int id);
    void updateZoom();
    void zoomOut();
    void zoomIn();
    void zoomToFit();
    void enableGoKeyActions(bool ok);
    void keyRight();
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
    void scrollToCurrentRow();
    void zoomToggle();
    // status functions
    void updateStatus(bool keepBase = true, QString s = "", QString source = "");
    void clearStatus();
    // caching status functions
    void setThreadRunStatusInactive();
    void setCacheStatusVisibility();
    void updateIconBestFit();
//    void updateImageCachePosition();  rgh trigger imageCache
    void updateMetadataThreadRunStatus(bool isRun, bool showCacheLabel, QString calledBy = "");
    void updateImageCachingThreadRunStatus(bool isRun, bool showCacheLabel);
    void updateMetadataCacheStatus(int row, bool clear = false);
    void updateImageCacheStatus(QString instruction,
                                ImageCacheData::Cache cache,
                                QString source);
    // caching
    void updateImageCachePositionAfterDelay(); // rghcachechange
    void loadMetadataCache2ndPass();
    void refreshCurrentAfterReload();
    void updateIconsVisible(bool useCurrentRow);
    bool isCurrentThumbVisible();
    void numberIconsVisibleChange();
    void loadMetadataChunk();
//    void loadMetadataChunkAfterScroll();
//    void loadMetadataCacheThumbScrollEvent();
    void thumbHasScrolled();
    void gridHasScrolled();
    void tableHasScrolled();
    void loadMetadataCacheAfterDelay();
//    void loadMetadataCache(int startRow = 0);
    void loadEntireMetadataCache(QString source);
    void loadImageCacheForNewFolder();
    void launchBuildFilters();
//    void loadFilteredImageCache();
    void addNewBookmarkFromMenu();
    void addNewBookmarkFromContextMenu();
    void reportMetadata();
    void checkDirState(const QModelIndex &, int, int);
    void bookmarkClicked(QTreeWidgetItem *item, int col);
    void setRatingBadgeVisibility();
    void setShootingInfoVisibility();
//    void selectTokenString();
    void tokenEditor();
//    void toggleThumbWrap();
    void setIngested();
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
    void setClassificationBadgeThumbDiam(int d);
    void setPrefPage(int page);
    void setDisplayResolution();
    void setCombineRawJpg();
    void nextSlide();
    void prevRandomSlide();
//    void updateImageCacheSize(int mb);
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

    void setCentralView();

    void setThumbDockVisibity();
    void setFolderDockVisibility();
    void setFavDockVisibility();
    void setFilterDockVisibility();
    void setMetadataDockVisibility();
    void setEmbelDockVisibility();
    void setMetadataDockFixedSize();    // rgh finish or remove

    void toggleThumbDockVisibity();
    void toggleEmbelDockVisibility();
    void toggleFolderDockVisibility();
    void toggleFavDockVisibility();
    void toggleFilterDockVisibility();
    void toggleMetadataDockVisibility();

    void setMenuBarVisibility();
    void setStatusBarVisibility();

    void reportState();

    void openFolder();
    void refreshCurrentFolder();
    void openUsbFolder();
    void saveAsFile();
    void copyFolderPathFromContext();
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
    void invokeWorkspaceFromAction(QAction *workAction);
    void invokeWorkspace(const workspaceData &w);
    void invokeRecentFolder(QAction *recentFolderActions);
    void invokeIngestHistoryFolder(QAction *ingestHistoryFolderActions);
    void snapshotWorkspace(workspaceData &wsd);
    void manageWorkspaces();
    void deleteWorkspace(int n);
    void renameWorkspace(int n, QString name);
    void reassignWorkspace(int n);
    void defaultWorkspace();
    void reportWorkspace(int n);
    void loadWorkspaces();

    void help();
    void helpShortcuts();
    void helpWelcome();

    void thriftyCache();
    void moderateCache();
    void greedyCache();

private:
    QApplication *app;
//    friend class ProgressBar;   // mw1


    QMenuBar *thumbsMenuBar;
    QMenu *fileMenu;
    QMenu *openWithMenu;
    QMenu *recentFoldersMenu;
    QMenu *ingestHistoryFoldersMenu;
    QMenu *editMenu;
    QMenu *ratingsMenu;
    QMenu *labelsMenu;
    QMenu *goMenu;
    QMenu *filterMenu;
    QMenu *sortMenu;
    QMenu *embelMenu;
    QMenu *viewMenu;
       QMenu *zoomSubMenu;
    QMenu *windowMenu;
        QMenu *workspaceMenu;
    QMenu *helpMenu;
        QMenu *helpDiagnosticsMenu;

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
    QAction *copyPathActionFromContext;
    QAction *openWithMenuAction;
        QAction *manageAppAction;
        QList<QAction *> appActions;
    QAction *recentFoldersAction;
        QList<QAction *> recentFolderActions;
    QAction *ingestHistoryFoldersAction;
        QList<QAction *> ingestHistoryFolderActions;
    QAction *subFoldersAction;
    QAction *collapseFoldersAction;
    QAction *addBookmarkAction;
    QAction *addBookmarkActionFromContext;
    QAction *removeBookmarkAction;
    QAction *refreshBookmarkAction;
    QAction *ingestAction;
    QAction *ejectAction;
    QAction *ejectActionFromContextMenu;
    QAction *combineRawJpgAction;
    QAction *refreshFoldersAction;
    QAction *renameAction;
    QAction *showImageCountAction;
    QAction *runDropletAction;
    QAction *reportMetadataAction;
    QAction *mediaReadSpeedAction;
    QAction *reportHueCountAction;
    QAction *exitAction;

    // Open with menu in File menu

    // Edit menu
    QAction *selectAllAction;
    QAction *invertSelectionAction;
    QAction *copyAction;
    QAction *copyImageAction;
    QAction *deleteAction;
    QAction *rejectAction;
    QAction *refineAction;
    QAction *pickAction;                // shortcut "`"
    QAction *pick1Action;               // added for shortcut "P"
    QAction *popPickHistoryAction;
    QAction *pickUnlessRejectedAction;
    QAction *filterPickAction;
    QAction *addThumbnailsAction;
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
    QAction *keyHomeAction;
    QAction *keyEndAction;
    QAction *keyPageUpAction;
    QAction *keyPageDownAction;

    QAction *keyScrollLeftAction;
    QAction *keyScrollRightAction;

    QAction *keyScrollDownAction;
    QAction *keyScrollUpAction;

    QAction *keyScrollPageLeftAction;
    QAction *keyScrollPageRightAction;

    QAction *keyScrollPageDownAction;
    QAction *keyScrollPageUpAction;

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

    QAction *filterUpdateAction;
    QAction *clearAllFiltersAction;
    QAction *expandAllAction;
    QAction *collapseAllAction;
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
    QActionGroup *embelGroupAction;
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
    QAction *infoVisibleAction;
    QAction *infoSelectAction;
    QActionGroup *iconGroupAction;
    QAction *zoomToAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomToggleAction;
    QActionGroup *zoomGroupAction;
//    QAction *thumbsWrapAction;
    QAction *thumbsEnlargeAction;
    QAction *thumbsShrinkAction;
//    QAction *thumbsFitAction;   // used?? rgh
//    QAction *showThumbLabelsAction;
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
    QAction *helpRevealLogFileAction;

    // Help Diagnostics Menu
    QAction *diagnosticsAllAction;
    QAction *diagnosticsCurrentAction;
    QAction *diagnosticsErrorsAction;
    QAction *diagnosticsMainAction;
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

    // Context menus
    QList<QAction *> *fsTreeActions;
    QList<QAction *> *filterActions;

    // General
    QAction *thriftyCacheAction;         // only available via shortcut key "F10"
    QAction *moderateCacheAction;        // only available via shortcut key "F11"
    QAction *greedyCacheAction;          // only available via shortcut key "F12"
    QAction *testAction;                 // only available via shortcut key "Shift+Ctrl+Alt+T"
    QAction *testNewFileFormatAction;    // only available via shortcut key "Shift+Ctrl+Alt+F"

    // Might need
    QAction *pasteAction;
    QAction *pasteImageAction;

    // Not needed
    //    QAction *cutAction;
    //    QAction *copyAction;
    //    QAction *copyToAction;
    //    QAction *moveToAction;
    //    QAction *deleteAction;

    QScrollArea *welcome;       // welcome screen for first time use
    QWidget *messageView;
    Ui::message msg;
    QLineEdit *filterBar;
    QLabel *statusLabel;
    BarBtn *reverseSortBtn;
    BarBtn *colorManageToggleBtn;
    BarBtn *cacheMethodBtn;
    QLabel *filterStatusLabel;
    QLabel *subfolderStatusLabel;
    QLabel *rawJpgStatusLabel;
    QLabel *slideShowStatusLabel;
    QLabel *cacheStatusLabel;
    ProgressBar *progressBar;
    QLabel *progressLabel;
    QPixmap *progressPixmap;
    QLabel *centralLabel;
    QLabel *statusBarSpacer;
    QLabel *metadataThreadRunningLabel;
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
    ImageCacheData *icd;
    BuildFilters *buildFilters;
    QItemSelectionModel *selectionModel;
    Metadata *metadata;
    IconView *thumbView;
    IconView *gridView;
    TableView *tableView;
    ImageView *imageView;
    EmbelExport *embelExport;
    EmbelProperties *embelProperties;
    Preferences *pref;
    QFrame *embelFrame;
    Embel *embel;
    InfoString *infoString;
    PropertyEditor *propertyEditor;
    QHeaderView *headerView;
    CompareImages *compareImages;

    MetadataCache *metadataCacheThread;
//    MdCacheMgr *mdCacheMgr;
//    MetaHash metaHash;

    ImageCache *imageCacheThread;
    Thumb *thumb;
    InfoView *infoView;
    IngestDlg *ingestDlg;
    SaveAsDlg *saveAsDlg;
    LoadUsbDlg *loadUsbDlg;
    AboutDlg *aboutDlg;
    WorkspaceDlg *workspaceDlg;
    PreferencesDlg *preferencesDlg;
    UpdateApp *updateAppDlg;
    ZoomDlg *zoomDlg = nullptr;
    QTimer *slideShowTimer;
//    QWidget *folderDockOrigWidget;
//    QWidget *favDockOrigWidget;
//    QWidget *filterDockOrigWidget;
//    QWidget *metadataDockOrigWidget;
//    QWidget *thumbDockOrigWidget;
    QWidget *folderDockEmptyWidget;
    QWidget *favDockEmptyWidget;
    QWidget *filterDockEmptyWidget;
    QWidget *metadataDockEmptyWidget;
    QWidget *embelDockEmptyWidget;
    QWidget *thumbDockEmptyWidget;
    QVBoxLayout *imageViewContainer;

//    QModelIndexList selectedImages;
    QModelIndexList selectedRows;
    QModelIndex currentIdx;
    QStandardItemModel *imageModel;

    // pick history
    struct Pick {
        QString path;
        QString status;
    } pick;
    QStack<Pick> *pickStack;

    // used in visibility and focus setting for docks
    enum {SetFocus, SetVisible, SetInvisible} dockToggle;

    QString prevScreenName;                 // the monitor being used be winnow
    QPoint prevScreenLoc = QPoint(-1,-1);   // the centroid of Winnow window in monitor
    qreal prevDevicePixelRatio = -1;

    bool metadataLoaded;
    bool ignoreDockResize;
    bool wasThumbDockVisible;
    bool workspaceChange;
    bool isUpdatingState;
    bool embelDockTabActivated;

    bool isFilterChange = false;        // prevent fileSelectionChange
    bool isRefreshingDM = false;
    QString refreshCurrentPath;

    bool simulateJustInstalled;
    bool isSettings = false;
    bool isStressTest;
    bool hasGridBeenActivated;
    bool isSlideshowPaused;

    bool isLeftMouseBtnPressed = false;
    bool isMouseDrag = false;

    // loadImageList
    bool timeToQuit;
    QList<QFileInfo> fileInfoList;

    bool sortMenuUpdateToMatchTable = false;

    QString imageCacheFilePath;
    QTimer *imageCacheTimer;

    bool newScrollSignal;           // used for scroll signal delay in case many/sec
    QTimer *metadataCacheScrollTimer;

    int prevCentralView;
    QString mouseOverFolder;        // current mouseover folder in folder dock used
                                    // in context menu to eject usb drives

    QString rating = "";
    QString colorClass = "";
    bool isPick;
    bool isReject;

    QFile pickLog;

    QString folderDockTabText;
    QString favDockTabText;
    QString filterDockTabText;
    QString metadataDockTabText;
    QString embelDockTabText;

    void createEmbel();
    void createDocks();
    void createFolderDock();
    void createFavDock();
    void createFilterDock();
    void createMetadataDock();
    void createThumbDock();
    void createEmbelDock();
    void embelDockActivated(QDockWidget *dockWidget);
    void embelDockVisibilityChange();
    void updateState();
    void clearAll();
    void deleteViewerImage();
    void selectCurrentViewDir();
    void addMenuSeparator(QWidget *widget);
    void createActions();
    void createBookmarks();
    void createCaching();
    void createCentralWidget();
    void createCompareView();
    void createDataModel();
    void createFilterView();
    void createFSTree();
    void createImageView();
    void createInfoView();
    void createInfoString();
    void createMenus();
    void createTableView();
    void createThumbView();
    void createGridView();
    void createSelectionModel();
    void createStatusBar();
    int availableSpaceForProgressBar();
    void updateProgressBarWidth();
    void updateStatusBar();
    void createMessageView();
    void createAppStyle();
    void setupCentralWidget();
    void initialize();
    void setupPlatform();
    void setfsModelFlags();
    void writeSettings();
    bool loadSettings();
    void loadShortcuts(bool defaultShortcuts);
    void saveSelection();
    void recoverSelection();
    void openLog();
    void closeLog();
    void clearLog();
    void openErrLog();
    void closeErrLog();
    void clearErrLog();
    bool isDevelopment();

    bool isValidPath(QString &path);
    QString getSelectedPath();
    void wheelEvent(QWheelEvent *event);
//    bool event(QEvent *event);
    void copy();
    void deleteFiles();
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
    void insertThumbnails();

    qulonglong memoryReqdForPicks();
    qulonglong memoryReqdForSelection();
    QString enquote(QString &s);

    void diagnosticsAll();
    void diagnosticsCurrent();
    QString diagnostics();
    void diagnosticsMain();
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
    void diagnosticsReport(QString reportString);

    void mediaReadSpeed();
    void reportHueCount();
    void stressTest();
    void test();                    // for debugging
    template<typename T> void test2(T& io, int x);
    void testNewFileFormat();       // for debugging
    QElapsedTimer testTime;

    //    void setCopyCutActions(bool setEnabled);
    //    void setDeleteAction(bool setEnabled);
    //    void setInterfaceEnabled(bool enable);
    //    void setViewerKeyEventsEnabled(bool enabled);

};
#endif // MAINWINDOW_H
