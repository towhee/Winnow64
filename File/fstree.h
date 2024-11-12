#include <QtWidgets>
#include <QHash>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Utilities/utilities.h"
#include "HoverDelegate.h"

#ifndef FSTREE_H
#define FSTREE_H

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
    FSModel(QWidget *parent, Metadata &metadata, bool &combineRawJpg);
	bool hasChildren(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;
    void refresh();
    // void refresh(const QModelIndex &index);
    void refresh(const QString &dPath);
    bool showImageCount;
    bool &combineRawJpg;
    bool forceRefresh = true;
    Metadata &metadata;
    int imageCountColumn = 4;

signals:
    void update() const;        // const req'd but shows warning

private:
    QDir *dir;
    void insertCount(QString dPath, QString value);
    void insertCombineCount(QString dPath, QString value);
    mutable QHash <QString, QString> count;
    mutable QHash <QString, QString> combineCount;
    mutable QModelIndex testIdx;
};

class FSTree : public QTreeView
{
	Q_OBJECT

public:
    FSTree(QWidget *parent, Metadata *metadata);
    void createModel();
    void setShowImageCount(bool showImageCount);
    bool isShowImageCount();
    qlonglong selectionCount();

    FSModel *fsModel;
    FSFilter *fsFilter;

	QModelIndex getCurrentIndex();
    void scrollToCurrent();

    bool combineRawJpg;
    QString hoverFolderName;

    QFileSystemWatcher volumesWatcher;

public slots:
    bool select(QString dirPath);
    void resizeColumns();
    void refreshModel();
    void onRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);

private slots:
    void wheelStopped();
    void expandedSelectRecursively(const QPersistentModelIndex &index);
    void hasExpanded(const QPersistentModelIndex &index);
    // void handleExpandedSelection(const QPersistentModelIndex &index);

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
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

signals:
	void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void indexExpanded();
    void selectionChange();
    void folderSelectionChange(QString dPath, QString op, bool newInstance, bool recurse = false);
    void datamodelQueue(QString dPath, bool isAdding);
    void addToDataModel(QString dPath);
    void removeFromDataModel(QString dPath);
    void abortLoadDataModel();
    void deleteFiles(QStringList srcPaths);
    void renameEjectAction(QString path);
    void renameEraseMemCardContextAction(QString path);
    void renamePasteContextAction(QString folderName);
    void renameDeleteFolderAction(QString folderName);
    void renameCopyFolderPathAction(QString folderName);
    void renameRevealFileAction(QString folderName);
    void addBookmarkAction(QString folderName);
    void status(bool keepBase, QString msg, QString src);

private:
    void selectItemAndChildren(const QModelIndex &index);
    void selectRecursively(const QPersistentModelIndex &index);
    void selectAndExpandRecursively(const QPersistentModelIndex &index);
    void expandAndWait(const QPersistentModelIndex &index);
    QStringList getSelectedFolderPaths() const;
    QSet<QPersistentModelIndex> nodesToExpand;
    QElapsedTimer expansionTimer;
    bool eventLoopRunning;
    QPersistentModelIndex justExpandedIndex;
    QModelIndex dndOrigSelection;
    QFileSystemModel fileSystemModel;
    QItemSelectionModel* treeSelectionModel;
    Metadata *metadata;
    HoverDelegate *delegate;
    QDir *dir;
    QStringList *fileFilters;
    QList<QPersistentModelIndex> recursedForSelection;
    // QModelIndexList recursedForSelection;
    QModelIndex rightClickIndex;
    int imageCountColumnWidth;
    QElapsedTimer t;
    QTimer wheelTimer;
    bool wheelSpinningOnEntry;
    bool isRecursiveSelection = false;
    bool isDebug = false;
};

#endif // FSTREE_H

