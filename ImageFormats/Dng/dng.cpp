#include "dng.h"
#include "Main/global.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Raw/tiffwalk.h"
#include "ImageFormats/Raw/losslessjpeg.h"
#include "ImageFormats/Raw/rawimage.h"
#include <cmath>

DNG::DNG()
{
}

bool DNG::parse(MetadataParameters &p,
                ImageMetadata &m,
                IFD *ifd,
                IPTC */*iptc*/,
                Exif *exif,
                Jpeg *jpeg,
                GPS *gps)
{
    // p.file.open happens in readMetadata

    quint32 startOffset = 0;

    // first two bytes is the endian order
    quint16 order = u.get16(p.file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // should be magic number 42 next
    if (u.get16(p.file.read(2), isBigEnd) != 42) return false;

    // read offset to first IFD
    quint32 ifdOffset = u.get32(p.file.read(4), isBigEnd);
    p.hdr = "IFD0";
    p.offset = ifdOffset;
    p.hash = &exif->hash;
    ifd->readIFD(p);

    m.lengthFull = 1;  // set arbitrary length to avoid error msg as tif do not
                          // have full size embedded jpg

    // IFD0: *******************************************************************

    // IFD0 Offsets
    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue;

    quint32 ifdEXIFOffset = 0;
    if (ifd->ifdDataHash.contains(34665))
        ifdEXIFOffset = ifd->ifdDataHash.value(34665).tagValue;

    quint32 ifdPhotoshopOffset = 0;
    if (ifd->ifdDataHash.contains(34377))
        ifdPhotoshopOffset = ifd->ifdDataHash.value(34377).tagValue;

    quint32 ifdIPTCOffset = 0;
    if (ifd->ifdDataHash.contains(33723))
        ifdIPTCOffset = ifd->ifdDataHash.value(33723).tagValue;

    quint32 ifdXMPOffset = 0;
    if (ifd->ifdDataHash.contains(700)) {
        m.isXmp = true;
        ifdXMPOffset = ifd->ifdDataHash.value(700).tagValue;
        m.xmpSegmentOffset = ifdXMPOffset;
        m.xmpSegmentLength = static_cast<quint32>(ifd->ifdDataHash.value(700).tagCount);
    }

    // IFD0: Model
    (ifd->ifdDataHash.contains(272))
        ? m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount)
        : m.model = "";

    // IFD0: Make
    (ifd->ifdDataHash.contains(271))
        ? m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
        ifd->ifdDataHash.value(271).tagCount)
        : m.make = "";

    // IFD0: Title (ImageDescription)
    (ifd->ifdDataHash.contains(270))
        ? m.title = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(270).tagCount)
        : m.title = "";

    // IFD0: Creator (artist)
    (ifd->ifdDataHash.contains(315))
        ? m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(315).tagCount)
        : m.creator = "";

    // IFD0: Copyright
    (ifd->ifdDataHash.contains(33432))
            ? m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                                  ifd->ifdDataHash.value(33432).tagCount)
            : m.copyright = "";

    // IFD0: width
    (ifd->ifdDataHash.contains(256))
        ? m.width = ifd->ifdDataHash.value(256).tagValue
        : m.width = 0;

    // IFD0: height
    (ifd->ifdDataHash.contains(257))
        ? m.height = ifd->ifdDataHash.value(257).tagValue
        : m.height = 0;

    // IFD0: orientation
    (ifd->ifdDataHash.contains(274))
        ? m.orientation = ifd->ifdDataHash.value(274).tagValue
        : m.orientation = 0;

    p.offset = 0;
    if (!m.width || !m.height) jpeg->getDimensions(p, m);

    // SubIFDs: ****************************************************************
    /* The DNG subIFDs each contain info for the embedded previews.  We record each
       one and then determine which ones are JPG and assign the smallest as a thumb
       and the largest as the full size preview JPG.
       */
    struct JpgInfo {
        quint32 width;
        quint32 height;
        quint32 offset;
        quint32 length;
    } jpgInfo;

    QList<JpgInfo> jpgs;
    QList<quint32> ifdOffsets;
    if (ifd->ifdDataHash.contains(330)) {
        int count = static_cast<int>(ifd->ifdDataHash.value(330).tagCount);
        if (count > 1) {
            quint32 addr = ifd->ifdDataHash.value(330).tagValue;
            ifdOffsets = ifd->getSubIfdOffsets(p.file, addr, count);
        }
        else ifdOffsets.append(ifd->ifdDataHash.value(330).tagValue);

        QString hdr;
        count = 0;
        quint32 smallest = 999999;
        quint32 largest = 0;
        int smallJpg = 0;
        int largeJpg = 0;

        // iterate subIFDs looking for embedded JPGs
        for (int i = 0; i < ifdOffsets.length(); ++i) {
            p.hdr = "SubIFD" + QString::number(i + 1);
            p.offset = ifdOffsets[i];
            p.hash = &exif->hash;
            ifd->readIFD(p);
            if (ifd->ifdDataHash.contains(273)) {
                // is it a JPG
                quint32 offset = ifd->ifdDataHash.value(273).tagValue;
                p.file.seek(offset);
                if (u.get16(p.file.read(2), true) != 0xFFD8) break;  // order = 4949 so reverse
                // yes it is a JPG
                jpgInfo.offset = offset;
                jpgInfo.length = ifd->ifdDataHash.value(279).tagValue;
                jpgInfo.width = ifd->ifdDataHash.value(256).tagValue;
                jpgInfo.height = ifd->ifdDataHash.value(257).tagValue;
                /*
                qDebug() << "DNG::parse" << "i =" << i
                         << "jpgInfo.offset =" << jpgInfo.offset
                         << "jpgInfo.length" << jpgInfo.length
                         << "jpgInfo.width" << jpgInfo.width
                         << "jpgInfo.height" << jpgInfo.height;
//                */
                jpgs.append(jpgInfo);
                // find smallest and largest
                if (jpgInfo.width < smallest) {
                    smallest = jpgInfo.width;
                    smallJpg = count;
                }
                if (jpgInfo.width > largest) {
                    largest = jpgInfo.width;
                    largeJpg = count;
                }
                count++;
            }
            // check embedded raw file to determine true image dimensions as the preview
            // might be smaller (for reporting only)
            else {
                if (ifd->ifdDataHash.contains(256) && ifd->ifdDataHash.contains(257)) {
                    m.width =static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
                    m.height =static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
                }
            }
        }
        if (jpgs.length() > 0) {
            m.widthPreview = static_cast<uint>(jpgs.at(largeJpg).width);
            m.heightPreview = static_cast<uint>(jpgs.at(largeJpg).height);
            if (m.widthPreview > m.width) {
                m.width = m.widthPreview;
                m.height = m.heightPreview;
            }
            m.offsetFull = static_cast<uint>(jpgs.at(largeJpg).offset);
            m.lengthFull = static_cast<uint>(jpgs.at(largeJpg).length);
            m.offsetThumb = static_cast<uint>(jpgs.at(smallJpg).offset);
            m.lengthThumb = static_cast<uint>(jpgs.at(smallJpg).length);
        }
    }

    // EXIF: *******************************************************************

    if (ifdEXIFOffset) {
        p.hdr = "IFD Exif";
        p.offset = ifdEXIFOffset;
        p.hash = &exif->hash;
        ifd->readIFD(p);
    }

    // EXIF: created datetime
    QString createdExif;
    (ifd->ifdDataHash.contains(36868))
        ? createdExif = u.getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount).left(19)
        : createdExif = "";
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      isBigEnd);
        if (x < 1 ) {
            int t = qRound(1 / x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = static_cast<float>(x);
        }
        else {
            uint t = static_cast<uint>(x);
//            uint t = (uint)x;
            m.exposureTime = QString::number(t);
            m.exposureTimeNum = t;
        }
        m.exposureTime += " sec";
    } else {
        m.exposureTime = "";
    }

    // EXIF: aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33437).tagValue + startOffset,
                                      isBigEnd);
        m.aperture = "f/" + QString::number(x, 'f', 1);
        m.apertureNum = static_cast<float>(qRound(x * 10) / 10.0);
    } else {
        m.aperture = "";
        m.apertureNum = 0;
    }

    // EXIF: ISO
    if (ifd->ifdDataHash.contains(34855)) {
        quint32 x = ifd->ifdDataHash.value(34855).tagValue;
        m.ISONum = static_cast<int>(x);
        m.ISO = QString::number(m.ISONum);
    } else {
        m.ISO = "";
        m.ISONum = 0;
    }

    // EXIF: Exposure compensation
    if (ifd->ifdDataHash.contains(37380)) {
        // tagType = 10 signed rational
        double x = u.getReal_s(p.file,
                                      ifd->ifdDataHash.value(37380).tagValue + startOffset,
                                      isBigEnd);
        m.exposureCompensation = QString::number(x, 'f', 1) + " EV";
        m.exposureCompensationNum = x;
    } else {
        m.exposureCompensation = "";
        m.exposureCompensationNum = 0;
    }

    // EXIF: focal length
    if (ifd->ifdDataHash.contains(37386)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(37386).tagValue + startOffset,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // EXIF: lens model
    (ifd->ifdDataHash.contains(42036))
            ? m.lens = u.getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
                                  ifd->ifdDataHash.value(42036).tagCount)
            : m.lens = "";

    // Photoshop: **************************************************************
    /* Unlike TIFF, DNG is not parsed for an IRB (Photoshop Image Resource Block,
       tag 34377) thumbnail. The DNG spec stores previews in subIFDs (tag 330),
       which are handled above: the smallest embedded JPG becomes the thumb and the
       largest the full preview. An IRB thumb only exists if the DNG was round-tripped
       through Photoshop, so at best it would be a fallback when no subIFD JPG preview
       is present - a rare combination. If that fallback is ever needed, call
       IRB::readThumb() here gated on !m.lengthThumb so it cannot clobber the proper
       subIFD thumb (mirroring Tiff::parse). ifdPhotoshopOffset (read above) is
       retained for that purpose. */

    // IPTC: *******************************************************************
    // Get title if available

//    if (ifdIPTCOffset) readIPTC(ifdIPTCOffset);

    // jpg preview metadata report information
    if (p.report) {
        p.offset = m.offsetFull;
        Jpeg jpeg;
        IPTC iptc;
        GPS gps;
        jpeg.parse(p, m, ifd, &iptc, exif, &gps);
    }

    // read GPSIFD
    if (offsetGPS) {
        p.file.seek(offsetGPS);
        p.hdr = "IFD GPS";
        p.hash = &gps->hash;
        p.offset = offsetGPS;
        ifd->readIFD(p);

        if (ifd->ifdDataHash.contains(1)) {  // 1 = GPSLatitudeRef
            // process GPS info
            QString gpsCoord = gps->decode(p.file, ifd->ifdDataHash, isBigEnd);
            m.gpsCoord = gpsCoord;
        }
    }

    // read XMP
    bool okToReadXmp = true;
    if (m.isXmp && okToReadXmp && !G::stop) {
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength, p.instance);
        if (xmp.isValid) {
            p.xmpModifyDate = QDateTime::fromString(xmp.getItem("modifydate"), Qt::ISODate);
            m.rating = xmp.getItem("Rating");
            m.label = xmp.getItem("Label");
            m.title = xmp.getItem("title");
            m.cameraSN = xmp.getItem("SerialNumber");
            if (m.lens.isEmpty()) m.lens = xmp.getItem("Lens");
            m.lensSN = xmp.getItem("LensSerialNumber");
            if (m.creator.isEmpty()) m.creator = xmp.getItem("creator");
            m.copyright = xmp.getItem("rights");
            m.email = xmp.getItem("email");
            m.url = xmp.getItem("url");
        }

        // save original values so can determine if edited when writing changes
        m._rating = m.rating;
        m._label = m.label;
        m._title = m.title;
        m._creator = m.creator;
        m._copyright = m.copyright;
        m._email  = m.email ;
        m._url = m.url;
        m._orientation = m.orientation;
        m._rotationDegrees = m.rotationDegrees;

        if (p.report) p.xmpString = xmp.srcToString();
    }

    return true;
}

/* ------------------------------------------------------------------------------------------
   DngRaw::UnpackCfa  --  Adobe DNG sensor unpack (uncompressed + lossless JPEG, strip + tile)
   ------------------------------------------------------------------------------------------ */

namespace {

/* DNG / TIFF-EP tag numbers used here. */
enum DngTag {
    ImageWidth = 256, ImageLength = 257, BitsPerSample = 258, Compression = 259,
    PhotometricInterpretation = 262, StripOffsets = 273, RowsPerStrip = 278,
    StripByteCounts = 279, TileWidth = 322, TileLength = 323, TileOffsets = 324,
    TileByteCounts = 325, NewSubfileType = 254, CFAPatternTag = 33422,
    BlackLevel = 50714, WhiteLevel = 50717, ColorMatrix1 = 50721, ColorMatrix2 = 50722,
    AsShotNeutral = 50728
};
const int PHOTO_CFA = 32803;

/* Place one decoded lossless-JPEG segment (component-interleaved) into the CFA at (gx,gy). */
void placeLjpeg(const LosslessJpeg::Image &im, int gx, int gy,
                std::vector<uint16_t> &cfa, int W, int H)
{
    const int cs = im.components;
    for (int ty = 0; ty < im.height; ++ty) {
        const int cy = gy + ty;
        if (cy < 0 || cy >= H) continue;
        for (int tx = 0; tx < im.width; ++tx) {
            for (int c = 0; c < cs; ++c) {
                const int cx = gx + tx * cs + c;
                if (cx < 0 || cx >= W) continue;
                cfa[size_t(cy) * W + cx] =
                    im.samples[(size_t(ty) * im.width + tx) * cs + c];
            }
        }
    }
}

} // namespace

bool DngRaw::UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw)
{
    Q_UNUSED(m)
    using namespace TiffWalk;

    Reader r;
    if (!r.init(&file)) { errMsg = "DNG: not a TIFF/EP file."; return false; }

    /* Walk IFD0 + SubIFDs + the next-IFD chain; keep the CFA image IFD (Photometric == CFA),
       preferring the full-resolution one (NewSubfileType bit0 == 0) with the most pixels. */
    QList<quint32> queue { r.firstIfd() };
    QSet<quint32> seen;
    Ifd cfaIfd;
    Ifd ifd0;                       // first IFD: holds the colour tags (ColorMatrix, AsShotNeutral)
    quint64 bestPixels = 0;
    bool haveIfd0 = false;

    while (!queue.isEmpty()) {
        const quint32 off = queue.takeFirst();
        if (off == 0 || seen.contains(off)) continue;
        seen.insert(off);

        Ifd tags;
        QList<quint32> subs;
        quint32 next = 0;
        if (!r.readIfd(off, tags, subs, next)) continue;
        if (!haveIfd0) { ifd0 = tags; haveIfd0 = true; }
        for (quint32 s : subs) queue << s;
        if (next) queue << next;

        if (tags.contains(PhotometricInterpretation) &&
            int(r.scalar(tags[PhotometricInterpretation])) == PHOTO_CFA &&
            tags.contains(ImageWidth) && tags.contains(ImageLength)) {
            const bool reduced = tags.contains(NewSubfileType) &&
                                 (r.scalar(tags[NewSubfileType]) & 1);
            const quint64 px = quint64(r.scalar(tags[ImageWidth])) *
                               r.scalar(tags[ImageLength]);
            if (!reduced && px > bestPixels) { bestPixels = px; cfaIfd = tags; }
        }
    }

    if (cfaIfd.isEmpty()) { errMsg = "DNG: no CFA image found."; return false; }

    const int W   = int(r.scalar(cfaIfd[ImageWidth]));
    const int H   = int(r.scalar(cfaIfd[ImageLength]));
    const int bps = cfaIfd.contains(BitsPerSample) ? int(r.scalar(cfaIfd[BitsPerSample])) : 16;
    const int comp = cfaIfd.contains(Compression) ? int(r.scalar(cfaIfd[Compression])) : 1;
    if (W <= 0 || H <= 0 || bps <= 0 || bps > 16) { errMsg = "DNG: bad CFA geometry."; return false; }
    if (comp != 1 && comp != 7) { errMsg = "DNG: unsupported compression (only 1 / 7)."; return false; }

    raw.width = W;
    raw.height = H;
    raw.cfa.assign(size_t(W) * size_t(H), 0);

    /* Build the list of segments (offset, length, grid position) -- tiles or strips. */
    struct Seg { quint32 off, len; int gx, gy; };
    QVector<Seg> segs;
    int segW, segH;
    if (cfaIfd.contains(TileOffsets) && cfaIfd.contains(TileWidth)) {
        segW = int(r.scalar(cfaIfd[TileWidth]));
        segH = int(r.scalar(cfaIfd[TileLength]));
        const QVector<quint32> offs = r.u32s(cfaIfd[TileOffsets]);
        const QVector<quint32> lens = r.u32s(cfaIfd[TileByteCounts]);
        if (segW <= 0 || segH <= 0 || offs.size() != lens.size() || offs.isEmpty()) {
            errMsg = "DNG: bad tile layout."; return false;
        }
        const int across = (W + segW - 1) / segW;
        for (int i = 0; i < offs.size(); ++i)
            segs << Seg{ offs[i], lens[i], (i % across) * segW, (i / across) * segH };
    } else if (cfaIfd.contains(StripOffsets)) {
        segW = W;
        segH = cfaIfd.contains(RowsPerStrip) ? int(r.scalar(cfaIfd[RowsPerStrip])) : H;
        if (segH <= 0) segH = H;
        const QVector<quint32> offs = r.u32s(cfaIfd[StripOffsets]);
        const QVector<quint32> lens = r.u32s(cfaIfd[StripByteCounts]);
        if (offs.size() != lens.size() || offs.isEmpty()) { errMsg = "DNG: bad strip layout."; return false; }
        for (int i = 0; i < offs.size(); ++i)
            segs << Seg{ offs[i], lens[i], 0, i * segH };
    } else {
        errMsg = "DNG: no strips or tiles."; return false;
    }

    /* Decode each segment into the CFA. */
    for (const Seg &s : segs) {
        if (!file.seek(s.off)) { errMsg = "DNG: seek to segment failed."; return false; }
        const QByteArray buf = file.read(int(s.len));
        if (buf.size() < int(s.len)) { errMsg = "DNG: short segment read."; return false; }

        if (comp == 7) {
            LosslessJpeg::Image im;
            QString lerr;
            if (!LosslessJpeg::Decode(reinterpret_cast<const uint8_t *>(buf.constData()),
                                      size_t(buf.size()), im, &lerr)) {
                errMsg = "DNG: lossless JPEG decode failed (" + lerr + ").";
                return false;
            }
            placeLjpeg(im, s.gx, s.gy, raw.cfa, W, H);
        } else {
            /* Uncompressed: 16-bit little/big-endian samples, segW x segH, row-major. */
            const uchar *p = reinterpret_cast<const uchar *>(buf.constData());
            const int rows = qMin(segH, H - s.gy);
            const int cols = qMin(segW, W - s.gx);
            const size_t avail = size_t(buf.size()) / 2;
            for (int ty = 0; ty < rows; ++ty) {
                for (int tx = 0; tx < cols; ++tx) {
                    const size_t idx = size_t(ty) * segW + tx;
                    if (idx >= avail) break;
                    const uchar *q = p + idx * 2;
                    const uint16_t v = r.big() ? uint16_t((q[0] << 8) | q[1])
                                               : uint16_t((q[1] << 8) | q[0]);
                    raw.cfa[size_t(s.gy + ty) * W + (s.gx + tx)] = v;
                }
            }
        }
    }

    /* CFA pattern. */
    if (cfaIfd.contains(CFAPatternTag)) {
        const QByteArray pat = r.bytes(cfaIfd[CFAPatternTag]);
        if (pat.size() >= 4) {
            uint8_t pc[4];
            for (int i = 0; i < 4; ++i) pc[i] = uint8_t(pat[i]);
            raw.pattern = cfaPatternFromPlaneColor(pc);
        }
    }
    if (raw.pattern == CfaPattern::Unknown) raw.pattern = CfaPattern::RGGB;

    /* Levels. */
    raw.white = cfaIfd.contains(WhiteLevel) ? uint16_t(r.scalar(cfaIfd[WhiteLevel]))
                                            : uint16_t((1u << bps) - 1);
    if (cfaIfd.contains(BlackLevel)) {
        const QVector<double> bl = r.reals(cfaIfd[BlackLevel]);
        for (int i = 0; i < 4; ++i) {
            const double v = bl.isEmpty() ? 0.0 : bl[i < bl.size() ? i : 0];
            raw.black[i] = uint16_t(v < 0 ? 0 : v + 0.5);
        }
    }

    /* Colour: DNG carries its own. xyzToCam = ColorMatrix2 (D65) when present, else
       ColorMatrix1. WB from AsShotNeutral (camMul = 1 / neutral). These tags live in IFD0. */
    const Ifd &col = haveIfd0 ? ifd0 : cfaIfd;
    const quint16 cmTag = col.contains(ColorMatrix2) ? quint16(ColorMatrix2)
                        : col.contains(ColorMatrix1) ? quint16(ColorMatrix1) : 0;
    if (cmTag) {
        const QVector<double> cm = r.reals(col[cmTag]);
        if (cm.size() >= 9)
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    raw.xyzToCam[i][j] = float(cm[i * 3 + j]);
    }
    if (col.contains(AsShotNeutral)) {
        const QVector<double> n = r.reals(col[AsShotNeutral]);
        if (n.size() >= 3 && n[0] > 0 && n[1] > 0 && n[2] > 0) {
            raw.camMul[0] = float(1.0 / n[0]);
            raw.camMul[1] = float(1.0 / n[1]);
            raw.camMul[2] = float(1.0 / n[2]);
            raw.camMul[3] = float(1.0 / n[1]);
        }
    }

    return true;
}
