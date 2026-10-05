#include "Datamodel/datamodel.h"
#include "Metadata/indexmetadata.h"
#include "Utilities/versionkey.h"

#include <QDataStream>
#include <QSaveFile>

/*
    THE LIBRARY SNAPSHOT -- see "The Parked Library" in notes/Documentation.txt.

    What DataModel::parkScope keeps in memory, written to a file at quit so the next
    session's first Library is a restore (DataModel::restoreScope) instead of the catalog
    query and the streamed fill. Everything a restore needs that the index cannot answer
    cheaply is in it; everything that is per-session is left out and comes back the way a
    fresh fill leaves it:

      IN   the row store (every value, the interned strings, the folder ancestry), the
           path index, the folder list and counts, the version row count, the memory
           estimate, and the FilterPanel result paths for the delta re-run.
      OUT  the icons (the index serves the visible ones in milliseconds), the scratch
           store (file offsets and cache state, which a fresh fill does not have either),
           the issue lists, the raw-info cache, the keyword memo. The version masters'
           metadata is rebuilt from the index, since a missing master would make the
           load-end version reconcile REMOVE its version rows.

    VALIDITY is three checks here and one by the caller. Here: the format, the build (the
    executable's size and time -- a rebuild may have changed what a row means, and a
    misread row would be shown as fact), and the index identity (Catalog::catalogId). The
    caller: the change sequence -- rows the index rewrote after baseSeq are re-read, and
    too many of them means a full load instead.
*/

namespace {
constexpr quint32 kSnapMagic = 0x574c4942;      // "WLIB"
constexpr quint32 kSnapFormat = 1;

QString buildFingerprint()
{
    const QFileInfo fi(QCoreApplication::applicationFilePath());
    return QString::number(fi.size()) + "-"
           + QString::number(fi.lastModified().toMSecsSinceEpoch());
}
}  // namespace

bool DataModel::saveLibrarySnapshot(const QString &file, const LibrarySnapshotMeta &meta,
                                    QString *why)
{
    auto failed = [why](const QString &w) { if (why) *why = w; return false; };
    if (meta.baseSeq < 0 || meta.catalogId.isEmpty()) return failed("no index identity");

    /*  The parked set if there is one (a quit from Folders), else what is loaded. */
    const ParkedScope *p = parked.get();
    const RowStore &rows = p ? p->rows : rowStore;
    if (rows.size() == 0) return failed("no rows");

    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) return failed(f.errorString());
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_6_0);

    out << kSnapMagic << kSnapFormat << buildFingerprint() << meta.catalogId
        << meta.baseSeq << meta.currentKey << qint32(meta.totalMatches) << meta.resultPaths;
    if (p) {
        out << p->folderList << p->folderImageCount << p->firstFolderPathWithImages
            << qint32(p->versionRowCount) << p->fPathRow << p->bytesUsed
            << p->bytesUsedSampleTotal << qint32(p->bytesUsedSampleCount);
    }
    else {
        QStringList fl;
        QHash<QString, int> fic;
        {
            QMutexLocker lk(&dmMutex);
            fl = folderList;
            fic = folderImageCount;
        }
        QHash<QString, int> fpr;
        {
            QReadLocker l(&fPathRowLock);
            fpr = fPathRow;
        }
        out << fl << fic << firstFolderPathWithImages << qint32(mVersionRowCount) << fpr
            << bytesUsed << bytesUsedSampleTotal << qint32(bytesUsedSampleCount);
    }
    rows.save(out);

    if (out.status() != QDataStream::Ok) { f.cancelWriting(); return failed("write error"); }
    if (!f.commit()) return failed(f.errorString());
    return true;
}

std::unique_ptr<DataModel::ParkedScope>
DataModel::readLibrarySnapshot(const QString &file, LibrarySnapshotMeta &meta, QString *why)
{
    auto failed = [why](const QString &w) {
        if (why) *why = w;
        return std::unique_ptr<ParkedScope>();
    };
    QFile f(file);
    if (!f.exists()) return failed("no snapshot");
    if (!f.open(QIODevice::ReadOnly)) return failed(f.errorString());
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_6_0);

    quint32 magic = 0, format = 0;
    QString fingerprint;
    qint32 total = 0;
    in >> magic >> format;
    if (magic != kSnapMagic || format != kSnapFormat) return failed("other format");
    in >> fingerprint;
    if (fingerprint != buildFingerprint()) return failed("written by another build");
    in >> meta.catalogId >> meta.baseSeq >> meta.currentKey >> total >> meta.resultPaths;
    meta.totalMatches = total;
    if (meta.catalogId != Catalog::instance().catalogId()) return failed("another index");

    auto p = std::make_unique<ParkedScope>();
    qint32 versionRows = 0, sampleCount = 0;
    in >> p->folderList >> p->folderImageCount >> p->firstFolderPathWithImages
       >> versionRows >> p->fPathRow >> p->bytesUsed >> p->bytesUsedSampleTotal
       >> sampleCount;
    p->versionRowCount = versionRows;
    p->bytesUsedSampleCount = sampleCount;
    if (in.status() != QDataStream::Ok) return failed("truncated");
    if (!p->rows.load(in)) return failed("row layout differs");
    if (p->rows.size() == 0 || p->fPathRow.size() > p->rows.size())
        return failed("inconsistent");

    for (const QString &folder : std::as_const(p->folderList)) p->folderSet.insert(folder);
    p->currentKey = meta.currentKey;
    p->scope.scope = G::Scope::Catalog;
    p->scope.reconcile = false;
    p->scope.hydrated = true;

    /*  The load-flag counters, from the rows themselves; no icon is loaded. */
    p->rows.forEachRow({{G::MetadataStatusColumn, Qt::EditRole},
                        {G::VideoColumn, Qt::EditRole},
                        {G::AvailabilityColumn, Qt::EditRole}},
                       [&](int, const QVariant *v) {
        const int st = v[0].toInt();
        if (st != G::MetaNotAttempted) ++p->metadataAttempted;
        if (st == G::MetaLoaded) ++p->metadataLoaded;
        if (v[1].toBool()) ++p->videoRows;
        if (v[2].toInt() != int(Catalog::Availability::Present)) ++p->iconUnloadable;
    });

    /*  THE VERSION MASTERS, from the index -- see the header comment. */
    QSet<QString> masters;
    for (auto it = p->fPathRow.cbegin(); it != p->fPathRow.cend(); ++it)
        if (VersionKey::isVersion(it.key())) masters.insert(VersionKey::sourceOf(it.key()));
    if (!masters.isEmpty()) {
        const QHash<QString, CatalogRow> mrows =
            Catalog::instance().rowsForPaths(QStringList(masters.cbegin(), masters.cend()));
        for (auto it = mrows.cbegin(); it != mrows.cend(); ++it) {
            if (it.value().versions.isEmpty()) continue;
            ImageMetadata m;
            IndexMetadata::fill(m, it.value(),
                                QDateTime::fromSecsSinceEpoch(it.value().srcMtime),
                                p->fPathRow.value(it.key(), -1), 0);
            p->versionMasters.insert(it.key(), m);
        }
    }
    return p;
}
