#include <QtTest>
#include <QBuffer>
#include <QDataStream>

#include "Utilities/zchunkdevice.h"

/*
    The chunked zlib stream under the library snapshot (Utilities/zchunkdevice.h). A
    QDataStream on top must read back exactly what it wrote across chunk boundaries -- the
    chunk is made small here so one stream spans dozens -- and a stream cut short or
    damaged must FAIL rather than end quietly, because the reader would otherwise hand back
    a partial Library as a whole one.
*/
class tst_zchunkdevice : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsAcrossChunks();
    void emptyStreamRoundTrips();
    void truncatedStreamFails();
    void corruptChunkFails();

private:
    static QByteArray writeSample(int chunk, QStringList &strings, QVector<qint32> &ints)
    {
        for (int i = 0; i < 5000; ++i) {
            strings << QString("/Volumes/Photos/2024/%1/IMG_%2.NEF").arg(i % 37).arg(i);
            ints << i * 7 - 3;
        }
        QByteArray file;
        QBuffer buf(&file);
        buf.open(QIODevice::WriteOnly);
        ZChunkDevice z(&buf, chunk, 1);
        z.open(QIODevice::WriteOnly);
        QDataStream out(&z);
        out << strings << ints << QString("end");
        z.close();
        return file;
    }
};

void tst_zchunkdevice::roundTripsAcrossChunks()
{
    QStringList strings;
    QVector<qint32> ints;
    const QByteArray file = writeSample(8192, strings, ints);
    QVERIFY(file.size() > 0);

    QBuffer buf(const_cast<QByteArray *>(&file));
    buf.open(QIODevice::ReadOnly);
    ZChunkDevice z(&buf, 8192, 1);
    QVERIFY(z.open(QIODevice::ReadOnly));
    QDataStream in(&z);
    QStringList s2;
    QVector<qint32> i2;
    QString tail;
    in >> s2 >> i2 >> tail;
    QCOMPARE(in.status(), QDataStream::Ok);
    QCOMPARE(s2, strings);
    QCOMPARE(i2, ints);
    QCOMPARE(tail, QString("end"));
    QVERIFY(z.streamError().isEmpty());

    // well compressed: these paths repeat heavily
    qint64 raw = 0;
    for (const QString &s : strings) raw += s.size() * 2;
    QVERIFY(file.size() < raw / 3);
}

void tst_zchunkdevice::emptyStreamRoundTrips()
{
    QByteArray file;
    {
        QBuffer buf(&file);
        buf.open(QIODevice::WriteOnly);
        ZChunkDevice z(&buf);
        z.open(QIODevice::WriteOnly);
        z.close();
    }
    QCOMPARE(file.size(), 4);                       // just the end mark
    QBuffer buf(&file);
    buf.open(QIODevice::ReadOnly);
    ZChunkDevice z(&buf);
    QVERIFY(z.open(QIODevice::ReadOnly));
    char c;
    QCOMPARE(z.read(&c, 1), qint64(0));
}

void tst_zchunkdevice::truncatedStreamFails()
{
    QStringList strings;
    QVector<qint32> ints;
    QByteArray file = writeSample(8192, strings, ints);
    file.chop(file.size() / 2);                     // cut mid-stream

    QBuffer buf(&file);
    buf.open(QIODevice::ReadOnly);
    ZChunkDevice z(&buf, 8192, 1);
    QVERIFY(z.open(QIODevice::ReadOnly));
    QDataStream in(&z);
    QStringList s2;
    QVector<qint32> i2;
    in >> s2 >> i2;
    QVERIFY(in.status() != QDataStream::Ok);
    QVERIFY(!z.streamError().isEmpty());
}

void tst_zchunkdevice::corruptChunkFails()
{
    QStringList strings;
    QVector<qint32> ints;
    QByteArray file = writeSample(8192, strings, ints);
    for (int i = 40; i < 80; ++i) file[i] = char(file[i] ^ 0x5a);   // inside chunk 1

    QBuffer buf(&file);
    buf.open(QIODevice::ReadOnly);
    ZChunkDevice z(&buf, 8192, 1);
    QVERIFY(z.open(QIODevice::ReadOnly));
    QDataStream in(&z);
    QStringList s2;
    in >> s2;
    QVERIFY(in.status() != QDataStream::Ok);
}

QTEST_MAIN(tst_zchunkdevice)
#include "tst_zchunkdevice.moc"
