/*
    StaleThumb -- recognising an embedded JPEG thumbnail left behind when the image's
    pixels were rotated (Image/stalethumb.h). Pins the shape rule, the orientation turn
    mirrored from Thumb::checkOrientation, and that nothing unknown or square is ever
    called stale.
*/
#include <QtTest>
#include <QBuffer>
#include <QImage>
#include <QTemporaryDir>
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
    void fileCheckReadsTheExifThumbnail();
    void fileCheckIsNeverStaleOnAGuess();

private:
    QTemporaryDir tmp;
    static QByteArray jpegOf(int w, int h)
    {
        QImage im(w, h, QImage::Format_RGB32);
        im.fill(Qt::gray);
        QByteArray out;
        QBuffer buf(&out);
        buf.open(QIODevice::WriteOnly);
        im.save(&buf, "JPG", 80);
        return out;
    }
    /*  A JPEG of the main size with an APP1 Exif whose IFD1 holds a thumbnail of the
        thumb size -- the minimum StaleThumb::exifThumbOffset walks: TIFF header, an
        empty IFD0 linking to IFD1, and IFD1's 513/514. Either byte order. */
    QString writeWithThumb(const QString &name, QSize main, QSize thumb, bool le)
    {
        const QByteArray mainJpg = jpegOf(main.width(), main.height());
        const QByteArray thumbJpg = jpegOf(thumb.width(), thumb.height());
        QByteArray t;                                   // TIFF, offsets from its start
        auto put16 = [&](quint16 v) {
            if (le) { t.append(char(v & 0xFF)); t.append(char(v >> 8)); }
            else    { t.append(char(v >> 8));   t.append(char(v & 0xFF)); }
        };
        auto put32 = [&](quint32 v) {
            if (le) { put16(v & 0xFFFF); put16(v >> 16); }
            else    { put16(v >> 16);    put16(v & 0xFFFF); }
        };
        t.append(le ? "II" : "MM");
        put16(42);
        put32(8);                   // IFD0 at 8
        put16(0);                   // IFD0: no entries
        put32(14);                  // next IFD (IFD1) at 14
        put16(2);                   // IFD1: two entries, 14 + 2 + 24 + 4 = 44
        put16(513); put16(4); put32(1); put32(44);
        put16(514); put16(4); put32(1); put32(quint32(thumbJpg.size()));
        put32(0);
        t.append(thumbJpg);
        QByteArray app1("Exif\0\0", 6);
        app1.append(t);
        const int len = app1.size() + 2;
        QByteArray file("\xFF\xD8\xFF\xE1", 4);
        file.append(char(len >> 8));
        file.append(char(len & 0xFF));
        file.append(app1);
        file.append(mainJpg.mid(2));                    // the main image, minus its SOI
        const QString path = tmp.filePath(name);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) f.write(file);
        return path;
    }
};

void TestStaleThumb::onlyJpegIsChecked()
{
    QVERIFY(StaleThumb::isChecked("jpg"));
    QVERIFY(StaleThumb::isChecked("JPEG"));
    QVERIFY(StaleThumb::isChecked("jpe"));
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

void TestStaleThumb::fileCheckReadsTheExifThumbnail()
{
    // the Trent River case, both byte orders: portrait pixels, landscape thumbnail
    for (bool le : {true, false}) {
        const QString stale = writeWithThumb(le ? "s_le.jpg" : "s_be.jpg",
                                             QSize(200, 300), QSize(160, 106), le);
        QCOMPARE(StaleThumb::jpegFrameSize(stale), QSize(200, 300));
        QVERIFY(StaleThumb::fileIsStale(stale));
        const QString good = writeWithThumb(le ? "g_le.jpg" : "g_be.jpg",
                                            QSize(200, 300), QSize(106, 160), le);
        QVERIFY(!StaleThumb::fileIsStale(good));
    }
}

void TestStaleThumb::fileCheckIsNeverStaleOnAGuess()
{
    // no Exif thumbnail at all, a missing file, and something that is not a JPEG
    const QString plain = tmp.filePath("plain.jpg");
    {
        QFile f(plain);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(jpegOf(200, 300));
    }
    QCOMPARE(StaleThumb::jpegFrameSize(plain), QSize(200, 300));
    QVERIFY(!StaleThumb::fileIsStale(plain));
    QVERIFY(!StaleThumb::fileIsStale(tmp.filePath("missing.jpg")));
    const QString junk = tmp.filePath("junk.jpg");
    {
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a jpeg at all");
    }
    QVERIFY(!StaleThumb::fileIsStale(junk));
}

QTEST_MAIN(TestStaleThumb)
#include "tst_stalethumb.moc"
