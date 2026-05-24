// Unit tests for IFD::readIFD — the TIFF/EXIF Image File Directory walk.
//
// An IFD is: a 2-byte entry count, then N 12-byte entries (tagId, tagType,
// tagCount, value/offset), then a 4-byte "next IFD" offset. readIFD parses each
// entry into ifdDataHash and returns the next-IFD offset. It reads from a QFile,
// so the fixture is a hand-built IFD written to a QTemporaryFile.
//
// readIFD does not dereference value offsets — it stores the raw 4-byte value —
// so inline values suffice. It also has a special case: a big-endian short
// (tagType 3, tagCount 1) is read from the first 2 bytes of the value field.

#include <QtTest>
#include <QTemporaryFile>
#include "Metadata/ifd.h"
#include "utilities.h"

class TstIfd : public QObject
{
    Q_OBJECT
private slots:
    void readIFD_bigEndian();
    void readIFD_littleEndian();

private:
    // Build one 12-byte IFD entry. value4 must be exactly 4 bytes.
    static QByteArray entry(quint16 id, quint16 type, quint32 count,
                            const QByteArray &value4, bool bigEnd)
    {
        Q_ASSERT(value4.size() == 4);
        return Utilities::put16(id, bigEnd) + Utilities::put16(type, bigEnd)
             + Utilities::put32(count, bigEnd) + value4;
    }

    // Write bytes to a temp file and open `p.file` on it (read-only).
    static bool loadIntoFile(MetadataParameters &p, QTemporaryFile &tmp,
                             const QByteArray &bytes)
    {
        if (!tmp.open()) return false;
        tmp.write(bytes);
        tmp.flush();
        p.file.setFileName(tmp.fileName());
        return p.file.open(QIODevice::ReadOnly);
    }
};

void TstIfd::readIFD_bigEndian()
{
    const bool be = true;
    QByteArray ifd;
    ifd += Utilities::put16(3, be);                                   // entry count
    // ImageWidth (256), LONG(4), count 1, value 1920 -> else branch (get32)
    ifd += entry(256, 4, 1, Utilities::put32(1920, be), be);
    // Orientation (274), SHORT(3), count 1 -> big-endian special case:
    // value read from the first 2 bytes (6 = rotate 90 CW), next 2 skipped.
    ifd += entry(274, 3, 1, Utilities::put16(6, be) + Utilities::put16(0, be), be);
    // ImageHeight (257), LONG(4), count 1, value 1080
    ifd += entry(257, 4, 1, Utilities::put32(1080, be), be);
    ifd += Utilities::put32(0, be);                                   // next IFD = none

    MetadataParameters p;
    p.hash = nullptr;
    p.offset = 0;
    p.report = false;
    QTemporaryFile tmp;
    QVERIFY(loadIntoFile(p, tmp, ifd));

    IFD reader;
    const quint32 next = reader.readIFD(p, be);
    p.file.close();

    QCOMPARE(next, quint32(0));
    QCOMPARE(reader.ifdDataHash.size(), 3);

    QVERIFY(reader.ifdDataHash.contains(256));
    QCOMPARE(reader.ifdDataHash.value(256).tagType,  quint32(4));
    QCOMPARE(reader.ifdDataHash.value(256).tagCount, quint32(1));
    QCOMPARE(reader.ifdDataHash.value(256).tagValue, quint32(1920));

    QCOMPARE(reader.ifdDataHash.value(257).tagValue, quint32(1080));

    // The short special-case path.
    QCOMPARE(reader.ifdDataHash.value(274).tagType,  quint32(3));
    QCOMPARE(reader.ifdDataHash.value(274).tagValue, quint32(6));
}

void TstIfd::readIFD_littleEndian()
{
    const bool be = false;
    QByteArray ifd;
    ifd += Utilities::put16(2, be);                                   // entry count
    ifd += entry(256, 4, 1, Utilities::put32(1920, be), be);
    ifd += entry(257, 4, 1, Utilities::put32(1080, be), be);
    ifd += Utilities::put32(0, be);

    MetadataParameters p;
    p.hash = nullptr;
    p.offset = 0;
    p.report = false;
    QTemporaryFile tmp;
    QVERIFY(loadIntoFile(p, tmp, ifd));

    IFD reader;
    const quint32 next = reader.readIFD(p, be);
    p.file.close();

    QCOMPARE(next, quint32(0));
    QCOMPARE(reader.ifdDataHash.size(), 2);
    QCOMPARE(reader.ifdDataHash.value(256).tagValue, quint32(1920));
    QCOMPARE(reader.ifdDataHash.value(257).tagValue, quint32(1080));
}

QTEST_GUILESS_MAIN(TstIfd)
#include "tst_ifd.moc"
