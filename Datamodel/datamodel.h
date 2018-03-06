#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "Main/global.h"


class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters);

public slots:
    void filterChanged(QTreeWidgetItem* x, int col);

private slots:

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:
    void reloadImageCache();

private:
    Filters *filters;
    bool isFinished;
};

class DataModel : public QStandardItemModel
{
    Q_OBJECT
public:
    DataModel(QWidget *parent, Metadata *metadata, Filters *filters);

    bool load(QString &dir, bool includeSubfolders);
    void addMetadata();
    void updateImageList();
    void sortThumbs(int sortColumn, bool isReverse);
    QModelIndex find(QString fPath);

    SortFilter *sf;
    QFileInfoList fileInfoList;
    QStringList imageFilePathList;
//    QFileInfoList dirFileInfoList;
    QDir::SortFlags thumbsSortFlags;
    QString currentFolderPath;

signals:
    void popup(QString msg, int ms, float opacity);

public slots:
    void refine();

private:
    QWidget *mw;
    Metadata *metadata;
    Filters *filters;

    QDir *dir;
    QStringList *fileFilters;
    QList<QStandardItem*> *thumbList;
    QFileInfo fileInfo;
    QImage emptyImg;

    bool addFiles();

};

#endif // DATAMODEL_H
