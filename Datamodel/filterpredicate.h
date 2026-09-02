#ifndef FILTERPREDICATE_H
#define FILTERPREDICATE_H

#include <QVariant>
#include <QVector>
#include <memory>

/*
    THE FILTERS TREE, COMPILED.

    SortFilter::filterAcceptsRow used to answer "is this row visible" by walking
    the whole Filters QTreeWidget with a QTreeWidgetItemIterator, once PER ROW,
    comparing QVariants as it went. Two things are wrong with that, and only one
    of them is about speed.

    IT IS O(rows x items). The tree holds one item per DISTINCT VALUE the folder
    contains -- every keyword, every camera model, every lens, every focal
    length -- so a few hundred items is ordinary and the whole tree is walked
    for every row on every filter change, sort and insert. At 250,000 rows that
    is tens of millions of pointer-chasing QVariant compares to answer a
    question whose inputs changed once.

    IT READS A WIDGET THAT ANOTHER PASS IS WRITING. BuildFilters adds and
    removes items as metadata arrives, and filterAcceptsRow reads
    (*filter)->checkState(0) while it does -- there is a `// crash` comment on
    exactly that line. Suspending filtering during a rebuild papers over it; it
    does not make the read safe.

    So the tree is compiled ONCE per change, on the GUI thread, into the plain
    data below, and filterAcceptsRow evaluates that instead. The compiled form
    is handed out as a shared_ptr<const>, the same shape ProxySnapshot uses in
    Datamodel/modelsync.h: a reader holds its copy alive for the duration of its
    work while the GUI thread swaps in a new one, so a rebuild can no longer
    pull items out from under a row being tested.

    -------------------------------------------------------------------------
    THE SEMANTICS ARE COPIED, NOT REDESIGNED. Three states, not two:
    Qt::Checked INCLUDES, Qt::PartiallyChecked EXCLUDES, Qt::Unchecked is
    "don't care".

      o Includes are OR-ed within a category and AND-ed between categories.
      o An exclude is an outright rejection evaluated ACROSS categories: if the
        row carries an excluded value it is out, whatever else matches. That is
        what makes "include Vancouver, exclude USA" mean what it reads like --
        the keyword vocabulary is flat, so an ambiguous name is separated by
        ruling one of its parents out rather than by descending a tree.
      o An exclude must NOT make a category count as "filtering". A category
        holding only exclusions is not narrowing the set to those items, it is
        subtracting them; were it to count, every row lacking any of its items
        would be rejected, which is the whole set. Here that falls out of the
        structure rather than out of a flag: `includes` being empty IS the
        category not filtering, and excludes live in their own list.

    ORDER OF REJECTION DOES NOT MATTER, which is what makes evaluating a
    category completely before moving to the next one equivalent to the old
    interleaved walk. The old code checked category N's include verdict when it
    reached category N+1's header, so an exclude in N+1 could be reached before
    N's verdict; but every path either returns false or continues, and the
    function is pure, so whichever rejection wins the answer is the same.
*/

struct FilterCategory
{
    int column = 0;                 // G::dataModelColumns, from the category's ColumnRole
    QVector<QVariant> includes;     // Qt::Checked      -- OR within, AND across
    QVector<QVariant> excludes;     // Qt::PartiallyChecked -- reject on hit
    /*  The search category's "any" state: searchTrue checked while its text is
        still the placeholder means "a search is armed but nothing typed", which
        matched every row. Kept as a flag rather than as an include, because it
        matches without comparing anything. */
    bool includeAll = false;

    bool isFiltering() const { return includeAll || !includes.isEmpty() || !excludes.isEmpty(); }
};

struct FilterPredicate
{
    QVector<FilterCategory> categories;

    /*  Nothing checked anywhere: every row passes, and no column is even read.
        Worth its own answer because it is the ordinary case. */
    bool acceptsEverything() const
    {
        for (const FilterCategory &c : categories) if (c.isFiltering()) return false;
        return true;
    }

    /*  THE TWO COMPARISONS ARE DELIBERATELY DIFFERENT, because the code this
        replaces made them differently and the job here is to move the cost, not
        the meaning.

        An INCLUDE on a QStringList column did .contains(filterValue) with
        filterValue still a QVariant, so QString == QVariant promoted the left
        side and compared as VARIANTS -- type-aware. An EXCLUDE did
        .contains(filterValue.toString()), a plain string compare. Every filter
        value on a keyword category is a QString, so the two agree today; they
        are kept apart so that if one ever stops being a QString the behaviour
        changes in the same place it always would have. */
    static bool includeHit(const QVariant &dataValue, const QVariant &filterValue)
    {
        if (dataValue.typeId() == QMetaType::QStringList)
            return dataValue.toStringList().contains(filterValue);
        return dataValue == filterValue;
    }
    static bool excludeHit(const QVariant &dataValue, const QVariant &filterValue)
    {
        if (dataValue.typeId() == QMetaType::QStringList)
            return dataValue.toStringList().contains(filterValue.toString());
        return dataValue == filterValue;
    }

    /*  IS THIS ROW VISIBLE. Takes a callable rather than a model so the whole
        decision can be tested without one -- valueFor(column) returns what
        data(Qt::EditRole) would return for that column of the row.

        It is called ONCE PER FILTERING CATEGORY, which is most of what made the
        old version expensive: walking the tree fetched the same cell once per
        ITEM, so a category holding 200 keywords read it 200 times. */
    template <typename Fetch>
    bool accepts(Fetch &&valueFor) const
    {
        for (const FilterCategory &cat : categories) {
            if (!cat.isFiltering()) continue;

            const QVariant dataValue = valueFor(cat.column);

            /*  Exclusions first and answered immediately: no other category can
                readmit a row the user has said to leave out. */
            for (const QVariant &ex : cat.excludes)
                if (excludeHit(dataValue, ex)) return false;

            /*  includeAll matches without comparing; an empty include list is
                the category not narrowing anything -- the old
                isCategoryUnchecked, which an exclusion deliberately did not
                clear. */
            if (cat.includeAll || cat.includes.isEmpty()) continue;

            bool isMatch = false;
            for (const QVariant &in : cat.includes)
                if (includeHit(dataValue, in)) { isMatch = true; break; }
            if (!isMatch) return false;         // no match in category
        }
        return true;
    }
};

using FilterPredicatePtr = std::shared_ptr<const FilterPredicate>;

#endif // FILTERPREDICATE_H
