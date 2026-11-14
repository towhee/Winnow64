#include <QtTest>

#include "Cache/deferredrows.h"

/*
    Pins Cache/deferredrows.h: how ImageCache keeps a row it cannot decode yet without
    blocking its thread (Documentation 4.4.10), and the bounded idle backstop.

    The bugs these stand in for: a row whose metadata landed late was skipped and never
    woken (a blank loupe until the user stepped away and back), and a raw with no
    embedded-JPG offset yet spent its whole attempt budget in a tight loop.
*/
class tst_deferredrows : public QObject
{
    Q_OBJECT
private slots:
    void deferAndClearReasons();
    void wakeClearsOnlyNamedKeys();
    void backstopClearsGeometryNotMeta();
    void backstopIsBounded();
    void progressRestoresBudget();
    void armOnlyWithPendingWork();
    void clearKeepsTimerArmed();
    void snapshotWakesOnlyStructure();
};

void tst_deferredrows::deferAndClearReasons()
{
    DeferredRows d;
    d.defer("a", DeferredRows::Meta);
    d.defer("a", DeferredRows::Geometry);
    QVERIFY(d.has("a", DeferredRows::Meta));
    QVERIFY(d.has("a", DeferredRows::Geometry));

    d.clearReason("a", DeferredRows::Meta);
    QVERIFY(!d.has("a", DeferredRows::Meta));
    QVERIFY(d.has("a", DeferredRows::Geometry));     // the other reason survives
    QVERIFY(d.contains("a"));

    d.clearReason("a", DeferredRows::Geometry);
    QVERIFY(!d.contains("a"));                       // no reason left, no entry
    QCOMPARE(d.count(), 0);

    d.defer("", DeferredRows::Meta);                 // an empty path is never deferred
    d.defer("b", 0);                                 // nor is a row with no reason
    QCOMPARE(d.count(), 0);
}

void tst_deferredrows::wakeClearsOnlyNamedKeys()
{
    DeferredRows d;
    d.defer("a", DeferredRows::Meta);
    d.defer("b", DeferredRows::Meta);

    QVERIFY(!d.wake({"x", "y"}));                    // nothing of ours: do not dispatch
    QCOMPARE(d.count(), 2);

    QVERIFY(d.wake({"x", "a"}));                     // one of ours: dispatch
    QVERIFY(!d.contains("a"));
    QVERIFY(d.contains("b"));
}

void tst_deferredrows::backstopClearsGeometryNotMeta()
{
    DeferredRows d;
    d.defer("raw", DeferredRows::Geometry);
    d.defer("late", DeferredRows::Meta);
    d.defer("both", DeferredRows::Meta | DeferredRows::Geometry);

    QVERIFY(d.arm(true));
    QVERIFY(d.fire(true));
    QVERIFY(!d.contains("raw"));                     // geometry has no wake: retried
    QVERIFY(d.has("late", DeferredRows::Meta));      // waits for its rowsChanged
    QVERIFY(d.has("both", DeferredRows::Meta));
    QVERIFY(!d.has("both", DeferredRows::Geometry));
}

void tst_deferredrows::backstopIsBounded()
{
    DeferredRows d;
    int fired = 0;
    for (int i = 0; i < 100; ++i) {
        if (!d.arm(true)) break;
        if (d.fire(true)) ++fired;
    }
    QCOMPARE(fired, DeferredRows::kMaxFires);
    QVERIFY(!d.arm(true));                           // spent: a hopeless row stops here
}

void tst_deferredrows::progressRestoresBudget()
{
    DeferredRows d;
    while (d.arm(true)) d.fire(true);
    QCOMPARE(d.firesSinceProgress(), DeferredRows::kMaxFires);
    d.noteProgress();                                // an image was cached
    QVERIFY(d.arm(true));
}

void tst_deferredrows::armOnlyWithPendingWork()
{
    DeferredRows d;
    QVERIFY(!d.arm(false));                          // idle and nothing queued
    QVERIFY(d.arm(true));
    QVERIFY(!d.arm(true));                           // one timer at a time
    QVERIFY(!d.fire(false));                         // work finished meanwhile
    QCOMPARE(d.firesSinceProgress(), 0);             // and that costs no budget
    QVERIFY(!d.isArmed());
}

void tst_deferredrows::clearKeepsTimerArmed()
{
    DeferredRows d;
    d.defer("a", DeferredRows::Meta);
    QVERIFY(d.arm(true));
    d.clear();                                       // folder change, timer in flight
    QCOMPARE(d.count(), 0);
    QVERIFY(d.isArmed());
    QVERIFY(!d.arm(true));                           // no second timer
    d.fire(false);
    QVERIFY(!d.isArmed());
}

void tst_deferredrows::snapshotWakesOnlyStructure()
{
    // a new proxy snapshot ends Structure deferrals, nothing else (4.4.15)
    DeferredRows d;
    d.defer("a", DeferredRows::Structure);
    d.defer("b", DeferredRows::Structure | DeferredRows::Meta);
    d.defer("c", DeferredRows::Meta);
    QVERIFY(d.wakeReason(DeferredRows::Structure));
    QVERIFY(!d.contains("a"));
    QVERIFY(d.has("b", DeferredRows::Meta));
    QVERIFY(!d.has("b", DeferredRows::Structure));
    QVERIFY(d.has("c", DeferredRows::Meta));
    QVERIFY(!d.wakeReason(DeferredRows::Structure));     // nothing left to wake
}

QTEST_GUILESS_MAIN(tst_deferredrows)
#include "tst_deferredrows.moc"
