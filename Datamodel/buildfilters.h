#ifndef BUILDFILTERS_H
#define BUILDFILTERS_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Datamodel/filters.h"
#include "Metadata/metadata.h"

class BuildFilters : public QThread
{
    Q_OBJECT

public:
    BuildFilters(QObject *parent, DataModel *dm, Metadata *metadata, Filters *filters);

    enum Action {
        Reset,
        Update,
        UpdateCategory
    } action;

    enum AfterAction {
        NoAfterAction,
        Search,
        QuickFilter,
        MostRecentDay,
        NoFilterChange
    } afterAction;

    enum Category {
        PickEdit,
        RatingEdit,
        LabelEdit,
        TitleEdit,
        CreatorEdit,
        MissingThumbEdit,
        CompareEdit
    } category;

    void stop();
    void abortIfRunning();
    void reset(bool collapse = true);
    void recount();

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
    void filterChange(QString src);

public slots:
    void build(BuildFilters::AfterAction newAction = NoAfterAction);
    void rebuild();
    void update();
    void updateCategory(BuildFilters::Category category,
                        BuildFilters::AfterAction newAction = NoAfterAction);
    void updateZeroCountCheckedItems(QTreeWidgetItem *cat, int dmColumn);
   // void updateCategoryItems(QTreeWidgetItem *item, int dmColumn);

private:
    void done();
    void appendUniqueItems();
    void updateUnfilteredSearchCount();
    void updateUnfilteredCounts();
    void updateFilteredCounts();
    void updateCategoryItems();
    void time(QString msg);

    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Filters *filters;
    bool isReset;                           // if true, reset the filter tree filters (new folder)
    int instance;                           // instance of the datamodel
    int dmRows = 0;                         // rows in datamodel (get once at start)

    bool debugBuildFilters = false;
    bool reportTime = false;
    quint64 msTot = 0;

    // progress
    QMap <QString, int>uniqueItemCount;     // total unique items per category in filters
    int totUniqueItems;                     // total unique items in all categories in filters
    double progress = 0;                    // 0-100 progress for progressBar
    QElapsedTimer buildFiltersTimer;
};

#endif // BUILDFILTERS_H
