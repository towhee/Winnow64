#ifndef DNG_H
#define DNG_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Raw/rawformat.h"

class DNG
{
public:
    DNG();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               IPTC *iptc,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    Utilities u;
};

/*
    Sensor unpack for Adobe DNG (the per-format override of RawFormat). Self-contained: it walks
    the DNG's TIFF/EP IFDs itself (via TiffWalk) to find the CFA image, reads its geometry,
    strips/tiles, CFA pattern, levels, and the embedded colour (ColorMatrix2 + AsShotNeutral),
    and decodes the sensor data. Because DNG carries its own colour, no per-model matrix table is
    needed and nothing is plumbed at parse time -- the decode reads everything from the file.

    Handles UNCOMPRESSED (Compression 1) and LOSSLESS-JPEG (Compression 7) DNG, in either strip
    or tile layout. Other compressions return false -> ImageDecoder falls back to the embedded
    preview. See notes/Documentation.txt "RAW DECODING".
*/
class DngRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // DNG_H
