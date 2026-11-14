#include <QtTest>
#include <QThread>
#include <atomic>

#include "Datamodel/modelsync.h"

/*
    Pins Datamodel/modelsync.h: what worker threads may read of the model, and when the
    two halves agree (Documentation 4.4.15).

    flagsMoveTogether runs a reader against a writer flipping a row between NotAttempted
    and Loaded. The status used to be published as two compare-and-swaps, and a reader
    between them saw "attempted, not loaded" -- ImageCache's "Metadata read failed",
    which skipped the row with nothing to wake it (C8).
*/
class tst_modelsync : public QObject
{
    Q_OBJECT
private slots:
    void setFlagsTouchesOnlyTheMask();
    void flagsMoveTogether();
    void modelViewConsistency();
    void spliceShiftsRowsBelow();
};

void tst_modelsync::setFlagsTouchesOnlyTheMask()
{
    RowSyncArray a(2);
    a.setFlag(0, RowSync::IconLoaded, true);
    a.setFlags(0, RowSync::MetaAttempted | RowSync::MetaLoaded,
               RowSync::MetaAttempted | RowSync::MetaLoaded);
    QVERIFY(a.has(0, RowSync::IconLoaded));         // outside the mask: untouched
    QVERIFY(a.has(0, RowSync::MetaLoaded));
    a.setFlags(0, RowSync::MetaAttempted | RowSync::MetaLoaded, 0);
    QVERIFY(!a.has(0, RowSync::MetaAttempted));
    QVERIFY(!a.has(0, RowSync::MetaLoaded));
    QVERIFY(a.has(0, RowSync::IconLoaded));
    a.setFlags(5, RowSync::MetaLoaded, RowSync::MetaLoaded);  // out of range: ignored
    QCOMPARE(a.flags(1), quint8(0));
}

void tst_modelsync::flagsMoveTogether()
{
    RowSyncArray a(1);
    const quint8 mask = RowSync::MetaAttempted | RowSync::MetaLoaded;
    std::atomic<bool> stop{false};
    std::atomic<int> torn{0};
    QThread *reader = QThread::create([&] {
        while (!stop.load()) {
            const quint8 f = a.flags(0);
            if ((f & RowSync::MetaAttempted) && !(f & RowSync::MetaLoaded)) ++torn;
        }
    });
    reader->start();
    for (int i = 0; i < 200000; ++i)
        a.setFlags(0, mask, (i & 1) ? mask : 0);
    stop = true;
    reader->wait();
    delete reader;
    QCOMPARE(torn.load(), 0);
}

void tst_modelsync::modelViewConsistency()
{
    auto sync = std::make_shared<RowSyncArray>(3);
    sync->setStructureGen(7);
    auto snap = std::make_shared<ProxySnapshot>();
    snap->structureGen = 7;
    ModelView mv{snap, sync};
    QVERIFY(mv.consistent());

    // rows moved: RowSync is spliced now, the snapshot follows a turn later
    auto spliced = std::make_shared<RowSyncArray>(4);
    spliced->setStructureGen(8);
    mv.sync = spliced;
    QVERIFY(!mv.consistent());

    auto rebuilt = std::make_shared<ProxySnapshot>();
    rebuilt->structureGen = 8;
    mv.snap = rebuilt;
    QVERIFY(mv.consistent());

    QVERIFY(!ModelView{}.consistent());              // nothing published yet
}

void tst_modelsync::spliceShiftsRowsBelow()
{
    RowSyncArray old(3);
    old.setFlag(0, RowSync::IsVideo, true);
    old.setFlag(2, RowSync::MetaLoaded, true);
    RowSyncArray grown(4);
    grown.copySpliced(old, 1, 1);                   // one row opened at 1
    QVERIFY(grown.has(0, RowSync::IsVideo));
    QCOMPARE(grown.flags(1), quint8(0));            // the new row starts empty
    QVERIFY(grown.has(3, RowSync::MetaLoaded));     // old row 2 is now row 3
    RowSyncArray shrunk(2);
    shrunk.copySpliced(old, 1, -1);                 // row 1 closed
    QVERIFY(shrunk.has(0, RowSync::IsVideo));
    QVERIFY(shrunk.has(1, RowSync::MetaLoaded));    // old row 2 is now row 1
}

QTEST_GUILESS_MAIN(tst_modelsync)
#include "tst_modelsync.moc"
