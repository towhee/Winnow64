#ifndef BUILDFILTERS_H
#define BUILDFILTERS_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Datamodel/filters.h"
#include "Metadata/metadata.h"
#include "Main/global.h"

class BuildFilters : public QThread
{
    Q_OBJECT

public:
    BuildFilters(QObject *parent, DataModel *dm, Metadata *metadata, Filters *filters,
                 bool &combineRawJpg);
    void stop();
    void done();
    void reset();
    void setAfterAction(QString afterAction);
    void loadAllMetadata();
    void mapUniqueInstances();
    void updateCountFiltered();
    void countFiltered();
    void countUnfiltered();
    void unfilteredItemSearchCount();

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void stopped(QString src);
    void updateProgress(int progress);
    void addToDatamodel(ImageMetadata m, QString src);
    void finishedBuildFilters();
    void quickFilter();

public slots:
    void build();

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Filters *filters;
    QString afterAction = "";               // What to do when done
    bool &combineRawJpg;
    bool isReset;                           // if true, reset the filter tree filters (new folder)
    int instance;                           // instance of the datamodel
    int dmRows = 0;                         // rows in datamodel (get once at start)
    // progress
    QMap <QString, int>uniqueItemCount;     // total unique items per category in filters
    int totUniqueItems;                     // total unique items in all categories in filters
    int progress = 0;                       // 0-100 progress for progressBar
    QElapsedTimer buildFiltersTimer;
};

#endif // BUILDFILTERS_H
