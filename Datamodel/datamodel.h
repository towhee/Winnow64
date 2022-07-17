#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "Cache/framedecoder.h"
#include "progressbar.h"        // rgh used??
#include "Main/global.h"
//#include "Datamodel/HashMap.h"
//#include "Datamodel/HashNode.h"
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
              Filters *filters,
              bool &combineRawJpg);

    void setModelProperties();
    bool load(QString &dir, bool includeSubfoldersFlag);
    bool readMetadataForItem(int row);
    void clearDataModel();
    bool hasFolderChanged();
    void find(QString text);
    ImageMetadata imMetadata(QString fPath, bool updateInMetadata = false);
    void clearPicks();
    void remove(QString fPath);
    void insert(QString fPath);
    QModelIndex proxyIndexFromPath(QString fPath);
    int proxyRowFromModelRow(int dmRow);
    int modelRowFromProxyRow(int sfRow);
    QString diagnostics();
    QString diagnosticsErrors();
    QString diagnosticsForCurrentRow();
    void getDiagnosticsForRow(int row, QTextStream& rpt);
    bool updateFileData(QFileInfo fileInfo);
    bool metadataLoaded(int dmRow);
    bool isAllMetadataLoaded();
    void clearAllIcons();
    bool allIconsLoaded();
    bool iconLoaded(int sfRow);
    bool isIconCaching(int sfRow);
    void setIconCaching(int sfRow, bool state);
    int rowFromPath(QString fPath);
    void refreshRowFromPath();

    QMutex mutex;

    SortFilter *sf;
    QHash<QString, int> fPathRow;
    QStringList imageFilePathList;
    QDir::SortFlags thumbsSortFlags;
    int instance = 0;                   // used in setIcon to confirm model folder
    QString currentFolderPath;
    QString currentFilePath;            // used in caching to update image cache
    int currentRow;                     // used in caching to check if new image selected
    int firstVisibleRow;                // used to determine MetaRead priority queue
    int lastVisibleRow;                 // used to determine MetaRead priority queue
    int startIconRange;                 // used to determine MetaRead priority queue
    int endIconRange;                   // used to determine MetaRead priority queue
    int iconChunkSize;                  // max suggested number of icons to cache
    bool hasDupRawJpg;
    bool loadingModel = false;          // do not filter while loading datamodel
    bool basicFileInfoLoaded = false;   // not used. do not navigate until basic info loaded in datamodel

    // rgh check if reqd still
    bool forceBuildFilters = false;     // ignore buildFiltersMaxDelay if true
    int buildFiltersMaxDelay = 1000;    // quit if exceed and not forceBuildFilters
    QElapsedTimer buildFiltersTimer;

    QList<QFileInfo> modifiedFiles;     // used by MW::refreshCurrentFolder

    /* can be set from keyPressEvent in MW to terminate if recursive folder scan or
       building filters too long */
    bool abortLoadingModel;

signals:
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void centralMsg(QString message);
    void updateStatus(bool keepBase, QString s, QString source);

public slots:
//    void unfilteredItemSearchCount();
    void addAllMetadata();
    void setAllMetadataLoaded(bool isLoaded);
    bool addMetadataForItem(ImageMetadata m);
    void setIcon(QModelIndex dmIdx, QPixmap &pm, int fromInstance);
    void setIconFromVideoFrame(QModelIndex dmIdx, QPixmap &pm, int fromInstance,
                          qint64 duration, FrameDecoder *frameDecoder);
    void setValue(QModelIndex dmIdx, QVariant value, int role = Qt::EditRole);
    void setValueSf(QModelIndex sfIdx, QVariant value, int role = Qt::EditRole);
    void setValuePath(QString fPath, int col, QVariant value, int role = Qt::EditRole);
    void abortLoad();
    void rebuildTypeFilter();
    void searchStringChange(QString searchString);

private:
    QWidget *mw;
    Metadata *metadata;
    Filters *filters;
    bool &combineRawJpg;
    QList<QFileInfo> fileInfoList;
    static bool lessThan(const QFileInfo &i1, const QFileInfo &i2);

//    QMutex mutex;

    QDir *dir;
    QStringList *fileFilters;
    QFileInfo fileInfo;
    QImage emptyImg;
    bool includeSubfolders = false;

    bool endLoad(bool success);
    bool addFileData();
    void addFileDataForRow(int row, QFileInfo fileInfo);
    void rawPlusJpg();

    int imageCount;
    int countInterval = 0;
    QString buildMsg = "Building filters.  This could take a while to complete.<p>"
                       "Press \"Esc\" to stop<p>";
    QString buildSteps = "3";
    int step;

};

#endif // DATAMODEL_H
