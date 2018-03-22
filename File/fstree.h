

#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"

#ifndef FSTREE_H
#define FSTREE_H

class FSFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    FSFilter(QObject *parent);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const; // override;
};

class FSModel : public QFileSystemModel
{
public:
    FSModel(QWidget *parent, Metadata *metadata, bool showImageCount);
	bool hasChildren(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool showImageCount;

private:
    QDir *dir;
    int imageCountColumn = 4;
};

class FSTree : public QTreeView
{
	Q_OBJECT

public:
    FSTree(QWidget *parent, Metadata *metadata, bool showImageCount);

    FSModel *fsModel;
//    QFileSystemModel *fsModel;

    FSFilter *fsFilter;

	QModelIndex getCurrentIndex();
    void select(QString dirPath);
    void scrollToCurrent();
    bool showImageCount;
//	void setModelFlags();
    void showSupportedImageCount();

public slots:
    void resizeColumns();

protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);
    void paintEvent(QPaintEvent *event);

signals:
	void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);

private:
//    void walkTree(const QModelIndex &row);
	QModelIndex dndOrigSelection;
    QFileSystemModel fileSystemModel;
    Metadata *metadata;
    QDir *dir;
    QStringList *fileFilters;
    int imageCountColumnWidth;
};

#endif // FSTREE_H

