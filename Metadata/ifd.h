#ifndef IFD_H
#define IFD_H

#include <QtCore>

#include "Metadata/imagemetadata.h"

struct IFDData
{
    quint32 tagType;
    quint32 tagCount;
    quint32 tagValue;
    short tagReal;
};

class IFD
{
public:
    IFD();
    quint32 readIFD(MetadataParameters &p, bool isBigEnd = false);
    QList<quint32> getSubIfdOffsets(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd = false);
    quint32 readIFD_B(MetadataParameters &p, bool isBigEnd = false);
    bool writeIFDCount(MetadataParameters &p, ImageMetadata &m, quint16 count);
    bool writeIFDItem(MetadataParameters &p, ImageMetadata &m,
                      quint16 tagId, quint16 tagType, quint32 tagCount, quint32 tagValue);
    QList<quint32> getSubIfdOffsets_B(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd = false);
    QHash<uint, IFDData> ifdDataHash;
    QMutex mutex;
    /*  The EXIF ColorSpace tag (0xA001) from whichever IFD carried it, -1 if none.
        Recorded here because every format parser reads its EXIF directory through
        readIFD; Metadata::readMetadata resets it per image and reads it after the
        parse (Metadata/colorspaces.h). */
    int exifColorSpace = -1;
    /*  The EXIF Interop IFD pointer (0xA005), as stored (relative to the TIFF header), 0
        if none. Its InteropIndex "R03" is the DCF marker for an Adobe RGB file -- what a
        camera writes beside EXIF ColorSpace = Uncalibrated. Reset with exifColorSpace. */
    quint32 interopOffset = 0;
    /*  A DNG's PreviewColorSpace (50970): the space its embedded previews are rendered
        in -- 1 Gray, 2 sRGB, 3 Adobe RGB, 4 ProPhoto. -1 if none. Reset per image. */
    int dngPreviewColorSpace = -1;
    /*  The 4-character InteropIndex ("R98", "R03", "THM") of the Interop IFD at absolute
        file position ifdPos, read directly so it cannot disturb ifdDataHash, which a
        parser is usually still reading from. Empty if absent or unreadable. */
    static QByteArray readInteropIndex(QFile &file, qint64 ifdPos, bool isBigEnd);
};

#endif // IFD_H
