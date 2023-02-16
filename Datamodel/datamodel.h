#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/filters.h"
#include "Cache/framedecoder.h"
#include "progressbar.h"        // rgh used??
#include "Main/global.h"
#include "selectionorpicksdlg.h"
#include <QMessageBox>
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
    bool readMetadataForItem(int row, int instance);
    void clearDataModel();
    bool hasFolderChanged();
    bool contains(QString &path);
    void find(QString text);
    ImageMetadata imMetadata(QString fPath, bool updateInMetadata = false);
    bool isPick();
    void clearPicks();
    void remove(QString fPath);
    void insert(QString fPath);
    QModelIndex proxyIndexFromPath(QString fPath);
    int proxyRowFromModelRow(int dmRow);
    int modelRowFromProxyRow(int sfRow);
    QModelIndex modelIndexFromProxyIndex(QModelIndex sfIdx);
    QString diagnostics();
    QString diagnosticsErrors();
    QString diagnosticsForCurrentRow();
    void getDiagnosticsForRow(int row, QTextStream& rpt);
    bool updateFileData(QFileInfo fileInfo);
    bool metadataLoaded(int dmRow);
    bool subFolderImagesLoaded = false;
    bool isAllMetadataLoaded();
    int iconCount();
    void clearAllIcons();
    void setIconRange(int first, int last);
    void clearOutOfRangeIcons(int startRow);
    bool allIconsLoaded();
    bool iconLoaded(int sfRow, int instance);
    int rowFromPath(QString fPath);
    void refreshRowFromPathHash();
    int nextPick();
    int prevPick();
    int nearestPick();
//    void saveSelection();
//    void recoverSelection();
    bool getSelection(QStringList &list);
    QStringList getSelectionOrPicks();
//    QModelIndex getNearestSelectedIndex(int sfRow);
//    void invertSelection();
//    void chkForDeselection(int sfRow);
    bool isSelected(int row);

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
    int currentSfRow;                     // used in caching to check if new image selected
    int currentDmRow;                     // used in caching to check if new image selected
    QModelIndex currentSfIdx;
    QModelIndex currentDmIdx;
    int startIconRange;                 // used to determine MetaRead priority queue
    int endIconRange;                   // used to determine MetaRead priority queue
    int midIconRange;                   // used to determine MetaRead priority queue
    int iconChunkSize;                  // max suggested number of icons to cache
    int defaultIconChunkSize = 3000;    // used unless more is required (change in pref)

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
    void updateProgress(int progress);
    void updateStatus(bool keepBase, QString s, QString source);

public slots:
//    void unfilteredItemSearchCount();
    void addAllMetadata();
    void setAllMetadataLoaded(bool isLoaded);
    bool addMetadataForItem(ImageMetadata m, QString src);
    void setIcon(QModelIndex dmIdx, const QPixmap &pm, int fromInstance, QString src = "");
    void setIconFromVideoFrame(QModelIndex dmIdx, QPixmap &pm, int fromInstance,
                          qint64 duration);
    void setValue(QModelIndex dmIdx, QVariant value, int instance, QString src = "",
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void setValuePath(QString fPath, int col, QVariant value, int instance, int role = Qt::EditRole);
    void abortLoad();
    void rebuildTypeFilter();
    void searchStringChange(QString searchString);
//    void selectAll();
//    void selectFirst();
//    void selectLast();
//    void select(QModelIndex sfIdx);
//    void select(int sfRow);
//    void select(QString &fPath);
//    void selectNext();
//    void selectPrev();
//    void selectRandom();
//    void selectNextPick();
//    void selectPrevPick();

private:
    QWidget *mw;
    Metadata *metadata;
    Filters *filters;
    bool &combineRawJpg;
    QList<QFileInfo> fileInfoList;
    static bool lessThan(const QFileInfo &i1, const QFileInfo &i2);

//    QMutex mutex;
    bool mLock;

    QDir *dir;
    QStringList *fileFilters;
    QFileInfo fileInfo;
    QImage emptyImg;
    bool includeSubfolders = false;

    bool endLoad(bool success);
    bool addFileData();
    void addFileDataForRow(int row, QFileInfo fileInfo);
    void rawPlusJpg();
    double aspectRatio(int w, int h, int orientation);
    void setIconMax(const QPixmap &pm);
    bool instanceClash(QModelIndex idx, QString src);
    int imageCount;
    int countInterval = 0;
    QString buildMsg = "Building filters.  This could take a while to complete.<p>"
                       "Press \"Esc\" to stop<p>";
    QString buildSteps = "3";
    int step;

    bool isDebug = true;
    QString lastFunction = "";
    ImageMetadata mCopy;
    int line;
    int rowCountChk;
};

#endif // DATAMODEL_H
