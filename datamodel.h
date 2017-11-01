#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "metadata.h"
#include "filters.h"
#include "global.h"


class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters);

public slots:
    void filterChanged(QTreeWidgetItem* x, int col);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    Filters *filters;
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

    SortFilter *sf;
    QFileInfoList fileInfoList;
    QStringList imageFilePathList;
//    QFileInfoList dirFileInfoList;
    QDir::SortFlags thumbsSortFlags;
    QString currentFolderPath;

signals:

public slots:

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
