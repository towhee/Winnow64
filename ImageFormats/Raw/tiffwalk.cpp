#include "ImageFormats/Raw/tiffwalk.h"
#include <cstring>

namespace TiffWalk {

int Reader::typeSize(quint16 t)
{
    switch (t) {
    case 1: case 2: case 6: case 7: return 1;   // BYTE/ASCII/SBYTE/UNDEFINED
    case 3: case 8:                 return 2;   // SHORT/SSHORT
    case 4: case 9: case 11:        return 4;   // LONG/SLONG/FLOAT
    case 5: case 10: case 12:       return 8;   // RATIONAL/SRATIONAL/DOUBLE
    default:                        return 1;
    }
}

quint16 Reader::g16(const uchar *p) const
{
    return isBig ? quint16((p[0] << 8) | p[1]) : quint16((p[1] << 8) | p[0]);
}

quint32 Reader::g32(const uchar *p) const
{
    return isBig ? (quint32(p[0]) << 24 | quint32(p[1]) << 16 | quint32(p[2]) << 8 | p[3])
                 : (quint32(p[3]) << 24 | quint32(p[2]) << 16 | quint32(p[1]) << 8 | p[0]);
}

QByteArray Reader::readAt(quint32 off, int n)
{
    if (!file || !file->seek(baseOff + off)) return QByteArray();
    return file->read(n);
}

bool Reader::init(QFile *f, quint32 base)
{
    file = f;
    baseOff = base;
    if (!file || !file->seek(base)) return false;
    const QByteArray hdr = file->read(8);
    if (hdr.size() < 8) return false;
    const uchar *h = reinterpret_cast<const uchar *>(hdr.constData());
    if (h[0] == 'M' && h[1] == 'M') isBig = true;
    else if (h[0] == 'I' && h[1] == 'I') isBig = false;
    else return false;
    /* The "magic" word is 42 for standard TIFF, but raw formats vary (Olympus ORF 0x4F52/
       0x5352, Panasonic RW2 0x55, Canon CR2 0x4352). The endian marker plus a sane first-IFD
       offset is enough validation, so accept any magic. */
    first = g32(h + 4);
    return first >= 8;
}

bool Reader::readIfd(quint32 off, Ifd &tags, QList<quint32> &subIfds, quint32 &next)
{
    const QByteArray cnt = readAt(off, 2);
    if (cnt.size() < 2) return false;
    const int n = g16(reinterpret_cast<const uchar *>(cnt.constData()));
    const QByteArray body = readAt(off + 2, n * 12 + 4);
    if (body.size() < n * 12 + 4) return false;
    const uchar *b = reinterpret_cast<const uchar *>(body.constData());
    for (int i = 0; i < n; ++i) {
        const uchar *e = b + i * 12;
        Entry t;
        const quint16 tag = g16(e);
        t.type  = g16(e + 2);
        t.count = g32(e + 4);
        t.val4  = QByteArray(reinterpret_cast<const char *>(e + 8), 4);
        tags.insert(tag, t);
        if (tag == 330) {                           // SubIFDs
            const QVector<quint32> offs = u32s(t);
            for (quint32 s : offs) subIfds << s;
        }
    }
    next = g32(b + n * 12);
    return true;
}

quint32 Reader::ifdPointer(const Entry &e) const
{
    if (e.val4.size() < 4) return 0;
    return g32(reinterpret_cast<const uchar *>(e.val4.constData()));
}

QByteArray Reader::bytes(const Entry &e)
{
    const int n = typeSize(e.type) * int(e.count);
    if (n <= 0) return QByteArray();
    if (n <= 4) return e.val4.left(n);
    const uchar *v = reinterpret_cast<const uchar *>(e.val4.constData());
    return readAt(g32(v), n);
}

quint32 Reader::scalar(const Entry &e)
{
    const QByteArray b = bytes(e);
    if (b.size() < typeSize(e.type)) return 0;
    const uchar *p = reinterpret_cast<const uchar *>(b.constData());
    switch (e.type) {
    case 3: case 8:  return g16(p);
    case 1: case 7:  return p[0];
    default:         return g32(p);
    }
}

QVector<quint32> Reader::u32s(const Entry &e)
{
    QVector<quint32> out;
    const QByteArray b = bytes(e);
    const uchar *p = reinterpret_cast<const uchar *>(b.constData());
    const int sz = typeSize(e.type);
    const int n = sz ? int(b.size() / sz) : 0;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const uchar *q = p + i * sz;
        switch (e.type) {
        case 3: case 8:  out << g16(q); break;
        case 1: case 7:  out << q[0];   break;
        default:         out << g32(q); break;
        }
    }
    return out;
}

QVector<double> Reader::reals(const Entry &e)
{
    QVector<double> out;
    const QByteArray b = bytes(e);
    const uchar *p = reinterpret_cast<const uchar *>(b.constData());
    const int sz = typeSize(e.type);
    const int n = sz ? int(b.size() / sz) : 0;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const uchar *q = p + i * sz;
        switch (e.type) {
        case 5: {   // RATIONAL (u32/u32)
            const quint32 num = g32(q), den = g32(q + 4);
            out << (den ? double(num) / double(den) : 0.0);
            break;
        }
        case 10: {  // SRATIONAL (i32/i32)
            const qint32 num = qint32(g32(q)), den = qint32(g32(q + 4));
            out << (den ? double(num) / double(den) : 0.0);
            break;
        }
        case 11: {  // FLOAT
            quint32 bits = g32(q); float f; memcpy(&f, &bits, 4); out << double(f);
            break;
        }
        case 3:          out << double(g16(q)); break;
        case 8:          out << double(qint16(g16(q))); break;
        case 1: case 7:  out << double(q[0]); break;
        case 9:          out << double(qint32(g32(q))); break;
        default:         out << double(g32(q)); break;
        }
    }
    return out;
}

QString Reader::ascii(const Entry &e)
{
    QByteArray b = bytes(e);
    const int z = b.indexOf('\0');
    if (z >= 0) b = b.left(z);
    return QString::fromLatin1(b);
}

} // namespace TiffWalk
