

#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <QtWidgets>
#include "global.h"

class BookMarks : public QTreeWidget
{
	Q_OBJECT

public:
    BookMarks(QWidget *parent);
    void reloadBookmarks();
    QSet<QString> bookmarkPaths;

public slots:
	void removeBookmark();

private:
	QModelIndex dndOrigSelection;

private slots:
	void resizeTreeColumn(const QModelIndex &);

protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);

signals:
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
};

#endif // BOOKMARKS_H

