#include <QtTest>

#include "Datamodel/imagerow.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"   // keywordPrefixExpand, for the row budget case

/*
    THE PACKED ROW STORE IS NOW THE ONLY COPY.

    While the QStandardItems were still written beside it, DataModel::verify-
    RowStore() compared the two on every row and every covered column, and that
    comparison is what caught the rating/MPix/exposure-time type bugs. The items
    are gone, so the comparison is gone, and what it used to prove has to be
    pinned here instead.

    Two things it proved, both of which failed in practice at least once:

    UNSET IS NOT ZERO. An unwritten cell returned an INVALID QVariant and
    callers branch on that. The store returning a field's zero -- or an interned
    empty string -- turns "no duration" into "a duration of nothing". This
    matters more since the second batch of columns arrived, because those
    brought the first genuinely conditional resident columns.

    TEXT IS NOT NUMBER. ExposureTimeItemDelegate guards "value == 0" before
    computing 1/value; a QString holding the same digits walks past the guard
    and reaches qRound(+Inf). What must not drift is text-vs-number.
*/
class tst_imagerow : public QObject
{
    Q_OBJECT

private slots:
    void pathIsCoveredOnlyAtPathRole();
    void coversOnlyValueRoles();
    void searchAndIngestedAreBoolsNow();
    void variantComparisonIsTypeTolerant();
    void unwrittenFieldsReadBackAsInvalid();
    void conditionalColumnsStayUnsetUntilWritten();
    void availabilityRoundTripsAndDefaultsToPresent();
    void numericColumnsComeBackNumeric();
    void internedRepeatsCollapse();
    void insertRowsShiftsTheRowsAfterIt();
    void removeRowsSplicesRatherThanTruncates();
    void compactDropsScatteredRowsInOrder();
    void pickedCountStaysExact();
    void forEachRowMatchesValue();
    void watchedGenerationMovesOnlyForWatchedCells();
    void watchedChangesNameTheRowsUntilTheyCannot();
    void fieldGenerationMovesForItsFieldAndSplicesOnly();
    void prefixExpansionStaysInBudget();
    void folderPathsAllIsDerivedFromThePath();
    void folderAncestryCostsARowOneId();

private:
    static void fill(RowStore &s, int row, const QString &path)
    {
        s.setValue(row, G::PathColumn, G::PathRole, path);
        s.setValue(row, G::NameColumn, Qt::EditRole, path.section('/', -1));
        s.setValue(row, G::TypeColumn, Qt::EditRole, "JPG");
    }
};

void tst_imagerow::pathIsCoveredOnlyAtPathRole()
{
    /*  addFileDataForRow never sets EditRole on PathColumn, so the item was
        UNSET there and data() must keep returning an invalid QVariant. Claiming
        the column at EditRole made the store answer "" where the model answered
        nothing -- a different value to anything testing isValid(). */
    QVERIFY(RowStore::covers(G::PathColumn, G::PathRole));
    QVERIFY(!RowStore::covers(G::PathColumn, Qt::EditRole));
    QVERIFY(!RowStore::covers(G::PathColumn, Qt::DisplayRole));
}

void tst_imagerow::coversOnlyValueRoles()
{
    const QVector<int> held {
        G::NameColumn, G::TypeColumn, G::RatingColumn, G::ApertureColumn,
        G::KeywordsColumn, G::MetadataStatusColumn, G::IconLoadedColumn,
        // the second batch
        G::ExposureCompensationColumn, G::DurationColumn, G::FocusXColumn,
        G::AspectRatioColumn, G::IconAspectRatioColumn, G::OrientationColumn,
        G::RotationColumn, G::EmailColumn, G::UrlColumn,
        G::MetadataReadingColumn, G::_RatingColumn, G::_UrlColumn,
        G::PermissionsColumn, G::ReadWriteColumn, G::SidecarColumn,
        G::OrientationOffsetColumn, G::RotationDegreesColumn,
        G::ShootingInfoColumn, G::ErrColumn, G::DevelopColumn,
        G::DevPreviewKeyColumn
    };
    for (int c : held) {
        QVERIFY2(RowStore::covers(c, Qt::EditRole),
                 qPrintable(QString("column %1 not covered").arg(c)));
        QVERIFY(RowStore::covers(c, Qt::DisplayRole));
        /*  Presentation roles are not the store's. Alignment is a per-column
            table in datamodel.cpp and tooltips are derived from the cell. */
        QVERIFY(!RowStore::covers(c, Qt::TextAlignmentRole));
        QVERIFY(!RowStore::covers(c, Qt::ToolTipRole));
        QVERIFY(!RowStore::covers(c, Qt::DecorationRole));
    }
    /*  The decode-timing columns belong to the SCRATCH store, not here -- the
        two must never both claim a column. */
    for (int c : { int(G::NSThumbColumn), int(G::NSImageColumn),
                   int(G::LoadMsecPerMpColumn), int(G::RawRenderColumn),
                   int(G::OffsetFullColumn), int(G::ICCBufColumn) })
        QVERIFY2(!RowStore::covers(c, Qt::EditRole),
                 qPrintable(QString("column %1 claimed by both stores").arg(c)));
}

void tst_imagerow::searchAndIngestedAreBoolsNow()
{
    /*  These two were the last columns left on the QStandardItems, held back
        because each carried a different TYPE depending on which path wrote it
        last: "false" (QString) at row creation, a bool once the work ran,
        "true" (QString) from a third path. Both are bools throughout now --
        including Filters::searchTrue, whose filter value was a QString while
        searchFalse beside it was already a bool. */
    QVERIFY(RowStore::covers(G::SearchColumn, Qt::EditRole));
    QVERIFY(RowStore::covers(G::IngestedColumn, Qt::EditRole));

    RowStore s;
    s.resize(1);
    s.setValue(0, G::SearchColumn, Qt::EditRole, false);
    s.setValue(0, G::IngestedColumn, Qt::EditRole, true);
    QCOMPARE(s.value(0, G::SearchColumn).typeId(), int(QMetaType::Bool));
    QCOMPARE(s.value(0, G::IngestedColumn).typeId(), int(QMetaType::Bool));
    QCOMPARE(s.value(0, G::SearchColumn).toBool(), false);
    QCOMPARE(s.value(0, G::IngestedColumn).toBool(), true);
}

void tst_imagerow::variantComparisonIsTypeTolerant()
{
    /*  THE FACT THE DEFERRAL RESTED ON, AND IT WAS WRONG. The plan for this work
        asserted that QVariant(QString("false")).toBool() is TRUE -- "a non-empty
        string" -- and concluded that the mixed types were a live matching bug
        that only the filter could settle. Measured, Qt 6 special-cases the word,
        and QVariant comparison converts between bool and the strings "true" and
        "false" in both directions. So the mixed types cost nothing at runtime;
        settling them was tidying, not a fix, and the filtering signature over
        1,600 randomised trials was byte-identical across the change.

        Pinned here because the wrong version of it was written down twice and
        steered a decision, and because anything reading a legacy sidecar or an
        older catalog row still depends on the conversion behaving this way. */
    QCOMPARE(QVariant(QString("false")).toBool(), false);
    QCOMPARE(QVariant(QString("true")).toBool(), true);
    QCOMPARE(QVariant(QString("")).toBool(), false);
    QVERIFY(QVariant(QString("false")) == QVariant(false));
    QVERIFY(QVariant(false) == QVariant(QString("false")));
    QVERIFY(QVariant(QString("true")) == QVariant(true));
    QVERIFY(QVariant(true) == QVariant(QString("true")));
}

void tst_imagerow::unwrittenFieldsReadBackAsInvalid()
{
    RowStore s;
    s.resize(3);
    fill(s, 1, "/a/b/c.jpg");

    QCOMPARE(s.value(1, G::PathColumn, G::PathRole).toString(), QString("/a/b/c.jpg"));
    QCOMPARE(s.value(1, G::TypeColumn).toString(), QString("JPG"));

    // everything else on that row is still nothing, not empty
    QVERIFY(!s.value(1, G::TitleColumn).isValid());
    QVERIFY(!s.value(1, G::RatingColumn).isValid());
    QVERIFY(!s.value(1, G::ShootingInfoColumn).isValid());
    // and a row nobody touched at all
    QVERIFY(!s.value(0, G::NameColumn).isValid());
    // out of range is nothing, not a crash
    QVERIFY(!s.value(99, G::NameColumn).isValid());

    /*  An EMPTY STRING written deliberately is a value. It interns to the same
        -1 as "never written", so without the set mask these two would be
        indistinguishable -- which is exactly the bug the mask exists for. */
    s.setValue(1, G::TitleColumn, Qt::EditRole, QString());
    QVERIFY(s.value(1, G::TitleColumn).isValid());
    QCOMPARE(s.value(1, G::TitleColumn).toString(), QString());
}

void tst_imagerow::conditionalColumnsStayUnsetUntilWritten()
{
    /*  The columns that made the mask necessary: Duration is set only for
        video, IconAspectRatio only once an icon has been decoded, Err only on a
        row whose metadata failed. */
    RowStore s;
    s.resize(2);
    fill(s, 0, "/stills/DSC_0001.jpg");
    fill(s, 1, "/clips/MVI_0002.mov");

    QVERIFY(!s.value(0, G::DurationColumn).isValid());
    QVERIFY(!s.value(0, G::IconAspectRatioColumn).isValid());
    QVERIFY(!s.value(0, G::ErrColumn).isValid());

    s.setValue(1, G::DurationColumn, Qt::EditRole, QString("00:14"));
    s.setValue(1, G::IconAspectRatioColumn, Qt::EditRole, qreal(1.7777));
    s.setValue(1, G::ErrColumn, Qt::EditRole, QStringList{ "no embedded thumb" });

    QCOMPARE(s.value(1, G::DurationColumn).toString(), QString("00:14"));
    QCOMPARE(s.value(1, G::IconAspectRatioColumn).toDouble(), 1.7777);
    QCOMPARE(s.value(1, G::ErrColumn).toStringList(), QStringList{ "no embedded thumb" });
    // the still is untouched by any of it
    QVERIFY(!s.value(0, G::DurationColumn).isValid());
}

void tst_imagerow::availabilityRoundTripsAndDefaultsToPresent()
{
    /*  Catalog::Availability, held as an int: 0 Present, 1 Offline, 2 Missing.
        The icon delegate reads it once per paint and draws nothing for 0, which
        is every folder-scope row and most catalog rows -- so the default has to
        BE Present, not merely usually be. */
    RowStore s;
    s.resize(2);
    fill(s, 0, "/a/present.jpg");
    fill(s, 1, "/b/offline.jpg");

    /*  Unwritten reads back invalid, and toInt() on an invalid QVariant is 0 --
        which is Present. That is the delegate's fast path and it must stay
        true: a row nobody has asked about is not "unknown", it is openable
        until something says otherwise. */
    QVERIFY(!s.value(0, G::AvailabilityColumn).isValid());
    QCOMPARE(s.value(0, G::AvailabilityColumn).toInt(), 0);

    s.setValue(1, G::AvailabilityColumn, Qt::EditRole, int(1));   // Offline
    QCOMPARE(s.value(1, G::AvailabilityColumn).toInt(), 1);
    QVERIFY(s.value(1, G::AvailabilityColumn).typeId() != QMetaType::QString);
    // and the other row is untouched
    QCOMPARE(s.value(0, G::AvailabilityColumn).toInt(), 0);

    s.setValue(1, G::AvailabilityColumn, Qt::EditRole, int(2));   // Missing
    QCOMPARE(s.value(1, G::AvailabilityColumn).toInt(), 2);
}

void tst_imagerow::numericColumnsComeBackNumeric()
{
    RowStore s;
    s.resize(1);
    s.setValue(0, G::ShutterspeedColumn, Qt::EditRole, double(0.004));
    s.setValue(0, G::ApertureColumn, Qt::EditRole, double(5.6));
    s.setValue(0, G::ISOColumn, Qt::EditRole, int(400));
    s.setValue(0, G::FocalLengthColumn, Qt::EditRole, int(400));
    s.setValue(0, G::OrientationColumn, Qt::EditRole, int(6));
    s.setValue(0, G::PermissionsColumn, Qt::EditRole, uint(0x6666));
    s.setValue(0, G::FocusXColumn, Qt::EditRole, float(0.5f));

    /*  ExposureTimeItemDelegate guards "value == 0" before computing 1/value.
        A QString holding "0.004" is not equal to 0, so it walks past the guard
        -- and an unset one reaches qRound(+Inf), which is a debug ASSERT. This
        is the case that aborted the app the first time the reads moved across. */
    QVERIFY(s.value(0, G::ShutterspeedColumn).typeId() != QMetaType::QString);
    QVERIFY(s.value(0, G::ApertureColumn).typeId()     != QMetaType::QString);
    QVERIFY(s.value(0, G::ISOColumn).typeId()          != QMetaType::QString);
    QVERIFY(s.value(0, G::OrientationColumn).typeId()  != QMetaType::QString);
    QVERIFY(s.value(0, G::PermissionsColumn).typeId()  != QMetaType::QString);
    QCOMPARE(s.value(0, G::PermissionsColumn).toUInt(), 0x6666u);
    QCOMPARE(s.value(0, G::FocusXColumn).toFloat(), 0.5f);

    /*  And the reverse, which the shadow check also caught twice: Rating and
        MPix LOOK numeric and are held as TEXT. An unrated image has an EMPTY
        rating, which is a different thing from a rating of 0 -- the Filters
        category list shows it as a blank row, not a "0" row -- and MPix is
        QString::number(mp, 'f', 2), so holding a float and formatting on the
        way out returns 24.15999984741211 where the model says 24.16. */
    s.setValue(0, G::RatingColumn, Qt::EditRole, QString());
    s.setValue(0, G::MegaPixelsColumn, Qt::EditRole, QString("24.16"));
    QCOMPARE(s.value(0, G::RatingColumn).typeId(), int(QMetaType::QString));
    QCOMPARE(s.value(0, G::RatingColumn).toString(), QString());
    QCOMPARE(s.value(0, G::MegaPixelsColumn).toString(), QString("24.16"));
}

void tst_imagerow::internedRepeatsCollapse()
{
    RowStore s;
    const int n = 500;
    s.resize(n);
    for (int r = 0; r < n; ++r) {
        s.setValue(r, G::FolderNameColumn, Qt::EditRole, "2026-08-28 Heron");
        s.setValue(r, G::CameraModelColumn, Qt::EditRole, "NIKON Z 9");
        s.setValue(r, G::LensColumn, Qt::EditRole, "NIKKOR Z 400mm f/4.5");
        s.setValue(r, G::ShootingInfoColumn, Qt::EditRole, "1/1250 sec at f/5.6, ISO 800");
    }
    s.setValue(0, G::CameraModelColumn, Qt::EditRole, "NIKON Z 8");

    /*  A shoot shares these; that is the whole reason they are interned rather
        than held per row, and it is what turns 4.6 GB into 137 MB. */
    QCOMPARE(s.strings().distinctCount(), 5);
    QCOMPARE(s.value(499, G::LensColumn).toString(), QString("NIKKOR Z 400mm f/4.5"));
    QCOMPARE(s.value(0, G::CameraModelColumn).toString(), QString("NIKON Z 8"));
    QCOMPARE(s.value(1, G::CameraModelColumn).toString(), QString("NIKON Z 9"));
}

void tst_imagerow::insertRowsShiftsTheRowsAfterIt()
{
    /*  Rows are inserted in the MIDDLE (insertFiles), which re-addresses every
        row after them. This used to be handled by rebuilding the store from the
        QStandardItems; there are no items now, so it splices -- and a plain
        resize() could never be right, because it appends at the END. */
    RowStore s;
    s.resize(3);
    fill(s, 0, "/a.jpg"); fill(s, 1, "/b.jpg"); fill(s, 2, "/c.jpg");

    /*  insertRows GROWS the store itself, matching what the model has already
        done by the time rowsInserted fires -- so no resize() beforehand. */
    s.insertRows(1, 2);
    QCOMPARE(s.size(), 5);
    QCOMPARE(s.value(0, G::PathColumn, G::PathRole).toString(), QString("/a.jpg"));
    // the two new rows are blank, not copies
    QVERIFY(!s.value(1, G::PathColumn, G::PathRole).isValid());
    QVERIFY(!s.value(2, G::PathColumn, G::PathRole).isValid());
    QCOMPARE(s.value(3, G::PathColumn, G::PathRole).toString(), QString("/b.jpg"));
    QCOMPARE(s.value(4, G::PathColumn, G::PathRole).toString(), QString("/c.jpg"));
}

void tst_imagerow::removeRowsSplicesRatherThanTruncates()
{
    RowStore s;
    s.resize(4);
    fill(s, 0, "/a.jpg"); fill(s, 1, "/b.jpg");
    fill(s, 2, "/c.jpg"); fill(s, 3, "/d.jpg");

    s.removeRows(1, 2);
    QCOMPARE(s.size(), 2);
    QCOMPARE(s.value(0, G::PathColumn, G::PathRole).toString(), QString("/a.jpg"));
    /*  If this said "/b.jpg" the store would have truncated from the end and
        every surviving row past the deletion point would describe a different
        image -- silently. */
    QCOMPARE(s.value(1, G::PathColumn, G::PathRole).toString(), QString("/d.jpg"));

    // a removal running off the end clamps rather than corrupting
    s.removeRows(1, 99);
    QCOMPARE(s.size(), 1);
    QCOMPARE(s.value(0, G::PathColumn, G::PathRole).toString(), QString("/a.jpg"));
}

void tst_imagerow::compactDropsScatteredRowsInOrder()
{
/*
    DataModel::removeFiles compacts a scattered multi-file delete in one pass. Survivors
    keep their order, and newRow of the wrong size changes nothing.
*/
    RowStore s;
    s.resize(5);
    fill(s, 0, "/a.jpg"); fill(s, 1, "/b.jpg"); fill(s, 2, "/c.jpg");
    fill(s, 3, "/d.jpg"); fill(s, 4, "/e.jpg");

    s.compact({-1, -1});                    // wrong size: refused
    QCOMPARE(s.size(), 5);

    s.compact({0, -1, 1, -1, 2});           // drop b and d
    QCOMPARE(s.size(), 3);
    QCOMPARE(s.value(0, G::PathColumn, G::PathRole).toString(), QString("/a.jpg"));
    QCOMPARE(s.value(1, G::PathColumn, G::PathRole).toString(), QString("/c.jpg"));
    QCOMPARE(s.value(2, G::PathColumn, G::PathRole).toString(), QString("/e.jpg"));
}

void tst_imagerow::pickedCountStaysExact()
{
/*
    RowStore::pickedCount replaced a walk of every row, so nothing re-derives it: it has
    to stay exact through every write, splice and removal on its own, or isAnyPick and
    the status bar drift silently. pickGeneration must move whenever the answer over a
    SUBSET could have changed, because MW::getPicked caches its filtered count on it.
*/
    auto pick = [](RowStore &s, int row, const QString &v) {
        s.setValue(row, G::PickColumn, Qt::EditRole, v);
    };
    RowStore s;
    s.resize(6);
    QCOMPARE(s.pickedCount(), 0);

    quint64 g = s.pickGeneration();
    pick(s, 0, "Picked"); pick(s, 2, "Picked"); pick(s, 4, "Picked");
    QCOMPARE(s.pickedCount(), 3);
    QVERIFY(s.pickGeneration() != g);

    // re-writing the same value, or another non-pick value, changes nothing
    g = s.pickGeneration();
    pick(s, 0, "Picked");
    pick(s, 1, "Rejected");
    QCOMPARE(s.pickedCount(), 3);
    QCOMPARE(s.pickGeneration(), g);

    // unpick, and the ingest path's "Ingested"
    pick(s, 2, "Unpicked");
    pick(s, 4, "Ingested");
    QCOMPARE(s.pickedCount(), 1);
    pick(s, 2, "Picked"); pick(s, 4, "Picked");            // rows 0, 2, 4 picked

    // inserting blank rows adds no picks but moves the generation
    g = s.pickGeneration();
    s.insertRows(1, 2);                                     // picks now at 0, 4, 6
    QCOMPARE(s.pickedCount(), 3);
    QVERIFY(s.pickGeneration() != g);

    s.removeRows(3, 2);                                     // drops 3 and 4 -> picks 0, 4
    QCOMPARE(s.pickedCount(), 2);

    s.compact({0, 1, 2, 3, -1, 4});                         // 6 rows; drop row 4 (picked)
    QCOMPARE(s.pickedCount(), 1);
    QCOMPARE(s.value(0, G::PickColumn).toString(), QString("Picked"));

    s.resize(0);                                            // truncation uncounts
    QCOMPARE(s.pickedCount(), 0);

    s.resize(2);
    pick(s, 1, "Picked");
    s.clear();
    QCOMPARE(s.pickedCount(), 0);
}

void tst_imagerow::forEachRowMatchesValue()
{
/*
    BuildFilters::makeSnapshot reads the whole model through forEachRow instead of
    DataModel::data, on the promise that each cell is EXACTLY what value() returns --
    including invalid for an unset cell, which the snapshot turns into "" and a filter
    category shows as a blank item rather than "0" or "false".
*/
    RowStore s;
    s.resize(3);
    fill(s, 0, "/a.jpg");
    s.setValue(0, G::RatingColumn, Qt::EditRole, "3");
    s.setValue(0, G::ISOColumn, Qt::EditRole, 800);
    s.setValue(0, G::SearchColumn, Qt::EditRole, true);
    s.setValue(0, G::KeywordsAllColumn, Qt::EditRole, QStringList{"a", "b"});
    s.setValue(0, G::PathColumn, G::DupHideRawRole, true);
    fill(s, 2, "/c.jpg");                                   // row 1 left unwritten

    const QVector<QPair<int, int>> cells = {
        {G::RatingColumn, Qt::DisplayRole}, {G::ISOColumn, Qt::DisplayRole},
        {G::SearchColumn, Qt::DisplayRole}, {G::KeywordsAllColumn, Qt::DisplayRole},
        {G::PathColumn, G::DupHideRawRole}, {G::TypeColumn, Qt::DisplayRole},
    };
    /*  Collected, then compared: the callback may not call back into the store. */
    QVector<QVector<QVariant>> got;
    s.forEachRow(cells, [&](int row, const QVariant *val) {
        QCOMPARE(row, got.size());
        got.append(QVector<QVariant>(val, val + cells.size()));
    });
    QCOMPARE(got.size(), 3);
    for (int row = 0; row < got.size(); ++row)
        for (int i = 0; i < cells.size(); ++i)
            QCOMPARE(got[row][i], s.value(row, cells[i].first, cells[i].second));
    QVERIFY(!got[1][0].isValid());                          // unset stays unset
}

void tst_imagerow::watchedGenerationMovesOnlyForWatchedCells()
{
/*
    BuildFilters::makeSnapshot reuses its copy of the counted columns while this holds
    still, so a MISSED move is a stale filter count. It must move for a watched cell
    (including a path-column role, DupHideRaw) and for any change to the row set --
    and must NOT move for the icon and bookkeeping writes that run constantly while
    browsing, or the cache never survives to be reused.
*/
    RowStore s;
    s.resize(3);
    s.setWatchedCells({{G::RatingColumn, Qt::DisplayRole},
                       {G::PathColumn, G::DupHideRawRole}});
    quint64 g = s.watchedGeneration();

    // unwatched: the icon rect, a non-counted column, the path itself
    s.setValue(0, G::PathColumn, G::IconRectRole, QRect(0, 0, 10, 10));
    s.setValue(0, G::IconLoadedColumn, Qt::EditRole, true);
    s.setValue(0, G::PathColumn, G::PathRole, "/a.jpg");
    QCOMPARE(s.watchedGeneration(), g);

    // setting the SAME watch set again is not a change
    s.setWatchedCells({{G::RatingColumn, Qt::DisplayRole},
                       {G::PathColumn, G::DupHideRawRole}});
    QCOMPARE(s.watchedGeneration(), g);

    // watched: a rating (EditRole writes the same field DisplayRole reads)
    s.setValue(1, G::RatingColumn, Qt::EditRole, "4");
    QVERIFY(s.watchedGeneration() != g);
    g = s.watchedGeneration();
    s.setValue(2, G::PathColumn, G::DupHideRawRole, true);
    QVERIFY(s.watchedGeneration() != g);

    // the row set
    g = s.watchedGeneration(); s.insertRows(1, 1);   QVERIFY(s.watchedGeneration() != g);
    g = s.watchedGeneration(); s.removeRows(0, 1);   QVERIFY(s.watchedGeneration() != g);
    g = s.watchedGeneration(); s.compact({0, -1, 1}); QVERIFY(s.watchedGeneration() != g);
    g = s.watchedGeneration(); s.resize(5);          QVERIFY(s.watchedGeneration() != g);
    g = s.watchedGeneration(); s.clear();            QVERIFY(s.watchedGeneration() != g);
}

void tst_imagerow::watchedChangesNameTheRowsUntilTheyCannot()
{
/*
    BuildFilters::makeSnapshot re-reads only the rows this names. A row missing from it
    is a filter count that never updates; a structural change NOT flagged is a patch
    written to the wrong row. So: watched writes are listed, unwatched ones are not,
    taking clears, and anything that renumbers rows -- or too many rows to list --
    says structural instead.
*/
    RowStore s;
    s.resize(10);
    s.setWatchedCells({{G::RatingColumn, Qt::DisplayRole}});
    RowStore::WatchedChanges c = s.takeWatchedChanges();
    QVERIFY(c.structural);                                  // set changed / first take

    c = s.takeWatchedChanges();                             // nothing since
    QVERIFY(!c.structural);
    QVERIFY(c.rows.isEmpty());

    s.setValue(3, G::RatingColumn, Qt::EditRole, "5");
    s.setValue(7, G::RatingColumn, Qt::EditRole, "2");
    s.setValue(5, G::IconLoadedColumn, Qt::EditRole, true);  // not watched
    c = s.takeWatchedChanges();
    QVERIFY(!c.structural);
    QCOMPARE(c.rows, (QVector<int>{3, 7}));

    QVERIFY(s.takeWatchedChanges().rows.isEmpty());          // taking cleared it

    s.setValue(1, G::RatingColumn, Qt::EditRole, "1");
    s.removeRows(0, 1);                                     // renumbers: the 1 is stale
    c = s.takeWatchedChanges();
    QVERIFY(c.structural);
    QVERIFY(c.rows.isEmpty());

    s.resize(6000);
    s.takeWatchedChanges();
    for (int r = 0; r < 5000; ++r) s.setValue(r, G::RatingColumn, Qt::EditRole, "3");
    c = s.takeWatchedChanges();
    QVERIFY(c.structural);                                  // past the cap: rebuild
}

void tst_imagerow::fieldGenerationMovesForItsFieldAndSplicesOnly()
{
/*
    The proxy snapshot's path table and SortFilter's sort ranks are reused while this
    holds still. Missing a move is a stale path or a wrong sort; moving for other fields
    only costs a rebuild, but would defeat the cache while thumbnails load.
*/
    RowStore s;
    s.resize(3);
    const auto gen = [&] { return s.fieldGeneration(G::PathColumn, G::PathRole); };
    quint64 g = gen();
    s.setValue(0, G::RatingColumn, Qt::EditRole, "3");            // another field
    s.setValue(0, G::PathColumn, G::IconRectRole, QRect(0, 0, 1, 1)); // another role
    QCOMPARE(gen(), g);
    s.setValue(1, G::PathColumn, G::PathRole, "/b.jpg");          // a rename
    QVERIFY(gen() != g);
    g = gen(); s.insertRows(0, 1);  QVERIFY(gen() != g);
    g = gen(); s.removeRows(0, 1);  QVERIFY(gen() != g);
    g = gen(); s.clear();           QVERIFY(gen() != g);
    QCOMPARE(s.fieldGeneration(G::PathColumn, Qt::ToolTipRole), quint64(0)); // not held
}

void tst_imagerow::prefixExpansionStaysInBudget()
{
/*
    WHAT PREFIX EXPANSION COSTS A ROW, MEASURED RATHER THAN ARGUED.

    Keyword identity is becoming the full PATH, and the column the filters run on holds
    every ANCESTOR PREFIX of every path -- so a row carrying five keywords carries about
    eleven entries instead of five. That is the price of leaving filterpredicate.h's
    QStringList::contains and the catalog's indexed equality join untouched, and it is
    only worth paying if it is small.

    THE GATE IS THE DELTA, NOT THE ABSOLUTE, and that is a deliberate choice made after
    measuring both. The absolute is dominated by two things this change does not control:
    sizeof(ImageRow) is 472 bytes on its own, and QVector over-allocates, so resize(20000)
    reserves capacity for ~35,500 rows and roughly 366 bytes a row of the total is spare
    capacity the store really has allocated but is not using. Gating the absolute would
    mean gating those, and a red line drawn round them would fire on an unrelated change
    and say nothing true about keywords. The two builds below differ ONLY in whether the
    keyword column is expanded, so everything else cancels.

    (Note for anyone reading rowfields.h: its 573-bytes-a-row figure is STALE, from the
    prototype, and predates the second and third batches of columns. Measured here a
    populated row is ~1,050 bytes including that spare capacity, ~600 without it. That is
    a pre-existing fact about the row, not something keywords did, and it is still an
    order of magnitude under the ~19,900 the QStandardItems cost.)

    MEASURED: expansion costs SIX bytes a row -- far less than the arithmetic suggested,
    because siblings share ancestors (five leaves expand to ~11 distinct prefixes, not
    15) and because the ids land inside a QVector block the row had already rounded up
    to. The red line is 64, an order of magnitude of headroom, so it fires on a real
    regression and not on allocator noise.

    If it is ever breached the escape hatch is to drop keywordIds (the literal dc:subject
    list) from the row entirely: it is needed one image at a time at write time and is
    re-readable from the model column, which reclaims more than the expansion costs. Do
    not spend that pre-emptively.
*/
    const int kRows = 20000;
    const size_t kMaxExpansionBytesPerRow = 64;

    /*  A synthetic vocabulary shaped like a real one: 400 paths, mean depth about 3.2,
        names that repeat across the library exactly as a shoot's do. */
    QStringList vocab;
    const QStringList roots = {"Fauna", "Flora", "Location", "Category", "Activity"};
    for (const QString &root : roots) {
        for (int b = 0; b < 8; ++b) {
            const QString branch = root + "|Branch" + QString::number(b);
            vocab << branch;
            for (int l = 0; l < 9; ++l)
                vocab << branch + "|Leaf" + QString::number(l);
        }
    }
    QCOMPARE(vocab.size(), 400);

    /*  Identical in every respect except the keyword column, so the difference between
        the two returns is the cost of expansion and nothing else. */
    auto measure = [&](bool expand, int *distinctStrings) {
        RowStore s;
        s.resize(kRows);
        for (int row = 0; row < kRows; ++row) {
            const QString path = QString("/Volumes/Photos/2024/shoot/DSC_%1.NEF")
                                     .arg(row, 6, 10, QChar('0'));
            s.setValue(row, G::PathColumn, G::PathRole, path);
            s.setValue(row, G::NameColumn, Qt::EditRole, path.section('/', -1));
            s.setValue(row, G::TypeColumn, Qt::EditRole, "NEF");
            s.setValue(row, G::TitleColumn, Qt::EditRole, "A title for every row");

            QStringList assigned;
            for (int k = 0; k < 5; ++k)
                assigned << vocab.at((row * 5 + k) % vocab.size());

            s.setValue(row, G::KeywordsAllColumn, Qt::EditRole,
                       expand ? keywordPrefixExpand(assigned) : assigned);
            s.setValue(row, G::KeywordsColumn, Qt::EditRole, assigned);
        }
        if (distinctStrings) *distinctStrings = s.strings().distinctCount();
        return s.approxBytes() / size_t(kRows);
    };

    int distinctFlat = 0, distinctExpanded = 0;
    const size_t flat     = measure(false, &distinctFlat);
    const size_t expanded = measure(true,  &distinctExpanded);
    const size_t delta    = (expanded > flat) ? expanded - flat : 0;

    /*  Reported unconditionally, not only on failure: the numbers are the point of the
        case, and a silent pass tells the next person nothing about which way they
        moved. */
    qInfo() << "bytes/row  flat:" << flat << " expanded:" << expanded
            << " delta:" << delta
            << "| interned strings  flat:" << distinctFlat
            << " expanded:" << distinctExpanded;

    QVERIFY2(delta < kMaxExpansionBytesPerRow,
             qPrintable(QString("prefix expansion costs %1 bytes/row, ceiling %2 -- read "
                                "the comment above before moving the ceiling")
                            .arg(delta).arg(kMaxExpansionBytesPerRow)));

    /*  THE INTERNER MUST SCALE WITH THE VOCABULARY, NOT THE ROW COUNT. Expansion adds the
        ancestor paths as strings of their own, so this number grows -- but it must stay a
        function of how many distinct paths exist, not of how many images carry them. If
        it ever approached kRows the interning would have broken and the per-row figures
        above would be measuring the wrong thing entirely. */
    QVERIFY(distinctExpanded > distinctFlat);
    QVERIFY2(distinctExpanded < kRows / 10,
             "interned strings must scale with the vocabulary, not the row count");
}

void tst_imagerow::folderPathsAllIsDerivedFromThePath()
{
/*
    G::FolderPathsAllColumn is what the Filters Folders category filters on: the folder
    and every folder above it, so a checked parent is an exact QStringList::contains. It
    is DERIVED -- written through G::FolderPathColumn and never on its own.
*/
    RowStore s;
    s.resize(3);
    QVERIFY(RowStore::covers(G::FolderPathColumn, Qt::EditRole));
    QVERIFY(RowStore::covers(G::FolderPathsAllColumn, Qt::EditRole));

    // unset reads back invalid, like every other conditional column
    QVERIFY(!s.value(0, G::FolderPathColumn).isValid());
    QVERIFY(!s.value(0, G::FolderPathsAllColumn).isValid());

    // written with a trailing slash: stored in the one spelling
    s.setValue(0, G::FolderPathColumn, Qt::EditRole, "/U/Photos/2020/DxO/");
    QCOMPARE(s.value(0, G::FolderPathColumn).toString(), QString("/U/Photos/2020/DxO"));
    QCOMPARE(s.value(0, G::FolderPathsAllColumn).toStringList(),
             QStringList({"/", "/U", "/U/Photos", "/U/Photos/2020",
                          "/U/Photos/2020/DxO"}));

    // two folders with the same NAME stay two folders
    s.setValue(1, G::FolderPathColumn, Qt::EditRole, "/U/Photos/2021/DxO");
    const QStringList other = s.value(1, G::FolderPathsAllColumn).toStringList();
    QVERIFY(!other.contains("/U/Photos/2020/DxO"));
    QVERIFY(other.contains("/U/Photos"));

    // the derived column refuses a write: no bit set, nothing stored
    s.setValue(2, G::FolderPathsAllColumn, Qt::EditRole, QStringList({"/nonsense"}));
    QVERIFY(!s.value(2, G::FolderPathColumn).isValid());
    QVERIFY(!s.value(2, G::FolderPathsAllColumn).isValid());

    // clear() drops the ancestry cache with the strings it is keyed on
    s.clear();
    s.resize(1);
    s.setValue(0, G::FolderPathColumn, Qt::EditRole, "/Z");
    QCOMPARE(s.value(0, G::FolderPathsAllColumn).toStringList(), QStringList({"/", "/Z"}));
}

void tst_imagerow::folderAncestryCostsARowOneId()
{
/*
    The ancestry is held ONCE PER FOLDER and shared by the rows in it, so the folder-path
    column must cost a row about the qint32 it adds -- not a list of ancestors each. The
    ceiling (16 bytes) is loose enough for padding and strict enough to fail if the
    ancestry ever lands on the row.
*/
    const int kRows = 20000;
    const size_t kMaxBytesPerRow = 16;
    auto measure = [&](bool withFolder) {
        RowStore s;
        s.resize(kRows);
        for (int row = 0; row < kRows; ++row) {
            const QString folder = QString("/Volumes/Photos/2024/shoot%1").arg(row / 250);
            s.setValue(row, G::PathColumn, G::PathRole,
                       folder + QString("/DSC_%1.NEF").arg(row, 6, 10, QChar('0')));
            s.setValue(row, G::TypeColumn, Qt::EditRole, "NEF");
            if (withFolder) s.setValue(row, G::FolderPathColumn, Qt::EditRole, folder);
        }
        return s.approxBytes() / size_t(kRows);
    };
    const size_t without = measure(false);
    const size_t with    = measure(true);
    const size_t delta   = with > without ? with - without : 0;
    qInfo() << "bytes/row  without folder path:" << without << " with:" << with
            << " delta:" << delta;
    QVERIFY2(delta < kMaxBytesPerRow,
             qPrintable(QString("folder path costs %1 bytes/row, ceiling %2")
                            .arg(delta).arg(kMaxBytesPerRow)));
}

QTEST_MAIN(tst_imagerow)
#include "tst_imagerow.moc"
