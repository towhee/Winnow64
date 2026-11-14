#include <QtTest>

#include "Views/loupeshown.h"

/*
    Pins Views/loupeshown.h: what ImageView believes is on screen (Documentation
    4.4.11). The first case is the bug: A shown, arrow to an uncached B (black), arrow
    straight back to A. The old shortcut compared against currentImagePath, which the
    miss left naming A, so A was "already displayed" over a black loupe though it was
    in the cache, and nothing ever repainted it.
*/
class tst_loupeshown : public QObject
{
    Q_OBJECT
private slots:
    void backToImageBeforeMissLoads();
    void sameFullImageSkips();
    void replaceAlwaysLoads();
    void interimSurvivesMiss();
    void fullSurvivesForcedReloadMiss();
    void renderPromotesInterim();
    void clearShowsNothing();
    void placeholderIsNotFull();
    void placeholderSurvivesMissAndHitReplacesIt();
};

void tst_loupeshown::backToImageBeforeMissLoads()
{
    LoupeShown s;
    s.noteHit("A");
    QVERIFY(!s.noteMiss("B"));                       // blank the loupe for B
    QVERIFY(!s.isShowing("A"));
    QVERIFY(!s.isShowing("B"));                      // nothing is up
    QVERIFY(!s.canSkipLoad("A", false));             // the fix: A must load
}

void tst_loupeshown::sameFullImageSkips()
{
    LoupeShown s;
    s.noteHit("A");
    QVERIFY(s.canSkipLoad("A", false));
    QVERIFY(!s.canSkipLoad("B", false));
}

void tst_loupeshown::replaceAlwaysLoads()
{
    LoupeShown s;
    s.noteHit("A");
    QVERIFY(!s.canSkipLoad("A", true));              // refreshViewsOnCacheChange
}

void tst_loupeshown::interimSurvivesMiss()
{
    LoupeShown s;
    s.noteInterim("A");                              // Develop stand-in while decoding
    QVERIFY(!s.canSkipLoad("A", false));             // not the real image: try the cache
    QVERIFY(s.noteMiss("A"));                        // still decoding: keep the stand-in
    QVERIFY(s.isShowing("A"));
    QCOMPARE(s.state(), LoupeShown::Interim);
    QVERIFY(!s.noteMiss("B"));                       // a different image: blank
    QCOMPARE(s.state(), LoupeShown::None);
}

void tst_loupeshown::fullSurvivesForcedReloadMiss()
{
    LoupeShown s;
    s.noteHit("A");
    QVERIFY(s.noteMiss("A"));                        // invalidated: keep it until the
    QVERIFY(s.isFull("A"));                          // new decode repaints
}

void tst_loupeshown::renderPromotesInterim()
{
    LoupeShown s;
    s.noteInterim("A");
    s.noteRender("B");                               // a stale render changes nothing
    QCOMPARE(s.state(), LoupeShown::Interim);
    s.noteRender("A");
    QVERIFY(s.isFull("A"));
}

void tst_loupeshown::clearShowsNothing()
{
    LoupeShown s;
    s.noteHit("A");
    s.clear();
    QVERIFY(!s.isShowing("A"));
    QVERIFY(!s.isShowing(""));
    QVERIFY(!s.noteMiss("A"));
}

void tst_loupeshown::placeholderIsNotFull()
{
    // a Preview miss shows the thumbnail scaled up (4.4.14): still owed the real pixels
    LoupeShown s;
    s.notePlaceholder("B");
    QVERIFY(s.isShowing("B"));
    QVERIFY(!s.isFull("B"));
    QVERIFY(!s.canSkipLoad("B", false));        // a later load of B must not be skipped
    QCOMPARE(QString(s.stateName()), QString("Placeholder"));
}

void tst_loupeshown::placeholderSurvivesMissAndHitReplacesIt()
{
    LoupeShown s;
    s.notePlaceholder("B");
    QVERIFY(s.noteMiss("B"));                   // re-selecting B: keep the thumbnail
    QCOMPARE(s.state(), LoupeShown::Placeholder);
    s.noteHit("B");
    QVERIFY(s.isFull("B"));
    s.notePlaceholder("C");
    QVERIFY(!s.noteMiss("A"));                  // a miss on another image blanks
    QCOMPARE(s.state(), LoupeShown::None);
}

QTEST_GUILESS_MAIN(tst_loupeshown)
#include "tst_loupeshown.moc"
