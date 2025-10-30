#ifndef RAWBASE_H
#define RAWBASE_H

// ============================================================================
// File: rawbase.h
// Purpose: Common foundation for RAW-like decoders (TIFF/DNG/ARW/NEF/CR2/CR3...)
// Author: (c) 2025 — extracted from Winnow TIFF code and generalized
// Notes:
//  - Keeps your existing decode/parse signatures so callers don’t change.
//  - Pipeline stages: loadContainer() -> extractRawPlane() -> demosaic()
//                    -> colorCorrect() -> toQImage()
//  - Subclasses implement container specifics + sensor/CFA specifics.
//  - Thread-safety: one instance per file; avoid sharing mutable state.
// ============================================================================
#pragma once

// #include <QtCore>
// #include <QImage>
// #include <QFile>
// #include <QBuffer>
// #include <QImageIOHandler>

#include <QtWidgets>

#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/irb.h"
#include "Metadata/iptc.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "Metadata/metareport.h"
#include "ImageFormats/Tiff/tiff.h"

// Forward declare to reduce build deps
class JPEG;  // your JPEG helper

// CFA pattern keys (DNG/RAW-like). Keep simple for now.
enum class CFAPattern : quint8 { Unknown=0, RGGB, BGGR, GRBG, GBRG };

struct RawPlaneView {
    // Interleaved mosaic plane (e.g., Bayer) or already separated channels
    // Owned storage in rawBuffer; view is just pointer + stride
    const quint16* dataU16 = nullptr;   // 16-bit linear
    const quint8*  dataU8  = nullptr;   // 8-bit (rare for true RAW)
    int width = 0;                      // in pixels of the mosaic plane
    int height = 0;
    int strideBytes = 0;                // row bytes in source plane
    int bitsPerSample = 0;              // 10/12/14/16
    CFAPattern cfa = CFAPattern::Unknown;
    bool littleEndian = true;
};

struct ColorMatrix {
    // 3x3 matrix for camera->XYZ or camera->linear sRGB
    float m[9] {1,0,0, 0,1,0, 0,0,1};
};

class RawBase : public QObject {
    Q_OBJECT
public:
    explicit RawBase(QString srcPath = QString(), QObject* parent=nullptr);
    virtual ~RawBase() override;

    // --- High-level API (keeps your current caller signatures) --------------
    virtual bool parse(MetadataParameters &p,
                       ImageMetadata &m,
                       IFD *ifd,
                       IRB *irb,
                       IPTC *iptc,
                       Xmp *xmp,
                       GPS *gps);

    virtual bool parse(ImageMetadata &m, QString fPath);
    virtual bool parseForDecoding(MetadataParameters &p, IFD *ifd);

    // decode using QFile mapped to memory
    virtual bool decode(MetadataParameters &p, QImage &image, int newSize = 0);

    // decode using QFile not mapped (legacy signature)
    virtual bool decode(ImageMetadata &m, QString &fPath, QImage &image,
                        bool thumb = false, int maxDim = 0);

    // decode from cache decoder (legacy path)
    virtual bool decode(QString fPath, quint32 offset, QImage &image);

    // Accessors
    QString sourcePath() const { return fPath; }
    QSize rawSize() const { return QSize(rawW, rawH); }
    int rawBitsPerSample() const { return bps; }
    CFAPattern rawCFA() const { return cfa; }

protected:
    // --- Container phase (subclass specializations) -------------------------
    virtual bool loadContainer(MetadataParameters &p) = 0;        // open + map + validate
    virtual bool unloadContainer();                               // close + unmap

    // Fill out structural details common to decode path
    virtual bool readIFDsAndTags(MetadataParameters &p, IFD *ifd) = 0;

    // Locate and expose the sensor plane (Bayer/packed)
    virtual bool extractRawPlane(MetadataParameters &p, RawPlaneView &out) = 0;

    // If available, try to extract an embedded preview (JPEG/HEIC)
    virtual bool tryExtractEmbeddedPreview(MetadataParameters &p, QImage &image,
                                           int maxDim);

    // --- Processing phase (usually shared) ---------------------------------
    virtual bool unpackAndLinearize(const RawPlaneView &src, QVector<float> &r,
                                    QVector<float> &g, QVector<float> &b);

    virtual bool demosaic(const RawPlaneView &src,
                          QVector<float> &r,
                          QVector<float> &g,
                          QVector<float> &b);

    virtual bool colorCorrect(const QVector<float> &r,
                              const QVector<float> &g,
                              const QVector<float> &b,
                              QImage &out, bool to16bit = true);

    // Utilities helpers
    static CFAPattern cfaFromTag(quint32 cfaPatternTag, const QByteArray &pattern);
    static void endianSwap16(void* data, int elements);

protected:
    // --- Shared state -------------------------------------------------------
    QString fPath;
    QFile file;           // unmapped file handle (some RAWs need seeks)
    uchar *mapData = nullptr;
    qint64 mapSize = 0;

    // Basic RAW properties (filled by subclass during readIFDsAndTags)
    int rawW = 0;
    int rawH = 0;
    int bps = 0;                  // bits per sample
    CFAPattern cfa = CFAPattern::Unknown;
    bool isLittleEndian = true;
    ColorMatrix camToRGB;         // simple 3x3; refined later if needed

    // Working buffers (avoid reallocations)
    QByteArray rawBuffer;         // holds unpacked mosaic if needed
};
#endif // RAWBASE_H
