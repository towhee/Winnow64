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
    void updateIsRunning(bool isRunning);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Filters *filters;
    bool &combineRawJpg;
    QMap <QString, int>instances;
    int totInstances;
    int progress = 0;
    QString buildSteps = "3";

};

#endif // BUILDFILTERS_H
