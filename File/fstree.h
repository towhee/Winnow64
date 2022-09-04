

#include <QtWidgets>
#include <QHash>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Utilities/utilities.h"

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
    void refresh(const QModelIndex &index);
    bool showImageCount;
    bool &combineRawJpg;
    bool forceRefresh = true;
    Metadata &metadata;
    int imageCountColumn = 4;

signals:
    void update() const;

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

    FSModel *fsModel;
    FSFilter *fsFilter;

	QModelIndex getCurrentIndex();
    bool select(QString dirPath);
    void scrollToCurrent();

    bool combineRawJpg;
    QString rightMouseClickPath;

public slots:
    void resizeColumns();
    void expand(const QModelIndex &);
    void refreshModel();

private slots:

protected:
    void mousePressEvent(QMouseEvent *event) override;       // debugging
    void mouseReleaseEvent(QMouseEvent *event) override;     // debugging
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

signals:
	void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void selectionChange();
    void folderSelection();
    void abortLoadDataModel();
    void deleteFiles(QStringList srcPaths);

private:
	QModelIndex dndOrigSelection;
    QFileSystemModel fileSystemModel;
    Metadata *metadata;
    QDir *dir;
    QStringList *fileFilters;
    QModelIndex rightClickIndex;
    int imageCountColumnWidth;
    QElapsedTimer t;
};

#endif // FSTREE_H

