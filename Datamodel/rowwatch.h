#ifndef ROWWATCH_H
#define ROWWATCH_H

#include <QMutex>
#include <QSet>
#include <QString>

/*
    "TELL ME WHEN THIS ROW'S METADATA LANDS" -- the watch set behind
    DataModel::watchRowLoaded / rowsChanged (Documentation 4.4.10).

    THE PROTOCOL, which is what makes the wake impossible to lose:

      worker (any thread)               GUI (DataModel::setData)
      1. test the row's RowSync flag     a. publish the row's RowSync flag
      2. not set: watch(key)             b. take(key) -- true means a worker is
      3. test the flag AGAIN                waiting, so post it a wake
      4. still not set: defer the row

    watch and take share one mutex. If take (b) runs before watch (2), the flag (a) was
    published before the mutex was released, so the worker's re-test (3) sees it. If
    watch runs first, take finds the key and the GUI posts the wake. Either way the
    worker learns the row is loaded. A watch is one-shot: take removes it.

    A watch whose row loads between steps 1 and 3 is left in the set (the worker saw the
    flag and moved on); it fires harmlessly on the row's next metadata write or is
    dropped by clear() with the model.

    Thread-safe. Pure, so tests/unit/tst_rowwatch.cpp pins it, including the protocol
    under two real threads.
*/
class RowWatch
{
public:
    void watch(const QString &key)
    {
        if (key.isEmpty()) return;
        QMutexLocker lock(&mutex);
        keys.insert(key);
    }

    /*  Cheap when nothing is watched (the common case): one uncontended lock and an
        isEmpty test. keyOf is called only when something is watched, under the lock,
        so the caller can defer an expensive key lookup. */
    template <typename KeyOf>
    bool take(KeyOf keyOf)
    {
        QMutexLocker lock(&mutex);
        if (keys.isEmpty()) return false;
        const QString key = keyOf();
        return !key.isEmpty() && keys.remove(key);
    }

    bool take(const QString &key) { return take([&key] { return key; }); }

    void clear()
    {
        QMutexLocker lock(&mutex);
        keys.clear();
    }

    int count()
    {
        QMutexLocker lock(&mutex);
        return int(keys.size());
    }

private:
    QMutex mutex;
    QSet<QString> keys;
};

#endif // ROWWATCH_H
