

#include <QtWidgets>
#include "global.h"
#include "metadata.h"

#ifndef FSTREE_H
#define FSTREE_H

class FSModel : public QFileSystemModel
{
public:
    FSModel(QWidget *parent, Metadata *metadata);
	bool hasChildren(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;

    int imageCountColumn = 4;

private:
    QDir *dir;
//    QStringList *fileFilters;
};

class FSTree : public QTreeView
{
	Q_OBJECT

public:
    FSTree(QWidget *parent, Metadata *metadata);
	FSModel *fsModel;
	QModelIndex getCurrentIndex();
	void setModelFlags();

protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);

signals:
	void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);

private:
	QModelIndex dndOrigSelection;
    QFileSystemModel fileSystemModel;

private slots:
	void resizeTreeColumn(const QModelIndex &);
};

#endif // FSTREE_H

