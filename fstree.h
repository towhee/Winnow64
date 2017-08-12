

#include <QtWidgets>
#include "global.h"

#ifndef FSTREE_H
#define FSTREE_H

class FSModel : public QFileSystemModel
{
public:
	bool hasChildren(const QModelIndex &parent) const;
};

class FSTree : public QTreeView
{
	Q_OBJECT

public:
	FSTree(QWidget *parent);
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

