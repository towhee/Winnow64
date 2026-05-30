// Unit tests for Utilities byte-parsing primitives (get8/16/24/32/64, put*).
//
// These endian-aware readers underpin the entire Metadata/EXIF and ImageFormats
// decoding stack, so a regression here corrupts file reading silently. The tests
// pin the documented behaviour with known byte sequences in both byte orders.

#include <QtTest>
#include "utilities.h"

class TstByteOps : public QObject
{
    Q_OBJECT
private slots:
    void get8_basic();
    void get8_empty();
    void get16_endian();
    void get16_sizeGuard();
    void get24_endian();
    void get32_endian();
    void get64_endian();
    void get24_sizeGuard();
    void get32_sizeGuard();
    void get64_sizeGuard();
    void put8_roundtrip();
    void put16_endian();
    void put32_endian();
    void roundtrip16();
    void roundtrip32();
};

// Build a QByteArray from raw byte values.
static QByteArray bytes(std::initializer_list<int> vals)
{
    QByteArray b;
    for (int v : vals) b.append(static_cast<char>(v));
    return b;
}

void TstByteOps::get8_basic()
{
    QCOMPARE(Utilities::get8(bytes({0xAB})), quint8(0xAB));
    QCOMPARE(Utilities::get8(bytes({0xFF})), quint8(0xFF));
    QCOMPARE(Utilities::get8(bytes({0x00})), quint8(0x00));
}

void TstByteOps::get8_empty()
{
    QCOMPARE(Utilities::get8(QByteArray()), quint8(0));
}

void TstByteOps::get16_endian()
{
    const QByteArray b = bytes({0x12, 0x34});
    QCOMPARE(Utilities::get16(b, true),  quint16(0x1234)); // big-endian
    QCOMPARE(Utilities::get16(b, false), quint16(0x3412)); // little-endian
}

void TstByteOps::get16_sizeGuard()
{
    // get16 explicitly guards size < 2 and returns 0.
    QCOMPARE(Utilities::get16(bytes({0x12}), true), quint16(0));
    QCOMPARE(Utilities::get16(QByteArray(),  true), quint16(0));
}

void TstByteOps::get24_endian()
{
    const QByteArray b = bytes({0x12, 0x34, 0x56});
    QCOMPARE(Utilities::get24(b, true),  quint32(0x123456));
    QCOMPARE(Utilities::get24(b, false), quint32(0x563412));
}

void TstByteOps::get32_endian()
{
    const QByteArray b = bytes({0x12, 0x34, 0x56, 0x78});
    QCOMPARE(Utilities::get32(b, true),  quint32(0x12345678));
    QCOMPARE(Utilities::get32(b, false), quint32(0x78563412));
}

void TstByteOps::get64_endian()
{
    const QByteArray b = bytes({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF});
    QCOMPARE(Utilities::get64(b, true),  quint64(Q_UINT64_C(0x0123456789ABCDEF)));
    QCOMPARE(Utilities::get64(b, false), quint64(Q_UINT64_C(0xEFCDAB8967452301)));
}

void TstByteOps::put8_roundtrip()
{
    QCOMPARE(Utilities::get8(Utilities::put8(0xAB)), quint8(0xAB));
}

void TstByteOps::put16_endian()
{
    QCOMPARE(Utilities::put16(0x1234, true),  bytes({0x12, 0x34}));
    QCOMPARE(Utilities::put16(0x1234, false), bytes({0x34, 0x12}));
}

void TstByteOps::put32_endian()
{
    QCOMPARE(Utilities::put32(0x12345678, true),  bytes({0x12, 0x34, 0x56, 0x78}));
    QCOMPARE(Utilities::put32(0x12345678, false), bytes({0x78, 0x56, 0x34, 0x12}));
}

void TstByteOps::roundtrip16()
{
    for (quint16 v : {quint16(0), quint16(1), quint16(0x00FF),
                      quint16(0xFF00), quint16(0xFFFF), quint16(0x1234)}) {
        QCOMPARE(Utilities::get16(Utilities::put16(v, true),  true),  v);
        QCOMPARE(Utilities::get16(Utilities::put16(v, false), false), v);
    }
}

void TstByteOps::roundtrip32()
{
    for (quint32 v : {0u, 1u, 0x000000FFu, 0xFF000000u, 0xFFFFFFFFu, 0x12345678u}) {
        QCOMPARE(Utilities::get32(Utilities::put32(v, true),  true),  v);
        QCOMPARE(Utilities::get32(Utilities::put32(v, false), false), v);
    }
}

// Too-short (but non-empty) buffers must return 0, not read past the end.
// get16 always guarded this; get24/32/40/48/64 now do too.
void TstByteOps::get24_sizeGuard()
{
    QCOMPARE(Utilities::get24(bytes({0x12, 0x34}), true),  quint32(0));
    QCOMPARE(Utilities::get24(bytes({0x12, 0x34}), false), quint32(0));
}

void TstByteOps::get32_sizeGuard()
{
    QCOMPARE(Utilities::get32(bytes({0x12, 0x34, 0x56}), true), quint32(0));
}

void TstByteOps::get64_sizeGuard()
{
    QCOMPARE(Utilities::get64(bytes({0x01, 0x23, 0x45, 0x67}), true), quint64(0));
}

QTEST_GUILESS_MAIN(TstByteOps)
#include "tst_byteops.moc"
