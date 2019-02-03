#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "progressbar.h"
#include "Main/global.h"
#include <algorithm>                // req'd for sorting fileInfoList

class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg);

public slots:
    void filterChange();

private slots:

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:
    void reloadImageCache();
//    void nullFilter();

private:
    Filters *filters;
    bool &combineRawJpg;
    bool isFinished;
};

class DataModel : public QStandardItemModel
{
    Q_OBJECT
public:
    DataModel(QWidget *parent,
              Metadata *metadata,
              ProgressBar *progressBar,
              Filters *filters,
              bool &combineRawJpg);

    bool load(QString &dir, bool includeSubfolders);
    void clear();
    void addMetadata(ProgressBar *progressBar, bool isShowCacheStatus);
    void updateImageList();
    void sortThumbs(int sortColumn, bool isReverse);
    QModelIndex find(QString fPath);

    SortFilter *sf;
    QStringList imageFilePathList;
    QDir::SortFlags thumbsSortFlags;
    QString currentFolderPath;
    bool hasDupRawJpg;

    // can be set from keyPressEvent in MW to terminate if recursive folder scan too long
    bool timeToQuit;

signals:
    void popup(QString msg, int ms, float opacity);
    void closePopup();
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void msg(QString message);
    void updateProgress(int fromItem, int toItem, int items, QColor doneColor, QString source);

public slots:
    void filterItemCount();

private:
    QWidget *mw;
    Metadata *metadata;
    Filters *filters;
    ProgressBar *progressBar;
    bool &combineRawJpg;
    QList<QFileInfo> fileInfoList;
    static bool lessThan(const QFileInfo &i1, const QFileInfo &i2);

    QDir *dir;
    QStringList *fileFilters;
    QList<QStandardItem*> *thumbList;
    QFileInfo fileInfo;
    QImage emptyImg;

    bool addFileData();
    void rawPlusJpg();
};

#endif // DATAMODEL_H
