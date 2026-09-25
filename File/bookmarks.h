#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "File/fstree.h"
#include "HoverDelegate.h"

class BookMarks : public QTreeWidget
{
	Q_OBJECT

public:
    BookMarks(QWidget *parent, DataModel *dm, Metadata *metadata,
              bool showImageCount, bool &combineRawJpg);
    void reloadBookmarks();
    void saveBookmarks(QSettings *setting);
    void updateCount();
    void updateCount(QString fPath);
    QStringList bookmarksWithImages();

    /*  The bookmarked folder of a row. Kept in its own role, not in the tooltip, so the
        tooltip is free to say why a row is dimmed. */
    enum { PathRole = Qt::UserRole + 1 };
    static QString pathOf(const QTreeWidgetItem *item);

    /*  BOOKMARKS FOLLOW THE SOURCE: Folders or Library, whichever G::scope says. In the
        Library a bookmark does not load its folder -- the Library is already loaded --
        it filters the Library to it, exactly as a plain click on that folder in LibTree
        does, and its count is the Library's count for it. MW::setScope sets the mode;
        MW::updateLibraryTree pushes the Library's anchors and per-folder counts. */
    void setLibraryMode(bool on);
    void setLibraryFolders(const QStringList &anchors, const QMap<QString, int> &perFolder);
    /*  The Library folder filter a click on this bookmark asks for: the folder itself
        when it is inside the Library, the Library's include folders beneath it when it
        sits above them, and empty when the Library holds nothing there. */
    QStringList libraryFolders(const QString &path) const;
    /*  MW's push of the Library folder filter: highlight the bookmark that asks for
        exactly this filter, or nothing. Emits nothing. */
    void syncFromLibraryFilter(const QStringList &includes, const QStringList &excludes);
    bool isLibraryMode() const { return libraryMode; }

    QSet<QString> bookmarkPaths;
    bool showImageCount;
    bool &combineRawJpg;
    QString rightMouseClickPath;
    QString hoverFolderName;

public slots:
    void select(QString fPath);
    void removeBookmark();
    void updateBookmarks();
    void howThisWorks();

private:
    void addBookmark(QString itemPath);
    void resizeColumns();
    int diskCount(const QString &path);
    void styleItem(QTreeWidgetItem *item);

    bool libraryMode = false;
    bool libraryKnown = false;          // the Library's folders have been pushed
    QStringList libraryAnchors;
    QMap<QString, int> libraryTotals;   // images at or beneath each folder

	QModelIndex dndOrigSelection;
    DataModel *dm;
    Metadata *metadata;
    HoverDelegate *delegate;
    QDir *dir;
    QStringList *fileFilters;
    int imageCountColumn = 2;
    int imageCountColumnWidth;
    QTreeWidgetItem *rightClickItem;
    QElapsedTimer rapidClick;

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
    void updateCounts();
    void folderSelection(QString dPath, QString modifier, QString src);
    void renameEjectAction(QString path);
    void renameEraseMemCardContextAction(QString path);
    void renameRemoveBookmarkAction(QString folderName);
    void renameCopyFolderPathAction(QString folderName);
    void renameRevealFileAction(QString folderName);
    void status(bool keepBase, QString msg = "", QString src = "");
};

#endif // BOOKMARKS_H

