#ifndef IMAGEROW_H
#define IMAGEROW_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QVariant>

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

    THREADING. GUI thread only, exactly like the QStandardItems it shadows.
    Every worker-thread write already arrives through a queued setValDm/setValSf
    signal, and every worker-thread READ now goes through the published views in
    Datamodel/modelsync.h. Nothing here needs a lock, and adding one would
    disguise a caller that should not be here.
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
    qint32 searchId = -1, dimensionsId = -1;

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
};

class RowStore
{
public:
    void clear() { mRows.clear(); mStrings.clear(); }
    void resize(int n) { if (n != mRows.size()) mRows.resize(n); }
    int  size() const { return mRows.size(); }
    bool contains(int row) const { return row >= 0 && row < mRows.size(); }
    const Interner &strings() const { return mStrings; }

    /*  Which datamodel columns this store can answer. Deliberately explicit
        rather than "everything not listed as scratch": a column silently
        assumed to be covered would be a column verifyRowStore quietly stopped
        checking. */
    static bool covers(int column, int role);

    QVariant value(int row, int column) const;
    void setValue(int row, int column, const QVariant &v);

private:
    QVector<ImageRow> mRows;
    Interner mStrings;
};

#endif // IMAGEROW_H
