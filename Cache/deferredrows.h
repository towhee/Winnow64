#ifndef DEFERREDROWS_H
#define DEFERREDROWS_H

#include <QHash>
#include <QString>
#include <QStringList>

/*
    ROWS WAITING ON SOMETHING, NEVER ON A BLOCKED THREAD (Documentation 4.4.10).

    ImageCache keeps a row it cannot decode yet in its queue and marks it here with the
    reason, instead of blocking the cache thread until the reason goes away:

      Meta      the row's metadata is not loaded. DataModel::rowsChanged wakes it
                (watchRowLoaded). okToDecode re-tests the RowSync flag each time it
                meets the row, so the mark is bookkeeping for the wake, not a gate.
      Geometry  the decoder found no embedded-JPG offset yet (ImageDecoder status
                NoGeometry). Nothing wakes it, so it IS a gate: okToDecode skips the
                row until the idle backstop clears the mark.

    The idle backstop is the bounded retry for rows nothing wakes. When the cache goes
    idle with rows still queued it arms one timer; each firing clears the Geometry
    marks and dispatches. Progress (an image cached, a new position) resets the count;
    kMaxFires firings without progress stop it, so a row that can never be decoded
    cannot keep the cache thread busy.

    Keyed by path. Owned by one thread (the ImageCache thread); not thread-safe. Pure, so
    tests/unit/tst_deferredrows.cpp pins it.
*/
class DeferredRows
{
public:
    /*  Structure: the proxy snapshot was behind the model's rows, so the row's flags
        could not be read (Documentation 4.4.15). Woken by proxySnapshotPublished. */
    enum Reason { Meta = 0x1, Geometry = 0x2, Structure = 0x4 };

    static constexpr int kBackstopMs = 250;
    static constexpr int kMaxFires = 8;         // 2 s without progress, then stop

    void defer(const QString &key, int reason)
    {
        if (key.isEmpty() || !reason) return;
        rows[key] |= reason;
    }

    void clearReason(const QString &key, int reason)
    {
        auto it = rows.find(key);
        if (it == rows.end()) return;
        it.value() &= ~reason;
        if (it.value() == 0) rows.erase(it);
    }

    bool has(const QString &key, int reason) const { return rows.value(key) & reason; }
    bool contains(const QString &key) const { return rows.contains(key); }
    int count() const { return int(rows.size()); }

    /*  A wake for these keys (DataModel::rowsChanged). Returns true if any of them was
        deferred, i.e. the caller should dispatch. Clears every reason: the row is
        re-evaluated from scratch by okToDecode. */
    bool wake(const QStringList &keys)
    {
        bool any = false;
        for (const QString &k : keys) {
            if (rows.remove(k)) any = true;
        }
        return any;
    }

    /*  Clear one reason from every row (a new proxy snapshot ends every Structure
        deferral). Returns true if any row had it. */
    bool wakeReason(int reason)
    {
        bool any = false;
        for (auto it = rows.begin(); it != rows.end(); ) {
            if (it.value() & reason) {
                any = true;
                it.value() &= ~reason;
                if (it.value() == 0) { it = rows.erase(it); continue; }
            }
            ++it;
        }
        return any;
    }

    /*  A folder change or cache reload. armed is left alone: a timer already started
        still fires once (and finds nothing to do), and a second one must not start. */
    void clear()
    {
        rows.clear();
        fires = 0;
    }

    // --- idle backstop ---

    /*  The cache went idle. Returns true if the caller should start the timer: work is
        still pending, no timer is running, and the budget is not spent. */
    bool arm(bool workPending)
    {
        if (armed || !workPending || fires >= kMaxFires) return false;
        armed = true;
        return true;
    }

    /*  The timer fired. Returns true if the caller should dispatch. Geometry marks are
        cleared so those rows get another decode; Meta marks stay (okToDecode re-tests
        the flag anyway). */
    bool fire(bool workPending)
    {
        armed = false;
        if (!workPending) return false;
        ++fires;
        for (auto it = rows.begin(); it != rows.end(); ) {
            it.value() &= ~Geometry;
            if (it.value() == 0) it = rows.erase(it);
            else ++it;
        }
        return true;
    }

    // Progress: the backstop gets its whole budget back.
    void noteProgress() { fires = 0; }

    bool isArmed() const { return armed; }
    int firesSinceProgress() const { return fires; }

private:
    QHash<QString, int> rows;
    bool armed = false;
    int fires = 0;
};

#endif // DEFERREDROWS_H
