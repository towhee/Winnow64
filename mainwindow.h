

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>
#include <QDesktopWidget>
#include "thumbview.h"
#include "imageview.h"
#include "imagecache.h"
#include "thumbcache.h"
#include "metadata.h"
#include "mdcache.h"
#include "copypickdlg.h"
#include "prefdlg.h"
#include "workspacedlg.h"
#include "popup.h"
#include "bookmarks.h"
#include "infoview.h"
#include "sstream"
#include <iostream>
#include <iomanip>

class MW : public QMainWindow
{
    Q_OBJECT

    friend class Prefdlg;

public:
    MW(QWidget *parent = 0);

    int copyCutCount;   // req'd?

    QSettings *setting;
    QMap<QString, QAction *> actionKeys;
    QMap<QString, QString> externalApps;
    QSet<QString> bookmarkPaths;

    struct workspaceData {
        QString accelNum;       // for accelerator
        QString name;           // used for menu
        QByteArray geometry;
        QByteArray state;
        bool isWindowTitleBarVisible;
        bool isMenuBarVisible;
        bool isStatusBarVisible;
        bool isFolderDockVisible;
        bool isFavDockVisible;
        bool isMetadataDockVisible;
        bool isThumbDockVisible;
        bool isImageDockVisible;
        bool isFolderDockLocked;
        bool isFavDockLocked;
        bool isMetadataDockLocked;
        bool isThumbDockLocked;
        int thumbSpacing;
        int thumbPadding;
        int thumbWidth;
        int thumbHeight;
        int labelFontSize;
        bool showThumbLabels;
        bool isThumbWrap;
        bool isVerticalTitle;
        bool isImageInfoVisible;
        bool includeSubfolders;
        bool isIconDisplay;            // list vs icon in thumbView
        bool isLoupeDisplay;
        bool isGridDisplay;
        bool isCompareDisplay;
    };
    workspaceData ws;   // hold values for workspace n
    QList<workspaceData> *workspaces;

    // persistant data

    // action surogates
    /*
    Some persistent values are required for class creation and for actions
    that are mutually dependent - so which one do we do first?  Save the
    values in struct mwd and use later.
     */
    workspaceData mwd;  // hold the persistent start values from QSettings

//    bool isShowHiddenFiles;     // needed?
    // preferences: files
    bool rememberLastDir;
    QString lastDir;
    bool inclSubfolders;
    int folderMaxWidth = 600;

    // preferences: slideshow
    int slideShowDelay;
    bool slideShowRandom;
    bool slideShowWrap = true;

    // preferences: cache
    int cacheSizeMB;
    bool isShowCacheStatus;
    int cacheStatusWidth;
    int cacheWtAhead;

    QString currentViewDir;

    bool isSlideShowActive;
    bool copyOp;

protected:
    void closeEvent(QCloseEvent *event);

    //    void mouseDoubleClickEvent(QMouseEvent *event);
    //    void mousePressEvent(QMouseEvent *event);

public slots:
    void folderSelectionChange();
    void fileSelectionChange();

    void setStatus(QString state);
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void showCacheStatus(const QImage &imCacheStatus, QString mb);
    void setThumbDockFeatures(Qt::DockWidgetArea area);
    void setThumbDockFloatFeatures(bool isFloat);

    //    bool event(QEvent *event);
    //    void fileSelectionChange(const QItemSelection& selection);
    //    void showViewer();
    //    void loadImagefromThumb(const QModelIndex &idx);
    //    void hideViewer();
    //    void showBusyStatus(bool busy);

signals:

private slots:
    void about();
    void copyPicks();
    void sortThumbnails();
//    void reload();
    void setIncludeSubFolders();
    void preferences();
    void oldPreferences();
    void toggleFullScreen();
    void escapeFullScreen();
    void loupeDisplay();
    void gridDisplay();
    void compareDisplay();
//    void updateActions();
    void updateStatus(QString s1);
    void updateMetadataThreadRunStatus(bool isRun);
    void updateThumbThreadRunStatus(bool isRun);
    void updateImageThreadRunStatus(bool isRun);
    void loadMetadataCache();
    void loadThumbCache();
    void loadImageCache();
    void addNewBookmark();
    void reportMetadata();
    void checkDirState(const QModelIndex &, int, int);
    void bookmarkClicked(QTreeWidgetItem *item, int col);
    void setThumbsFilter();
    void clearThumbsFilter();
    void setShootingInfo();
    void togglePick();
    void updatePick();
    void toggleFilterPick();
    void setGeneralParameters(bool rememberFolder, bool inclSubfolders);
    void setSlideShowParameters(int delay, bool isRandom);
    void slideShow();
    void nextSlide();
    void setCacheParameters(int size, bool show, int width, int wtAhead);
    void setThumbDockParameters(bool isThumbWrap, bool isVerticalTitle);
    void loadNextImage();
    void loadPrevImage();
    void loadUpImage();
    void loadDownImage();
    void loadFirstImage();
    void loadLastImage();
    void loadRandomImage();
    void selectAllThumbs();
    void removeBookmark();
    void zoomIn();
    void zoomOut();
    void zoom100();
    void zoomToFit();
    void rotateLeft();
    void rotateRight();

//    void setDocksVisibility(bool visible);

    void showHiddenFiles();
    void setThumbLabels();
    void chooseExternalApp();
    void updateExternalApps();
    void runExternalApp();
    void cleanupSender();
    void externalAppError();

    void setCentralView();
    void setThumbDockVisibity();
    void setFolderDockVisibility();
    void setFavDockVisibility();
    void setMetadataDockVisibility();
    void setMenuBarVisibility();
    void setStatusBarVisibility();
    void setWindowsTitleBarVisibility();
    void setFolderDockLockMode();
    void setFavDockLockMode();
    void setMetadataDockLockMode();
    void setThumbDockLockMode();
    void setAllDocksLockMode();
    void reportState();

    void openOp();
    void revealFile();
    void openInFinder();
    void openInExplorer();

    void newWorkspace();
    QString fixDupWorkspaceName(QString name);
    void invokeWorkspace(QAction *workAction);
    void manageWorkspaces();
    void deleteWorkspace(int n);
    void renameWorkspace(int n, QString name);
    void reassignWorkspace(int n);
    void defaultWorkspace();

    //    void cutThumbs();
    //    void copyThumbs();
    //    void pasteThumbs();

private:

    QMenuBar *thumbsMenuBar;
    QMenu *fileMenu;
    QMenu *recentFoldersMenu;
    QMenu *editMenu;
    QMenu *goMenu;
    QMenu *sortMenu;
    QMenu *viewMenu;
    QMenu *windowMenu;
    QMenu *workspaceMenu;
    QMenu *helpMenu;
    QMenu *zoomSubMenu;
    QMenu *transformSubMenu;
    QMenu *viewSubMenu;
    QMenu *imageFileSubMenu;
    QMenu *MirroringSubMenu;
    QMenu *openWithMenu;

    // File menu
    QAction *openAction;
    QAction *revealFileAction;
    QAction *subFoldersAction;
    QAction *addBookmarkAction;
    QAction *removeBookmarkAction;
    QAction *ingestAction;
    QAction *renameAction;
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
    QAction *togglePickAction;
    QAction *toggleFilterPickAction;
    QAction *rotateLeftAction;
    QAction *rotateRightAction;
    QAction *prefAction;
    QAction *oldPrefAction;

    // Go Menu
    QAction *nextThumbAction;
    QAction *prevThumbAction;
    QAction *upThumbAction;
    QAction *downThumbAction;
    QAction *firstThumbAction;
    QAction *lastThumbAction;
    QAction *nextPickAction;
    QAction *prevPickAction;

    // View Menu
    QAction *slideShowAction;
    QAction *randomImageAction; // req'd by slideshow
    QAction *fullScreenAction;
    QAction *escapeFullScreenAction;
    QAction *infoVisibleAction;
    QActionGroup *centralGroupAction;
    QAction *asGridAction;
    QAction *asLoupeAction;
    QAction *asCompareAction;
    QActionGroup *iconGroupAction;
    QAction *asListAction;
    QAction *asIconsAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomOrigAction;
    QAction *zoomFitAction;
    QAction *thumbsEnlargeAction;
    QAction *thumbsShrinkAction;
    QAction *thumbsFitAction;   // used?? rgh
    QAction *showThumbLabelsAction;
    QAction *reverseSortAction;

    // Window Menu
    QAction *defaultWorkspaceAction;
    QAction *newWorkspaceAction;
    QAction *manageWorkspaceAction;
    QList<QAction *> workspaceActions;

    QAction *folderDockVisibleAction;
    QAction *favDockVisibleAction;
    QAction *metadataDockVisibleAction;
    QAction *thumbDockVisibleAction;
    QAction *windowsTitleBarVisibleAction;
    QAction *menuBarVisibleAction;
    QAction *statusBarVisibleAction;

    QAction *folderDockLockAction;
    QAction *favDockLockAction;
    QAction *metadataDockLockAction;
    QAction *thumbDockLockAction;
    QAction *allDocksLockAction;

    // Workspace Menu

    // Help Menu
    QAction *aboutAction;
    QAction *helpAction;

    // General

    // Might need
    QAction *openWithMenuAction;
    QAction *pasteAction;
    QAction *pasteImageAction;
    QActionGroup *sortTypesGroup;
    QAction *actName;
    QAction *actTime;
    QAction *actSize;
    QAction *actType;

    // Not needed
    //    QAction *cutAction;
    //    QAction *copyAction;
    //    QAction *copyToAction;
    //    QAction *moveToAction;
    //    QAction *deleteAction;

    QLineEdit *filterBar;
    QLabel *stateLabel;
    QLabel *cacheLabel;
    QLabel *metadataThreadRunningLabel;
    QLabel *thumbThreadRunningLabel;
    QLabel *imageThreadRunningLabel;
    QDockWidget *folderDock;
    QDockWidget *favDock;
    QDockWidget *thumbDock;
    FSTree *fsTree;
    BookMarks *bookmarks;
    QHBoxLayout *mainLayout;
    QDockWidget *metadataDock;
    Metadata *metadata;
    ThumbView *thumbView;
    ImageView *imageView;
    MetadataCache *metadataCacheThread;
    ImageCache *imageCacheThread;
    ThumbCache *thumbCacheThread;
    InfoView *infoView;
    CopyPickDlg *copyPickDlg;
    WorkspaceDlg *workspaceDlg;
    PopUp *popUp;
    QTimer *slideShowTimer;
    QWidget *folderDockOrigWidget;
    QWidget *favDockOrigWidget;
    QWidget *metadataDockOrigWidget;
    QWidget *thumbDockOrigWidget;
    QWidget *folderDockEmptyWidget;
    QWidget *favDockEmptyWidget;
    QWidget *metadataDockEmptyWidget;
    QWidget *thumbDockEmptyWidget;
    QVBoxLayout *imageViewContainer;

    bool metadataLoaded;

    enum CentralWidgets	{           // req'd?
        thumbViewIdx = 0,
        imageViewIdx
    };

    bool isInitializing;

    bool needThumbsRefresh;         // req'd?
    bool thumbViewBusy;             // req'd?
    bool shouldMaximize;            // req'd?

    QMovie *busyMovie;              // req'd?
    QLabel *busyLabel;              // req'd?

    void setupMainWindow(bool resetSettings);
    void loadShortcuts(bool defaultShortcuts);
    void setupDocks();
    void updateState();
    void deleteViewerImage();
    void selectCurrentViewDir();
    void handleStartupArgs();
    void addMenuSeparator(QWidget *widget);
    void createImageView();
    void createThumbView();
    void createActions();
    void createMenus();
    void createStatusBar();
    void setfsModelFlags();
    void createFSTree();
    void createBookmarks();
    void writeSettings();
    void loadSettings();
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
    void reportWorkspace(int n);

    //    void setCopyCutActions(bool setEnabled);
    //    void setDeleteAction(bool setEnabled);
    //    void setInterfaceEnabled(bool enable);
    //    void setViewerKeyEventsEnabled(bool enabled);

};

#endif // MAINWINDOW_H

