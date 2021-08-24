#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "progressbar.h"
#include "Main/global.h"
#include "Datamodel/HashMap.h"
#include "Datamodel/HashNode.h"
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
              Metadata *metadata,
              ProgressBar *progressBar,
              Filters *filters,
              bool &combineRawJpg);

    bool load(QString &dir, bool includeSubfoldersFlag);
    bool readMetadataForItem(int row);
    void clearDataModel();
    bool hasFolderChanged();
    void find(QString text);
    ImageMetadata imMetadata(QString fPath);
    void clearPicks();
    void remove(QString fPath);
    QModelIndex proxyIndexFromPath(QString fPath);
    int proxyRowFromModelRow(int dmRow);
    int modelRowFromProxyRow(int sfRow);
    QString diagnostics();
    QString diagnosticsErrors();
    QString diagnosticsForCurrentRow();
    void getDiagnosticsForRow(int row, QTextStream& rpt);
    bool updateFileData(QFileInfo fileInfo);

    SortFilter *sf;
    QHash<QString, int> fPathRow;
    QStringList imageFilePathList;
    QDir::SortFlags thumbsSortFlags;
    QString currentFolderPath;
    QString currentFilePath;            // used in caching to update image cache
    int currentRow;                     // used in caching to check if new image selected
    bool hasDupRawJpg;
    bool loadingModel = false;          // do not filter while loading datamodel
    bool basicFileInfoLoaded = false;   // do not navigate until basic info loaded in datamodel

    // rgh check if reqd still
    bool forceBuildFilters = false;     // ignore buildFiltersMaxDelay if true
    int buildFiltersMaxDelay = 1000;    // quit if exceed and not forceBuildFilters
    QElapsedTimer buildFiltersTimer;

    QList<QFileInfo> modifiedFiles;

    /* can be set from keyPressEvent in MW to terminate if recursive folder scan or
       building filters too long */
    bool timeToQuit;
    bool alt;

    // IMAGE CACHE STRUCTURES
    // concurrent image cache hash
//    CTSL::HashMap<QString, QImage> imCache;

    struct Cache {
        int key;                    // current image
        int prevKey;                // used to establish directionof travel
        int toCacheKey;             // next file to cache
        int toDecacheKey;           // next file to remove from cache
        bool isForward;             // direction of travel for caching algorithm
        bool maybeDirectionChange;  // direction change but maybe below change threshold
        int step;                   // difference between key and prevKey
        int sumStep;                // sum of step until threshold
        int directionChangeThreshold;//number of steps before change direction of cache
        int wtAhead;                // ratio cache ahead vs behind * 10 (ie 7 = ratio 7/10)
        int totFiles;               // number of images available
        int currMB;                 // the current MB consumed by the cache
        int maxMB;                  // maximum MB available to cache
        int minMB;                  // minimum MB available to cache
        int folderMB;               // MB required for all files in folder
        int targetFirst;            // beginning of target range to cache
        int targetLast;             // end of the target range to cache
        bool isShowCacheStatus;     // show in app status bar
        bool usePreview;            // cache smaller pixmap for speedier initial display
        QSize previewSize;          // monitor display dimensions for scale of previews
    } cache;

signals:
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void msg(QString message);
    void updateStatus(bool keepBase, QString s, QString source);

public slots:
//    void unfilteredItemSearchCount();
    void addAllMetadata();
    bool addMetadataForItem(ImageMetadata m);
    void rebuildTypeFilter();
    void searchStringChange(QString searchString);

private:
    QWidget *mw;
    Metadata *metadata;
    Filters *filters;
    ProgressBar *progressBar;
    bool &combineRawJpg;
    QList<QFileInfo> fileInfoList;
    static bool lessThan(const QFileInfo &i1, const QFileInfo &i2);

    QMutex mutex;

    QDir *dir;
    QStringList *fileFilters;
    QList<QStandardItem*> *thumbList;
    QFileInfo fileInfo;
    QImage emptyImg;
    bool includeSubfolders = false;

    bool addFileData();
    void rawPlusJpg();

    int addFilesMaxDelay = 1000;    // quit if exceed and not forceBuildFilters

    int imageCount;
    int countInterval = 0;
    QElapsedTimer t;
    QString buildMsg = "Building filters.  This could take a while to complete.<p>"
                       "Press \"Esc\" to stop<p>";
    QString buildSteps = "3";
    int step;

};

#endif // DATAMODEL_H
