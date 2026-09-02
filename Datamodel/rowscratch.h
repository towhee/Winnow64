#ifndef ROWSCRATCH_H
#define ROWSCRATCH_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>

#include "Main/global.h"
#include "Datamodel/imagerow.h"     // Interner

/*
    THE SCRATCH STORE -- what is true about an image only while something is
    WORKING on it.

    WHY IT IS SEPARATE FROM THE ROW STORE. Datamodel/rowfields.h draws one
    distinction that the whole 250,000-row catalog rests on: a RESIDENT field is
    needed to list, sort, filter and draw a row, so every row must carry it; a
    SCRATCH field -- a segment offset, an ICC buffer, a decoder id, a cache flag
    -- is needed only while a decoder or the ImageCache is actually on that row.
    Roughly a third of the current 92 columns are scratch. Held as
    QStandardItems they are paid for on every row of the scope regardless, which
    is the single largest slice of the ~19,900 bytes a row measured at.

    So they move to a side table keyed by row rather than into ImageRow. A row
    that nothing has touched has NO ENTRY AT ALL, which is the shape the eventual
    "populated for the rows in flight" policy needs; an entry that exists is a
    few dozen packed bytes instead of two dozen QStandardItems.

    THIS STEP DOES NOT EVICT. Nothing here drops an entry on its own, because
    evicting a segment offset means being able to get it BACK -- a re-read of
    the file's metadata for that row -- and that path belongs with Stage 4's
    gap-filling loader, not with a storage change. What this step buys is the
    storage shape and the per-column arithmetic; eviction then becomes a policy
    added on top of it, in one place, rather than a rewrite.

    UNSET IS A VALUE. QStandardItemModel::data returns an INVALID QVariant for a
    cell nobody wrote, and callers do branch on that. So every field carries a
    bit in setMask and value() returns an invalid QVariant until the bit is set:
    "never written" and "written as 0" stay different facts, exactly as they are
    in the items. Getting this wrong would not have shown up as a crash -- it
    would have shown up as an offset of 0 being treated as a real offset.

    INTERNING, for the two fields that would otherwise dominate. iccBuf is a
    real ICC profile, commonly 500 bytes to 4 KB, and a shoot shares ONE of
    them; iccSpace is a handful of distinct strings across a whole library.
    Both are held once and referenced by a qint32. decoderErrMsg is not
    interned: it is written only on a failure and can embed a row number, so
    interning it would grow a table that is never trimmed in exchange for
    nothing.

    THREADING. GUI thread only, exactly like the QStandardItems it shadows.
    Every worker-thread write arrives through a queued setValDm/setValSf signal.
*/

/*  Blobs, interned. Same shape as Interner (Datamodel/imagerow.h) but keyed on
    QByteArray; kept here rather than templated because these are the only two
    users and one shared template would be harder to read than two short
    classes. */
class BlobInterner
{
public:
    qint32 id(const QByteArray &b)
    {
        if (b.isEmpty()) return -1;
        auto it = mIds.constFind(b);
        if (it != mIds.cend()) return *it;
        mValues.append(b);
        const qint32 n = mValues.size() - 1;
        mIds.insert(b, n);
        return n;
    }
    QByteArray value(qint32 id) const
    {
        return (id >= 0 && id < mValues.size()) ? mValues.at(id) : QByteArray();
    }
    void clear() { mIds.clear(); mValues.clear(); }
    int distinctCount() const { return mValues.size(); }

private:
    QHash<QByteArray, qint32> mIds;
    QVector<QByteArray> mValues;
};

/*  One row's in-flight state. Field order groups the containers, then the
    32-bit scalars, then the bytes, so the padding is not paid twice. */
struct RowScratch
{
    /*  IFD offsets. The model holds QList<QVariant> because that is what a
        QStandardItem cell wanted; they are plain file offsets, so they are held
        as such here and the variant list is rebuilt on the way out. */
    QVector<quint32> ifdOffsets;
    QString decoderErrMsg;

    qint32 iccBufId = -1;              // BlobInterner
    qint32 iccSpaceId = -1;            // Interner

    quint32 offsetFull = 0, lengthFull = 0;
    quint32 offsetThumb = 0, lengthThumb = 0;
    quint32 ifd0Offset = 0;
    quint32 xmpSegmentOffset = 0, xmpSegmentLength = 0;
    quint32 iccSegmentOffset = 0, iccSegmentLength = 0;

    qint32 widthOrigPreview = 0, heightOrigPreview = 0;
    qint32 samplesPerPixel = 0;
    qint32 attempts = 0, decoderId = 0, decoderStatus = 0;

    float  cacheSizeMB = 0.0f;

    /*  Which of the fields above have actually been written. See "UNSET IS A
        VALUE" above -- without this, an unwritten offset reads back as 0 rather
        than as nothing, and 0 is a legal offset. */
    quint32 setMask = 0;

    bool isBigEnd = false;
    bool isXmp = false;
    bool isCaching = false;
    bool isCached = false;
};

class ScratchStore
{
public:
    /*  Which datamodel columns this store answers. Explicit, for the same
        reason RowStore::covers is: a column silently assumed covered is a
        column verifyRowStore quietly stopped checking. */
    static bool covers(int column, int role);

    bool contains(int row) const { return mRows.contains(row); }
    int  count() const { return mRows.size(); }

    QVariant value(int row, int column) const;
    void setValue(int row, int column, const QVariant &v);

    void remove(int row) { mRows.remove(row); }
    void clear() { mRows.clear(); mStrings.clear(); mBlobs.clear(); }

    const Interner &strings() const { return mStrings; }
    const BlobInterner &blobs() const { return mBlobs; }

private:
    QHash<int, RowScratch> mRows;
    Interner mStrings;
    BlobInterner mBlobs;
};

#endif // ROWSCRATCH_H
