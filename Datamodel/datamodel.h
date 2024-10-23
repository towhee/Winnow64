#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
//#include "Cache/framedecoder.h"
#include "progressbar.h"        // rgh used??
#include "Main/global.h"
#include "selectionorpicksdlg.h"
#include <QMessageBox>
#include <algorithm>                // req'd for sorting fileInfoList
#include "Log/issue.h"

class SortFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortFilter(QObject *parent, Filters *filters, bool &combineRawJpg);
    bool isFinished();
    bool isSuspended();

public slots:
    void filterChange(QString src = "");
    void suspend(bool suspendFiltering);

private slots:

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:

private:
    Filters *filters;
    bool &combineRawJpg;
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
    bool load(QString &dir, bool includeSubfoldersFlag);
    bool readMetadataForItem(int row, int instance);
    bool refreshMetadataForItem(int sfRow, int instance);
    void clearDataModel();
    void newInstance();
    bool hasFolderChanged();
    bool contains(QString &path);
    void find(QString text);
    ImageMetadata imMetadata(QString fPath, bool updateInMetadata = false);
    bool isPick();
    void clearPicks();
    void remove(QString fPath);
    int insert(QString fPath);
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
    bool isAllMetadataAttempted();
    bool isAllMetadataLoaded();
    QList<int> metadataNotLoaded();
    int iconCount();
    void clearAllIcons();
    bool isAllIconsLoaded();
    bool isAllIconChunkLoaded(int first, int last);
    bool iconLoaded(int sfRow, int instance);
    bool isIconRangeLoaded();
    void setIconRange(int sfRow);
    void setChunkSize(int chunkSize);
    bool isPath(QString fPath);
    int rowFromPath(QString fPath);
    int proxyRowFromPath(QString fPath);
    QString pathFromProxyRow(int sfRow);
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

    QMutex mutex;

    SortFilter *sf;
    QItemSelectionModel *selectionModel;
    QHash<QString, int> fPathRow;
    QStringList imageFilePathList;
    QDir::SortFlags thumbsSortFlags;

    // current status
    int instance = 0;                   // each new load of DataModel increments the instance
    QModelIndex instanceParent;         // &index.parent() != &instanceParent means instance clash
    QString currentFolderPath;
    QString currentFilePath;            // used in caching to update image cache
    int currentSfRow;                   // used in caching to check if new image selected
    int currentDmRow;                   // used in caching to check if new image selected
    QModelIndex currentSfIdx;
    QModelIndex currentDmIdx;

    int hugeThreshold = G::maxIconChunk;
    int firstVisibleIcon;
    int lastVisibleIcon;
    int visibleIcons;
    int startIconRange;
    int endIconRange;
    int iconChunkSize;                  // max suggested number of icons to cache
    int defaultIconChunkSize = 3000;    // used unless more is required (change in pref)
    bool checkChunkSize;                // true if iconChunkSize < rowCount()

    bool hasDupRawJpg;
    bool loadingModel = false;          // do not filter while loading datamodel
    bool basicFileInfoLoaded = false;   // not used. do not navigate until basic info loaded in datamodel
    bool folderHasMissingEmbeddedThumb;        // jpg/tiff only

    // rgh check if reqd still
    bool forceBuildFilters = false;     // ignore buildFiltersMaxDelay if true
    int buildFiltersMaxDelay = 1000;    // quit if exceed and not forceBuildFilters
    QElapsedTimer buildFiltersTimer;

    QList<QFileInfo> modifiedFiles;     // used by MW::refreshCurrentFolder

    /* errors are tracked by image (datamodel row) or general errors if not related to a
       specific image */
    QStringList generalErrors;

    /* can be set from keyPressEvent in MW to terminate if recursive folder scan or
       building filters too long */
    bool abortLoadingModel;

    bool showThumbNailSymbolHelp = true;
    void setShowThumbNailSymbolHelp(bool showHelp);
    bool okManyImagesWarning();

signals:
    void addedFolderToDM(QString folderName);
    void updateClassification();        // req'd for 1st image, loaded before metadata cached
    void centralMsg(QString message);
    void updateProgress(int progress);
    void updateStatus(bool keepBase, QString s, QString source);

public slots:
//    void unfilteredItemSearchCount();
    void enqueueFolderSelection(const QString &folderPath, bool isAdding);
    void addAllMetadata();
    void setAllMetadataLoaded(bool isLoaded);
    bool addMetadataAndIconForItem(ImageMetadata m, QModelIndex dmIdx, const QPixmap &pm,
                                   int fromInstance, QString src);
    bool addMetadataForItem(ImageMetadata m, QString src);
    void setIcon(QModelIndex dmIdx, const QPixmap &pm, int fromInstance, QString src = "");
    void setIconFromVideoFrame(QModelIndex dmIdx, QPixmap &pm, int fromInstance, qint64 duration);
    void setValue(QModelIndex dmIdx, QVariant value, int instance, QString src = "",
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValuePath(QString fPath, int col, QVariant value, int instance, int role = Qt::EditRole);
    void setCurrent(QModelIndex dmIdx, int instance);
    void setCurrentSF(QModelIndex sfIdx, int instance);
    void issue(const QSharedPointer<Issue>& issue);
    QStringList rptIssues(int sfRow);
    // void errDM(Issue issue);
    // void errGeneral(Issue issue);
    void abortLoad();
    void rebuildTypeFilter();
    void searchStringChange(QString searchString);

private slots:
    void processNextFolder();

private:
    // QWidget *mw;
    Metadata *metadata;
    Filters *filters;
    bool &combineRawJpg;
    QList<QFileInfo> fileInfoList;
    static bool lessThan(const QFileInfo &i1, const QFileInfo &i2);

    // all folders in the datamodel
    QStringList folderList;
    // Pair of folderPath and operation type (true=add, false=remove)
    QQueue<QPair<QString, bool>> folderQueue;
    QMutex queueMutex;
    bool isProcessing = false;

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
    bool includeSubfolders = false;

    QItemSelection savedSelection;

    void addFolder(const QString &folderPath);
    void removeFolder(const QString &folderPath);
    bool endLoad(bool success);
    bool addFileData();
    void addFileDataForRow(int row, QFileInfo fileInfo);
    void rawPlusJpg();
    double aspectRatio(int w, int h, int orientation);
    void processErr(Error e);
    void updateLoadStatus();
    // bool tooManyImagesWarning();

    int imageCount;
    int folderCount;
    int countInterval = 0;

    bool isDebug = false;
    ImageMetadata mCopy;
    int line;
    int rowCountChk;

    QString errMsg;

    void setThumbnailLegend();
    QString thumbnailHelp;
};

#endif // DATAMODEL_H
