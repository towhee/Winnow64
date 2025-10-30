// ============================================================================
// File: tiffdecoder.h
// Purpose: TIFF/DNG-style decoder built on RawBase (replaces old Tiff class)
// ============================================================================

#ifndef TIFFDECODER_H
#define TIFFDECODER_H

#include <QtWidgets>
#include "ImageFormats/Base/rawbase.h"

class TiffDecoder : public RawBase {
    Q_OBJECT
public:
    explicit TiffDecoder(QString srcPath = QString(), QObject* parent=nullptr);
    ~TiffDecoder() override;

protected:
    // RawBase overrides
    bool loadContainer(MetadataParameters &p) override;
    bool unloadContainer() override;
    bool readIFDsAndTags(MetadataParameters &p, IFD *ifd) override;
    bool extractRawPlane(MetadataParameters &p, RawPlaneView &out) override;
    bool tryExtractEmbeddedPreview(MetadataParameters &p, QImage &image, int maxDim) override;

private:
    // Helper: read a tag safely via libtiff and mirror into your IFD
    bool readTagUShort(TIFF* t, quint32 tag, quint16 &val);
    bool readTagUInt(TIFF* t, quint32 tag, quint32 &val);
    bool readTagFloat(TIFF* t, quint32 tag, float &val);
    bool readTagBytes(TIFF* t, quint32 tag, QByteArray &bytes);

    // For libtiff lifecycle
    TIFF *tiff = nullptr;
};

#endif // TIFFDECODER_H
