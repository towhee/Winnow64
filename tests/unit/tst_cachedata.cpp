#include <QtTest>
#include <QThread>
#include <atomic>

#include "Cache/cachedata.h"

/*
    Pins Cache/cachedata.h: the decoded-image hash every thread shares (Documentation
    4.4.13). The hash is private now; these check the locked API that replaced direct
    access, and that a hit is always something to paint.

    getNeverReturnsANullHit runs a reader against a writer that inserts, replaces,
    removes and clears the same keys. CompareView and copyImage used to call contains()
    and then read the hash directly -- a gap in which the cache thread trimmed the entry
    and the caller painted a default-constructed (null) image (C25).
*/
class tst_cachedata : public QObject
{
    Q_OBJECT
private slots:
    void nullImageIsNeverStored();
    void byteAccounting();
    void replaceIfPresentNeverResurrects();
    void renameMovesImageAndBytes();
    void forEachAndKeys();
    void getNeverReturnsANullHit();
};

static QImage img(int w, int h = 10)
{
    QImage im(w, h, QImage::Format_ARGB32);
    im.fill(Qt::red);
    return im;
}

void tst_cachedata::nullImageIsNeverStored()
{
    ImageCacheData d(nullptr);
    d.insert("a", QImage());
    QVERIFY(!d.contains("a"));
    QCOMPARE(d.count(), 0);
    QCOMPARE(d.sizeBytes(), quint64(0));
    d.insert("", img(4));                           // nor an empty key
    QCOMPARE(d.count(), 0);
}

void tst_cachedata::byteAccounting()
{
    ImageCacheData d(nullptr);
    d.insert("a", img(10));
    d.insert("b", img(20));
    QCOMPARE(d.sizeBytes(), quint64(img(10).sizeInBytes() + img(20).sizeInBytes()));
    d.insert("a", img(30));                         // replace: old bytes leave
    QCOMPARE(d.sizeBytes(), quint64(img(30).sizeInBytes() + img(20).sizeInBytes()));
    d.remove("b");
    d.remove("missing");                            // no underflow
    QCOMPARE(d.sizeBytes(), quint64(img(30).sizeInBytes()));
    d.clear();
    QCOMPARE(d.sizeBytes(), quint64(0));
    QVERIFY(d.isEmpty());
}

void tst_cachedata::replaceIfPresentNeverResurrects()
{
    ImageCacheData d(nullptr);
    QVERIFY(!d.replaceIfPresent("a", img(10)));     // trimmed meanwhile: stays gone
    QVERIFY(!d.contains("a"));
    d.insert("a", img(10));
    QVERIFY(d.replaceIfPresent("a", img(12)));      // a rotation
    QImage got;
    QVERIFY(d.get("a", got));
    QCOMPARE(got.width(), 12);
    QCOMPARE(d.sizeBytes(), quint64(img(12).sizeInBytes()));
    QVERIFY(!d.replaceIfPresent("a", QImage()));    // never swapped for nothing
}

void tst_cachedata::renameMovesImageAndBytes()
{
    ImageCacheData d(nullptr);
    d.insert("old", img(10));
    d.insert("new", img(20));                       // will be overwritten
    QVERIFY(d.rename("old", "new"));
    QVERIFY(!d.contains("old"));
    QImage got;
    QVERIFY(d.get("new", got));
    QCOMPARE(got.width(), 10);
    QCOMPARE(d.sizeBytes(), quint64(img(10).sizeInBytes()));
    QVERIFY(!d.rename("absent", "x"));
}

void tst_cachedata::forEachAndKeys()
{
    ImageCacheData d(nullptr);
    d.insert("a", img(1));
    d.insert("b", img(2));
    QStringList keys = d.keys();
    keys.sort();
    QCOMPARE(keys, (QStringList{"a", "b"}));
    int widths = 0;
    d.forEach([&](const QString &, const QImage &im) { widths += im.width(); });
    QCOMPARE(widths, 3);
}

void tst_cachedata::getNeverReturnsANullHit()
{
    ImageCacheData d(nullptr);
    std::atomic<bool> stop{false};
    std::atomic<int> nullHits{0};
    std::atomic<int> hits{0};

    QThread *reader = QThread::create([&] {
        while (!stop.load()) {
            for (int k = 0; k < 8; ++k) {
                QImage got;
                if (d.get(QString::number(k), got)) {
                    ++hits;
                    if (got.isNull()) ++nullHits;
                }
                d.contains(QString::number(k));
                d.count();
            }
        }
    });
    reader->start();

    const QImage a = img(16), b = img(24);
    for (int i = 0; i < 20000; ++i) {
        const QString k = QString::number(i % 8);
        switch (i % 5) {
        case 0: d.insert(k, a); break;
        case 1: d.replaceIfPresent(k, b); break;
        case 2: d.remove(k); break;
        case 3: d.insert(k, b); break;
        case 4: if (i % 997 == 4) d.clear(); break;
        }
    }
    stop = true;
    reader->wait();
    delete reader;

    QVERIFY(hits.load() > 0);
    QCOMPARE(nullHits.load(), 0);
    // the byte total still matches what is there
    quint64 sum = 0;
    d.forEach([&](const QString &, const QImage &im) { sum += quint64(im.sizeInBytes()); });
    QCOMPARE(d.sizeBytes(), sum);
}

QTEST_GUILESS_MAIN(tst_cachedata)
#include "tst_cachedata.moc"
