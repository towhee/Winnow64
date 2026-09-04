#ifndef FILTERSNAPSHOT_H
#define FILTERSNAPSHOT_H

#include <QString>
#include <QStringList>
#include <QVector>

/*
    A PLAIN-DATA COPY of the datamodel columns BuildFilters counts.

    WHY IT EXISTS. BuildFilters is a QThread, and its counting passes used to
    read dm and dm->sf directly -- around seventy calls of the shape
    dm->index(row, G::PickColumn).data(). QStandardItemModel and
    QSortFilterProxyModel are not thread-safe, and the GUI thread is writing
    into that same model throughout a folder load (addMetadataForItem, setIcon1),
    so those reads were a genuine data race that happened to be quiet. The
    snapshot is taken ON THE GUI THREAD before the worker starts, and the worker
    then reads nothing but this.

    IT IS ALSO WHY THE THREE PASS GROUPS COLLAPSED into one table. The same
    fifteen categories were written out three times -- once to build the item
    lists, once for the unfiltered counts and once for the filtered counts --
    and the copies had to be kept in step by hand. Here a category is one row of
    kFilterCatColumn plus one entry in the sink table, so a category added to
    the panel cannot reach two of the three and miss the third.

    THE STRINGS ARE CHEAP. QString is implicitly shared and QVariant::toString()
    hands back a reference to the same buffer, so a row of fourteen values is
    fourteen pointer copies, not fourteen allocations.

    inProxy IS CAPTURED, NOT RECOMPUTED. The filtered counts need "which rows
    survive the current filter", which is a proxy question; asking the proxy per
    row from the worker is the race this class removes. It is filled by walking
    the proxy once on the GUI thread while the snapshot is taken, so the
    filtered and unfiltered counts describe the SAME instant -- which reading
    the two models separately never guaranteed.
*/

namespace FilterCat {
    /*  The countable categories, in the order the Filters panel shows them.
        Keywords are not here: a row carries a LIST of them, so they are counted
        from FilterSnapshotRow::keywords rather than from v[]. */
    /*  Availability holds the LABEL ("Present"/"Offline"/"Missing"), not the int the
        model holds. Every other slot is the column's own string; this one is
        translated when the snapshot is taken, because a category item shows a word
        and the compare value is put back to the code by Filters::filterValueFor. */
    /*  Month and Iso are the columns' own strings like the rest, with one wrinkle
        each: Month holds the NAME the model was written with (Catalog::monthLabel),
        and Iso -- an int in the model -- is right-justified when the snapshot is
        taken, the way FocalLength is, so it counts and sorts as a number. */
    enum Slot {
        Search, Pick, Rating, Label, Type, FolderName, Year, Month, Day,
        CameraModel, Lens, FocalLength, Iso, Title, Creator, Availability, Compare,
        SlotCount
    };
}

struct FilterSnapshotRow {
    QString v[FilterCat::SlotCount];    // already trimmed, ready to count
    QStringList keywords;               // G::KeywordsAllColumn, trimmed
    bool hiddenRaw = false;             // combineRawJpg && G::DupHideRawRole
    bool inProxy   = false;             // survives the current proxy filter
};

struct FilterSnapshot {
    int instance = -1;                  // dm->instance when taken
    QVector<FilterSnapshotRow> rows;    // by DATAMODEL row
    int proxyRows = 0;                  // rows with inProxy set

    bool isEmpty() const { return rows.isEmpty(); }
    void clear() { instance = -1; rows.clear(); proxyRows = 0; }
};

#endif // FILTERSNAPSHOT_H
