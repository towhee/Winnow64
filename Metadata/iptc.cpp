#include "iptc.h"

IPTC::IPTC()
{

}

void IPTC::read(QFile &file, quint32 &offset, ImageMetadata &m)
{
    /*
    If the IPTC data is in a JPG segment then there is some header info to get
    through to get to the start of the actual IPTC data blocks.

    If the IPTC data is in a photoshop TIF file, then the IPTC offset is directly
    to the first IPTC data block so we can skip the search req'd if it was JPG.
    */
//    bool isBigEnd = true;            // true = default, only IFD/EXIF can be little endian
    bool foundIPTC = false;
    file.seek(offset);

    // check to see if the offset is a JPG segment
    quint32 marker = Utilities::get16(file.peek(2));
    if (marker == 0xFFED) {
        // skip the APP marker FFED and length bytes
        file.seek(offset + 2);
        quint32 segmentLength = Utilities::get16(file.read(2));
        quint32 count = 0;
        while (!foundIPTC && count < segmentLength) {
            count +=2;
            // find "8BIM" = 0x3842 0x494D
            if (Utilities::get16(file.read(2)) == 0x3842) {
                if (Utilities::get16(file.read(2)) == 0x494D) {
                    // is it IPTC data?
                    if (Utilities::get16(file.read(2)) == 0x0404) foundIPTC = true;
                }
            }
        }
        if (!foundIPTC) return;

        // calc pascal-style string length
        int pasStrLen = file.read(1).toInt() + 1;
        pasStrLen = (pasStrLen % 2) ? pasStrLen + 1: pasStrLen;
        file.seek(file.pos() + pasStrLen - 1);
        // read size of resource data
//        quint32 resourceDataSize = get4(file.read(4));
//        qint64 endResourceData = file.pos() + resourceDataSize - 4;
    }

    // read IPTC data blocks searching for title (0x05)
    bool foundTitle = false;
    while (!foundTitle) {
        // every block starts with tag marker 0x1C
        uint tagMarker = Utilities::get8(file.read(1));
        if (tagMarker != 0x1C) break;
        uint recordNumber = Utilities::get8(file.read(1));
        uint tag = Utilities::get8(file.read(1));
        uint dataLength = static_cast<uint>(Utilities::get16(file.read(2)));
        // Sanity: a dataLength that would seek past EOF is malformed; bail rather
        // than read garbage that propagates into UI text and XMP sidecars.
        if (file.pos() + dataLength > static_cast<qint64>(file.size())) break;
        if (recordNumber == 2 && tag == 5) {
            QByteArray raw = file.read(dataLength);
            // Strip control bytes (NUL, BEL, …) before we hand this off to QString.
            // Printable text and common whitespace pass through; UTF-8 multibyte
            // sequences (>= 0x80) pass through and are decoded by fromUtf8.
            QByteArray clean;
            clean.reserve(raw.size());
            for (char c : raw) {
                const unsigned char u = static_cast<unsigned char>(c);
                if (u >= 0x20 || u == '\t' || u == '\n' || u == '\r') clean.append(c);
            }
            // IPTC IIM ObjectName is spec'd at 64 octets; cap generously at 1 KiB.
            m.title = QString::fromUtf8(clean).left(1024);
            foundTitle = true;
        }
        else file.seek(file.pos() + dataLength);
    }
}
