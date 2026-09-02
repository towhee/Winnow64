#ifndef IMAGEROW_H
#define IMAGEROW_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QVariant>
#include <QRect>
#include <QReadWriteLock>

#include "Main/global.h"

/*
    THE PACKED ROW STORE -- the in-memory form of an image.

    WHY. A datamodel row is currently 92 QStandardItems, measured at ~19,900
    bytes (see "The Row Store" in Documentation.txt), so a 250,000-row catalog
    would want 4.6 GB and cannot be held at all -- which is the entire reason
    the catalog needs a Load button and a kResultLimit. The same row as a packed
    struct with the repeated strings interned measured 573 bytes, 137 MB at
    250k. That 34x is what makes "browse the whole catalog" possible.

    HOW IT IS BEING INTRODUCED. Not by swapping DataModel's base class in one
    step: that has no working intermediate state, so nothing could be verified
    until everything was done. Instead the store is maintained ALONGSIDE the
    QStandardItems, written by the same DataModel::setData that writes them, and
    DataModel::verifyRowStore() walks every row and every covered column and
    compares the two. When that reports clean over real folders, the reads move
    across and the items go. The store is the thing under test until then.

    INTERNING IS WHAT MAKES IT SMALL. Folder, type, make, model, lens, year,
    day, creator, copyright and every keyword are the same handful of values
    across a whole shoot, so they are held once and referenced by a qint32 id.
    Path, name and title are genuinely per-row and are stored as QStrings; path
    alone is ~190 of the 573 bytes and is kept because it is the identity key
    that icd, fPathRow and the proxy snapshot all use -- composing it per access
    would trade 45 MB for an allocation on every hot path.

    THREADING. Writes are GUI thread only; every worker-thread write arrives
    through a queued setValDm/setValSf signal. READS are the reason there is a
    lock, and the reason is worth stating because an earlier version of this
    comment said confidently that there would never be one.

    Stage 0.3 moved BuildFilters, MetaRead and the image cache's own bookkeeping
    onto the published views in Datamodel/modelsync.h, but it did NOT convert
    every off-thread read of dm->sf -- the remaining ones are the known
    QSortFilterProxyModel exposure that tests/tsan/run_tsan_proxy.sh exists to
    reproduce, and whose fix is deliberately deferred.

    While the QStandardItems held the values that was survivable by accident: a
    worker read an item pointer out of a list that only changes when a ROW is
    inserted. Now the value lives here, and the INTERNER appends on every string
    written -- so its QVector reallocates all through the metadata load while a
    decoder thread is reading from it. That is a use-after-free, not a stale
    read, and ThreadSanitizer caught it on the first run after the items went.

    So the lock is the price of the deferred proxy readers, not a design
    preference, and it comes out with them.
*/

class Interner
{
public:
    qint32 id(const QString &s)
    {
        if (s.isEmpty()) return -1;
        auto it = mIds.constFind(s);
        if (it != mIds.cend()) return *it;
        mValues.append(s);
        const qint32 n = mValues.size() - 1;
        mIds.insert(s, n);
        return n;
    }
    QString value(qint32 id) const
    {
        return (id >= 0 && id < mValues.size()) ? mValues.at(id) : QString();
    }
    void clear() { mIds.clear(); mValues.clear(); }
    int distinctCount() const { return mValues.size(); }

private:
    QHash<QString, qint32> mIds;
    QVector<QString> mValues;
};

/*  One image. Field order groups the strings together and the small scalars
    together so the padding is not paid twice. */
struct ImageRow
{
    // genuinely per-row text
    QString path;
    QString name;
    QString title;
    QString searchText;

    /*  Interned repeats. Several of these look like numbers but are stored as
        TEXT by the model and read back as text by the Filters categories
        (aperture "f/5.6", shutter "1/1250", focal length "400"), so they are
        interned as written rather than re-derived -- the store's job is to
        return exactly what the model returned, not a tidier version of it.
        Created/Modified are formatted timestamp strings for the same reason,
        and intern well because a shoot shares most of them. */
    qint32 folderId = -1, typeId = -1, makeId = -1, modelId = -1, lensId = -1;
    qint32 yearId = -1, dayId = -1, creatorId = -1, copyrightId = -1;
    qint32 labelId = -1, pickId = -1, gpsId = -1;
    qint32 dimensionsId = -1;

    qint32 createdId = -1, modifiedId = -1;
    /*  Rating and MPix look like numbers and are NOT. The model holds
        ImageMetadata::rating, a QString that is EMPTY when unrated -- and an
        empty rating is a different thing from a rating of 0, which the Filters
        category list shows as a blank row rather than a "0" row. MPix is
        QString::number(mp, 'f', 2), so holding a float and formatting on the
        way out returns 24.15999984741211 where the model says 24.16. Both were
        stored numerically first and the shadow verification caught both, which
        is the whole reason it exists. */
    qint32 ratingId = -1, megaPixelsId = -1;

    QVector<qint32> keywordIds;        // dc:subject leaves
    QVector<qint32> keywordPathIds;    // lr:hierarchicalSubject paths
    QVector<qint32> keywordAllIds;     // the flattened vocabulary that is filtered on

    // scalars
    qint64 byteSize = 0;
    qint32 rowNumber = 0;
    qint32 width = 0, height = 0;      // int in the model; see addMetadataForItem
    /*  These four ARE numeric in the model (ImageMetadata::apertureNum,
        exposureTimeNum, focalLengthNum, ISONum), and their delegates depend on
        it: ExposureTimeItemDelegate guards with "value == 0" before computing
        1/value, and a QString there slips past the guard and reaches
        qRound(+Inf). Store the model's own type. */
    double aperture = 0.0;
    double exposureTime = 0.0;
    int    focalLength = 0;
    int    iso = 0;
    quint8 metaStatus = 0;
    bool   isVideo = false;
    bool   iconLoaded = false;
    bool   isCompare = false;      // a BOOL in the model, read back as text

    /*  --- the second batch of columns, moved off the items after the first
        deletion pass measured 31 of the 93 still there. Same rules as above:
        store the model's own TYPE, and intern anything a shoot repeats.

        devPreviewKey is a recipe hash and err is a list of load errors on the
        few rows that had one -- neither repeats, so neither is interned; an
        absent key or error list costs one -1 and one empty QStringList. */
    QStringList err;                   // G::ErrColumn, a QStringList in the model
    qint32 emailId = -1, urlId = -1, shootingInfoId = -1;
    qint32 exposureCompId = -1, durationId = -1, aspectRatioId = -1, rotationId = -1;
    qint32 devPreviewKeyId = -1;
    /*  The "original value" columns -- what the file said before the user edited
        it, so an edit can be reverted and a sidecar written only when it really
        differs. They intern as well as their live counterparts do. */
    qint32 _ratingId = -1, _labelId = -1, _creatorId = -1, _titleId = -1;
    qint32 _copyrightId = -1, _emailId = -1, _urlId = -1;

    quint32 permissions = 0;           // a uint in the model (fileInfo.permissions())
    quint32 orientationOffset = 0;
    qint32  orientation = 0;           // int; see addMetadataForItem
    qint32  rotationDegrees = 0;
    float   focusX = -1.0f, focusY = -1.0f;
    double  iconAspectRatio = 0.0;     // qreal in the model; set only when an icon lands

    /*  --- the last things on the items: column 0's CUSTOM ROLES. Not values,
        which is why they outlived three passes of value-column work, but they
        are per-row facts all the same and holding them was the only reason a
        QStandardItem still existed on most rows.

        The four raw+jpg pairing roles are described in datamodel.cpp; note
        dupOtherIdx is an int ROW and its callers test isValid() to mean "not
        paired", so it needs the set mask as much as any conditional column
        does. iconRect is written per visible icon by IconView and read by the
        delegate, so it is a field rather than a hash entry. */
    QRect  iconRect;                   // G::IconRectRole
    qint32 dupRawTypeId = -1;          // G::DupRawTypeRole, interned ("ORF", "NEF")
    qint32 dupOtherIdx = -1;           // G::DupOtherIdxRole, a datamodel row
    bool   dupHideRaw = false;         // G::DupHideRawRole
    bool   dupIsJpg = false;           // G::DupIsJpgRole

    bool isSearch = false;             // G::SearchColumn, settled on bool
    bool isIngested = false;           // G::IngestedColumn, settled on bool
    bool metadataReading = false;
    bool isReadWrite = false;
    bool hasSidecar = false;
    bool developEdited = false;

    /*  WHICH FIELDS HAVE ACTUALLY BEEN WRITTEN. The scratch store learned this
        first (see rowscratch.h) and it matters more here, because this batch
        brought in the first genuinely CONDITIONAL resident columns: Duration is
        set only for video, IconAspectRatio only once an icon has been decoded,
        Err only on a row that failed. An unset QStandardItem returns an INVALID
        QVariant and callers branch on it, so returning the field's zero -- or an
        interned empty string -- would turn "no duration" into "a duration of
        nothing", which reads as a value to anything testing isValid().

        Two words rather than one because 63 columns is already too close to 64
        to leave the next one added to chance. */
    quint64 setLo = 0, setHi = 0;
};

class RowStore
{
public:
    void clear()
    {
        QWriteLocker l(&mLock);
        mRows.clear(); mStrings.clear();
    }
    void resize(int n)
    {
        QWriteLocker l(&mLock);
        if (n != mRows.size()) mRows.resize(n);
    }

    /*  ROW SPLICING. The store is indexed by row, so an insert or a removal in
        the MIDDLE shifts the meaning of every row after it. While the items
        were still there this was handled by rebuilding the store from them;
        with the items gone there is nothing to rebuild from, so the vector is
        spliced directly -- which is also what a plain resize() could never do
        correctly, since it truncates from the END. */
    void insertRows(int at, int count)
    {
        QWriteLocker l(&mLock);
        if (count <= 0 || at < 0 || at > mRows.size()) return;
        mRows.insert(at, count, ImageRow());
    }
    void removeRows(int at, int count)
    {
        QWriteLocker l(&mLock);
        if (count <= 0 || at < 0 || at >= mRows.size()) return;
        count = qMin(count, mRows.size() - at);
        mRows.remove(at, count);
    }
    int  size() const { QReadLocker l(&mLock); return mRows.size(); }
    bool contains(int row) const
    {
        QReadLocker l(&mLock);
        return row >= 0 && row < mRows.size();
    }
    /*  Reporting only, and NOT locked -- the caller holds a reference into the
        interner, which no lock taken here could keep valid. GUI thread. */
    const Interner &strings() const { return mStrings; }

    /*  Which datamodel columns this store can answer. Deliberately explicit
        rather than "everything not listed as scratch": a column silently
        assumed to be covered would be a column verifyRowStore quietly stopped
        checking. */
    /*  Coverage is per (COLUMN, ROLE), not per column. Column 0 alone carries
        six roles -- the path, the icon rect and the four raw+jpg pairing facts
        -- and none of them is the cell's value. */
    static bool covers(int column, int role);

    QVariant value(int row, int column, int role = Qt::EditRole) const;
    void setValue(int row, int column, int role, const QVariant &v);

private:
    mutable QReadWriteLock mLock;
    QVector<ImageRow> mRows;
    Interner mStrings;
};

#endif // IMAGEROW_H
