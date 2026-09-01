#ifndef MODELSYNC_H
#define MODELSYNC_H

#include <QHash>
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>

/*
    WHAT WORKER THREADS ARE ALLOWED TO SEE OF THE DATAMODEL.

    QStandardItemModel and QSortFilterProxyModel are not thread-safe, and the
    GUI thread writes into both continuously during a folder load. ImageCache
    and MetaRead nevertheless need per-row facts while they run. These two types
    are how they get them, and they answer two DIFFERENT kinds of question --
    which is why they are separate objects with separate lifetimes.

      RowSyncArray  -- values that CHANGE while the worker runs (metadata
                       status, the cached-image size estimate). Lock-free
                       atomics, indexed by DATAMODEL row, written by the GUI
                       thread in DataModel::setData. It must be live: ImageCache
                       ::waitForMetaRead polls metadata status in a wait loop, so
                       a point-in-time copy would never see it change.

      ProxySnapshot -- the ORDER and IDENTITY of rows: proxy row to datamodel
                       row and file path, and the reverse lookup. These change
                       only when rows are inserted or removed or the proxy is
                       re-sorted or re-filtered, so a copy republished on those
                       events is always current, and it is the only safe way to
                       ask "what is at proxy row N" off the GUI thread.

    Both are handed out as shared_ptr<const> and replaced wholesale rather than
    edited in place. A worker takes its own reference and holds it for the
    duration of a call, so a rebuild on the GUI thread cannot pull the object out
    from under it -- the mistake ThreadSanitizer caught in the BuildFilters
    snapshot, which was a plain member (see "Worker Threads and the Model" in
    Documentation.txt).
*/

/*  Per-datamodel-row live state. One cache line is not worth chasing here: the
    array is read far more than written and the writes are already serialised on
    the GUI thread. */
struct RowSync {
    /* Bit flags. Kept in one atomic so a reader sees a coherent set rather than
       a torn mix of two separately-published booleans. */
    enum Flag : quint8 {
        MetaAttempted = 1 << 0,
        MetaLoaded    = 1 << 1,
        IconLoaded    = 1 << 2,
        IsVideo       = 1 << 3
    };
    std::atomic<quint8> flags{0};
    /* G::CacheSizeColumn in MB -- an estimate at row creation, refined when the
       metadata is read, and the number ImageCache budgets its target range on. */
    std::atomic<float> cacheMB{0.0f};
};

class RowSyncArray {
public:
    explicit RowSyncArray(int n) : mSize(n), mRows(n > 0 ? new RowSync[n] : nullptr) {}

    int size() const { return mSize; }
    bool contains(int dmRow) const { return dmRow >= 0 && dmRow < mSize; }

    /*  Unchecked in release paths is not worth the risk here: a stale row index
        from a worker is exactly the case this exists to survive. */
    quint8 flags(int dmRow) const {
        if (!contains(dmRow)) return 0;
        return mRows[dmRow].flags.load(std::memory_order_relaxed);
    }
    float cacheMB(int dmRow) const {
        if (!contains(dmRow)) return 0.0f;
        return mRows[dmRow].cacheMB.load(std::memory_order_relaxed);
    }
    bool has(int dmRow, RowSync::Flag f) const { return flags(dmRow) & f; }

    void setFlag(int dmRow, RowSync::Flag f, bool on) {
        if (!contains(dmRow)) return;
        RowSync &r = mRows[dmRow];
        quint8 cur = r.flags.load(std::memory_order_relaxed);
        quint8 want;
        do { want = on ? (cur | f) : quint8(cur & ~f); }
        while (!r.flags.compare_exchange_weak(cur, want, std::memory_order_relaxed));
    }
    void setCacheMB(int dmRow, float mb) {
        if (!contains(dmRow)) return;
        mRows[dmRow].cacheMB.store(mb, std::memory_order_relaxed);
    }

    // Carry existing values onto a larger array when the model grows.
    void copyFrom(const RowSyncArray &other) {
        const int n = qMin(mSize, other.mSize);
        for (int i = 0; i < n; ++i) {
            mRows[i].flags.store(other.mRows[i].flags.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            mRows[i].cacheMB.store(other.mRows[i].cacheMB.load(std::memory_order_relaxed),
                                   std::memory_order_relaxed);
        }
    }

private:
    int mSize = 0;
    std::unique_ptr<RowSync[]> mRows;
};

typedef std::shared_ptr<RowSyncArray> RowSyncPtr;

/*  The proxy's order and identity, copied on the GUI thread. */
struct ProxySnapshot {
    int instance = -1;
    QVector<int> dmRowOf;               // by proxy row -> datamodel row
    QVector<QString> pathOf;            // by proxy row -> absolute file path
    QHash<QString, int> sfRowOfPath;    // reverse lookup, replaces proxyRowFromPath

    int rowCount() const { return dmRowOf.size(); }
    bool contains(int sfRow) const { return sfRow >= 0 && sfRow < dmRowOf.size(); }
    int dmRow(int sfRow) const { return contains(sfRow) ? dmRowOf.at(sfRow) : -1; }
    QString path(int sfRow) const { return contains(sfRow) ? pathOf.at(sfRow) : QString(); }
    int sfRow(const QString &fPath) const { return sfRowOfPath.value(fPath, -1); }
};

typedef std::shared_ptr<const ProxySnapshot> ProxySnapshotPtr;

#endif // MODELSYNC_H
