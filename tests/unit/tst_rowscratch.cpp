#include <QtTest>

#include "Datamodel/rowscratch.h"
#include "Main/global.h"

/*
    THE SCRATCH STORE MUST BEHAVE LIKE THE ITEMS IT REPLACES.

    Datamodel/rowscratch.h takes the ~24 in-flight columns off every row and
    puts them in a hash keyed by row, so a row nothing has decoded or cached
    costs nothing. That is only a safe swap if the store answers a read exactly
    as an unset or set QStandardItem would, and the one that matters is the
    UNSET answer: QStandardItemModel::data returns an invalid QVariant for a
    cell nobody wrote, and callers branch on it. A store returning the field's
    zero instead would turn "no offset yet" into "offset 0", which is a legal
    offset and would fail silently, in a decoder, on some files only.

    These cases pin that, plus the type each column comes back as (a delegate
    branches on text-vs-number, see verifyRowStore) and the interning that keeps
    an ICC profile from being paid for once per row.
*/
class tst_rowscratch : public QObject
{
    Q_OBJECT

private slots:
    void coversTheScratchColumnsAndOnlyOnValueRoles();
    void doesNotCoverResidentColumns();
    void unwrittenFieldsReadBackAsInvalid();
    void untouchedRowsHaveNoEntry();
    void valuesRoundTripWithTheirType();
    void ifdOffsetsRoundTripAsAVariantList();
    void iccBuffersAreInternedNotCopiedPerRow();
    void clearEmptiesEverything();
};

void tst_rowscratch::coversTheScratchColumnsAndOnlyOnValueRoles()
{
    const QVector<int> scratch {
        G::OffsetFullColumn, G::LengthFullColumn,
        G::WidthOrigPreviewColumn, G::HeightOrigPreviewColumn,
        G::OffsetThumbColumn, G::LengthThumbColumn,
        G::samplesPerPixelColumn, G::isBigEndianColumn,
        G::ifd0OffsetColumn, G::ifdOffsetsColumn,
        G::XmpSegmentOffsetColumn, G::XmpSegmentLengthColumn, G::IsXMPColumn,
        G::ICCSegmentOffsetColumn, G::ICCSegmentLengthColumn,
        G::ICCBufColumn, G::ICCSpaceColumn,
        G::CacheSizeColumn, G::IsCachingColumn, G::IsCachedColumn,
        G::AttemptsColumn, G::DecoderIdColumn,
        G::DecoderReturnStatusColumn, G::DecoderErrMsgColumn
    };
    for (int c : scratch) {
        QVERIFY2(ScratchStore::covers(c, Qt::EditRole),
                 qPrintable(QString("column %1 not covered at EditRole").arg(c)));
        QVERIFY(ScratchStore::covers(c, Qt::DisplayRole));
        /*  Presentation roles stay with the items -- a QStandardItem keeps
            Edit and Display in one slot and everything else in others. */
        QVERIFY(!ScratchStore::covers(c, Qt::TextAlignmentRole));
        QVERIFY(!ScratchStore::covers(c, Qt::ToolTipRole));
        QVERIFY(!ScratchStore::covers(c, G::PathRole));
    }
}

void tst_rowscratch::doesNotCoverResidentColumns()
{
    /*  The two stores must never claim the same column: verifyRowStore picks
        which one to compare against by asking both, so an overlap would leave
        one of them unchecked. */
    const QVector<int> resident {
        G::PathColumn, G::NameColumn, G::TypeColumn, G::RatingColumn,
        G::ApertureColumn, G::KeywordsColumn, G::MetadataStatusColumn,
        G::IconLoadedColumn, G::SearchTextColumn
    };
    for (int c : resident)
        QVERIFY2(!ScratchStore::covers(c, Qt::EditRole),
                 qPrintable(QString("column %1 claimed by both stores").arg(c)));
}

void tst_rowscratch::unwrittenFieldsReadBackAsInvalid()
{
    ScratchStore s;
    // A row that exists because ONE field was written.
    s.setValue(7, G::IsCachingColumn, true);
    QVERIFY(s.contains(7));
    QCOMPARE(s.value(7, G::IsCachingColumn).toBool(), true);

    /*  Every other field of that same row is still unset, and must read as
        nothing rather than as zero. */
    QVERIFY(!s.value(7, G::OffsetFullColumn).isValid());
    QVERIFY(!s.value(7, G::AttemptsColumn).isValid());
    QVERIFY(!s.value(7, G::DecoderErrMsgColumn).isValid());

    // ... and zero written EXPLICITLY is a value, not an absence.
    s.setValue(7, G::OffsetFullColumn, quint32(0));
    QVERIFY(s.value(7, G::OffsetFullColumn).isValid());
    QCOMPARE(s.value(7, G::OffsetFullColumn).toUInt(), 0u);
}

void tst_rowscratch::untouchedRowsHaveNoEntry()
{
    ScratchStore s;
    s.setValue(3, G::OffsetFullColumn, quint32(1234));
    QCOMPARE(s.count(), 1);
    // Reading a row nobody wrote must not conjure an entry for it.
    QVERIFY(!s.value(99, G::OffsetFullColumn).isValid());
    QCOMPARE(s.count(), 1);
    // Nor must a write to a column this store does not hold.
    s.setValue(99, G::NameColumn, "not mine");
    QCOMPARE(s.count(), 1);

    s.remove(3);
    QCOMPARE(s.count(), 0);
    QVERIFY(!s.contains(3));
}

void tst_rowscratch::valuesRoundTripWithTheirType()
{
    ScratchStore s;
    const int r = 2;
    s.setValue(r, G::OffsetFullColumn,          quint32(4294967295u));
    s.setValue(r, G::LengthThumbColumn,         quint32(16384));
    s.setValue(r, G::WidthOrigPreviewColumn,    int(6000));
    s.setValue(r, G::samplesPerPixelColumn,     int(3));
    s.setValue(r, G::isBigEndianColumn,         true);
    s.setValue(r, G::IsXMPColumn,               false);
    s.setValue(r, G::ICCSpaceColumn,            QString("sRGB IEC61966-2.1"));
    s.setValue(r, G::CacheSizeColumn,           float(24.5f));
    s.setValue(r, G::AttemptsColumn,            int(2));
    s.setValue(r, G::DecoderIdColumn,           int(-1));
    s.setValue(r, G::DecoderErrMsgColumn,       QString("length = 0"));

    QCOMPARE(s.value(r, G::OffsetFullColumn).toUInt(), 4294967295u);
    QCOMPARE(s.value(r, G::LengthThumbColumn).toUInt(), 16384u);
    QCOMPARE(s.value(r, G::WidthOrigPreviewColumn).toInt(), 6000);
    QCOMPARE(s.value(r, G::samplesPerPixelColumn).toInt(), 3);
    QCOMPARE(s.value(r, G::isBigEndianColumn).toBool(), true);
    QCOMPARE(s.value(r, G::IsXMPColumn).toBool(), false);
    QCOMPARE(s.value(r, G::ICCSpaceColumn).toString(), QString("sRGB IEC61966-2.1"));
    QCOMPARE(s.value(r, G::CacheSizeColumn).toFloat(), 24.5f);
    QCOMPARE(s.value(r, G::AttemptsColumn).toInt(), 2);
    QCOMPARE(s.value(r, G::DecoderIdColumn).toInt(), -1);
    QCOMPARE(s.value(r, G::DecoderErrMsgColumn).toString(), QString("length = 0"));

    /*  TEXT-VS-NUMBER is what a delegate branches on -- ExposureTimeItemDelegate
        guards "value == 0" before computing 1/value, and a QString holding the
        same digits walks straight past that guard. So the store must hand back
        a number where the model held one, and a string where it held a string. */
    QVERIFY(s.value(r, G::OffsetFullColumn).typeId()    != QMetaType::QString);
    QVERIFY(s.value(r, G::CacheSizeColumn).typeId()     != QMetaType::QString);
    QCOMPARE(s.value(r, G::isBigEndianColumn).typeId(),   int(QMetaType::Bool));
    QCOMPARE(s.value(r, G::DecoderErrMsgColumn).typeId(), int(QMetaType::QString));
    QCOMPARE(s.value(r, G::ICCSpaceColumn).typeId(),      int(QMetaType::QString));

    // Rewriting a field replaces it rather than accumulating.
    s.setValue(r, G::AttemptsColumn, int(0));
    QCOMPARE(s.value(r, G::AttemptsColumn).toInt(), 0);
}

void tst_rowscratch::ifdOffsetsRoundTripAsAVariantList()
{
    ScratchStore s;
    QVariantList in { QVariant(quint32(12)), QVariant(quint32(4096)),
                      QVariant(quint32(65536)) };
    s.setValue(1, G::ifdOffsetsColumn, in);

    const QVariant out = s.value(1, G::ifdOffsetsColumn);
    QCOMPARE(out.typeId(), int(QMetaType::QVariantList));
    const QVariantList l = out.toList();
    QCOMPARE(l.size(), 3);
    QCOMPARE(l.at(0).toUInt(), 12u);
    QCOMPARE(l.at(1).toUInt(), 4096u);
    QCOMPARE(l.at(2).toUInt(), 65536u);

    // An empty list is still a WRITTEN value, distinct from never written.
    s.setValue(1, G::ifdOffsetsColumn, QVariantList());
    QVERIFY(s.value(1, G::ifdOffsetsColumn).isValid());
    QVERIFY(s.value(1, G::ifdOffsetsColumn).toList().isEmpty());
}

void tst_rowscratch::iccBuffersAreInternedNotCopiedPerRow()
{
    ScratchStore s;
    QByteArray profile(2048, '\x7f');
    QByteArray other(1024, '\x01');

    for (int r = 0; r < 500; ++r) s.setValue(r, G::ICCBufColumn, profile);
    s.setValue(500, G::ICCBufColumn, other);

    /*  A shoot shares ONE profile, so 500 rows must hold one copy of it, not
        500. This is the whole reason the blob is interned rather than stored on
        the row: at 2 KB a row it would be 500 MB across a 250,000-row catalog. */
    QCOMPARE(s.blobs().distinctCount(), 2);
    QCOMPARE(s.value(0, G::ICCBufColumn).toByteArray(), profile);
    QCOMPARE(s.value(499, G::ICCBufColumn).toByteArray(), profile);
    QCOMPARE(s.value(500, G::ICCBufColumn).toByteArray(), other);

    // An empty profile is a written value that interns to nothing.
    s.setValue(501, G::ICCBufColumn, QByteArray());
    QCOMPARE(s.blobs().distinctCount(), 2);
    QVERIFY(s.value(501, G::ICCBufColumn).isValid());
    QVERIFY(s.value(501, G::ICCBufColumn).toByteArray().isEmpty());
}

void tst_rowscratch::clearEmptiesEverything()
{
    ScratchStore s;
    s.setValue(0, G::ICCBufColumn, QByteArray(64, 'x'));
    s.setValue(0, G::ICCSpaceColumn, QString("Adobe RGB (1998)"));
    QCOMPARE(s.count(), 1);

    s.clear();
    QCOMPARE(s.count(), 0);
    QCOMPARE(s.blobs().distinctCount(), 0);
    QCOMPARE(s.strings().distinctCount(), 0);
    QVERIFY(!s.value(0, G::ICCBufColumn).isValid());
}

QTEST_MAIN(tst_rowscratch)
#include "tst_rowscratch.moc"
