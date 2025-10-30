#include "ImageFormats/Tiff/tiffdecoder.h"
#include "ImageFormats/Base/rawbase.h"
#include "ImageFormats/Jpeg/jpeg.h"  // your existing helper

TiffDecoder::TiffDecoder(QString srcPath, QObject* parent)
    : RawBase(std::move(srcPath), parent) {}

TiffDecoder::~TiffDecoder() { unloadContainer(); }

bool TiffDecoder::loadContainer(MetadataParameters &p)
{
    Q_UNUSED(p);
    if (fPath.isEmpty()) return false;
#ifdef Q_OS_MAC
    tiff = TIFFOpen(fPath.toUtf8().constData(), "rm");
    if (!tiff) return false;
    // Also keep QFile open for potential raw strip reads if needed
    file.setFileName(fPath);
    file.open(QIODevice::ReadOnly);
    return true;
#else
    // On Windows you included libtiff headers; adapt open accordingly
    tiff = TIFFOpen(fPath.toStdString().c_str(), "rm");
    if (!tiff) return false;
    file.setFileName(fPath);
    file.open(QIODevice::ReadOnly);
    return true;
#endif
}

bool TiffDecoder::unloadContainer()
{
    if (tiff) { TIFFClose(tiff); tiff=nullptr; }
    return RawBase::unloadContainer();
}

bool TiffDecoder::readIFDsAndTags(MetadataParameters &p, IFD *ifd)
{
    Q_UNUSED(p);
    if (!tiff) return false;

    quint32 w=0,h=0; quint16 bits=0; quint16 planar=1; quint16 photo=0;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetField(tiff, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photo);

    rawW = int(w); rawH = int(h); bps = int(bits);

    // Try CFA detection via DNG tags
    uint32 cfaRepeat[2] = {0,0};
    const void* cfaPtr = nullptr; uint32 cfaCount = 0;
    if (TIFFGetField(tiff, TIFFTAG_CFAREPEATPATTERNDIM, &cfaRepeat) &&
        TIFFGetField(tiff, TIFFTAG_CFAPATTERN, &cfaCount, &cfaPtr) && cfaPtr && cfaCount>0) {
        QByteArray pat(reinterpret_cast<const char*>(cfaPtr), int(cfaCount));
        cfa = RawBase::cfaFromTag(0, pat);
    }

    // Optionally mirror essential tags into your IFD (if provided)
    if (ifd) {
        // ifd->imageWidth = rawW; ifd->imageLength = rawH; ifd->bitsPerSample = bps; // adapt to your fields
        // ifd->planarConfiguration = planar;
        // ifd->photometricInterpretation = photo;
    }
    return true;
}

bool TiffDecoder::extractRawPlane(MetadataParameters &p, RawPlaneView &out)
{
    Q_UNUSED(p);
    if (!tiff) return false;

    // If this is a classic RGB TIFF, we can short-circuit to QImage via libtiff; but
    // since we want RAW pipeline, we try to read the packed mosaic from strips/tiles.

    // For a skeleton, use TIFFReadRGBAImage to get something working; real code should
    // read raw strips and keep mosaic. This keeps the interface compiling now.
    QVector<quint32> temp(rawW*rawH);
    if (!TIFFReadRGBAImage(tiff, rawW, rawH, reinterpret_cast<uint32*>(temp.data()), 0))
        return false;

    // Convert into 16-bit single-plane grayscale as a placeholder for RAW
    rawBuffer.resize(rawW*rawH*2);
    auto *dst = reinterpret_cast<quint16*>(rawBuffer.data());
    for (int i=0;i<rawW*rawH;++i){
        quint32 px = temp[i];
        quint8 r = (px>>16)&0xFF, g=(px>>8)&0xFF, b=px&0xFF;
        dst[i] = quint16(((int(r)+int(g)+int(b))/3) * 257); // 8->16
    }

    out.dataU16 = reinterpret_cast<const quint16*>(rawBuffer.constData());
    out.width = rawW; out.height = rawH; out.strideBytes = rawW*2; out.bitsPerSample = 16;
    out.cfa = cfa==CFAPattern::Unknown?CFAPattern::RGGB:cfa; // placeholder
    out.littleEndian = true;
    return true;
}

bool TiffDecoder::tryExtractEmbeddedPreview(MetadataParameters &p, QImage &image, int maxDim)
{
    Q_UNUSED(p);
    if (!tiff || maxDim<=0) return false;

    // Minimal path: some TIFFs carry JPEG thumbnails (SubIFD or JPEG tables). For
    // skeleton, use TIFFReadRGBAImage downsized as a preview.
    uint32 W = 0, H = 0; TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &W); TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &H);
    if (!W || !H) return false;

    QVector<quint32> temp(W*H);
    if (!TIFFReadRGBAImage(tiff, int(W), int(H), reinterpret_cast<uint32*>(temp.data()), 0))
        return false;

    QImage rgba(int(W), int(H), QImage::Format_RGBA8888);
    for (int y=0;y<int(H);++y){
        auto *row = reinterpret_cast<quint8*>(rgba.scanLine(y));
        for (int x=0;x<int(W);++x){
            quint32 px = temp[y*int(W)+x];
            row[4*x+0]=(px>>16)&0xFF; row[4*x+1]=(px>>8)&0xFF; row[4*x+2]=px&0xFF; row[4*x+3]=255;
        }
    }
    if (maxDim>0) image = rgba.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else image = rgba;
    return true;
}

bool TiffDecoder::readTagUShort(TIFF* t, uint32 tag, uint16 &val)
{ return TIFFGetField(t, tag, &val) == 1; }
bool TiffDecoder::readTagUInt(TIFF* t, uint32 tag, uint32 &val)
{ return TIFFGetField(t, tag, &val) == 1; }
bool TiffDecoder::readTagFloat(TIFF* t, uint32 tag, float &val)
{ return TIFFGetField(t, tag, &val) == 1; }
bool TiffDecoder::readTagBytes(TIFF* t, uint32 tag, QByteArray &bytes)
{
    void* data=nullptr; uint32 count=0;
    if (TIFFGetField(t, tag, &count, &data) != 1 || !data || !count) return false;
    bytes = QByteArray(reinterpret_cast<const char*>(data), int(count));
    return true;
}
