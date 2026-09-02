#include <QtTest>

#include "Datamodel/imagerow.h"
#include "Main/global.h"

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
    void searchAndIngestedAreDeliberatelyNotCovered();
    void unwrittenFieldsReadBackAsInvalid();
    void conditionalColumnsStayUnsetUntilWritten();
    void numericColumnsComeBackNumeric();
    void internedRepeatsCollapse();
    void insertRowsShiftsTheRowsAfterIt();
    void removeRowsSplicesRatherThanTruncates();

private:
    static void fill(RowStore &s, int row, const QString &path)
    {
        s.setValue(row, G::PathColumn, path);
        s.setValue(row, G::NameColumn, path.section('/', -1));
        s.setValue(row, G::TypeColumn, "JPG");
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

void tst_imagerow::searchAndIngestedAreDeliberatelyNotCovered()
{
    /*  Both hold a different TYPE depending on which path wrote them last --
        "false" (QString) at row creation, a bool once the work runs, "true"
        (QString) from a third path. A store with one type per column cannot
        reproduce that and must not pretend to. Settling them decides what the
        FILTER matches, so it belongs with the compiled predicate, not here.
        This case exists so that "not covered" stays a decision rather than an
        oversight someone quietly fixes. */
    QVERIFY(!RowStore::covers(G::SearchColumn, Qt::EditRole));
    QVERIFY(!RowStore::covers(G::IngestedColumn, Qt::EditRole));
}

void tst_imagerow::unwrittenFieldsReadBackAsInvalid()
{
    RowStore s;
    s.resize(3);
    fill(s, 1, "/a/b/c.jpg");

    QCOMPARE(s.value(1, G::PathColumn).toString(), QString("/a/b/c.jpg"));
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
    s.setValue(1, G::TitleColumn, QString());
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

    s.setValue(1, G::DurationColumn, QString("00:14"));
    s.setValue(1, G::IconAspectRatioColumn, qreal(1.7777));
    s.setValue(1, G::ErrColumn, QStringList{ "no embedded thumb" });

    QCOMPARE(s.value(1, G::DurationColumn).toString(), QString("00:14"));
    QCOMPARE(s.value(1, G::IconAspectRatioColumn).toDouble(), 1.7777);
    QCOMPARE(s.value(1, G::ErrColumn).toStringList(), QStringList{ "no embedded thumb" });
    // the still is untouched by any of it
    QVERIFY(!s.value(0, G::DurationColumn).isValid());
}

void tst_imagerow::numericColumnsComeBackNumeric()
{
    RowStore s;
    s.resize(1);
    s.setValue(0, G::ShutterspeedColumn, double(0.004));
    s.setValue(0, G::ApertureColumn, double(5.6));
    s.setValue(0, G::ISOColumn, int(400));
    s.setValue(0, G::FocalLengthColumn, int(400));
    s.setValue(0, G::OrientationColumn, int(6));
    s.setValue(0, G::PermissionsColumn, uint(0x6666));
    s.setValue(0, G::FocusXColumn, float(0.5f));

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
    s.setValue(0, G::RatingColumn, QString());
    s.setValue(0, G::MegaPixelsColumn, QString("24.16"));
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
        s.setValue(r, G::FolderNameColumn, "2026-08-28 Heron");
        s.setValue(r, G::CameraModelColumn, "NIKON Z 9");
        s.setValue(r, G::LensColumn, "NIKKOR Z 400mm f/4.5");
        s.setValue(r, G::ShootingInfoColumn, "1/1250 sec at f/5.6, ISO 800");
    }
    s.setValue(0, G::CameraModelColumn, "NIKON Z 8");

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
    QCOMPARE(s.value(0, G::PathColumn).toString(), QString("/a.jpg"));
    // the two new rows are blank, not copies
    QVERIFY(!s.value(1, G::PathColumn).isValid());
    QVERIFY(!s.value(2, G::PathColumn).isValid());
    QCOMPARE(s.value(3, G::PathColumn).toString(), QString("/b.jpg"));
    QCOMPARE(s.value(4, G::PathColumn).toString(), QString("/c.jpg"));
}

void tst_imagerow::removeRowsSplicesRatherThanTruncates()
{
    RowStore s;
    s.resize(4);
    fill(s, 0, "/a.jpg"); fill(s, 1, "/b.jpg");
    fill(s, 2, "/c.jpg"); fill(s, 3, "/d.jpg");

    s.removeRows(1, 2);
    QCOMPARE(s.size(), 2);
    QCOMPARE(s.value(0, G::PathColumn).toString(), QString("/a.jpg"));
    /*  If this said "/b.jpg" the store would have truncated from the end and
        every surviving row past the deletion point would describe a different
        image -- silently. */
    QCOMPARE(s.value(1, G::PathColumn).toString(), QString("/d.jpg"));

    // a removal running off the end clamps rather than corrupting
    s.removeRows(1, 99);
    QCOMPARE(s.size(), 1);
    QCOMPARE(s.value(0, G::PathColumn).toString(), QString("/a.jpg"));
}

QTEST_MAIN(tst_imagerow)
#include "tst_imagerow.moc"
