#ifndef CANON_H
#define CANON_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Raw/rawformat.h"

class Canon : public QObject
{
    Q_OBJECT

public:
    Canon();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    QHash<quint32, QString> canonMakerHash, canonFileInfoHash;
    Utilities u;
};

/*
    Sensor unpack for Canon CR2 (the per-format override of RawFormat). Self-contained: walks the
    CR2's TIFF IFDs (via TiffWalk), decodes the lossless-JPEG raw in the last IFD, reassembles the
    Canon vertical SLICES (tag 0xC640) into the full sensor mosaic, crops to the active area from
    the Canon makernote SensorInfo (0xE0), and reads the as-shot white balance from ColorData
    (0x4001). The colour matrix comes from the shared per-model table (cameramatrix.cpp).

    Handles the common lossless-JPEG CR2 (Compression 6). CR3 is a different container -- see
    CanonCR3. See notes/Documentation.txt "RAW DECODING".
*/
class CanonRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // CANON_H
