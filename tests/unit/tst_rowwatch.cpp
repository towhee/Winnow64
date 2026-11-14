#include <QtTest>
#include <QThread>
#include <atomic>

#include "Datamodel/rowwatch.h"

/*
    Pins Datamodel/rowwatch.h: the one-shot watch behind DataModel::watchRowLoaded and
    rowsChanged (Documentation 4.4.10).

    noLostWake runs the protocol on two real threads many times. The worker tests a
    flag, watches, and tests again; the "GUI" publishes the flag and then takes the
    watch. In every round the worker must either have seen the flag or have been
    taken (woken). A round where neither happened is the lost wake that left the
    current image black: the cache thread gave up on the row and nobody told it the
    row had landed.
*/
class tst_rowwatch : public QObject
{
    Q_OBJECT
private slots:
    void oneShot();
    void lookupOnlyWhenWatched();
    void clearDropsWatches();
    void noLostWake();
};

void tst_rowwatch::oneShot()
{
    RowWatch w;
    QVERIFY(!w.take(QString("a")));                  // nothing watched
    w.watch("a");
    w.watch("a");                                    // a set: watched once
    QCOMPARE(w.count(), 1);
    QVERIFY(w.take(QString("a")));
    QVERIFY(!w.take(QString("a")));                  // one-shot
    w.watch("");                                     // an empty key is never watched
    QCOMPARE(w.count(), 0);
}

void tst_rowwatch::lookupOnlyWhenWatched()
{
    RowWatch w;
    int lookups = 0;
    auto keyOf = [&] { ++lookups; return QString("a"); };
    QVERIFY(!w.take(keyOf));
    QCOMPARE(lookups, 0);                            // the common case costs no lookup
    w.watch("b");
    QVERIFY(!w.take(keyOf));
    QCOMPARE(lookups, 1);
    QCOMPARE(w.count(), 1);                          // b is still watched
}

void tst_rowwatch::clearDropsWatches()
{
    RowWatch w;
    w.watch("a");
    w.watch("b");
    w.clear();
    QCOMPARE(w.count(), 0);
    QVERIFY(!w.take(QString("a")));
}

void tst_rowwatch::noLostWake()
{
    constexpr int rounds = 20000;
    RowWatch w;
    std::atomic<int> flag{-1};          // the round whose row is "loaded"
    std::atomic<int> go{-1};            // the round both sides may start
    std::atomic<int> workerSaw{0};      // worker: saw the flag (1) or deferred (2)
    std::atomic<int> lost{0};
    std::atomic<int> workerDone{-1};

    QThread *worker = QThread::create([&] {
        for (int r = 0; r < rounds; ++r) {
            while (go.load(std::memory_order_acquire) != r) {}
            const QString key = QString::number(r);
            int saw;
            if (flag.load(std::memory_order_acquire) == r) saw = 1;
            else {
                w.watch(key);
                saw = flag.load(std::memory_order_acquire) == r ? 1 : 2;
            }
            workerSaw.store(saw, std::memory_order_release);
            workerDone.store(r, std::memory_order_release);
        }
    });
    worker->start();

    for (int r = 0; r < rounds; ++r) {
        workerSaw.store(0);
        go.store(r, std::memory_order_release);
        const QString key = QString::number(r);
        flag.store(r, std::memory_order_release);    // publish first ...
        const bool took = w.take(key);               // ... then take the watch
        while (workerDone.load(std::memory_order_acquire) != r) {}
        if (workerSaw.load() == 2 && !took) ++lost;  // deferred and never woken
        w.clear();                                   // drop a watch the worker
    }                                                // left after seeing the flag
    worker->wait();
    delete worker;
    QCOMPARE(lost.load(), 0);
}

QTEST_GUILESS_MAIN(tst_rowwatch)
#include "tst_rowwatch.moc"
