/*
    StaleThumb -- recognising an embedded JPEG thumbnail left behind when the image's
    pixels were rotated (Image/stalethumb.h). Pins the shape rule, the orientation turn
    mirrored from Thumb::checkOrientation, and that nothing unknown or square is ever
    called stale.
*/
#include <QtTest>
#include "Image/stalethumb.h"

class TestStaleThumb : public QObject
{
    Q_OBJECT

private slots:
    void onlyJpegIsChecked();
    void portraitImageLandscapeThumbIsStale();
    void matchingShapesAreNotStale();
    void squareOrUnknownIsNeverStale();
    void turnMirrorsCheckOrientation();
    void cachedThumbIsJudgedAfterTheTurn();
};

void TestStaleThumb::onlyJpegIsChecked()
{
    QVERIFY(StaleThumb::isChecked("jpg"));
    QVERIFY(StaleThumb::isChecked("JPEG"));
    QVERIFY(!StaleThumb::isChecked("nef"));
    QVERIFY(!StaleThumb::isChecked("tif"));
    QVERIFY(!StaleThumb::isChecked(""));
}

void TestStaleThumb::portraitImageLandscapeThumbIsStale()
{
    // the Trent River case: SOF 2000x3008, IFD1 thumb 160x106
    QVERIFY(StaleThumb::embeddedIsStale(QSize(160, 106), 2000, 3008));
    QVERIFY(StaleThumb::embeddedIsStale(QSize(106, 160), 3008, 2000));
}

void TestStaleThumb::matchingShapesAreNotStale()
{
    QVERIFY(!StaleThumb::embeddedIsStale(QSize(160, 106), 3008, 2000));
    QVERIFY(!StaleThumb::embeddedIsStale(QSize(106, 160), 2000, 3008));
    // a different aspect of the same shape (a 4:3 thumb on a 3:2 image) is not stale
    QVERIFY(!StaleThumb::embeddedIsStale(QSize(160, 120), 3008, 2000));
}

void TestStaleThumb::squareOrUnknownIsNeverStale()
{
    QVERIFY(!StaleThumb::embeddedIsStale(QSize(160, 160), 2000, 3008));
    QVERIFY(!StaleThumb::embeddedIsStale(QSize(160, 106), 2000, 2000));
    QVERIFY(!StaleThumb::embeddedIsStale(QSize(160, 106), 0, 0));
    QVERIFY(!StaleThumb::embeddedIsStale(QSize(), 2000, 3008));
}

void TestStaleThumb::turnMirrorsCheckOrientation()
{
    QCOMPARE(StaleThumb::iconTurnDegrees(1, 0), 0);
    QCOMPARE(StaleThumb::iconTurnDegrees(3, 0), 180);
    QCOMPARE(StaleThumb::iconTurnDegrees(6, 0), 90);
    QCOMPARE(StaleThumb::iconTurnDegrees(8, 0), 270);
    // rotationDegrees adds only where checkOrientation adds it
    QCOMPARE(StaleThumb::iconTurnDegrees(6, 90), 180);
    QCOMPARE(StaleThumb::iconTurnDegrees(8, 180), 90);
    QCOMPARE(StaleThumb::iconTurnDegrees(1, 90), 0);
    QCOMPARE(StaleThumb::iconTurnDegrees(0, 0), 0);
}

void TestStaleThumb::cachedThumbIsJudgedAfterTheTurn()
{
    // stored 3008x2000 with Orientation 6: the cached icon was turned to portrait
    QVERIFY(!StaleThumb::cachedIsStale(QSize(170, 256), 3008, 2000, 6, 0));
    QVERIFY(StaleThumb::cachedIsStale(QSize(256, 170), 3008, 2000, 6, 0));
    // Orientation 1, upright portrait pixels: a landscape cached icon is stale
    QVERIFY(StaleThumb::cachedIsStale(QSize(256, 170), 2000, 3008, 1, 0));
    QVERIFY(!StaleThumb::cachedIsStale(QSize(170, 256), 2000, 3008, 1, 0));
    // 180 does not change the shape
    QVERIFY(!StaleThumb::cachedIsStale(QSize(256, 170), 3008, 2000, 3, 0));
}

QTEST_GUILESS_MAIN(TestStaleThumb)
#include "tst_stalethumb.moc"
