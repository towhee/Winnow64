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
    void build();
    void done();
    void loadAllMetadata();
    void mapUniqueInstances();
    void updateCountFiltered();
    void countFiltered();
    void countUnfiltered();
    void unfilteredItemSearchCount();

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void updateProgress(int progress);
    void finishedBuildFilters();

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Filters *filters;
    bool &combineRawJpg;
    int sfRows = 0;                         // rows in filtered datamodel (get once at start)
    int dmRows = 0;                         // rows in datamodel (get once at start)
    QMap <QString, int>instances;           // total items per category in filters
    int totInstances;                       // total items in all categories in filters
    int progress = 0;                       // 0-100 progress for progressBar
    QElapsedTimer buildFiltersTimer;
};

#endif // BUILDFILTERS_H
