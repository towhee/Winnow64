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

    enum Action {
        Reset,
        Update,
        PickEdit,
        RatingEdit,
        LabelEdit,
        TitleEdit,
        CreatorEdit
    } action;

//    enum Category {
//        PickEdit,
//        RatingEdit,
//        LabelEdit,
//        TitleEdit,
//        CreatorEdit
//    } category;

    void stop();
    void reset();

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void stopped(QString src);
    void updateProgress(int progress);
    void addToDatamodel(ImageMetadata m, QString src);
    void finishedBuildFilters();
    void quickFilter();
    void filterLastDay();
    void searchTextEdit();

public slots:
    void build(BuildFilters::Action action = Action::Reset, QString afterAction = "");
//    void update();
//    void updateCategory();

private:
    void done();
    void appendUniqueItems();
    void updateFilteredCounts();
    void updateCategoryItems();
    void time(QString msg);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Filters *filters;
    QString afterAction = "";               // What to do when done
    bool &combineRawJpg;
//    bool filtersBuilt = false;
    bool isReset;                           // if true, reset the filter tree filters (new folder)
    int instance;                           // instance of the datamodel
    int dmRows = 0;                         // rows in datamodel (get once at start)

    bool debugBuildFilters = false;
    bool reportTime = false;
    quint64 totms = 0;

    // progress
    QMap <QString, int>uniqueItemCount;     // total unique items per category in filters
    int totUniqueItems;                     // total unique items in all categories in filters
    double progress = 0;                       // 0-100 progress for progressBar
    QElapsedTimer buildFiltersTimer;
};

#endif // BUILDFILTERS_H
