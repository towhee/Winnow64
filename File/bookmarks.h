#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "File/fstree.h"
#include "HoverDelegate.h"

class BookMarks : public QTreeWidget
{
	Q_OBJECT

public:
    BookMarks(QWidget *parent, Metadata *metadata, bool showImageCount, bool &combineRawJpg);
    void reloadBookmarks();
    void saveBookmarks(QSettings *setting);
    void select(QString fPath);
    void updateCount();
    void updateCount(QString fPath);

    QSet<QString> bookmarkPaths;
    bool showImageCount;
    bool &combineRawJpg;
    QString rightMouseClickPath;
    QString hoverFolderName;

public slots:
	void removeBookmark();
    void updateBookmarks();

private:
    void addBookmark(QString itemPath);
    void resizeColumns();

	QModelIndex dndOrigSelection;
    Metadata *metadata;
    HoverDelegate *delegate;
    QDir *dir;
    QStringList *fileFilters;
    int imageCountColumn = 2;
    int imageCountColumnWidth;
    QTreeWidgetItem *rightClickItem;

//    virtual bool event(QEvent *event) override;

private slots:

protected:
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void deleteFiles(QStringList srcPaths);
    void refreshFSTree();
    void folderSelection(QString dPath, QString modifier, QString src);
    void renameEjectAction(QString path);
    void renameEraseMemCardContextAction(QString path);
    void renameRemoveBookmarkAction(QString folderName);
    void renameCopyFolderPathAction(QString folderName);
    void renameRevealFileAction(QString folderName);
    void status(bool keepBase, QString msg = "", QString src = "");
};

#endif // BOOKMARKS_H

