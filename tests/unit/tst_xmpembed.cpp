#include <QtTest>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>

#include "Metadata/xmpembed.h"
#include "Metadata/xmp.h"

/*
    Writing XMP INTO image files -- what "Permit image file modification" turns on, and
    what Lightroom Classic does for JPEG, TIFF, PNG and DNG, where it ignores sidecars.

    THE PROPERTY UNDER TEST IS WHAT DOES NOT CHANGE. A metadata write that damages the
    image is the one failure a user cannot recover from, so every splice here is checked
    three ways: the new packet reads back, the image still decodes to the same pixels,
    and the bytes outside the packet are exactly the bytes that were there before.

    The JPEG and PNG fixtures are real images encoded by Qt; the TIFFs are built by hand
    in both byte orders, because the TIFF path edits the file in place and has to be
    checked at the level of individual offsets.
*/

using XmpEmbed::Result;

namespace {

QByteArray doc(const QByteArray &title)
{
    return "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF "
           "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
           "<rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
           "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">" + title
           + "</rdf:li></rdf:Alt></dc:title></rdf:Description></rdf:RDF></x:xmpmeta>";
}

QImage testImage()
{
    QImage im(16, 12, QImage::Format_RGB32);
    for (int y = 0; y < im.height(); ++y)
        for (int x = 0; x < im.width(); ++x)
            im.setPixel(x, y, qRgb(x * 15, y * 20, (x + y) * 7));
    return im;
}

QByteArray encode(const QImage &im, const char *format)
{
    QByteArray b;
    QBuffer buf(&b);
    buf.open(QIODevice::WriteOnly);
    im.save(&buf, format, 100);
    return b;
}

QImage decode(const QByteArray &b)
{
    QImage im;
    im.loadFromData(b);
    return im.convertToFormat(QImage::Format_RGB32);
}

const QByteArray kJpegNs("http://ns.adobe.com/xap/1.0/\0", 29);

/* A 1x1 8-bit greyscale TIFF: header, the pixel at 8, IFD0 at 10. */
QByteArray tiff(bool big)
{
    auto p16 = [&](QByteArray &b, quint16 v) {
        if (big) { b.append(char(v >> 8)); b.append(char(v)); }
        else { b.append(char(v)); b.append(char(v >> 8)); }
    };
    auto p32 = [&](QByteArray &b, quint32 v) {
        for (int i = 0; i < 4; ++i)
            b.append(char(v >> (big ? 8 * (3 - i) : 8 * i)));
    };
    QByteArray b(big ? "MM" : "II");
    p16(b, 42);
    p32(b, 10);
    b.append(char(0x80));           // the pixel
    b.append('\0');                 // word alignment
    struct E { quint16 tag, type; quint32 count, value; };
    const QList<E> entries = {
        {256, 3, 1, 1}, {257, 3, 1, 1}, {258, 3, 1, 8}, {259, 3, 1, 1},
        {262, 3, 1, 1}, {273, 4, 1, 8}, {274, 3, 1, 1}, {277, 3, 1, 1}, {278, 3, 1, 1},
        // Copyright "ABC\0": a tag AFTER 700, so the insertion has to sort
        {279, 4, 1, 1}, {33432, 2, 4, 0},
    };
    p16(b, quint16(entries.size()));
    for (const E &e : entries) {
        p16(b, e.tag);
        p16(b, e.type);
        p32(b, e.count);
        // a SHORT is left-justified in the 4-byte value field
        if (e.type == 3) { p16(b, quint16(e.value)); p16(b, 0); }
        else if (e.type == 2) b += QByteArray("ABC\0", 4);
        else p32(b, e.value);
    }
    p32(b, 0);                      // no next IFD
    return b;
}

quint32 ifd0Of(const QByteArray &b)
{
    const bool big = b.startsWith("MM");
    quint32 v = 0;
    for (int i = 0; i < 4; ++i) {
        const quint32 byte = uchar(b[4 + i]);
        v |= big ? byte << (8 * (3 - i)) : byte << (8 * i);
    }
    return v;
}

QList<quint16> tagsAt(const QByteArray &b, quint32 ifd)
{
    const bool big = b.startsWith("MM");
    auto g16 = [&](qint64 i) {
        return big ? quint16((uchar(b[i]) << 8) | uchar(b[i + 1]))
                   : quint16(uchar(b[i]) | (uchar(b[i + 1]) << 8));
    };
    QList<quint16> tags;
    const quint16 n = g16(ifd);
    for (int i = 0; i < n; ++i) tags << g16(ifd + 2 + i * 12);
    return tags;
}

/* A real JPEG with an Exif APP1 after its JFIF segment: a little-endian TIFF header and
   an IFD0 holding one entry, Orientation = 1. */
QByteArray jpegWithOrientation()
{
    const QByteArray plain = encode(testImage(), "JPG");
    QByteArray tiffPart("II*\0\x08\0\0\0", 8);
    tiffPart += QByteArray("\x01\0", 2);                          // one entry
    tiffPart += QByteArray("\x12\x01\x03\0\x01\0\0\0\x01\0\0\0", 12);  // 274 SHORT 1 = 1
    tiffPart += QByteArray(4, '\0');                               // no next IFD
    QByteArray payload = QByteArray("Exif\0\0", 6) + tiffPart;
    QByteArray seg("\xFF\xE1", 2);
    const int len = payload.size() + 2;
    seg.append(char(len >> 8));
    seg.append(char(len & 0xFF));
    seg += payload;
    const qint64 app0End = 2 + 2 + ((uchar(plain[4]) << 8) | uchar(plain[5]));
    return plain.left(app0End) + seg + plain.mid(app0End);
}

int changedBytes(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) return -1;
    int n = 0;
    for (qint64 i = 0; i < a.size(); ++i) if (a[i] != b[i]) ++n;
    return n;
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(bytes), bytes.size());
}

}  // namespace

class tst_xmpembed : public QObject
{
    Q_OBJECT

private slots:
    void jpegInsertThenReplace();
    void jpegGoesAfterExif();
    void jpegTooLargeIsRefused();
    void jpegNotAJpeg();
    void pngInsertThenReplace();
    void tiffAddsTagWithoutMovingAnything_data();
    void tiffAddsTagWithoutMovingAnything();
    void tiffRepointsExistingTag();
    void bigTiffIsUnsupported();
    void writeThenReadThroughTheFile();
    void xmpWritesLightroomShapes();
    void jpegOrientationPatchedInPlace();
    void tiffOrientationPatchedInPlace_data();
    void tiffOrientationPatchedInPlace();
    void orientationWithNothingToPatch();
    void xmpKeepsForeignProperties();

private:
    QTemporaryDir tmp;
};

void tst_xmpembed::jpegInsertThenReplace()
{
    const QImage im = testImage();
    const QByteArray src = encode(im, "JPG");
    QVERIFY(!src.isEmpty());

    QByteArray meta;
    QCOMPARE(XmpEmbed::readJpeg(src, meta), Result::NoPacket);

    QByteArray once;
    QCOMPARE(XmpEmbed::spliceJpeg(src, doc("First"), once), Result::Ok);
    QCOMPARE(XmpEmbed::readJpeg(once, meta), Result::Ok);
    QCOMPARE(meta, doc("First"));
    QCOMPARE(decode(once), decode(src));

    // everything after the inserted segment is the original, byte for byte
    const qint64 at = once.indexOf(kJpegNs) - 4;
    QVERIFY(at > 0);
    const qint64 segLen = 2 + ((uchar(once[at + 2]) << 8) | uchar(once[at + 3]));
    QCOMPARE(once.left(at) + once.mid(at + segLen), src);

    QByteArray twice;
    QCOMPARE(XmpEmbed::spliceJpeg(once, doc("Second"), twice), Result::Ok);
    QCOMPARE(XmpEmbed::readJpeg(twice, meta), Result::Ok);
    QCOMPARE(meta, doc("Second"));
    QCOMPARE(twice.count(kJpegNs), 1);             // replaced, not added
    QCOMPARE(decode(twice), decode(src));
}

void tst_xmpembed::jpegGoesAfterExif()
{
/*
    Exif must stay directly after SOI (or after JFIF) to remain valid, so a new XMP
    segment goes after it rather than in front of it.
*/
    const QByteArray plain = encode(testImage(), "JPG");
    QByteArray exif("\xFF\xE1\x00\x10" "Exif\0\0" "IIxxxxxx", 16);
    exif[3] = char(14);
    QVERIFY(plain.startsWith("\xFF\xD8"));
    // SOI, APP0 (JFIF), then our Exif, then the rest
    const qint64 app0End = 2 + 2 + ((uchar(plain[4]) << 8) | uchar(plain[5]));
    const QByteArray src = plain.left(app0End) + exif.left(16) + plain.mid(app0End);

    QByteArray out;
    QCOMPARE(XmpEmbed::spliceJpeg(src, doc("X"), out), Result::Ok);
    QVERIFY(out.indexOf("Exif") < out.indexOf(kJpegNs));
    QCOMPARE(out.indexOf("Exif"), src.indexOf("Exif"));
}

void tst_xmpembed::jpegTooLargeIsRefused()
{
    const QByteArray src = encode(testImage(), "JPG");
    QByteArray out;
    QCOMPARE(XmpEmbed::spliceJpeg(src, doc(QByteArray(70000, 'a')), out), Result::TooLarge);
    QVERIFY(out.isEmpty());
}

void tst_xmpembed::jpegNotAJpeg()
{
    QByteArray out, meta;
    QCOMPARE(XmpEmbed::spliceJpeg("not a jpeg at all", doc("X"), out), Result::Malformed);
    // a truncated header: a segment length that runs past the end
    QCOMPARE(XmpEmbed::readJpeg(QByteArray("\xFF\xD8\xFF\xE1\x40\x00", 6), meta),
             Result::Malformed);
}

void tst_xmpembed::pngInsertThenReplace()
{
    const QImage im = testImage();
    const QByteArray src = encode(im, "PNG");
    QVERIFY(!src.isEmpty());

    QByteArray meta, once, twice;
    QCOMPARE(XmpEmbed::readPng(src, meta), Result::NoPacket);
    QCOMPARE(XmpEmbed::splicePng(src, doc("First"), once), Result::Ok);
    QCOMPARE(XmpEmbed::readPng(once, meta), Result::Ok);
    QCOMPARE(meta, doc("First"));
    QVERIFY(once.indexOf("iTXt") < once.indexOf("IDAT"));
    QCOMPARE(decode(once), decode(src));

    QCOMPARE(XmpEmbed::splicePng(once, doc("Second"), twice), Result::Ok);
    QCOMPARE(XmpEmbed::readPng(twice, meta), Result::Ok);
    QCOMPARE(meta, doc("Second"));
    QCOMPARE(twice.count("XML:com.adobe.xmp"), 1);
    QCOMPARE(decode(twice), decode(src));

    /* The chunk's CRC, recomputed here independently: libpng accepts a bad CRC on an
       ancillary chunk with only a warning, so decoding alone would not catch one. */
    const qint64 at = twice.indexOf("iTXt") - 4;
    const quint32 len = (quint32(uchar(twice[at])) << 24) | (uchar(twice[at + 1]) << 16)
                        | (uchar(twice[at + 2]) << 8) | uchar(twice[at + 3]);
    quint32 c = 0xFFFFFFFFu;
    for (qint64 i = at + 4; i < at + 8 + len; ++i) {
        c ^= uchar(twice[i]);
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    c ^= 0xFFFFFFFFu;
    const qint64 crcAt = at + 8 + len;
    const quint32 stored = (quint32(uchar(twice[crcAt])) << 24)
                           | (uchar(twice[crcAt + 1]) << 16)
                           | (uchar(twice[crcAt + 2]) << 8) | uchar(twice[crcAt + 3]);
    QCOMPARE(stored, c);
}

void tst_xmpembed::tiffAddsTagWithoutMovingAnything_data()
{
    QTest::addColumn<bool>("big");
    QTest::newRow("little-endian") << false;
    QTest::newRow("big-endian") << true;
}

void tst_xmpembed::tiffAddsTagWithoutMovingAnything()
{
/*
    No tag 700 yet: the packet and a copy of IFD0 with the entry added go on the END,
    and only the header's 4-byte IFD0 pointer changes. The original IFD stays where it
    was, unreferenced, so the file is recoverable at every step.
*/
    QFETCH(bool, big);
    const QString path = tmp.filePath(big ? "mm.tif" : "ii.tif");
    const QByteArray src = tiff(big);
    writeFile(path, src);

    QByteArray meta;
    QCOMPARE(XmpEmbed::readTiff(path, meta), Result::NoPacket);
    QCOMPARE(XmpEmbed::appendTiff(path, doc("First")), Result::Ok);

    const QByteArray out = readFile(path);
    QCOMPARE(out.left(4), src.left(4));
    QCOMPARE(out.mid(8, src.size() - 8), src.mid(8));       // nothing existing moved
    QVERIFY(ifd0Of(out) >= quint32(src.size()));
    QCOMPARE(ifd0Of(out) % 2, 0u);
    const QList<quint16> tags = tagsAt(out, ifd0Of(out));
    QCOMPARE(tags, (QList<quint16>{256, 257, 258, 259, 262, 273, 274, 277, 278, 279, 700,
                                   33432}));

    QCOMPARE(XmpEmbed::readTiff(path, meta), Result::Ok);
    QCOMPARE(meta, doc("First"));

    if (QImageReader::supportedImageFormats().contains("tiff")) {
        QImage im(path);
        QCOMPARE(im.size(), QSize(1, 1));
        QCOMPARE(qGray(im.pixel(0, 0)), 0x80);
    }
}

void tst_xmpembed::tiffRepointsExistingTag()
{
/*
    Tag 700 present: the new packet is appended and ONLY that entry's count and offset
    change. The header, IFD0's position and every other byte stay put.
*/
    const QString path = tmp.filePath("repoint.tif");
    writeFile(path, tiff(false));
    QCOMPARE(XmpEmbed::appendTiff(path, doc("First")), Result::Ok);
    const QByteArray before = readFile(path);

    QCOMPARE(XmpEmbed::appendTiff(path, doc("Second")), Result::Ok);
    const QByteArray after = readFile(path);

    QByteArray meta;
    QCOMPARE(XmpEmbed::readTiff(path, meta), Result::Ok);
    QCOMPARE(meta, doc("Second"));
    QCOMPARE(ifd0Of(after), ifd0Of(before));
    QVERIFY(after.size() > before.size());

    // only the 700 entry's count and offset differ within the old extent
    const qint64 entry = ifd0Of(before) + 2 + 10 * 12;        // 700 is the 11th entry
    for (qint64 i = 0; i < before.size(); ++i) {
        if (i >= entry + 4 && i < entry + 12) continue;
        if (before[i] != after[i]) QFAIL(qPrintable(QString("byte %1 changed").arg(i)));
    }
}

void tst_xmpembed::bigTiffIsUnsupported()
{
    const QString path = tmp.filePath("big.tif");
    QByteArray b = tiff(false);
    b[2] = 43;                                  // BigTIFF magic
    writeFile(path, b);
    QCOMPARE(XmpEmbed::appendTiff(path, doc("X")), Result::Unsupported);
    QCOMPARE(readFile(path), b);
}

void tst_xmpembed::writeThenReadThroughTheFile()
{
/*
    The public pair, through a real file and a real Xmp: an edited title survives the
    trip into the JPEG and back, accents and CJK included (they used to go through
    toLatin1()).
*/
    const QString path = tmp.filePath("roundtrip.jpg");
    writeFile(path, encode(testImage(), "JPG"));

    QByteArray meta;
    QCOMPARE(XmpEmbed::read(path, meta), Result::NoPacket);
    Xmp xmp(meta, 0);
    QVERIFY(xmp.isValid);
    const QString title = QString::fromUtf8("Café 東京");
    QVERIFY(xmp.setItem("title", title.toUtf8()));
    QVERIFY(xmp.setItem("Rating", "4"));
    QCOMPARE(XmpEmbed::write(path, xmp.docToByteArray()), Result::Ok);

    QCOMPARE(XmpEmbed::read(path, meta), Result::Ok);
    Xmp back(meta, 0);
    QVERIFY(back.isValid);
    QCOMPARE(back.getItem("title"), title);
    QCOMPARE(back.getItem("Rating"), QString("4"));
    QCOMPARE(decode(readFile(path)), decode(encode(testImage(), "JPG")));
}

void tst_xmpembed::xmpWritesLightroomShapes()
{
/*
    dc:title and dc:rights are language alternatives and dc:creator an ordered list --
    the shapes the XMP spec requires and Lightroom reads. Winnow wrote all three as
    plain attributes. Email and url live in Iptc4xmpCore:CreatorContactInfo INSIDE
    rdf:Description; the skeleton used to put it outside, where no one else looks.
*/
    Xmp xmp(QByteArray(), 0);
    QVERIFY(xmp.isValid);
    QVERIFY(xmp.setItem("title", "Heron"));
    QVERIFY(xmp.setItem("rights", "2026 R Hill"));
    QVERIFY(xmp.setItem("creator", "R Hill"));
    QVERIFY(xmp.setItem("email", "a@b.c"));
    // Xmp's printer writes attributes as name = "value"; compare without the spaces
    const QByteArray out = xmp.docToByteArray().replace(" = ", "=");

    QVERIFY(out.contains("<dc:title>"));
    QVERIFY(out.contains("<rdf:Alt>"));
    QVERIFY(out.contains("xml:lang=\"x-default\""));
    QVERIFY(out.contains("<dc:creator>"));
    QVERIFY(out.contains("<rdf:Seq>"));
    QVERIFY(!out.contains("dc:title=\""));
    const qint64 descOpen = out.indexOf("<rdf:Description");
    const qint64 descClose = out.indexOf("</rdf:Description>");
    const qint64 contact = out.indexOf("<Iptc4xmpCore:CreatorContactInfo");
    QVERIFY(contact > descOpen && contact < descClose);

    Xmp back(out, 0);
    QCOMPARE(back.getItem("title"), QString("Heron"));
    QCOMPARE(back.getItem("rights"), QString("2026 R Hill"));
    QCOMPARE(back.getItem("creator"), QString("R Hill"));
    QCOMPARE(back.getItem("email"), QString("a@b.c"));

    // an empty value clears it
    QVERIFY(back.setItem("title", ""));
    QVERIFY(!back.docToByteArray().contains("dc:title"));
}

void tst_xmpembed::xmpKeepsForeignProperties()
{
/*
    A packet lifted out of an image carries other applications' data -- Lightroom's
    develop settings above all. Editing the rating must leave them in the document.
*/
    const QByteArray src =
        "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF "
        "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">"
        "<rdf:Description rdf:about=\"\" xmlns:crs=\"http://ns.adobe.com/camera-raw-settings/1.0/\" "
        "crs:Exposure2012=\"+0.35\">"
        "<crs:ToneCurvePV2012><rdf:Seq><rdf:li>0, 0</rdf:li></rdf:Seq></crs:ToneCurvePV2012>"
        "</rdf:Description></rdf:RDF></x:xmpmeta>";
    Xmp xmp(src, 0);
    QVERIFY(xmp.isValid);
    QVERIFY(xmp.setItem("Rating", "5"));
    const QByteArray out = xmp.docToByteArray().replace(" = ", "=");
    QVERIFY(out.contains("crs:Exposure2012=\"+0.35\""));
    QVERIFY(out.contains("<crs:ToneCurvePV2012>"));
    QVERIFY(out.contains("<rdf:li>0, 0</rdf:li>"));
    QVERIFY(out.contains("xmp:Rating=\"5\""));
}

void tst_xmpembed::jpegOrientationPatchedInPlace()
{
/*
    Rotation into the file is a 2-byte patch of the value already in IFD0's Orientation
    entry: one byte differs (little-endian 1 -> 6), the file is the same size, and the
    pixels decode as before (the decoder is told nothing about orientation here).
*/
    const QString path = tmp.filePath("orient.jpg");
    const QByteArray src = jpegWithOrientation();
    writeFile(path, src);

    int o = 0;
    QCOMPARE(XmpEmbed::readOrientation(path, o), Result::Ok);
    QCOMPARE(o, 1);
    QCOMPARE(XmpEmbed::writeOrientation(path, 6), Result::Ok);
    QCOMPARE(XmpEmbed::readOrientation(path, o), Result::Ok);
    QCOMPARE(o, 6);
    const QByteArray out = readFile(path);
    QCOMPARE(changedBytes(src, out), 1);
    QCOMPARE(decode(out), decode(src));

    QCOMPARE(XmpEmbed::writeOrientation(path, 9), Result::Malformed);   // not an orientation
    QCOMPARE(readFile(path), out);
}

void tst_xmpembed::tiffOrientationPatchedInPlace_data()
{
    QTest::addColumn<bool>("big");
    QTest::newRow("little-endian") << false;
    QTest::newRow("big-endian") << true;
}

void tst_xmpembed::tiffOrientationPatchedInPlace()
{
    QFETCH(bool, big);
    const QString path = tmp.filePath(big ? "orient-mm.tif" : "orient-ii.tif");
    const QByteArray src = tiff(big);
    writeFile(path, src);

    int o = 0;
    QCOMPARE(XmpEmbed::readOrientation(path, o), Result::Ok);
    QCOMPARE(o, 1);
    QCOMPARE(XmpEmbed::writeOrientation(path, 8), Result::Ok);
    QCOMPARE(XmpEmbed::readOrientation(path, o), Result::Ok);
    QCOMPARE(o, 8);
    QCOMPARE(changedBytes(src, readFile(path)), 1);

    // and the XMP append still finds its way round the patched IFD
    QCOMPARE(XmpEmbed::appendTiff(path, doc("X")), Result::Ok);
    QCOMPARE(XmpEmbed::readOrientation(path, o), Result::Ok);
    QCOMPARE(o, 8);
}

void tst_xmpembed::orientationWithNothingToPatch()
{
/*
    Unsupported -- not an error -- so Metadata::writeOrientation quietly keeps the
    rotation in the sidecar: a PNG, and a JPEG with no Exif segment at all.
*/
    const QString png = tmp.filePath("orient.png");
    writeFile(png, encode(testImage(), "PNG"));
    QCOMPARE(XmpEmbed::writeOrientation(png, 6), Result::Unsupported);

    const QString jpg = tmp.filePath("noexif.jpg");
    const QByteArray src = encode(testImage(), "JPG");
    writeFile(jpg, src);
    QCOMPARE(XmpEmbed::writeOrientation(jpg, 6), Result::Unsupported);
    QCOMPARE(readFile(jpg), src);
}

QTEST_MAIN(tst_xmpembed)
#include "tst_xmpembed.moc"
