

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
#include "dialogs.h"
#include "copypickdlg.h"
#include "prefdlg.h"
#include "workspacedlg.h"

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

    // persistant data test
    int thumbWidth;
    int thumbHeight;

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
    };

    workspaceData ws;    // should this be a pointer?
    QList<workspaceData> *workspaces;

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
    void slideShow();
    void slideShowHandler();
    void loadNextImage();
    void loadPrevImage();
    void loadUpImage();
    void loadDownImage();
    void loadFirstImage();
    void loadLastImage();
    void loadRandomImage();
    void selectAllThumbs();
    void removeBookmark();
    void thumbsEnlarge();
    void thumbsShrink();
    void thumbsFit();
    void zoomIn();
    void zoomOut();
    void zoom100();
    void zoomToFit();
    void rotateLeft();
    void rotateRight();

//    void setDocksVisibility(bool visible);

    void showHiddenFiles();
    void toggleLabels();
    void chooseExternalApp();
    void updateExternalApps();
    void runExternalApp();
    void cleanupSender();
    void externalAppError();


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

    void openOp();

    void newWorkspace();
    QString fixDupWorkspaceName(QString name);
    void invokeWorkspace(QAction *workAction);
    void manageWorkspaces();
    void deleteWorkspace(int n);
    void renameWorkspace(int n, QString name);
    void reassignWorkspace(int n);
    void defaultWorkspace();


    //    void reloadThumbsSlot();
    //    void refreshThumbsFromCache(QModelIndex &idx, QImage &thumb);
    //    void refreshThumbsFromCache();
    //    void setAppWindowTitle();
    //    void newImage();
    //    void updateIndexByViewerImage();
    //    void cutThumbs();
    //    void copyThumbs();
    //    void pasteThumbs();
    //    void keepZoom();
    //    void keepTransformClicked();
    //    void moveRight();
    //    void moveLeft();
    //    void moveUp();
    //    void moveDown();
    //    void goTop();
    //    void goBottom();
    //    void setClassicThumbs();
    //    void setCompactThumbs();
    //    void setSquarishThumbs();
    //    void setFsDockVisibility();
    //    void setBmDockVisibility();
    //    void setTagsDockVisibility();
    //    void setIiDockVisibility();
    //    void setPvDockVisibility();
    //    void toggleAllDocks();
    //    void toggleIconsList();
    //    void copyImagesTo();
    //    void moveImagesTo();

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
    QAction *openInFinderAction;
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
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomOrigAction;
    QAction *zoomFitAction;
    QAction *infoVisibleAction;
    QAction *asListAction;
    QAction *asThumbsAction;
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
    //    QAction *toggleIconsListAction;
    //    QActionGroup *thumbLayoutsGroup;
    //    QAction *refreshAction;
    //    QAction *thumbsGoTopAct;
    //    QAction *thumbsGoBottomAct;
    //    QAction *closeImageAct;
    //    QAction *zoomSubMenuAct;
    //    QAction *keepZoomAct;
    //    QAction *keepTransformAct;
    //    QAction *transformSubMenuAct;
    //    QAction *viewSubMenuAct;
    //    QAction *actClassic;
    //    QAction *actCompact;
    //    QAction *actSquarish;
    //    QAction *actShowHidden;
    //    QAction *actSmallIcons;
    //    QAction *toggleAllDocksAct;
    //    QAction *createDirAction;

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
    WsDlg *wsDlg;
    QTimer *SlideShowTimer;
    CopyMoveToDialog *copyMoveToDialog;
    QWidget *fsDockOrigWidget;
    QWidget *bmDockOrigWidget;
    QWidget *iiDockOrigWidget;
    QWidget *pvDockOrigWidget;
    QWidget *folderDockEmptyWidget;
    QWidget *favDockEmptyWidget;
    QWidget *metadataDockEmptyWidget;
    QWidget *thumbDockEmptyWidget;
    QVBoxLayout *imageViewContainer;

    bool interfaceDisabled;         // req'd?
    bool metadataLoaded;

    enum CentralWidgets	{           // req'd?
        thumbViewIdx = 0,
        imageViewIdx
    };

    bool initComplete;              // req'd?
    bool needThumbsRefresh;         // req'd?
    bool thumbViewBusy;             // req'd?
    bool shouldMaximize;            // req'd?

    QMovie *busyMovie;              // req'd?
    QLabel *busyLabel;              // req'd?

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
    void copyOrCutThumbs(bool copy);
    void showNewImageWarning(QWidget *parent);
    bool removeDirOp(QString dirToDelete);
    void addBookmark(QString path);
    void copyMoveImages(bool move);
    void populateWorkspace(int n, QString name);
    void reportWorkspace(int n);

    //    void refreshThumbs(bool noScroll);
    //    void loadCurrentImage(int currentRow);
    //    void createImageTags();
    //    void setCopyCutActions(bool setEnabled);
    //    void setDeleteAction(bool setEnabled);
    //    void setInterfaceEnabled(bool enable);
    //    void setViewerKeyEventsEnabled(bool enabled);

};

#endif // MAINWINDOW_H

