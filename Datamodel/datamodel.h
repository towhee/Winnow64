#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include <QMessageBox>
#include <QWaitCondition>           // req'd for removeFolder process
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "Cache/framedecoder.h"
#include "selectionorpicksdlg.h"
#include "Log/issue.h"

class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg);
    bool isFinished();
    bool isSuspended();
    bool &combineRawJpg;

public slots:
    void filterChange(QString src = "");
    void suspend(bool suspendFiltering, QString src = "");

private slots:

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:

private:
    Filters *filters;
    mutable bool finished;
    bool suspendFiltering;
};

class DataModel : public QStandardItemModel
{
    Q_OBJECT
public:
    DataModel(QObject *parent,
              Metadata *metadata,
              Filters *filters,
              bool &combineRawJpg);

    void setModelProperties();
    bool readMetadataForItem(int row, int instance);
    bool refreshMetadataForItem(int sfRow, int instance);
    qint64 rowBytesUsed(int dmRow);
    void clearDataModel();
    void newInstance();
    bool sourceModified(QStringList &added, QStringList &removed, QStringList&modified);
    bool isQueueEmpty();
    bool isQueueRemoveEmpty();
    bool contains(QString &path);
    void find(QString text);
    ImageMetadata imMetadata(QString fPath, bool updateInMetadata = false);
    bool isPick();
    void clearPicks();
    void remove(QString fPath);
    int insert(QString fPath);
    void refresh();
    QModelIndex indexFromPath(QString fPath);
    QModelIndex proxyIndexFromPath(QString fPath);
    QModelIndex proxyIndexFromModelIndex(QModelIndex dmIdx);
    int proxyRowFromModelRow(int dmRow);
    int modelRowFromProxyRow(int sfRow);
    QModelIndex modelIndexFromProxyIndex(QModelIndex sfIdx);
    int nearestProxyRowFromDmRow(int dmRow);
    QString diagnostics();
    QString diagnosticsForCurrentRow();
    void getDiagnosticsForRow(int row, QTextStream& rpt);
    bool updateFileData(QFileInfo fileInfo);
    bool metadataLoaded(int dmRow);
    bool missingThumbnails();
    bool isDimensions(int sfRow);
    bool subFolderImagesLoaded = false;
    bool isMetadataAttempted(int sfRow);
    bool isMetadataLoaded(int sfRow);
    // bool isAllMetadataAttempted();
    bool isAllMetadataLoaded();
    QList<int> metadataNotLoaded();
    int iconCount();
    void clearAllIcons();
    bool isAllIconsLoaded();
    // bool isAllIconChunkLoaded(int first, int last);
    bool iconLoaded(int sfRow, int instance);
    bool isIconRangeLoaded();
    void setIconRange(int sfRow);
    void setChunkSize(int chunkSize);
    bool isPath(QString fPath);
    void rebuildRowFromPathHash();
    int nextPick();
    int prevPick();
    int nearestPick();
    //QModelIndex nearestFiltered(QModelIndex dmIdx);
    bool getSelection(QStringList &list);
    QStringList getSelectionOrPicks();
    bool isSelected(int row);
    void saveSelection();
    void restoreSelection();

    void removeFolder(const QString &folderPath);

    QMutex mutex;
    QReadWriteLock rwLock;

    bool isProcessingFolders = false;

    SortFilter *sf;
    QItemSelectionModel *selectionModel;
    QStringList folderList;             // all folders in the datamodel
    QDir::SortFlags thumbsSortFlags;

    // fPathRow hash and methods for concurrent access
    QHash<QString, int> fPathRow;
    bool fPathRowContains(const QString &path);
    int  fPathRowValue(const QString &path);
    void fPathRowSet(const QString &path, const int row);
    void fPathRowRemove(const QString &path);
    void fPathRowClear();

    // current status
    int instance = 0;                   // each new load of DataModel increments the instance
    QModelIndex instanceParent;         // &index.parent() != &instanceParent means instance clash
    QString firstFolderPathWithImages;
    QString currentFilePath;            // used in caching to update image cache
    int currentSfRow;                   // used in caching to check if new image selected
    int currentDmRow;                   // used in caching to check if new image selected
    QModelIndex currentSfIdx;
    QModelIndex currentDmIdx;
    qint64 bytesUsed = 0;

    const QStringList raw = {"arw", "cr2", "cr3", "dng","nef", "orf", "raf", "sr2", "rw2"};
    const QStringList jpg = {"jpg", "jpeg"};

    int hugeIconThreshold = G::maxIconChunk;
    int firstVisibleIcon;
    int lastVisibleIcon;
    int visibleIcons;
    int startIconRange;
    int endIconRange;
    int iconChunkSize;                  // max suggested number of icons to cache
    int defaultIconChunkSize = 3000;    // used unless more is required (change in pref)
    bool checkChunkSize;                // true if iconChunkSize < rowCount()
    int scrollToIcon = 0;

    bool hasDupRawJpg;
    bool loadingModel = false;          // do not filter while loading datamodel
    bool basicFileInfoLoaded = false;   // not used. do not navigate until basic info loaded in datamodel
    bool folderHasMissingEmbeddedThumb;        // jpg/tiff only

    /* can be set from keyPressEvent in MW to terminate if recursive folder scan or
       building filters too long */
    bool abort;

    bool showThumbNailSymbolHelp = true;
    void setShowThumbNailSymbolHelp(bool showHelp);
    bool okManyImagesWarning();

    struct Fld {
        QString path;
        QString op;
    };

    QQueue<QPair<QString, bool>> folderQueue;

    // locations of symbols on a thumbnail so can show tooltip
    QHash<QString, QRect>iconSymbolRects;

signals:
    void stop(QString src);
    void folderChange();
    // void addedFolderToDM(QString folderName, QString op);
    // void removedFolderFromDM(QString folderName, QString op);
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void centralMsg(QString message);
    void updateProgress(int progress);
    void rowLoaded();
    void updateStatus(bool keepBase, QString s, QString source);
    void refreshViewsOnCacheChange(QString fPath, bool isCached, QString src);

public slots:
//    void unfilteredItemSearchCount();
    void enqueueFolderSelection(const QString &folderPath, QString op, bool recurse = false);
    void addAllMetadata();
    void setAllMetadataLoaded(bool isLoaded);
    bool addMetadataForItem(ImageMetadata m, QString src);
    QString primaryFolderPath();
    QVariant valueSf(int row, int column, int role = Qt::DisplayRole);
    void setIcon(QModelIndex dmIdx, const QPixmap &pm, int fromInstance, QString src = "");
    void setIconFromVideoFrame(QModelIndex dmIdx, QPixmap pm, int fromInstance, qint64 duration,
                               FrameDecoder *frameDecoder);
    void setValueDm(QModelIndex dmIdx, QVariant value, int instance, QString src = "",
                  int role = Qt::EditRole, int align = Qt::AlignLeft | Qt::AlignVCenter);
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValuePath(QString fPath, int col, QVariant value, int instance, int role = Qt::EditRole);
    void setCurrent(QModelIndex dmIdx, int instance);
    void setCurrent(QString fPath, int instance);
    void setCurrentSF(QModelIndex sfIdx, int instance);
    void setCached(int sfRow, bool isCached, int instance);
    void issue(const QSharedPointer<Issue>& issue);
    QStringList rptIssues(int sfRow);
    // void errDM(Issue issue);
    // void errGeneral(Issue issue);
    void abortLoad();
    void rebuildTypeFilter();
    void searchStringChange(QString searchString);
    void processNextFolder();
    void imageCacheWaiting(int sfRow);
    bool isAllMetadataAttempted();
    bool isAllIconChunkLoaded(int first, int last);
    int rowFromPath(QString fPath);
    int proxyRowFromPath(QString fPath, QString src = "");
    QString pathFromProxyRow(int sfRow);
    QString folderPathFromProxyRow(int sfRow);
    QString folderPathFromModelRow(int dmRow);

private slots:

private:
    Metadata *metadata;
    Filters *filters;
    bool &combineRawJpg;
    static bool lessThan(const QFileInfo &i1, const QFileInfo &i2);
    static bool lessThanCombineRawJpg(const QFileInfo &i1, const QFileInfo &i2);

    // Pair of folderPath and operation type (true=add, false=remove)
    void enqueueOp(const QString folderPath, const QString op);
    // QQueue<QPair<QString, bool>> folderQueue;
    QMutex queueMutex;

    enum ErrorType {
        General,
        DM
    };

    struct Error {
        ErrorType type;
        QString functionName;
        QString msg;
        int sfRow;
        QString fPath;
    };

    bool mLock;

    QDir *dir;
    QStringList *fileFilters;
    QFileInfo fileInfo;
    QImage emptyImg;

    QItemSelection savedSelection;

    void addFolder(const QString &folderPath);
    // void removeFolder(const QString &folderPath);
    bool endLoad(bool success);
    // bool addFileData();
    void addFileDataForRow(int row, QFileInfo fileInfo);
    void rawPlusJpg();
    double aspectRatio(int w, int h, int orientation);
    void processErr(Error e);
    void updateLoadStatus(int row);
    // bool tooManyImagesWarning();

    int countInterval = 0;

    bool isDebug = false;

    int imageCacheWaitingForRow = -1;

    QString errMsg;

    void setThumbnailLegend();
    QString thumbnailHelp;
};

#endif // DATAMODEL_H
