#ifndef BUILDFILTERS_H
#define BUILDFILTERS_H

#include <QtWidgets>
#include <memory>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Datamodel/filtersnapshot.h"
#include "Datamodel/filters.h"
#include "Metadata/metadata.h"

/*  ONE GUI MUTATION recorded by the worker and applied on the GUI thread.

    BuildFilters::run() used to call filters->addCategoryItems(),
    updateFilteredCountPerItem(), update(), setEachCatTextColor() and
    setEnabled() DIRECTLY -- mutating a QTreeWidget and its items from a worker
    thread, under nothing but a plain QMutex that the GUI thread does not take
    when it paints or when the user clicks an item. The worker now records what
    it wants done and the GUI thread does it. */
struct FilterOp {
    enum Kind {
        SearchCount,        // Filters::updateSearchCategoryCount(map, flag)
        AddItems,           // Filters::addCategoryItems(map, item)
        UnfilteredCount,    // Filters::updateUnfilteredCountPerItem(map, item)
        FilteredCount,      // Filters::updateFilteredCountPerItem(map, item)
        CategoryItems,      // Filters::updateCategoryItems(map, item)
        Update,             // Filters::update()
        TextColor,          // Filters::setEachCatTextColor()
        MenuUpdate,         // emit updateFilterMenu(src)
        Done                // BuildFilters::done()
    };
    Kind kind;
    QMap<QString,int> map;
    QTreeWidgetItem *item = nullptr;
    bool flag = false;                  // SearchCount: isFiltered
    QString src;                        // MenuUpdate
};
typedef QVector<FilterOp> FilterOps;

class BuildFilters : public QThread
{
    Q_OBJECT

public:
    BuildFilters(QObject *parent, DataModel *dm, Metadata *metadata, Filters *filters);

    enum Action {
        Reset,
        UpdateCounts,
        UpdateAllCounts,
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
    void reset(bool collapse = true);
    void recount();
    bool isIdle();
    bool isBusy();

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void stopped(QString src);
    void updateProgress(int progress);
    // void addToDatamodel(ImageMetadata m, QString src);
    void finishedBuildFilters();
    void quickFilter();
    void filterLastDay();
    void searchTextEdit();
    void filterChange(QString src);
    void updateFilterMenu(QString src);

public slots:
    void abortProcessing();
    void build(BuildFilters::AfterAction newAction = NoAfterAction);
    void rebuild();
    void update();
    void updateAllCounts();
    void updateCategory(BuildFilters::Category category,
                        BuildFilters::AfterAction newAction = NoAfterAction,
                        bool runSync = false);
    void updateZeroCountCheckedItems(QTreeWidgetItem *cat, int dmColumn);
   // void updateCategoryItems(QTreeWidgetItem *item, int dmColumn);

private:
    void done();
    void appendUniqueItems(const FilterSnapshot &snap, FilterOps &ops);
    void updateUnfilteredSearchCount(const FilterSnapshot &snap, FilterOps &ops);
    void updateUnfilteredCounts(const FilterSnapshot &snap, FilterOps &ops);
    void updateFilteredCounts(const FilterSnapshot &snap, FilterOps &ops);
    void updateCategoryItems(const FilterSnapshot &snap, FilterOps &ops);

    /*  Snapshot plumbing -- see Datamodel/filtersnapshot.h. takeSnapshot MUST
        run on the GUI thread (it reads dm and dm->sf); every entry point that
        start()s this thread calls it first. countSlot/countKeywords read only
        the snapshot and are what the worker calls. */
    /*  A snapshot is IMMUTABLE AND PER-RUN, handed round as a shared_ptr, not
        held as a member the two threads share.

        It was a plain member first, and ThreadSanitizer found the hole within
        one folder load: MW::buildFiltersWhenModelReady calls build() -- which
        starts this thread -- and then recount() on the GUI thread on the very
        next line, so takeSnapshot() cleared the QVector out from under
        appendUniqueItems() reading it. run() now takes its own reference at
        entry and keeps that object alive for its whole life, so a snapshot
        taken meanwhile is simply a different object. */
    std::shared_ptr<const FilterSnapshot> makeSnapshot() const;
    void publishSnapshot();

    QMap<QString,int> countSlot(const FilterSnapshot &snap, int slot,
                                bool filtered) const;
    QMap<QString,int> countKeywords(const FilterSnapshot &snap,
                                    bool filtered) const;

    /*  dispatchOps hands the recorded ops to the GUI thread -- inline when the
        caller already IS the GUI thread (updateCategory's runSync path), else
        stashed and posted. flushOps/applyOps run on the GUI thread only. */
    void dispatchOps(FilterOps &ops);
    void flushOps();
    void applyOps(const FilterOps &ops);

    /*  One filter category: a snapshot slot and the Filters tree item its
        counts are written to. See sinks() in the cpp. */
    struct Sink {
        int slot;
        QTreeWidgetItem *item;
        const char *name;
    };
    QVector<Sink> sinks() const;
    void time(QString msg);
    void setIdle();
    void setBusy();

    QMutex mutex;
    QWaitCondition condition;
    /* Status/cancellation flags shared between the GUI thread (abortProcessing,
       isIdle/isBusy) and the BuildFilters worker thread (run) — std::atomic to
       remove the TSan-confirmed data race (abort: run vs updateUnfilteredCounts). */
    std::atomic<bool> abort{false};
    std::atomic<bool> idle{true};
    DataModel *dm;
    Metadata *metadata;
    Filters *filters;
    bool isReset;                           // if true, reset the filter tree filters (new folder)
    int instance;                           // instance of the datamodel
    int dmRows = 0;                         // rows in datamodel (get once at start)
    // set by publishSnapshot on the GUI thread, taken by run(); guarded by mutex
    std::shared_ptr<const FilterSnapshot> pendingSnap;
    FilterOps pendingOps;                   // guarded by mutex; drained by flushOps

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
