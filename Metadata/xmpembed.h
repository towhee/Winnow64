#ifndef XMPEMBED_H
#define XMPEMBED_H

#include <QByteArray>
#include <QString>

/*
    Reading and writing the XMP packet INSIDE an image file -- what Lightroom Classic does
    for JPEG, TIFF, PNG and DNG, where it ignores any sidecar. See "Writing Metadata Into
    Image Files" in notes/Documentation.txt.

    CONTAINERS ONLY. This moves bytes: it finds the packet, lifts out the <x:xmpmeta>
    document, and splices a new one in. Deciding WHAT the document says is Xmp's job, and
    WHETHER to write into a file at all is Metadata::writeXMP's (the "Permit image file
    modification" preference, the backup). Nothing here reads a global, so tst_xmpembed
    can drive it against bytes built in the test.

    HOW EACH FORMAT IS WRITTEN

      JPEG   the APP1 "http://ns.adobe.com/xap/1.0/" segment is replaced, or inserted
             after the JFIF/Exif segments. Extended XMP segments are left untouched. The
             file is rebuilt in memory and replaced atomically (QSaveFile).
      PNG    the iTXt "XML:com.adobe.xmp" chunk is replaced, or inserted before the first
             IDAT, with its CRC. Rebuilt and replaced atomically.
      TIFF   IFD0 tag 700. NOTHING EXISTING IS MOVED OR OVERWRITTEN: the packet is
      DNG    appended at the end of the file, then the 700 entry is repointed -- or, when
             IFD0 has none, a copy of IFD0 with the entry added is appended too and the
             header is repointed. A TIFF can be hundreds of MB, which rules out rewriting
             it per keypress, and appending keeps every step recoverable: a crash before
             the final pointer write leaves the old file plus unreferenced bytes at the
             end. The cost is that each edit leaves the previous packet behind (a few KB).
             BigTIFF is not supported.

    A WRITE NEVER DISCARDS A PACKET IT CANNOT READ. read() reports Malformed for a packet
    it found but could not lift an <x:xmpmeta> out of, and the caller falls back to the
    sidecar rather than replacing what another application wrote with a fresh document.
*/
namespace XmpEmbed {

enum class Result {
    Ok,
    NoPacket,       // read: the container is fine and simply carries no XMP
    Unsupported,    // not JPEG/PNG/TIFF/DNG, or BigTIFF
    Malformed,      // the container, or the packet in it, could not be parsed
    TooLarge,       // JPEG: the packet does not fit one APP1 segment (65,502 bytes)
    IoError
};

QString describe(Result r);

// jpg/jpeg/tif/tiff/dng/png -- the formats this can write
bool canEmbed(const QString &fPath);

/* The <x:xmpmeta>...</x:xmpmeta> document in fPath. Ok with xmpmeta filled, NoPacket
   with it empty, or an error. */
Result read(const QString &fPath, QByteArray &xmpmeta);

/* Replace (or add) fPath's packet with xmpmeta, wrapped in an <?xpacket?> with padding
   so the next writer has room. */
Result write(const QString &fPath, const QByteArray &xmpmeta);

/* The pieces, exposed for tst_xmpembed. The in-memory ones never touch disk. */
QByteArray packet(const QByteArray &xmpmeta, int padding);
Result readJpeg(const QByteArray &file, QByteArray &xmpmeta);
Result spliceJpeg(const QByteArray &file, const QByteArray &xmpmeta, QByteArray &out);
Result readPng(const QByteArray &file, QByteArray &xmpmeta);
Result splicePng(const QByteArray &file, const QByteArray &xmpmeta, QByteArray &out);
Result readTiff(const QString &fPath, QByteArray &xmpmeta);
Result appendTiff(const QString &fPath, const QByteArray &xmpmeta);

}  // namespace XmpEmbed

#endif // XMPEMBED_H
