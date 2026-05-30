// Unit tests for Utilities::getReal / getReal_s — IFD rational decoding.
//
// EXIF/TIFF stores rationals as two 4-byte integers (numerator, denominator).
// getReal (IFD type 5, unsigned) and getReal_s (type 10, signed) decode them and
// underpin GPS coordinates, exposure time, f-number, focal length, etc. The
// readers are templated on QIODevice; QBuffer lets us test them fully in memory.

#include <QtTest>
#include <QBuffer>
#include "utilities.h"

class TstRational : public QObject
{
    Q_OBJECT
private slots:
    void getReal_basic();
    void getReal_endian();
    void getReal_zeroDenominator();
    void getReal_atOffset();
    void getReal_signedNegative();
};

// Build an 8-byte rational (numerator, denominator) in the given byte order.
static QByteArray rational(quint32 num, quint32 den, bool bigEnd)
{
    return Utilities::put32(num, bigEnd) + Utilities::put32(den, bigEnd);
}

static QByteArray bytes(std::initializer_list<int> vals)
{
    QByteArray b;
    for (int v : vals) b.append(static_cast<char>(v));
    return b;
}

void TstRational::getReal_basic()
{
    QByteArray ba = rational(1, 2, true);
    QBuffer buf(&ba); buf.open(QIODevice::ReadOnly);
    QCOMPARE(Utilities::getReal(buf, 0, true), 0.5);
}

void TstRational::getReal_endian()
{
    QByteArray be = rational(24, 10, true);
    QBuffer b1(&be); b1.open(QIODevice::ReadOnly);
    QCOMPARE(Utilities::getReal(b1, 0, true), 2.4);   // e.g. f/2.4

    QByteArray le = rational(24, 10, false);
    QBuffer b2(&le); b2.open(QIODevice::ReadOnly);
    QCOMPARE(Utilities::getReal(b2, 0, false), 2.4);
}

void TstRational::getReal_zeroDenominator()
{
    // Division-by-zero is guarded and returns 0.
    QByteArray ba = rational(5, 0, true);
    QBuffer buf(&ba); buf.open(QIODevice::ReadOnly);
    QCOMPARE(Utilities::getReal(buf, 0, true), 0.0);
}

void TstRational::getReal_atOffset()
{
    // Rational placed 2 bytes in; the reader must seek to the offset.
    QByteArray ba = bytes({0xAA, 0xBB}) + rational(3, 4, true);
    QBuffer buf(&ba); buf.open(QIODevice::ReadOnly);
    QCOMPARE(Utilities::getReal(buf, 2, true), 0.75);
}

void TstRational::getReal_signedNegative()
{
    // numerator 0xFFFFFFFF = -1 signed; /2 = -0.5 via getReal_s.
    QByteArray ba = rational(0xFFFFFFFFu, 2, true);
    QBuffer buf(&ba); buf.open(QIODevice::ReadOnly);
    QCOMPARE(Utilities::getReal_s(buf, 0, true), -0.5);
    // The unsigned reader treats the same bytes as a huge positive value.
    QVERIFY(Utilities::getReal(buf, 0, true) > 2.0e9);
}

QTEST_GUILESS_MAIN(TstRational)
#include "tst_rational.moc"
