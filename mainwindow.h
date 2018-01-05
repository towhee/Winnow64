#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include <QDesktopWidget>
#include "sstream"
#include <iostream>
#include <iomanip>

#if defined(Q_OS_MAC)
#include "macScale.h"
#include <ApplicationServices/ApplicationServices.h>
//#include <QtMacExtras>
#include <CoreServices/CoreServices.h>
#include "CoreGraphics/CoreGraphics.h"
#endif

#include "bookmarks.h"
#include "compareImages.h"
#include "copypickdlg.h"
#include "datamodel.h"
#include "dircompleter.h"
#include "imagecache.h"
#include "imageview.h"
#include "filters.h"
#include "fstree.h"
#include "infoview.h"
#include "mainwindow.h"
#include "mdcache.h"
#include "metadata.h"
#include "popup.h"
#include "prefdlg.h"
#include "global.h"
#include "thumbview.h"
#include "tableview.h"
#include "thumbcache.h"
#include "workspacedlg.h"
#include "zoomdlg.h"

#include "ui_helpform.h"
#include "ui_shortcutsform.h"
#include "ui_welcome.h"
#include "ui_message.h"

class MW : public QMainWindow
{
    Q_OBJECT

    friend class Prefdlg;
    friend class ThumbView;
    friend class InfoView;
//    friend class ZoomDlg;

public:
    MW(QWidget *parent = 0);

    bool isShift;               // used when opening if shift key pressed

    int debugThumbHt;

    int copyCutCount;   // req'd?

    QSettings *setting;
    QMap<QString, QAction *> actionKeys;
    QMap<QString, QString> externalApps;

    QDockWidget *thumbDock;

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
        bool isThumbDockVisible;
        bool isImageDockVisible;
        bool isFolderDockLocked;
        bool isFavDockLocked;
        bool isFilterDockLocked;
        bool isMetadataDockLocked;
        bool isThumbDockLocked;
        int thumbSpacing;
        int thumbPadding;
        int thumbWidth;
        int thumbHeight;
        int labelFontSize;
        bool showThumbLabels;
        bool wrapThumbs;
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

//    bool isShowHiddenFiles;     // needed?
    int folderMaxWidth = 600;       // not in preferences or QSetting
    // general
    int lastPrefPage;
    int displayHorizontalPixels;
    int displayVerticalPixels;
    bool autoIngestFolderPath;

    // preferences: files
    bool rememberLastDir;
    QString lastDir;
    bool inclSubfolders;
    int maxRecentFolders = 10;
    QString ingestRootFolder;

    // preferences: slideshow
    int slideShowDelay;
    bool slideShowRandom;
    bool slideShowWrap = true;

    // preferences: cache
    int cacheSizeMB;
    bool isShowCacheStatus;
    bool isShowCacheThreadActivity;
    int cacheStatusWidth;
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

    QString currentViewDir;

    enum centralWidgetTabs {
        StartTab,
        LoupeTab,
        CompareTab,
        TableTab,
        GridTab,
        MessageTab
    };

    QString prevMode;
    int currentRow;             // the current row in fileSelection
    bool modeChangeJustHappened;

    bool isCurrentFolderOkay;
    bool isSlideShowActive;
    bool copyOp;
    int maxThumbSpaceHeight;
    QString pickMemSize;

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void moveEvent(QMoveEvent *event);
    void resizeEvent(QResizeEvent *event);
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
//    void mouseMoveEvent(QMouseEvent *event);
//    void mousePressEvent(QMouseEvent *event);
//    void mouseReleaseEvent(QMouseEvent *event);
//    void mouseDoubleClickEvent(QMouseEvent *event);

public slots:
    void folderSelectionChange();
    void fileSelectionChange(QModelIndex current, QModelIndex previous);
//    void fileSelectionChange();
    void sortIndicatorChanged(int column, Qt::SortOrder sortOrder);
    void setStatus(QString state);
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void showCacheStatus(const QImage &imCacheStatus);
    void setThumbDockHeight();  // signal from thumbView
    void setThumbDockFeatures(Qt::DockWidgetArea area);
    void setThumbDockFloatFeatures(bool isFloat);
    void reindexImageCache();

signals:
    void resizeMW(QRect mainWindowRect, QRect centralWidgetRect);
    void closeZoomDlg();

private slots:
    void setShowImageCount();
    void about();
    void copyPicks();
    void setRating();
    void setColorClass();
    void quickFilter();
    void sortThumbnails();
//    void reload();
    void monitorPreference();
    void preferences(int page = -1);
    void oldPreferences();
    void toggleFullScreen();
    void escapeFullScreen();
    void loupeDisplay();
    void gridDisplay();
    void tableDisplay();
    void compareDisplay();
    void updateZoom();
    void zoomOut();
    void zoomIn();
    void zoomToFit();
    void keyRight();
    void keyLeft();
    void keyUp();
    void keyDown();
    void keyHome();
    void keyEnd();
//    void zoomTo(float zoomTo);
    void zoomToggle();
    void updateStatus(bool showFileCount, QString s = "");
    void clearStatus();
    void updateMetadataThreadRunStatus(bool isRun);
    void updateThumbThreadRunStatus(bool isRun);
    void updateImageThreadRunStatus(bool isRun);
    void loadMetadataCache();
    void loadThumbCache();
    void loadImageCache();
    void loadFilteredImageCache();
    void addNewBookmark();
    void reportMetadata();
    void checkDirState(const QModelIndex &, int, int);
    void bookmarkClicked(QTreeWidgetItem *item, int col);
    void setThumbsFilter();
    void clearThumbsFilter();
    void setShootingInfo();
    void setCacheStatusVisibility();
    void togglePick();
    void updatePick();
    void updateRating();
    void updateColorClass();
    void setPrefPage(int page);
    void setRememberLastDir(bool prefRememberFolder);
    void setIncludeSubFolders();
    void setMaxRecentFolders(int prefMaxRecentFolders);
    void setTrackpadScroll(bool trackpadScroll);
    void setDisplayResolution(int horizontalPixels, int verticalPixels);
    void setIngestRootFolder(QString rootFolder, bool isAuto);
    void setSlideShowParameters(int delay, bool isRandom);
    void setFullScreenDocks(bool isFolders, bool isFavs, bool isFilters,
                            bool isMetadata, bool isThumbs, bool isStatusBar);
    void slideShow();
    void nextSlide();
    void setCacheParameters(int size, bool show, int width, int wtAhead,
                            bool isCachePreview, bool activity);
    void selectAllThumbs();
    void removeBookmark();
    void rotateLeft();
    void rotateRight();
    void showHiddenFiles();
    void thumbsEnlarge();
    void thumbsShrink();
    void setThumbLabels();
    void setDockFitThumbs();
    void chooseExternalApp();
    void updateExternalApps();
    void runExternalApp();
    void cleanupSender();
    void externalAppError();

    void setFullNormal();
    void setCentralView();
    void setThumbDockVisibity();
    void setFolderDockVisibility();
    void setFavDockVisibility();
    void setFilterDockVisibility();
    void setMetadataDockVisibility();
    void setMenuBarVisibility();
    void setStatusBarVisibility();
    void setWindowsTitleBarVisibility();

    void setFolderDockFocus();
    void setFavDockFocus();
    void setFilterDockFocus();
    void setMetadataDockFocus();
    void setThumbDockFocus();

    void setFolderDockLockMode();
    void setFavDockLockMode();
    void setFilterDockLockMode();
    void setMetadataDockLockMode();
    void setThumbDockLockMode();
    void setAllDocksLockMode();
    void reportState();

    void openFolder();
    void revealFile();
    void openInFinder();
    void openInExplorer();

    void newWorkspace();
    QString fixDupWorkspaceName(QString name);
    void invokeWorkspaceFromAction(QAction *workAction);
    void invokeWorkspace(const workspaceData &w);
    void invokeRecentFolder(QAction *recentFolderActions);
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

    void delayScroll();
    void testMetadata();    // for debugging

    //    void cutThumbs();
//    void copyThumbs();
    //    void pasteThumbs();

private:

    QMenuBar *thumbsMenuBar;
    QMenu *fileMenu;
        QMenu *openWithMenu;
        QMenu *recentFoldersMenu;
    QMenu *editMenu;
    QMenu *goMenu;
    QMenu *filterMenu;
    QMenu *sortMenu;
    QMenu *viewMenu;
       QMenu *zoomSubMenu;
    QMenu *windowMenu;
        QMenu *workspaceMenu;
    QMenu *helpMenu;

    QMenu *viewSubMenu;
    QMenu *imageFileSubMenu;
    QMenu *MirroringSubMenu;
    QMenu *transformSubMenu;

    QList<QAction *> *mainContextActions;

    // File menu
    QAction *openAction;
    QAction *revealFileAction;
    QAction *openWithMenuAction;
    QAction *recentFoldersAction;
    QList<QAction *> recentFolderActions;
    QAction *subFoldersAction;
    QAction *addBookmarkAction;
    QAction *removeBookmarkAction;
    QAction *ingestAction;
    QAction *renameAction;
    QAction *showImageCountAction;
    QAction *runDropletAction;
    QAction *reportMetadataAction;
    QAction *exitAction;

    // Open with menu in File menu
    QAction *newAppAction;
    QAction *deleteAppAction;
    QAction *chooseAppAction;  // replaced by newAppAction?

    // Edit menu
    QAction *selectAllAction;
    QAction *invertSelectionAction;
    QAction *copyImagesAction;
    QAction *pickAction;
    QAction *filterPickAction;
    QAction *rotateLeftAction;
    QAction *rotateRightAction;
    QAction *prefAction;
    QAction *oldPrefAction;

    // Go Menu
    QAction *keyRightAction;
    QAction *keyLeftAction;
    QAction *keyUpAction;
    QAction *keyDownAction;
    QAction *keyHomeAction;
    QAction *keyEndAction;
    QAction *nextPickAction;
    QAction *prevPickAction;
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

    QAction *uncheckAllFiltersAction;
    QAction *expandAllAction;
    QAction *collapseAllAction;
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

    // View Menu
    QAction *slideShowAction;
    QAction *randomImageAction; // req'd by slideshow
    QAction *fullScreenAction;
    QAction *escapeFullScreenAction;
    QAction *infoVisibleAction;
    QActionGroup *centralGroupAction;
    QAction *asGridAction;
    QAction *asTableAction;
    QAction *asLoupeAction;
    QAction *asCompareAction;
    QActionGroup *iconGroupAction;
    QAction *zoomToAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomToggleAction;
    QActionGroup *zoomGroupAction;
    QAction *thumbsEnlargeAction;
    QAction *thumbsShrinkAction;
    QAction *thumbsFitAction;   // used?? rgh
//    QAction *showThumbLabelsAction;
    QAction *sortReverseAction;

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
    QAction *windowTitleBarVisibleAction;
    QAction *menuBarVisibleAction;
    QAction *statusBarVisibleAction;

    QAction *folderDockFocusAction;
    QAction *favDockFocusAction;
    QAction *filterDockFocusAction;
    QAction *metadataDockFocusAction;
    QAction *thumbDockFocusAction;

    QAction *folderDockLockAction;
    QAction *favDockLockAction;
    QAction *filterDockLockAction;
    QAction *metadataDockLockAction;
    QAction *thumbDockLockAction;
    QAction *allDocksLockAction;

    // Workspace Menu

    // Help Menu
    QAction *aboutAction;
    QAction *helpAction;
    QAction *helpShortcutsAction;
    QAction *helpWelcomeAction;

    // General
    QAction *testMetadataAction;    // only available via shortcut key

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
    QLabel *stateLabel;
    QLabel *cacheLabel;
    QLabel *centralLabel;
    QLabel *metadataThreadRunningLabel;
    QLabel *thumbThreadRunningLabel;
    QLabel *imageThreadRunningLabel;
    QDockWidget *folderDock;
    QDockWidget *favDock;
    QDockWidget *filterDock;
    FSTree *fsTree;
    BookMarks *bookmarks;
    Filters *filters;
    QWidget *centralWidget;
    QGridLayout *compareLayout;
    QStackedLayout *centralLayout;
    QDockWidget *metadataDock;
    DataModel *dm;
    QItemSelectionModel *selectionModel;
    Metadata *metadata;
    ThumbView *thumbView;
    ThumbView *gridView;
    TableView *tableView;
    ImageView *imageView;
    QHeaderView *headerView;
    CompareImages *compareImages;
    MetadataCache *metadataCacheThread;
    ImageCache *imageCacheThread;
    ThumbCache *thumbCacheThread;
    InfoView *infoView;
    CopyPickDlg *copyPickDlg;
    WorkspaceDlg *workspaceDlg;
    ZoomDlg *zoomDlg = 0;
    PopUp *popUp;
    QTimer *slideShowTimer;
    QWidget *folderDockOrigWidget;
    QWidget *favDockOrigWidget;
    QWidget *filterDockOrigWidget;
    QWidget *metadataDockOrigWidget;
    QWidget *thumbDockOrigWidget;
    QWidget *folderDockEmptyWidget;
    QWidget *favDockEmptyWidget;
    QWidget *filterDockEmptyWidget;
    QWidget *metadataDockEmptyWidget;
    QWidget *thumbDockEmptyWidget;
    QVBoxLayout *imageViewContainer;

    QStringList *subfolders;
    QStringList *recentFolders;

    QModelIndexList selectedImages;
    QModelIndex currentIdx;
    QStandardItemModel *imageModel;

    bool metadataLoaded;
    bool ignoreDockResize;
    bool wasThumbDockVisibleBeforeGridInvoked;
    bool isUpdatingState;

    bool isFirstTimeNoSettings;
    bool isInitializing;
    bool isStressTest;
    bool hasGridBeenActivated;

    bool isLeftMouseBtnPressed = false;
    bool isMouseDrag = false;

    bool sortMenuUpdateToMatchTable = false;

    bool needThumbsRefresh;         // req'd?
    bool thumbViewBusy;             // req'd?

    QMovie *busyMovie;              // req'd?
    QLabel *busyLabel;              // req'd?

    QString rating = "";
    QString labelColor = "";

    void createDocks();
    void updateState();
    void deleteViewerImage();
    void selectCurrentViewDir();
    void handleStartupArgs();
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
    void createMenus();
    void createTableView();
    void createThumbView();
    void createGridView();
    void createSelectionModel();
    void createStatusBar();
    void createMessageView();
    void createAppStyle();
    void setupCentralWidget();
    void setupPlatform();
    void setfsModelFlags();
    void writeSettings();
    bool loadSettings();
    void loadShortcuts(bool defaultShortcuts);
    void saveSelection();
    void recoverSelection();

    bool isValidPath(QString &path);
    QString getSelectedPath();
    void wheelEvent(QWheelEvent *event);
    bool event(QEvent *event);
    void copyOrCutThumbs(bool copy);
    void showNewImageWarning(QWidget *parent);
    bool removeDirOp(QString dirToDelete);
    void addBookmark(QString path);
    void copyMoveImages(bool move);
    void populateWorkspace(int n, QString name);
    void syncWorkspaceMenu();
    void getSubfolders(QString fPath);
    QString getPosition();
    QString getZoom();
    QString getPicked();
    void setActualDevicePixelRation();
    bool isFolderValid(QString fPath, bool report, bool isRemembered = false);
    void setCentralMessage(QString message);

    void addRecentFolder(QString fPath);
    void syncRecentFoldersMenu();

    qulonglong memoryReqdForPicks();
    QString formatMemoryReqd(qulonglong bytes);

    void stressTest();

    //    void setCopyCutActions(bool setEnabled);
    //    void setDeleteAction(bool setEnabled);
    //    void setInterfaceEnabled(bool enable);
    //    void setViewerKeyEventsEnabled(bool enabled);

};

#endif // MAINWINDOW_H

