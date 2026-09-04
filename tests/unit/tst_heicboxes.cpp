#include <QtTest>
#include <QBuffer>
#include <QTemporaryDir>

#include "ImageFormats/Heic/heic.h"
#include "Metadata/exif.h"
#include "Metadata/gps.h"
#include "Metadata/ifd.h"
#include "Metadata/imagemetadata.h"

/*
    THE ISO BASE MEDIA BOX WALK, pinned at the two points where it silently lost a whole
    class of file.

    EVERY iPhone HEIC WAS INVISIBLE TO THIS PARSER. Apple writes mdat with size == 1 --
    the spec's "a 64-bit largesize follows the type" -- and Heic::nextHeifBox treated any
    size below 2 as "extends to end of file". mdat comes FIRST in those files, so the walk
    swallowed everything after it: meta, iinf, iloc and iprp were never seen, no Exif item
    was ever found, and the file was declared unreadable while the loupe (which uses the
    platform decoder) showed it perfectly.

    AND A FILE WITH NO EXIF AT ALL IS STILL A PHOTOGRAPH -- a converted image or a
    screenshot carries none. The container's own ispe still gives the dimensions, so that
    is what the parser falls back to rather than failing.

    THE FIXTURES ARE BUILT HERE, byte by byte, because the point is the box header
    arithmetic and a real HEIC would test the HEVC decoder as well. Nothing here decodes a
    pixel.
*/
class tst_heicboxes : public QObject
{
    Q_OBJECT

private slots:
    void largesizeMdatDoesNotSwallowTheRestOfTheFile();
    void noExifStillYieldsDimensionsFromIspe();

private:
    static QByteArray be32(quint32 v);
    static QByteArray be64(quint64 v);
    static QByteArray box(const QByteArray &type, const QByteArray &payload);
    /* mdat written the way Apple writes it: size == 1, then the 64-bit largesize. */
    static QByteArray largesizeMdat(int payloadBytes);
    static QByteArray ftyp();
    static QByteArray metaWithIspe(quint32 w, quint32 h);
    QString writeFixture(const QByteArray &bytes, const QString &name);

    QTemporaryDir dir;
};

QByteArray tst_heicboxes::be32(quint32 v)
{
    QByteArray a(4, 0);
    for (int i = 0; i < 4; ++i) a[3 - i] = char((v >> (8 * i)) & 0xFF);
    return a;
}

QByteArray tst_heicboxes::be64(quint64 v)
{
    QByteArray a(8, 0);
    for (int i = 0; i < 8; ++i) a[7 - i] = char((v >> (8 * i)) & 0xFF);
    return a;
}

QByteArray tst_heicboxes::box(const QByteArray &type, const QByteArray &payload)
{
    return be32(quint32(8 + payload.size())) + type + payload;
}

QByteArray tst_heicboxes::largesizeMdat(int payloadBytes)
{
    const QByteArray payload(payloadBytes, 'x');
    /* size = 1, type, then the real length INCLUDING the 16-byte header. */
    return be32(1) + QByteArray("mdat") + be64(quint64(16 + payload.size())) + payload;
}

QByteArray tst_heicboxes::ftyp()
{
    /* major brand, minor version, then the compatible brands the parser scans. */
    return box("ftyp", QByteArray("heic") + be32(0) + QByteArray("mif1heic"));
}

QByteArray tst_heicboxes::metaWithIspe(quint32 w, quint32 h)
{
    /* ispe: version/flags, width, height. */
    const QByteArray ispe = box("ispe", be32(0) + be32(w) + be32(h));
    const QByteArray ipco = box("ipco", ispe);
    const QByteArray iprp = box("iprp", ipco);
    /* meta is a FullBox: 4 bytes of version/flags before its children. */
    return box("meta", be32(0) + iprp);
}

QString tst_heicboxes::writeFixture(const QByteArray &bytes, const QString &name)
{
    const QString path = QDir(dir.path()).absoluteFilePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return QString();
    f.write(bytes);
    f.close();
    return path;
}

void tst_heicboxes::largesizeMdatDoesNotSwallowTheRestOfTheFile()
{
    QVERIFY(dir.isValid());
    /* mdat FIRST, exactly as an iPhone writes it, with the metadata behind it. */
    const QByteArray file = ftyp() + largesizeMdat(4096) + metaWithIspe(4032, 3024);
    const QString path = writeFixture(file, "largesize.heic");
    QVERIFY(!path.isEmpty());

    MetadataParameters p;
    ImageMetadata m;
    IFD ifd; Exif exif; GPS gps;
    p.fPath = path;
    p.file.setFileName(path);
    QVERIFY(p.file.open(QIODevice::ReadOnly));
    Heic heic;
    const bool ok = heic.parseHeic(p, m, &ifd, &exif, &gps);
    p.file.close();

    QVERIFY2(ok, "a largesize mdat hid the meta box that follows it");
    QCOMPARE(m.width, 4032);
    QCOMPARE(m.height, 3024);
}

void tst_heicboxes::noExifStillYieldsDimensionsFromIspe()
{
    QVERIFY(dir.isValid());
    /* No iinf, so no Exif item exists at all -- the converted-file / screenshot case. */
    const QByteArray file = ftyp() + metaWithIspe(1280, 720) + box("mdat", QByteArray(64, 'x'));
    const QString path = writeFixture(file, "noexif.heic");
    QVERIFY(!path.isEmpty());

    MetadataParameters p;
    ImageMetadata m;
    IFD ifd; Exif exif; GPS gps;
    p.fPath = path;
    p.file.setFileName(path);
    QVERIFY(p.file.open(QIODevice::ReadOnly));
    Heic heic;
    const bool ok = heic.parseHeic(p, m, &ifd, &exif, &gps);
    p.file.close();

    QVERIFY2(ok, "a HEIC with no Exif is not an unreadable file");
    QCOMPARE(m.width, 1280);
    QCOMPARE(m.height, 720);
    /* The rest stays EMPTY rather than invented: "most metadata missing" is the truth
       about the file, and a made-up capture date would file it under the wrong year. */
    QVERIFY(m.model.isEmpty());
    QVERIFY(!m.createdDate.isValid());
}

QTEST_MAIN(tst_heicboxes)
#include "tst_heicboxes.moc"
