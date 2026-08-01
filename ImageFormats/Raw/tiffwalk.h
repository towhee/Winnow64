#ifndef TIFFWALK_H
#define TIFFWALK_H

#include <QFile>
#include <QHash>
#include <QList>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <cstdint>

/*
    Minimal TIFF/EP IFD reader shared by the RAW decoders (DngRaw today; the other vendor
    unpackers can adopt it). It is deliberately tiny: read the header, read IFDs (collecting
    SubIFD pointers), and interpret a tag's value -- as a scalar, an integer array, a real
    array (rationals / floats), bytes, or ASCII -- transparently handling the "inline if <= 4
    bytes, else at an offset" rule. It does NOT decode pixel data; the caller reads strips/tiles
    itself. Endian-aware. Values are re-read from the QFile on demand, so the file must stay
    open for the Reader's lifetime.
*/
namespace TiffWalk {

struct Entry {
    quint16 type = 0;
    quint32 count = 0;
    QByteArray val4;        // the 4 value-bytes from the IFD entry (inline value, or an offset)
};
using Ifd = QHash<quint16, Entry>;

class Reader {
public:
    /* Read the TIFF header. base != 0 is for an embedded TIFF whose offsets are relative to its
       own start (e.g. a Nikon type-3 MakerNote): all offsets passed to / returned from this
       Reader are then relative to base, and file reads are at base + offset. */
    bool init(QFile *f, quint32 base = 0);          // read TIFF header; false if not TIFF
    /* For an embedded IFD with a non-standard or absent TIFF header (e.g. an Olympus type-3
       MakerNote "OLYMPUS\0II\3\0"): supply the base, the first-IFD offset relative to base, and
       endianness directly. Offsets to/from this Reader are then relative to base. */
    void initEmbedded(QFile *f, quint32 base, quint32 firstIfdRel, bool bigEndian) {
        file = f; baseOff = base; first = firstIfdRel; isBig = bigEndian;
    }
    bool big() const { return isBig; }
    quint32 firstIfd() const { return first; }
    quint32 base() const { return baseOff; }

    /* Read the IFD at off into tags; append SubIFD (tag 330) offsets to subIfds; set next to
       the following IFD offset (0 if none). */
    bool readIfd(quint32 off, Ifd &tags, QList<quint32> &subIfds, quint32 &next);

    /* The 4 value-bytes as a u32 -- the IFD offset for a pointer tag (ExifIFD, MakerNote, ...),
       which can then be passed to readIfd(). */
    quint32 ifdPointer(const Entry &e) const;

    quint32 scalar(const Entry &e);                 // first value as u32 (SHORT/LONG/...)
    QVector<quint32> u32s(const Entry &e);          // all values as u32 (SHORT or LONG)
    QVector<double>  reals(const Entry &e);         // all values as double (rational/float/int)
    QByteArray bytes(const Entry &e);               // raw count*typeSize bytes
    QString    ascii(const Entry &e);

    static int typeSize(quint16 t);

private:
    QFile *file = nullptr;
    bool isBig = false;
    quint32 first = 0;
    quint32 baseOff = 0;
    quint16 g16(const uchar *p) const;
    quint32 g32(const uchar *p) const;
    QByteArray readAt(quint32 off, int n);
};

} // namespace TiffWalk

#endif // TIFFWALK_H
