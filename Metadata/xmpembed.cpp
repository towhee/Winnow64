#include "Metadata/xmpembed.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QVector>

#ifdef Q_OS_WIN
#include <io.h>
#else
#include <unistd.h>
#endif

namespace XmpEmbed {

namespace {

const QByteArray kJpegXmpNs("http://ns.adobe.com/xap/1.0/\0", 29);
const QByteArray kPngXmpKeyword("XML:com.adobe.xmp");
const QByteArray kPngSignature("\x89PNG\r\n\x1a\n", 8);

// One APP1 segment holds 65,533 payload bytes; the namespace takes 29 of them.
const int kJpegMaxPacket = 65533 - 29;

quint16 be16(const QByteArray &b, qint64 i)
{
    return quint16((uchar(b[i]) << 8) | uchar(b[i + 1]));
}

quint32 be32(const QByteArray &b, qint64 i)
{
    return (quint32(uchar(b[i])) << 24) | (quint32(uchar(b[i + 1])) << 16)
           | (quint32(uchar(b[i + 2])) << 8) | quint32(uchar(b[i + 3]));
}

void putBe16(QByteArray &b, quint16 v)
{
    b.append(char(v >> 8));
    b.append(char(v & 0xFF));
}

void putBe32(QByteArray &b, quint32 v)
{
    b.append(char(v >> 24));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
}

/* The <x:xmpmeta> document inside a packet. An <?xpacket?> wrapper, and anything
   around the document, is dropped -- write() adds a fresh wrapper. */
Result xmpmetaOf(const QByteArray &pkt, QByteArray &xmpmeta)
{
    const qint64 start = pkt.indexOf("<x:xmpmeta");
    if (start < 0) return Result::Malformed;
    const QByteArray close("</x:xmpmeta>");
    const qint64 end = pkt.indexOf(close, start);
    if (end < 0) return Result::Malformed;
    xmpmeta = pkt.mid(start, end + close.size() - start);
    return Result::Ok;
}

bool writeAtomically(const QString &fPath, const QByteArray &bytes)
{
    QSaveFile f(fPath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    if (f.write(bytes) != bytes.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

Result readWhole(const QString &fPath, QByteArray &bytes)
{
    QFile f(fPath);
    if (!f.open(QIODevice::ReadOnly)) return Result::IoError;
    bytes = f.readAll();
    return Result::Ok;
}

/* ---------------------------------------------------------------------------------
   JPEG
   --------------------------------------------------------------------------------- */

struct Segment {
    qint64 pos;         // offset of the 0xFF marker
    qint64 size;        // marker + length field + payload
    uchar marker;
};

/* The segments before the image data (SOS). Anything unexpected is Malformed: a file
   this cannot walk is not a file this should rewrite. */
Result walkJpeg(const QByteArray &f, QVector<Segment> &segs)
{
    const qint64 n = f.size();
    if (n < 4 || uchar(f[0]) != 0xFF || uchar(f[1]) != 0xD8) return Result::Malformed;
    qint64 pos = 2;
    while (true) {
        if (pos + 1 >= n || uchar(f[pos]) != 0xFF) return Result::Malformed;
        while (pos + 1 < n && uchar(f[pos + 1]) == 0xFF) ++pos;     // fill bytes
        if (pos + 1 >= n) return Result::Malformed;
        const uchar marker = uchar(f[pos + 1]);
        if (marker == 0xDA || marker == 0xD9) return Result::Ok;    // SOS / EOI
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {  // no length
            pos += 2;
            continue;
        }
        if (pos + 4 > n) return Result::Malformed;
        const quint16 len = be16(f, pos + 2);
        if (len < 2 || pos + 2 + len > n) return Result::Malformed;
        segs.append({pos, qint64(2) + len, marker});
        pos += 2 + len;
    }
}

bool isJpegXmp(const QByteArray &f, const Segment &s)
{
    return s.marker == 0xE1 && s.size >= 4 + kJpegXmpNs.size()
           && f.mid(s.pos + 4, kJpegXmpNs.size()) == kJpegXmpNs;
}

/* ---------------------------------------------------------------------------------
   PNG
   --------------------------------------------------------------------------------- */

struct Chunk {
    qint64 pos;         // offset of the length field
    qint64 size;        // length + type + data + crc
    QByteArray type;
    qint64 dataPos;
    quint32 dataLen;
};

Result walkPng(const QByteArray &f, QVector<Chunk> &chunks)
{
    if (!f.startsWith(kPngSignature)) return Result::Malformed;
    qint64 pos = kPngSignature.size();
    while (pos + 12 <= f.size()) {
        const quint32 len = be32(f, pos);
        if (qint64(len) > f.size() - pos - 12) return Result::Malformed;
        Chunk c{pos, qint64(12) + len, f.mid(pos + 4, 4), pos + 8, len};
        chunks.append(c);
        if (c.type == "IEND") return Result::Ok;
        pos += c.size;
    }
    return Result::Malformed;                   // ran out before IEND
}

bool isPngXmp(const QByteArray &f, const Chunk &c)
{
    return c.type == "iTXt" && c.dataLen > quint32(kPngXmpKeyword.size())
           && f.mid(c.dataPos, kPngXmpKeyword.size() + 1) == kPngXmpKeyword + '\0';
}

quint32 crc32(const QByteArray &bytes)
{
    static quint32 table[256];
    static bool ready = [] {
        for (quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        return true;
    }();
    Q_UNUSED(ready)
    quint32 c = 0xFFFFFFFFu;
    for (char ch : bytes) c = table[(c ^ uchar(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ---------------------------------------------------------------------------------
   TIFF / DNG
   --------------------------------------------------------------------------------- */

struct TiffLayout {
    bool big = false;           // Motorola byte order
    quint32 ifd0 = 0;
    quint16 count = 0;
    QByteArray entries;         // count * 12 bytes, as on disk
    QByteArray next;            // the 4-byte next-IFD offset, as on disk
    int xmpIndex = -1;          // entry index of tag 700
};

quint16 get16(const QByteArray &b, qint64 i, bool big)
{
    return big ? be16(b, i) : quint16(uchar(b[i]) | (uchar(b[i + 1]) << 8));
}

quint32 get32(const QByteArray &b, qint64 i, bool big)
{
    if (big) return be32(b, i);
    return quint32(uchar(b[i])) | (quint32(uchar(b[i + 1])) << 8)
           | (quint32(uchar(b[i + 2])) << 16) | (quint32(uchar(b[i + 3])) << 24);
}

QByteArray put16(quint16 v, bool big)
{
    QByteArray b;
    if (big) putBe16(b, v);
    else { b.append(char(v & 0xFF)); b.append(char(v >> 8)); }
    return b;
}

QByteArray put32(quint32 v, bool big)
{
    QByteArray b;
    if (big) putBe32(b, v);
    else for (int i = 0; i < 4; ++i) b.append(char((v >> (8 * i)) & 0xFF));
    return b;
}

Result readLayout(QFile &f, TiffLayout &t)
{
    const qint64 size = f.size();
    f.seek(0);
    const QByteArray h = f.read(8);
    if (h.size() < 8) return Result::Malformed;
    if (h.startsWith("MM")) t.big = true;
    else if (!h.startsWith("II")) return Result::Malformed;
    const quint16 magic = get16(h, 2, t.big);
    if (magic == 43) return Result::Unsupported;            // BigTIFF
    if (magic != 42) return Result::Malformed;
    t.ifd0 = get32(h, 4, t.big);
    if (t.ifd0 < 8 || t.ifd0 + 2 > size) return Result::Malformed;
    f.seek(t.ifd0);
    const QByteArray c = f.read(2);
    if (c.size() < 2) return Result::Malformed;
    t.count = get16(c, 0, t.big);
    if (t.count == 0 || t.ifd0 + 2 + qint64(t.count) * 12 + 4 > size) return Result::Malformed;
    t.entries = f.read(qint64(t.count) * 12);
    t.next = f.read(4);
    if (t.entries.size() != t.count * 12 || t.next.size() != 4) return Result::Malformed;
    for (int i = 0; i < t.count; ++i)
        if (get16(t.entries, i * 12, t.big) == 700) { t.xmpIndex = i; break; }
    return Result::Ok;
}

/* Flushed to the DEVICE before the pointer that makes it reachable is written, so the
   old file stays whole whatever point a crash interrupts. */
bool syncFile(QFile &f)
{
    if (!f.flush()) return false;
#ifdef Q_OS_WIN
    return _commit(f.handle()) == 0;
#else
    return ::fsync(f.handle()) == 0;
#endif
}

}  // namespace

/* ---------------------------------------------------------------------------------
   Public
   --------------------------------------------------------------------------------- */

QString describe(Result r)
{
    switch (r) {
    case Result::Ok:          return "ok";
    case Result::NoPacket:    return "no XMP packet";
    case Result::Unsupported: return "unsupported file type";
    case Result::Malformed:   return "the file's structure could not be read";
    case Result::TooLarge:    return "the metadata is too large to embed";
    case Result::IoError:     return "the file could not be read or written";
    }
    return QString();
}

bool canEmbed(const QString &fPath)
{
    static const QStringList formats = {"jpg", "jpeg", "tif", "tiff", "dng", "png"};
    return formats.contains(QFileInfo(fPath).suffix().toLower());
}

QByteArray packet(const QByteArray &xmpmeta, int padding)
{
    QByteArray p("<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n");
    p += xmpmeta;
    p += '\n';
    // the spec's recommended form: lines of spaces, so a later writer can grow in place
    const QByteArray line(99, ' ');
    for (int i = 0; i < padding / 100; ++i) p += line + '\n';
    p += "<?xpacket end=\"w\"?>";
    return p;
}

Result readJpeg(const QByteArray &file, QByteArray &xmpmeta)
{
    xmpmeta.clear();
    QVector<Segment> segs;
    const Result r = walkJpeg(file, segs);
    if (r != Result::Ok) return r;
    for (const Segment &s : segs) {
        if (!isJpegXmp(file, s)) continue;
        const qint64 start = s.pos + 4 + kJpegXmpNs.size();
        return xmpmetaOf(file.mid(start, s.pos + s.size - start), xmpmeta);
    }
    return Result::NoPacket;
}

Result spliceJpeg(const QByteArray &file, const QByteArray &xmpmeta, QByteArray &out)
{
    QVector<Segment> segs;
    const Result r = walkJpeg(file, segs);
    if (r != Result::Ok) return r;

    QByteArray pkt = packet(xmpmeta, 2048);
    if (pkt.size() > kJpegMaxPacket) pkt = packet(xmpmeta, 0);
    if (pkt.size() > kJpegMaxPacket) return Result::TooLarge;

    QByteArray seg;
    seg.append(char(0xFF));
    seg.append(char(0xE1));
    putBe16(seg, quint16(2 + kJpegXmpNs.size() + pkt.size()));
    seg += kJpegXmpNs;
    seg += pkt;

    for (const Segment &s : segs) {
        if (!isJpegXmp(file, s)) continue;
        out = file.left(s.pos) + seg + file.mid(s.pos + s.size);
        return Result::Ok;
    }

    /* None yet: after the leading JFIF (APP0) and Exif (APP1) segments, which both
       have to stay first to remain valid. */
    qint64 at = 2;
    for (const Segment &s : segs) {
        if (s.marker != 0xE0 && s.marker != 0xE1) break;
        at = s.pos + s.size;
    }
    out = file.left(at) + seg + file.mid(at);
    return Result::Ok;
}

Result readPng(const QByteArray &file, QByteArray &xmpmeta)
{
    xmpmeta.clear();
    QVector<Chunk> chunks;
    const Result r = walkPng(file, chunks);
    if (r != Result::Ok) return r;
    for (const Chunk &c : chunks) {
        if (!isPngXmp(file, c)) continue;
        /* keyword\0 compressionFlag compressionMethod language\0 translated\0 text.
           A compressed packet is Malformed rather than decompressed: it is rare, and
           reporting it keeps write() from replacing a packet it could not read. */
        const QByteArray data = file.mid(c.dataPos, c.dataLen);
        qint64 i = kPngXmpKeyword.size() + 1;
        if (i + 2 > data.size() || data[i] != 0) return Result::Malformed;
        i += 2;
        const qint64 lang = data.indexOf('\0', i);
        if (lang < 0) return Result::Malformed;
        const qint64 trans = data.indexOf('\0', lang + 1);
        if (trans < 0) return Result::Malformed;
        return xmpmetaOf(data.mid(trans + 1), xmpmeta);
    }
    return Result::NoPacket;
}

Result splicePng(const QByteArray &file, const QByteArray &xmpmeta, QByteArray &out)
{
    QVector<Chunk> chunks;
    const Result r = walkPng(file, chunks);
    if (r != Result::Ok) return r;

    QByteArray body("iTXt");
    body += kPngXmpKeyword;
    // keyword end, uncompressed, method 0, empty language, empty translation
    body += QByteArray(5, '\0');
    body += packet(xmpmeta, 0);     // the file is rewritten anyway: padding buys nothing
    QByteArray chunk;
    putBe32(chunk, quint32(body.size() - 4));
    chunk += body;
    putBe32(chunk, crc32(body));

    for (const Chunk &c : chunks) {
        if (!isPngXmp(file, c)) continue;
        out = file.left(c.pos) + chunk + file.mid(c.pos + c.size);
        return Result::Ok;
    }
    // before the image data, where readers that stop early still find it
    for (const Chunk &c : chunks) {
        if (c.type != "IDAT" && c.type != "IEND") continue;
        out = file.left(c.pos) + chunk + file.mid(c.pos);
        return Result::Ok;
    }
    return Result::Malformed;
}

Result readTiff(const QString &fPath, QByteArray &xmpmeta)
{
    xmpmeta.clear();
    QFile f(fPath);
    if (!f.open(QIODevice::ReadOnly)) return Result::IoError;
    TiffLayout t;
    const Result r = readLayout(f, t);
    if (r != Result::Ok) return r;
    if (t.xmpIndex < 0) return Result::NoPacket;

    const qint64 e = qint64(t.xmpIndex) * 12;
    const quint16 type = get16(t.entries, e + 2, t.big);
    const quint32 count = get32(t.entries, e + 4, t.big);
    if (type != 1 && type != 7) return Result::Malformed;   // BYTE or UNDEFINED
    QByteArray pkt;
    if (count <= 4) {
        pkt = t.entries.mid(e + 8, count);
    }
    else {
        const quint32 offset = get32(t.entries, e + 8, t.big);
        if (qint64(offset) + count > f.size()) return Result::Malformed;
        f.seek(offset);
        pkt = f.read(count);
    }
    return xmpmetaOf(pkt, xmpmeta);
}

Result appendTiff(const QString &fPath, const QByteArray &xmpmeta)
{
    QFile f(fPath);
    if (!f.open(QIODevice::ReadWrite)) return Result::IoError;
    TiffLayout t;
    Result r = readLayout(f, t);
    if (r != Result::Ok) return r;

    const QByteArray pkt = packet(xmpmeta, 2048);

    // 1. the packet, at a word boundary at the end of the file
    qint64 at = f.size();
    if (!f.seek(at)) return Result::IoError;
    if (at % 2) {
        if (f.write(QByteArray(1, '\0')) != 1) return Result::IoError;
        ++at;
    }
    if (at + pkt.size() + qint64(t.count + 1) * 12 + 8 > 0xFFFFFFFFLL)
        return Result::Unsupported;
    if (f.write(pkt) != pkt.size()) return Result::IoError;
    const quint32 pktOffset = quint32(at);

    QByteArray entry = put16(700, t.big) + put16(1, t.big)        // XMP, BYTE
                       + put32(quint32(pkt.size()), t.big) + put32(pktOffset, t.big);

    if (t.xmpIndex >= 0) {
        // 2. repoint the existing entry
        if (!syncFile(f)) return Result::IoError;
        if (!f.seek(t.ifd0 + 2 + qint64(t.xmpIndex) * 12)) return Result::IoError;
        if (f.write(entry) != entry.size()) return Result::IoError;
        return syncFile(f) ? Result::Ok : Result::IoError;
    }

    /* 2. no entry: a copy of IFD0 with one added, in tag order. Every value offset in
          it is absolute, so the copy points at exactly what the original did. */
    at += pkt.size();
    if (at % 2) {
        if (f.write(QByteArray(1, '\0')) != 1) return Result::IoError;
        ++at;
    }
    QByteArray ifd = put16(quint16(t.count + 1), t.big);
    bool placed = false;
    for (int i = 0; i < t.count; ++i) {
        if (!placed && get16(t.entries, i * 12, t.big) > 700) {
            ifd += entry;
            placed = true;
        }
        ifd += t.entries.mid(i * 12, 12);
    }
    if (!placed) ifd += entry;
    ifd += t.next;
    if (f.write(ifd) != ifd.size()) return Result::IoError;
    if (!syncFile(f)) return Result::IoError;

    // 3. the header, last
    if (!f.seek(4)) return Result::IoError;
    const QByteArray ptr = put32(quint32(at), t.big);
    if (f.write(ptr) != ptr.size()) return Result::IoError;
    return syncFile(f) ? Result::Ok : Result::IoError;
}

Result read(const QString &fPath, QByteArray &xmpmeta)
{
    const QString ext = QFileInfo(fPath).suffix().toLower();
    if (ext == "tif" || ext == "tiff" || ext == "dng") return readTiff(fPath, xmpmeta);
    if (ext != "jpg" && ext != "jpeg" && ext != "png") return Result::Unsupported;
    QByteArray bytes;
    const Result r = readWhole(fPath, bytes);
    if (r != Result::Ok) return r;
    return ext == "png" ? readPng(bytes, xmpmeta) : readJpeg(bytes, xmpmeta);
}

Result write(const QString &fPath, const QByteArray &xmpmeta)
{
    const QString ext = QFileInfo(fPath).suffix().toLower();
    if (ext == "tif" || ext == "tiff" || ext == "dng") return appendTiff(fPath, xmpmeta);
    if (ext != "jpg" && ext != "jpeg" && ext != "png") return Result::Unsupported;

    QByteArray bytes, out;
    Result r = readWhole(fPath, bytes);
    if (r != Result::Ok) return r;
    r = ext == "png" ? splicePng(bytes, xmpmeta, out) : spliceJpeg(bytes, xmpmeta, out);
    if (r != Result::Ok) return r;
    return writeAtomically(fPath, out) ? Result::Ok : Result::IoError;
}

}  // namespace XmpEmbed
