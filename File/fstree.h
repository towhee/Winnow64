

#include <QtWidgets>
#include <QHash>
#include "Main/global.h"
#include "Metadata/metadata.h"

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
//    Q_OBJECT
public:
    FSModel(QWidget *parent, Metadata &metadata, bool &combineRawJpg);
//    FSModel(QWidget *parent, Metadata &metadata, QHash<QString, QString> &count,
//                    QHash<QString, QString> &combineCount, bool &combineRawJpg);
	bool hasChildren(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool showImageCount;
    bool &combineRawJpg;
    bool forceRefresh = false;
//    QHash <QString, QString> &count;
//    QHash <QString, QString> &combineCount;
    Metadata &metadata;

protected:
//    bool event(QEvent *event) override;       // debugging

private:
    QDir *dir;
    int imageCountColumn = 4;
    void insertCount(QString dPath, QString value);
    void insertCombineCount(QString dPath, QString value);
    mutable QHash <QString, QString> count;
    mutable QHash <QString, QString> combineCount;
};

class FSTree : public QTreeView
{
	Q_OBJECT

public:
    FSTree(QWidget *parent, Metadata *metadata);
    void createModel();
    void refreshModel();
    void setShowImageCount(bool showImageCount);
    bool isShowImageCount();
    bool isVisibleMissingCount();
    void updateFolderImageCount(QString dirPath);
    void updateVisibleImageCount();

//    QFileSystemWatcher *watch;

    FSModel *fsModel;
    FSFilter *fsFilter;

	QModelIndex getCurrentIndex();
//    QModelIndex index(QString dirPath);
    bool select(QString dirPath);
    void scrollToCurrent();
//    void showSupportedImageCount();

//    bool newData;
    bool combineRawJpg;
    QString rightMouseClickPath;
//    QHash <QString, QString> count;
//    QHash <QString, QString> combineCount;

public slots:
    void resizeColumns();
    void expand(const QModelIndex &);
    void getVisibleImageCount(QString src);
    void getFolderImageCount(const QString dirPath, bool changed = false, QString src = "");

private slots:

protected:
//    bool event(QEvent *event) override;                   // debugging
    void mousePressEvent(QMouseEvent *event) override;       // debugging
    void mouseReleaseEvent(QMouseEvent *event) override;     // debugging
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
//    void focusInEvent(QFocusEvent *event);

signals:
	void dropOp(Qt::KeyboardModifiers keyMods, bool dirOp, QString cpMvDirPath);
    void selectionChange();
    void abortLoadDataModel();

private:
//    void walkTree(const QModelIndex &row);
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

