#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"

class BookMarks : public QTreeWidget
{
	Q_OBJECT

public:
    BookMarks(QWidget *parent, Metadata *metadata, bool showImageCount);
    void reloadBookmarks();
    void select(QString fPath);
    QSet<QString> bookmarkPaths;
    bool showImageCount;

public slots:
	void removeBookmark();

private:
	QModelIndex dndOrigSelection;
    Metadata *metadata;
    QDir *dir;
    QStringList *fileFilters;
    int imageCountColumn = 2;
    int imageCountColumnWidth;

private slots:

protected:
    void mousePressEvent(QMouseEvent *event);       // debugging
    void mouseReleaseEvent(QMouseEvent *event);     // debugging
    void resizeEvent(QResizeEvent *event);
	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);

signals:
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
};

#endif // BOOKMARKS_H

