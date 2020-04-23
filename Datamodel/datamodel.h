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
              Metadata *getMetadata,
              ProgressBar *progressBar,
              Filters *filters,
              bool &combineRawJpg);

    bool load(QString &dir, bool includeSubfoldersFlag);
    void clear();
    bool hasFolderChanged();
    void find(QString text);
    ImageMetadata getMetadata(QString fPath);
    void updateImageList();
    void clearPicks();
//    void sortThumbs(int sortColumn, bool isReverse);
    QModelIndex proxyIndexFromPath(QString fPath);
    QString diagnostics();
    bool updateFileData(QFileInfo fileInfo);

    SortFilter *sf;
    QHash<QString, int> fPathRow;
    QStringList imageFilePathList;
    QDir::SortFlags thumbsSortFlags;
    QString currentFolderPath;
    QString currentFilePath;            // used in caching to update image cache
    int currentRow;                     // used in caching to check if new image selected
    bool hasDupRawJpg;
    bool filtersBuilt;
    QList<QFileInfo> modifiedFiles;

    // can be set from keyPressEvent in MW to terminate if recursive folder scan too long
    bool timeToQuit;

signals:
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void msg(QString message);

public slots:
    void filteredItemCount();
    void unfilteredItemCount();
    void addAllMetadata(bool isShowCacheStatus = false);
    bool addMetadataForItem(ImageMetadata m);
    void buildFilters();
    void rebuildTypeFilter();

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
    bool includeSubfolders = false;

    bool addFileData();
    void rawPlusJpg();

    int imageCount;
    int countInterval = 0;
    QElapsedTimer t;
    QString buildMsg = "Building filters.  This could take a while to complete.<p>";
    QString buildSteps = "2";
    int step;

};

#endif // DATAMODEL_H
