

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
    bool filterAcceptsRow(int source_row,
                          const QModelIndex &source_parent) const; // override;
};

class FSModel : public QFileSystemModel
{
public:
    FSModel(QWidget *parent, Metadata *metadata, QHash<QString, QString> &count,
            QHash<QString, QString> &combineCount, bool &combineRawJpg);
	bool hasChildren(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool showImageCount;
    bool &combineRawJpg;
    QHash <QString, QString> &count;
    QHash <QString, QString> &combineCount;

private:
    QDir *dir;
    int imageCountColumn = 4;
};

class FSTree : public QTreeView
{
	Q_OBJECT

public:
    FSTree(QWidget *parent, Metadata *metadata);
    void createModel();
    void refreshModel();
    void setShowImageCount(bool showImageCount);
    bool getShowImageCount();


    FSModel *fsModel;
    FSFilter *fsFilter;

	QModelIndex getCurrentIndex();
    bool select(QString dirPath);
    void scrollToCurrent();
//    void showSupportedImageCount();

//    bool newData;
    bool combineRawJpg;
    QHash <QString, QString> count;
    QHash <QString, QString> combineCount;

public slots:
    void resizeColumns();
    void expand(const QModelIndex &);
    void getVisibleImageCount(QString src);
    void getImageCount(const QString dirPath, bool changed = false, QString src = "");

private slots:

protected:
    void mousePressEvent(QMouseEvent *event) override;       // debugging
    void mouseReleaseEvent(QMouseEvent *event) override;     // debugging
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
//    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent *event) override;
//    void focusInEvent(QFocusEvent *event);

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
    QElapsedTimer t;
};

#endif // FSTREE_H

