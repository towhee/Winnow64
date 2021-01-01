#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"

class BookMarks : public QTreeWidget
{
	Q_OBJECT

public:
    BookMarks(QWidget *parent, Metadata *metadata, bool showImageCount, bool &combineRawJpg);
    void reloadBookmarks();
    void select(QString fPath);
    void count();

    QSet<QString> bookmarkPaths;
    bool showImageCount;
    bool &combineRawJpg;
    QString rightMouseClickPath;

public slots:
	void removeBookmark();

private:
    void addBookmark(QString itemPath);
    void resizeColumns();

	QModelIndex dndOrigSelection;
    Metadata *metadata;
    QDir *dir;
    QStringList *fileFilters;
    int imageCountColumn = 2;
    int imageCountColumnWidth;
    QTreeWidgetItem *rightClickItem;

//    virtual bool event(QEvent *event) override;

private slots:

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
};

#endif // BOOKMARKS_H

