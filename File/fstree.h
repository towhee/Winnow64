#include <QtWidgets>
#include <QHash>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Utilities/utilities.h"
#include "ui_foldershelp.h"
#include "HoverDelegate.h"

#ifndef FSTREE_H
#define FSTREE_H

class ImageCounter : public QThread {
    Q_OBJECT

public:
    ImageCounter(const QString &path, Metadata &metadata,
                 bool &combineRawJpg, QStringList *fileFilters,
                 QObject *parent = nullptr);

signals:
    void countReady(const QString &path, int count);

protected:
    void run() override;

private:
    QString dPath;
    Metadata &metadata;
    bool combineRawJpg;
    QStringList *fileFilters;

    int computeImageCount(const QString &path);
};

// FSTreeDelegate.h
class FSTreeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FSTreeDelegate(QObject *parent=nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        QString path = index.data(QFileSystemModel::FilePathRole).toString();
        if (path == "/Users/roryhill/Pictures") {
            opt.palette.setColor(QPalette::Text, QColor(255,165,0));  // orange
        }
        QStyledItemDelegate::paint(painter, opt, index);
    }
};

class FSFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    FSFilter(QObject *parent);
    void refresh();

protected:
    bool filterAcceptsRow(int source_row,
                          const QModelIndex &source_parent) const; // override;
};

class FSModel : public QFileSystemModel
{
    Q_OBJECT

public:
    FSModel(QWidget *parent, Metadata &metadata, DataModel *dm, bool &combineRawJpg);
    bool hasChildren(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    void clearCount();
    void updateCount(const QString &dPath);
    enum { OverLimitRole = 300 };
    bool showImageCount;
    bool &combineRawJpg;
    bool forceRefresh = true;
    Metadata &metadata;
    DataModel *dm;
    int imageCountColumn = 4;
    QStringList *fileFilters;
    QStringList maxRecursedRoots;
    bool isMaxRecurse = false;

protected:
    QHash<int, QByteArray> roleNames() const override;

signals:
    void update() const;        // const req'd but shows warning

private:
    QDir *dir;
    mutable QHash <QString, QString> count;
};

class MW;  // forward declaration
class FSTree : public QTreeView
{
	Q_OBJECT

public:
    FSTree(MW *mw, DataModel *dm, Metadata *metadata, QWidget *parent = nullptr);
    ~FSTree() override;
    void createModel();
    void setShowImageCount(bool showImageCount);
    void updateAFolderCount(const QString &dPath);
    int imageCount(QString path);
    bool isShowImageCount();
    qlonglong selectionCount();

    void clearFolderOverLimit();
    void markFolderOverLimit(const QString& folderPath, bool on);
    bool isSelectingFolders = false;

    FSModel *fsModel;
    FSFilter *fsFilter;

	QModelIndex getCurrentIndex();
    void scrollToCurrent();
    QString selectSrc = "";
    QString currentFolderPath();
    QStringList selectedFolderPaths() const;

    void startCountSubdirs(const QString &root, int hardCap);
    void cancelCountSubdirs();

    bool combineRawJpg;
    QString hoverFolderName;

    QFileSystemWatcher volumesWatcher;

    QString diagnostics();

    void test();
    void debugSelectedFolders(QString msg = "");

public slots:
    bool select(QString folderPath, QString modifier = "None", QString src = "");
    void resizeColumns();
    void refreshModel();
    void updateCount();
    void onRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void howThisWorks();

private slots:
    void wheelStopped();
    void hasExpanded(const QPersistentModelIndex &index);
    void onItemExpanded(const QModelIndex &index);
    void onCountFinished(int count, bool wasCancelled);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;       // debugging
    void mouseReleaseEvent(QMouseEvent *event) override;     // debugging
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

signals:
	void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void indexExpanded();
    void selectionChange();
    void folderSelectionChange(QString dPath, G::FolderOp op, bool resetDataModel, bool recurse = false);
    void countProgress(bool keepBase, QString s, QString source);
    void countFinished(int finalCount, bool wasCancelled);
    void recurseCountProgress(int recurseCount);
    // void folderSelectionChange(QString dPath, QString op, bool resetDataModel, bool recurse = false);
    void datamodelQueue(QString dPath, bool isAdding);
    void addToDataModel(QString dPath);
    void removeFromDataModel(QString dPath);
    void abortLoadDataModel();
    void updateCounts();
    void deleteFiles(QStringList srcPaths);
    void renameEjectAction(QString path);
    void renameEraseMemCardContextAction(QString path);
    void renamePasteContextAction(QString folderName);
    void renameDeleteFolderAction(QString folderName);
    void renameCopyFolderPathAction(QString folderName);
    void renameRevealFileAction(QString folderName);
    void addBookmarkAction(QString folderName);
    void status(bool keepBase, QString msg, QString src);
    void centralMsg(QString message);

private:
    MW *mw = nullptr;
    bool isItemVisible(const QModelIndex idx);
    // void markFolderOverLimit(const QString& folderPath, bool on);
    int  countSubdirsFast(const QString& root, int hardCap) const;
    void selectRecursively(QString folderPath, bool toggle = false);
    int maxExpandLimit = 100;
    QColor overLimitColor = QColor(68,95,118);   // orange

    // Future thread to count recurvsive subdirs
    QString pendingFolderPath;
    bool pendingToggle = false;
    int countSubdirsFast(const QString &root, int hardCap);
    QFutureWatcher<int> watcherCount;

    struct ViewState {
        QPointF scrollPosition;
        QList<QModelIndex> selectedIndexes;
    };
    void saveState(ViewState& state) const;
    bool restoreState(const ViewState& state) const;
    QStringList selectVisibleBetween(const QModelIndex &idx1, const QModelIndex &idx2, bool recurse);
    QSet<QPersistentModelIndex> nodesToExpand;
    QElapsedTimer expansionTimer;
    QElapsedTimer rapidClick;
    bool eventLoopRunning;
    QPersistentModelIndex justExpandedIndex;

    QModelIndex dndOrigSelection;
    QModelIndex prevCurrentIndex;
    QFileSystemModel fileSystemModel;
    QItemSelectionModel* treeSelectionModel;
    Metadata *metadata;
    DataModel *dm;
    HoverDelegate *delegate;
    QDir *dir;
    QStringList *fileFilters;
    QList<QPersistentModelIndex> recursedForSelection;
    // QModelIndexList recursedForSelection;
    QModelIndex rightClickIndex;
    int imageCountColumnWidth;

    QString dumpModelContents(bool isSelectionOnly);

    QElapsedTimer t;
    QTimer wheelTimer;
    bool wheelSpinningOnEntry;
    bool isRecursiveSelection = false;
    bool isDebug = false;
};

#endif // FSTREE_H

