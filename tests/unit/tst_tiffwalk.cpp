/*
    TiffWalk::Reader::reals() -- interpreting a TIFF tag's values as doubles.

    WHY THIS TEST EXISTS. reals() switches on the TIFF type, and a type it does not name
    falls to a default that reads 4 bytes as an integer. That default is SILENT: it
    returns plausible-looking numbers for a type it cannot actually decode, so nothing
    upstream can tell a parsed value from a misparsed one.

    That is not hypothetical. Type 12 (DOUBLE) was missing, and DNG's NoiseProfile (tag
    51041) is DOUBLE -- so every DNG handed PMRID the low 4 mantissa bytes of each value
    read as an integer: k came back as ~3.8e9 instead of ~6.9e-5. The KSigma transform
    built from it multiplied the image away to a constant and divided the result by
    2.7e-13, clamping the whole mosaic to black; the render then showed a magenta cast
    (see notes/Documentation.txt, "NOISE MODEL (k,b)").

    Pinned here:
      o DOUBLE round-trips exactly, in BOTH endiannesses -- the two 4-byte halves have to
        be assembled in the right order, which is the easy half of this to get wrong.
      o the types that were already handled still decode (RATIONAL, FLOAT, SHORT), so the
        added case cannot have displaced one.
      o an out-of-line value (> 4 bytes, so read from the file at an offset) is what a
        real NoiseProfile is -- a DOUBLE is never inline.
*/

#include <QtTest>
#include <QTemporaryFile>
#include <cstring>

#include "ImageFormats/Raw/tiffwalk.h"

class TstTiffWalk : public QObject
{
    Q_OBJECT

private slots:
    void doubleTagRoundTrips_data();
    void doubleTagRoundTrips();
    void previouslyHandledTypesStillDecode_data();
    void previouslyHandledTypesStillDecode();

private:
    static QByteArray put16(quint16 v, bool be)
    {
        QByteArray a(2, '\0');
        uchar *p = reinterpret_cast<uchar *>(a.data());
        if (be) { p[0] = uchar(v >> 8); p[1] = uchar(v); }
        else    { p[0] = uchar(v);      p[1] = uchar(v >> 8); }
        return a;
    }
    static QByteArray put32(quint32 v, bool be)
    {
        QByteArray a(4, '\0');
        uchar *p = reinterpret_cast<uchar *>(a.data());
        if (be) { p[0] = uchar(v >> 24); p[1] = uchar(v >> 16); p[2] = uchar(v >> 8); p[3] = uchar(v); }
        else    { p[0] = uchar(v); p[1] = uchar(v >> 8); p[2] = uchar(v >> 16); p[3] = uchar(v >> 24); }
        return a;
    }
    static QByteArray putDouble(double d, bool be)
    {
        quint64 bits;
        std::memcpy(&bits, &d, 8);
        return put32(quint32(be ? bits >> 32 : bits & 0xffffffffu), be)
             + put32(quint32(be ? bits & 0xffffffffu : bits >> 32), be);
    }

    /* A minimal TIFF holding ONE tag whose value block sits out of line, after the IFD.
       Layout: header(8) | count(2) + entry(12) + next(4) | value bytes. */
    static QByteArray tiffWithTag(quint16 tag, quint16 type, quint32 count,
                                  const QByteArray &value, bool be)
    {
        const quint32 ifdOff = 8;
        const quint32 valOff = ifdOff + 2 + 12 + 4;
        QByteArray out;
        out += be ? "MM" : "II";
        out += put16(42, be);
        out += put32(ifdOff, be);
        out += put16(1, be);                                  // one entry
        out += put16(tag, be) + put16(type, be) + put32(count, be);
        out += value.size() <= 4 ? value.leftJustified(4, '\0') : put32(valOff, be);
        out += put32(0, be);                                  // no next IFD
        if (value.size() > 4) out += value;
        return out;
    }

    /* Run reals() over the single tag in such a file. */
    static QVector<double> readReals(const QByteArray &tiff, quint16 tag, bool be)
    {
        QTemporaryFile tmp;
        if (!tmp.open()) return {};
        tmp.write(tiff);
        tmp.flush();
        QFile f(tmp.fileName());
        if (!f.open(QIODevice::ReadOnly)) return {};
        TiffWalk::Reader r;
        if (!r.init(&f)) return {};
        if (r.big() != be) return {};
        TiffWalk::Ifd tags;
        QList<quint32> subs;
        quint32 next = 0;
        if (!r.readIfd(r.firstIfd(), tags, subs, next)) return {};
        if (!tags.contains(tag)) return {};
        return r.reals(tags[tag]);
    }
};

void TstTiffWalk::doubleTagRoundTrips_data()
{
    QTest::addColumn<bool>("bigEndian");
    QTest::newRow("little endian") << false;
    QTest::newRow("big endian")    << true;
}

void TstTiffWalk::doubleTagRoundTrips()
{
    QFETCH(bool, bigEndian);

    /* The real NoiseProfile from a Nikon D850 DNG: three (scale, offset) pairs, R/G/B.
       The green pair (index 1) is the one PMRID::resolveKB reads. */
    const QVector<double> profile = {
        8.32055390249485e-05, 4.06256592700627e-08,
        6.89879453727016e-05, 2.84416984092492e-08,
        7.72648203250172e-05, 3.65517563894486e-08
    };
    QByteArray value;
    for (double d : profile) value += putDouble(d, bigEndian);
    QCOMPARE(value.size(), 48);          // 6 doubles, necessarily out of line

    const QVector<double> got =
        readReals(tiffWithTag(51041, 12, 6, value, bigEndian), 51041, bigEndian);

    QCOMPARE(got.size(), profile.size());
    for (int i = 0; i < profile.size(); ++i)
        QVERIFY2(qFuzzyCompare(got[i] + 1.0, profile[i] + 1.0),
                 qPrintable(QString("value %1: got %2, want %3")
                                .arg(i).arg(got[i], 0, 'g', 17).arg(profile[i], 0, 'g', 17)));

    /* The specific failure this guards: the integer default returned the low 4 mantissa
       bytes, which for the green scale is 3765724038 -- 14 orders of magnitude out. */
    QVERIFY(got[2] < 1.0);
    QVERIFY(got[2] > 0.0);
}

void TstTiffWalk::previouslyHandledTypesStillDecode_data()
{
    QTest::addColumn<bool>("bigEndian");
    QTest::newRow("little endian") << false;
    QTest::newRow("big endian")    << true;
}

void TstTiffWalk::previouslyHandledTypesStillDecode()
{
    QFETCH(bool, bigEndian);

    // RATIONAL (5): AsShotNeutral-style 1/2 and 3/4.
    QByteArray rat = put32(1, bigEndian) + put32(2, bigEndian)
                   + put32(3, bigEndian) + put32(4, bigEndian);
    const QVector<double> gotRat =
        readReals(tiffWithTag(50728, 5, 2, rat, bigEndian), 50728, bigEndian);
    QCOMPARE(gotRat.size(), 2);
    QCOMPARE(gotRat[0], 0.5);
    QCOMPARE(gotRat[1], 0.75);

    // FLOAT (11), out of line so the offset path is exercised.
    const float fs[2] = {1.5f, -2.25f};
    QByteArray flt;
    for (float f : fs) {
        quint32 bits;
        std::memcpy(&bits, &f, 4);
        flt += put32(bits, bigEndian);
    }
    const QVector<double> gotFlt =
        readReals(tiffWithTag(700, 11, 2, flt, bigEndian), 700, bigEndian);
    QCOMPARE(gotFlt.size(), 2);
    QCOMPARE(gotFlt[0], 1.5);
    QCOMPARE(gotFlt[1], -2.25);

    // SHORT (3), inline (4 bytes).
    const QVector<double> gotShort =
        readReals(tiffWithTag(258, 3, 2, put16(14, bigEndian) + put16(16, bigEndian),
                              bigEndian),
                  258, bigEndian);
    QCOMPARE(gotShort.size(), 2);
    QCOMPARE(gotShort[0], 14.0);
    QCOMPARE(gotShort[1], 16.0);
}

QTEST_MAIN(TstTiffWalk)
#include "tst_tiffwalk.moc"
