#include "dng.h"
#include "Metadata/xmpapply.h"
#include "Main/global.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Raw/tiffwalk.h"
#include "ImageFormats/Raw/losslessjpeg.h"
#include "ImageFormats/Raw/rawimage.h"
#include "ImageFormats/Dng/dngjxl.h"
#include <opencv2/core.hpp>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

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

    /* An IFD holds a usable JPEG preview only if its strip starts with a JPEG SOI and
       it is not the raw data itself: a lossless-JPEG (compression 7) CFA or Linear Raw
       strip starts with FFD8 too. DNG 1.7 previews compressed as JPEG XL (52546, e.g.
       Lightroom pano/HDR merges) do not start with FFD8 and are skipped here; on macOS
       ImageDecoder reads them through ImageIO. */
    auto jpgInIfd = [&]() -> bool {
        if (!ifd->ifdDataHash.contains(273) || !ifd->ifdDataHash.contains(279)) return false;
        if (ifd->ifdDataHash.contains(262)) {
            const quint32 photometric = ifd->ifdDataHash.value(262).tagValue;
            // CFA or LinearRaw
            if (photometric == 32803 || photometric == 34892) return false;
        }
        if (!p.file.seek(ifd->ifdDataHash.value(273).tagValue)) return false;
        return u.get16(p.file.read(2), true) == 0xFFD8;
    };
    auto jpgFromIfd = [&]() -> JpgInfo {
        JpgInfo j;
        j.offset = ifd->ifdDataHash.value(273).tagValue;
        j.length = ifd->ifdDataHash.value(279).tagValue;
        j.width = ifd->ifdDataHash.value(256).tagValue;
        j.height = ifd->ifdDataHash.value(257).tagValue;
        return j;
    };

    /* IFD0 is usually a small thumbnail, sometimes a JPEG. When every subIFD preview is
       JPEG XL it is the only JPEG in the file, so it is a candidate too. */
    if (jpgInIfd()) jpgs.append(jpgFromIfd());

    QList<quint32> ifdOffsets;
    if (ifd->ifdDataHash.contains(330)) {
        int count = static_cast<int>(ifd->ifdDataHash.value(330).tagCount);
        if (count > 1) {
            quint32 addr = ifd->ifdDataHash.value(330).tagValue;
            ifdOffsets = ifd->getSubIfdOffsets(p.file, addr, count);
        }
        else ifdOffsets.append(ifd->ifdDataHash.value(330).tagValue);

        // iterate subIFDs looking for embedded JPGs (a non-JPEG one does not stop it)
        for (int i = 0; i < ifdOffsets.length(); ++i) {
            p.hdr = "SubIFD" + QString::number(i + 1);
            p.offset = ifdOffsets[i];
            p.hash = &exif->hash;
            ifd->readIFD(p);
            if (jpgInIfd()) {
                jpgInfo = jpgFromIfd();
                /*
                qDebug() << "DNG::parse" << "i =" << i
                         << "jpgInfo.offset =" << jpgInfo.offset
                         << "jpgInfo.length" << jpgInfo.length
                         << "jpgInfo.width" << jpgInfo.width
                         << "jpgInfo.height" << jpgInfo.height;
//                */
                jpgs.append(jpgInfo);
            }
            /* The full-resolution image (NewSubfileType 0) gives the true dimensions, as
               the preview might be smaller (for reporting only). Reduced-resolution and
               transparency-mask IFDs must not overwrite them. */
            else if (!ifd->ifdDataHash.contains(254)
                     || ifd->ifdDataHash.value(254).tagValue == 0) {
                if (ifd->ifdDataHash.contains(256) && ifd->ifdDataHash.contains(257)) {
                    m.width =static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
                    m.height =static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
                }
            }
        }
    }

    // smallest JPG is the thumb, largest the full size preview
    {
        quint32 smallest = 999999;
        quint32 largest = 0;
        int smallJpg = 0;
        int largeJpg = 0;
        for (int i = 0; i < jpgs.length(); ++i) {
            if (jpgs.at(i).width < smallest) {
                smallest = jpgs.at(i).width;
                smallJpg = i;
            }
            if (jpgs.at(i).width > largest) {
                largest = jpgs.at(i).width;
                largeJpg = i;
            }
        }
        if (jpgs.length() > 0) {
            m.widthOrigPreview = static_cast<uint>(jpgs.at(largeJpg).width);
            m.heightOrigPreview = static_cast<uint>(jpgs.at(largeJpg).height);
            if (m.widthOrigPreview > m.width) {
                m.width = m.widthOrigPreview;
                m.height = m.heightOrigPreview;
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
        applyXmp(xmp, m, p);

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
    LinearizationTable = 50712,
    BlackLevel = 50714, WhiteLevel = 50717, ColorMatrix1 = 50721, ColorMatrix2 = 50722,
    AsShotNeutral = 50728, AsShotWhiteXY = 50729, NoiseProfile = 51041,
    SamplesPerPixel = 277, SampleFormat = 339, BlackLevelRepeatDim = 50713,
    OpcodeList2 = 51009
};
const int PHOTO_CFA = 32803;
const int PHOTO_LINEAR_RAW = 34892;
const int COMP_NONE = 1, COMP_LJPEG = 7, COMP_JXL = 52546;

/* Every IFD in the file -- IFD0 first, then SubIFDs and the next-IFD chain,
   breadth first. Cycles and unreadable IFDs are skipped. */
QVector<TiffWalk::Ifd> allIfds(TiffWalk::Reader &r)
{
    QVector<TiffWalk::Ifd> out;
    QList<quint32> queue { r.firstIfd() };
    QSet<quint32> seen;
    while (!queue.isEmpty()) {
        const quint32 off = queue.takeFirst();
        if (off == 0 || seen.contains(off)) continue;
        seen.insert(off);
        TiffWalk::Ifd tags;
        QList<quint32> subs;
        quint32 next = 0;
        if (!r.readIfd(off, tags, subs, next)) continue;
        out << tags;
        for (quint32 sub : subs) queue << sub;
        if (next) queue << next;
    }
    return out;
}

/* The full-resolution (NewSubfileType bit0 == 0) image with this Photometric and the
   most pixels, or an empty Ifd. */
TiffWalk::Ifd mainImage(TiffWalk::Reader &r, const QVector<TiffWalk::Ifd> &ifds,
                        int photometric)
{
    TiffWalk::Ifd best;
    quint64 bestPixels = 0;
    for (const TiffWalk::Ifd &tags : ifds) {
        if (!tags.contains(PhotometricInterpretation) ||
            int(r.scalar(tags[PhotometricInterpretation])) != photometric ||
            !tags.contains(ImageWidth) || !tags.contains(ImageLength)) continue;
        const bool reduced = tags.contains(NewSubfileType) &&
                             (r.scalar(tags[NewSubfileType]) & 1);
        const quint64 px = quint64(r.scalar(tags[ImageWidth])) *
                           r.scalar(tags[ImageLength]);
        if (!reduced && px > bestPixels) { bestPixels = px; best = tags; }
    }
    return best;
}

/* One tile or strip: file offset, byte count, and its top-left in the image. */
struct Seg { quint32 off, len; int gx, gy; };

/* The tiles (or strips) of an image IFD, with the nominal segment size. */
bool segments(TiffWalk::Reader &r, const TiffWalk::Ifd &ifd, int W, int H,
              QVector<Seg> &segs, int &segW, int &segH, QString &err)
{
    segs.clear();
    if (ifd.contains(TileOffsets) && ifd.contains(TileWidth)) {
        segW = int(r.scalar(ifd[TileWidth]));
        segH = ifd.contains(TileLength) ? int(r.scalar(ifd[TileLength])) : 0;
        const QVector<quint32> offs = r.u32s(ifd[TileOffsets]);
        const QVector<quint32> lens = ifd.contains(TileByteCounts)
                                    ? r.u32s(ifd[TileByteCounts]) : QVector<quint32>();
        if (segW <= 0 || segH <= 0 || offs.size() != lens.size() || offs.isEmpty()) {
            err = "DNG: bad tile layout."; return false;
        }
        const int across = (W + segW - 1) / segW;
        for (int i = 0; i < offs.size(); ++i)
            segs << Seg{ offs[i], lens[i], (i % across) * segW, (i / across) * segH };
    } else if (ifd.contains(StripOffsets)) {
        segW = W;
        segH = ifd.contains(RowsPerStrip) ? int(r.scalar(ifd[RowsPerStrip])) : H;
        if (segH <= 0) segH = H;
        const QVector<quint32> offs = r.u32s(ifd[StripOffsets]);
        const QVector<quint32> lens = ifd.contains(StripByteCounts)
                                    ? r.u32s(ifd[StripByteCounts]) : QVector<quint32>();
        if (offs.size() != lens.size() || offs.isEmpty()) {
            err = "DNG: bad strip layout."; return false;
        }
        for (int i = 0; i < offs.size(); ++i)
            segs << Seg{ offs[i], lens[i], 0, i * segH };
    } else {
        err = "DNG: no strips or tiles."; return false;
    }
    return true;
}

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
    using namespace TiffWalk;

    Reader r;
    if (!r.init(&file)) { errMsg = "DNG: not a TIFF/EP file."; return false; }

    /* Walk IFD0 + SubIFDs + the next-IFD chain; keep the CFA image IFD (Photometric == CFA),
       preferring the full-resolution one (NewSubfileType bit0 == 0) with the most pixels. */
    const QVector<Ifd> ifds = allIfds(r);
    const bool haveIfd0 = !ifds.isEmpty();
    const Ifd ifd0 = haveIfd0 ? ifds.first() : Ifd();  // ColorMatrix, AsShotNeutral
    const Ifd cfaIfd = mainImage(r, ifds, PHOTO_CFA);

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
    QVector<Seg> segs;
    int segW, segH;
    if (!segments(r, cfaIfd, W, H, segs, segW, segH, errMsg)) return false;

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

    /* NoiseProfile (var = scale*x + offset, x in the black-subtracted white-normalised
       [0,1] domain) -- exactly PMRID's (k,b), so hand it straight through. Skip when a
       LinearizationTable is present: it remaps the domain the profile was measured in.
       Values are paired (scale, offset) per colour plane in CFAPlaneColor order; for
       Bayer that is R,G,B, so the green plane is index 1 (else the single/first pair). */
    if (!(cfaIfd.contains(LinearizationTable) ||
          (haveIfd0 && ifd0.contains(LinearizationTable)))) {
        QVector<double> v;
        if (cfaIfd.contains(NoiseProfile))                v = r.reals(cfaIfd[NoiseProfile]);
        else if (haveIfd0 && ifd0.contains(NoiseProfile)) v = r.reals(ifd0[NoiseProfile]);
        const int planes = int(v.size()) / 2;
        const int g = planes >= 2 ? 1 : 0;            // green plane in R,G,B order
        if (planes >= 1 && v[2 * g] > 0.0) {
            raw.hasNoiseProfile = true;
            raw.npScale  = v[2 * g];
            raw.npOffset = v[2 * g + 1] < 0.0 ? 0.0 : v[2 * g + 1];
        }
    }

    /* Colour: DNG carries its own (ColorMatrix + AsShotNeutral, both in IFD0). One
       definition, shared with the Apple Core Image engine -- see
       RawFormat::ReadAsShotColor. The rare DNG with no IFD0 falls back to the CFA IFD's
       own copy of the tags. */
    {
        RawSensorInfo ci;
        if (haveIfd0 && ReadAsShotColor(file, m, ci)) {
            for (int i = 0; i < 4; ++i) raw.camMul[i] = ci.camMul[i];
            if (ci.hasColorMatrix)
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j) raw.xyzToCam[i][j] = ci.xyzToCam[i][j];
        }
        else {
            const quint16 cmTag = cfaIfd.contains(ColorMatrix2) ? quint16(ColorMatrix2)
                                : cfaIfd.contains(ColorMatrix1) ? quint16(ColorMatrix1) : 0;
            if (cmTag) {
                const QVector<double> cm = r.reals(cfaIfd[cmTag]);
                if (cm.size() >= 9)
                    for (int i = 0; i < 3; ++i)
                        for (int j = 0; j < 3; ++j)
                            raw.xyzToCam[i][j] = float(cm[i * 3 + j]);
            }
            if (cfaIfd.contains(AsShotNeutral)) {
                const QVector<double> n = r.reals(cfaIfd[AsShotNeutral]);
                if (n.size() >= 3 && n[0] > 0 && n[1] > 0 && n[2] > 0) {
                    raw.camMul[0] = float(1.0 / n[0]);
                    raw.camMul[1] = float(1.0 / n[1]);
                    raw.camMul[2] = float(1.0 / n[2]);
                    raw.camMul[3] = float(1.0 / n[1]);
                }
            }
        }
    }

    return true;
}

/*
    DNG colour without a decode. A DNG carries its own characterisation, so this needs no
    model lookup at all: ColorMatrix2 (D65) when present, else ColorMatrix1, and the WB
    from AsShotNeutral (camMul = 1 / neutral). Both live in IFD0.

    Split out of UnpackCfa for the Apple Core Image engine, which never calls it -- see
    RawFormat::ReadAsShotColor.
*/
bool DngRaw::ReadAsShotColor(QFile &file, const ImageMetadata &m, RawSensorInfo &info)
{
    Q_UNUSED(m)
    using namespace TiffWalk;

    Reader r;
    if (!r.init(&file)) return false;
    Ifd ifd0; QList<quint32> subs; quint32 next = 0;
    if (!r.readIfd(r.firstIfd(), ifd0, subs, next)) return false;

    const quint16 cmTag = ifd0.contains(ColorMatrix2) ? quint16(ColorMatrix2)
                        : ifd0.contains(ColorMatrix1) ? quint16(ColorMatrix1) : 0;
    if (cmTag) {
        const QVector<double> cm = r.reals(ifd0[cmTag]);
        if (cm.size() >= 9) {
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    info.xyzToCam[i][j] = float(cm[i * 3 + j]);
            info.hasColorMatrix = true;
        }
    }
    if (ifd0.contains(AsShotNeutral)) {
        const QVector<double> n = r.reals(ifd0[AsShotNeutral]);
        if (n.size() >= 3 && n[0] > 0 && n[1] > 0 && n[2] > 0) {
            info.camMul[0] = float(1.0 / n[0]);
            info.camMul[1] = float(1.0 / n[1]);
            info.camMul[2] = float(1.0 / n[2]);
            info.camMul[3] = float(1.0 / n[1]);
        }
    } else if (ifd0.contains(AsShotWhiteXY) && info.hasColorMatrix) {
        /* The DNG spec's other as-shot form: a white chromaticity instead of a camera
           neutral. Lightroom's merged Linear Raw DNGs (pano/HDR) write only this, and
           without it they read the matrix neutral. neutral = ColorMatrix * XYZ(white). */
        const QVector<double> xy = r.reals(ifd0[AsShotWhiteXY]);
        if (xy.size() >= 2 && xy[0] > 0 && xy[1] > 0 && xy[0] + xy[1] < 1) {
            const double xyz[3] = { xy[0] / xy[1], 1.0, (1.0 - xy[0] - xy[1]) / xy[1] };
            double n[3];
            for (int i = 0; i < 3; ++i)
                n[i] = info.xyzToCam[i][0] * xyz[0] + info.xyzToCam[i][1] * xyz[1] +
                       info.xyzToCam[i][2] * xyz[2];
            if (n[0] > 0 && n[1] > 0 && n[2] > 0) {
                info.camMul[0] = float(n[1] / n[0]);
                info.camMul[1] = 1.0f;
                info.camMul[2] = float(n[1] / n[2]);
                info.camMul[3] = 1.0f;
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------------------------------------
   DngRaw::UnpackLinear  --  Linear Raw (already demosaiced) main image
   ------------------------------------------------------------------------------------------ */

namespace {

/* Big-endian reads for opcode lists (always big-endian, whatever the file's order). */
quint32 be32(const uchar *p) { return quint32(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
double beDouble(const uchar *p)
{
    quint64 u = 0;
    for (int i = 0; i < 8; ++i) u = (u << 8) | p[i];
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}

/*
    OpcodeList2 runs after the stored values are mapped to linear 0..1 (black subtracted,
    scaled by white) and before demosaic -- for a Linear Raw, on the final RGB. Only
    MapPolynomial (ID 8) is implemented: it is what Lightroom writes into JPEG XL Linear
    Raw files, where it undoes the per-channel encoding curve applied before compression,
    so without it the image is far too bright and badly cast. Any other opcode is
    reported and skipped (the optional ones are allowed to be).
*/
void applyOpcodeList2(const QByteArray &list, std::vector<float> &rgb, int W, int H,
                      QStringList &skipped)
{
    const uchar *p = reinterpret_cast<const uchar *>(list.constData());
    const uchar *end = p + list.size();
    if (end - p < 4) return;
    const quint32 count = be32(p);
    p += 4;
    for (quint32 k = 0; k < count && end - p >= 16; ++k) {
        const quint32 id = be32(p), flags = be32(p + 8), len = be32(p + 12);
        const uchar *q = p + 16;
        if (quint64(end - q) < len) break;
        p = q + len;

        if (id != 8 || len < 36) {
            skipped << QString("%1%2").arg(id).arg(flags & 1 ? " (optional)" : "");
            continue;
        }
        const int top = int(be32(q)), left = int(be32(q + 4));
        const int bottom = qMin(int(be32(q + 8)), H), right = qMin(int(be32(q + 12)), W);
        const int plane = int(be32(q + 16)), planes = int(be32(q + 20));
        const int rowPitch = qMax(1, int(be32(q + 24))), colPitch = qMax(1, int(be32(q + 28)));
        const int degree = int(be32(q + 32));
        if (degree < 0 || degree > 8 || len < quint32(36 + 8 * (degree + 1))) continue;
        double c[9];
        for (int i = 0; i <= degree; ++i) c[i] = beDouble(q + 36 + 8 * i);
        const int p0 = qMax(0, plane), p1 = qMin(3, plane + planes);
        if (p0 >= p1 || top >= bottom || left >= right) continue;

        const int rows = (bottom - top + rowPitch - 1) / rowPitch;
        cv::parallel_for_(cv::Range(0, rows), [&](const cv::Range &rr) {
            for (int ri = rr.start; ri < rr.end; ++ri) {
                float *row = rgb.data() + size_t(top + ri * rowPitch) * W * 3;
                for (int x = left; x < right; x += colPitch) {
                    for (int ch = p0; ch < p1; ++ch) {
                        float &v = row[size_t(x) * 3 + ch];
                        double y = c[degree];
                        for (int i = degree - 1; i >= 0; --i) y = y * v + c[i];
                        v = float(qBound(0.0, y, 1.0));
                    }
                }
            }
        });
    }
}

} // namespace

/*
    The Linear Raw main image: three samples per pixel, already demosaiced, scene-linear
    camera RGB -- what Lightroom's pano/HDR merges and "Linear DNG" conversions write.
    Tiles or strips, UNCOMPRESSED (1), LOSSLESS JPEG (7) or JPEG XL (52546, DNG 1.7, needs
    libjxl). Floating-point samples (Lightroom's HDR merges) are not supported yet and
    fail, so the caller falls back to the embedded preview.

    Per the DNG pipeline: LinearizationTable, then black subtraction and scaling by the
    white level to 0..1 (clipped), then OpcodeList2 (applyOpcodeList2). BlackLevel is per
    sample; a BlackLevelRepeatDim pattern larger than 1x1 is averaged per sample, and
    BlackLevelDeltaH/V are ignored (neither appears in Linear Raw files seen so far). The
    transparency mask of a pano is not applied: its outside area renders as the stretched
    edge pixels Lightroom stored there, exactly as in Lightroom's own embedded preview.
*/
RawFormat::LinearResult DngRaw::UnpackLinear(QFile &file, const ImageMetadata &m,
                                             RawImage &raw, std::vector<float> &rgb,
                                             const QAtomicInt *abort)
{
    using namespace TiffWalk;

    Reader r;
    if (!r.init(&file)) return LinearResult::NotLinear;    // UnpackCfa reports it
    const QVector<Ifd> ifds = allIfds(r);
    const Ifd lin = mainImage(r, ifds, PHOTO_LINEAR_RAW);
    if (lin.isEmpty()) return LinearResult::NotLinear;

    auto fail = [this](const QString &msg) {
        errMsg = "DNG Linear Raw: " + msg;
        return LinearResult::Failed;
    };
    const int W   = int(r.scalar(lin[ImageWidth]));
    const int H   = int(r.scalar(lin[ImageLength]));
    const int spp = lin.contains(SamplesPerPixel) ? int(r.scalar(lin[SamplesPerPixel])) : 1;
    const int bps = lin.contains(BitsPerSample) ? int(r.scalar(lin[BitsPerSample])) : 16;
    const int comp = lin.contains(Compression) ? int(r.scalar(lin[Compression])) : COMP_NONE;
    const int fmt = lin.contains(SampleFormat) ? int(r.scalar(lin[SampleFormat])) : 1;
    if (W <= 0 || H <= 0) return fail("bad geometry.");
    if (spp != 3) return fail(QString("%1 samples per pixel (only 3).").arg(spp));
    if (fmt != 1) return fail("floating-point samples are not supported.");
    if (bps <= 0 || bps > 16) return fail(QString("%1 bits per sample.").arg(bps));
    if (comp == COMP_JXL && !DngJxl::Available())
        return fail("JPEG XL compressed, and this build has no libjxl.");
    if (comp != COMP_NONE && comp != COMP_LJPEG && comp != COMP_JXL)
        return fail(QString("unsupported compression %1.").arg(comp));
    if (comp == COMP_NONE && bps != 16) return fail("uncompressed, but not 16-bit.");

    QVector<Seg> segs;
    int segW, segH;
    QString err;
    if (!segments(r, lin, W, H, segs, segW, segH, err)) return fail(err);

    /* Levels, per sample. */
    const double maxVal = double((1u << bps) - 1);
    double black[3] = {0, 0, 0}, white[3] = {maxVal, maxVal, maxVal};
    if (lin.contains(BlackLevel)) {
        const QVector<double> bl = r.reals(lin[BlackLevel]);
        int cells = 1;
        if (lin.contains(BlackLevelRepeatDim)) {
            const QVector<quint32> rd = r.u32s(lin[BlackLevelRepeatDim]);
            if (rd.size() >= 2) cells = qMax(1, int(rd[0] * rd[1]));
        }
        if (bl.size() >= cells * 3) {
            for (int c = 0; c < 3; ++c) {
                double sum = 0;
                for (int i = 0; i < cells; ++i) sum += bl[i * 3 + c];
                black[c] = sum / cells;
            }
        }
        else if (!bl.isEmpty()) black[0] = black[1] = black[2] = bl[0];
    }
    if (lin.contains(WhiteLevel)) {
        const QVector<double> wl = r.reals(lin[WhiteLevel]);
        for (int c = 0; c < 3; ++c)
            if (!wl.isEmpty()) white[c] = wl[c < wl.size() ? c : 0];
    }
    float offset[3], scale[3];
    for (int c = 0; c < 3; ++c) {
        if (white[c] <= black[c]) return fail("white level is not above black level.");
        offset[c] = float(black[c]);
        scale[c] = float(1.0 / (white[c] - black[c]));
    }
    QVector<quint32> table;
    if (lin.contains(LinearizationTable)) table = r.u32s(lin[LinearizationTable]);

    /* Read every segment first: QFile is not thread-safe; the decode is parallel. */
    QVector<QByteArray> bufs(segs.size());
    for (int i = 0; i < segs.size(); ++i) {
        if (!file.seek(segs[i].off)) return fail("seek to segment failed.");
        bufs[i] = file.read(int(segs[i].len));
        if (bufs[i].size() < int(segs[i].len)) return fail("short segment read.");
    }
    if (abort && abort->loadAcquire()) { errMsg = "Aborted"; return LinearResult::Failed; }

    rgb.assign(size_t(W) * size_t(H) * 3, 0.0f);
    const bool big = r.big();

    /* Decode the segments in parallel (cv::parallel_for_, as Demosaic: this runs on
       ImageCache decoder threads). Each writes its own rectangle of rgb. */
    std::atomic<bool> stop{false};
    QString segErr;
    std::mutex errMutex;
    cv::parallel_for_(cv::Range(0, int(segs.size())), [&](const cv::Range &range) {
        std::vector<float> f;
        std::vector<uint16_t> u;
        for (int i = range.start; i < range.end; ++i) {
            if (stop.load(std::memory_order_relaxed)) return;
            if (abort && abort->loadAcquire()) { stop = true; return; }
            const Seg &s = segs[i];
            const uchar *data = reinterpret_cast<const uchar *>(bufs[i].constData());
            const size_t size = size_t(bufs[i].size());

            /* Decode to stored sample values: tileW x tileH x 3, row-major. */
            int tw = 0, th = 0;
            QString e;
            bool ok = true;
            if (comp == COMP_JXL) {
                ok = DngJxl::Decode(data, size, 3, f, tw, th, &e);
                if (ok) for (float &v : f) v = float(v * maxVal);
            }
            else if (comp == COMP_LJPEG) {
                LosslessJpeg::Image im;
                ok = LosslessJpeg::Decode(data, size, im, &e);
                if (ok) {
                    /* Samples run pixel-interleaved along each row whatever the
                       component count, so a row holds width*components samples. */
                    tw = im.width * im.components / 3;
                    th = im.height;
                    f.resize(im.samples.size());
                    for (size_t k = 0; k < f.size(); ++k) f[k] = float(im.samples[k]);
                }
            }
            else {
                tw = segW;
                th = qMin(segH, int(size / (size_t(segW) * 6)));
                f.resize(size_t(tw) * th * 3);
                for (size_t k = 0; k < f.size(); ++k) {
                    const uchar *q = data + k * 2;
                    f[k] = float(big ? (q[0] << 8 | q[1]) : (q[1] << 8 | q[0]));
                }
            }
            if (!ok || tw <= 0 || th <= 0) {
                std::lock_guard<std::mutex> lock(errMutex);
                if (segErr.isEmpty()) segErr = "segment decode failed (" + e + ").";
                stop = true;
                return;
            }

            const int rows = qMin(th, H - s.gy), cols = qMin(tw, W - s.gx);
            for (int y = 0; y < rows; ++y) {
                const float *src = f.data() + size_t(y) * tw * 3;
                float *dst = rgb.data() + (size_t(s.gy + y) * W + s.gx) * 3;
                for (int x = 0; x < cols * 3; ++x) {
                    float v = src[x];
                    if (!table.isEmpty()) {
                        const int idx = qBound(0, int(v + 0.5f), int(table.size()) - 1);
                        v = float(table[idx]);
                    }
                    const int c = x % 3;
                    dst[x] = qBound(0.0f, (v - offset[c]) * scale[c], 1.0f);
                }
            }
        }
    });
    if (abort && abort->loadAcquire()) { errMsg = "Aborted"; return LinearResult::Failed; }
    if (!segErr.isEmpty()) return fail(segErr);

    if (lin.contains(OpcodeList2)) {
        QStringList skipped;
        applyOpcodeList2(r.bytes(lin[OpcodeList2]), rgb, W, H, skipped);
        if (!skipped.isEmpty())
            G::issue("Warning", "DNG OpcodeList2: skipped unsupported opcode(s) "
                     + skipped.join(", ") + ".", "DngRaw::UnpackLinear", -1,
                     file.fileName());
    }

    raw.width = W;
    raw.height = H;
    raw.white = 65535;      // unused: rgb is already 0..1
    RawSensorInfo ci;
    if (ReadAsShotColor(file, m, ci)) {
        for (int i = 0; i < 4; ++i) raw.camMul[i] = ci.camMul[i];
        if (ci.hasColorMatrix)
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) raw.xyzToCam[i][j] = ci.xyzToCam[i][j];
    }
    return LinearResult::Ok;
}

/*
    The largest JPEG XL compressed RGB preview, as 8-bit RGB. DNG 1.7 files from
    Lightroom's merges store every preview this way; the only JPEG in them is a 256 px
    IFD0 thumbnail (see DNG::parse), so this is what the loupe shows when the main image
    is not decoded. False when there is none, or the build has no libjxl.
*/
bool DngRaw::ReadJxlPreview(QFile &file, QImage &image)
{
    using namespace TiffWalk;
    if (!DngJxl::Available()) return false;

    Reader r;
    if (!r.init(&file)) return false;
    const QVector<Ifd> ifds = allIfds(r);
    Ifd best;
    quint64 bestPixels = 0;
    for (const Ifd &tags : ifds) {
        if (!tags.contains(Compression) || int(r.scalar(tags[Compression])) != COMP_JXL)
            continue;
        if (!tags.contains(PhotometricInterpretation) ||
            int(r.scalar(tags[PhotometricInterpretation])) != 2) continue;     // RGB
        if (!tags.contains(SamplesPerPixel) || int(r.scalar(tags[SamplesPerPixel])) != 3)
            continue;
        if (!tags.contains(ImageWidth) || !tags.contains(ImageLength)) continue;
        const quint64 px = quint64(r.scalar(tags[ImageWidth])) *
                           r.scalar(tags[ImageLength]);
        if (px > bestPixels) { bestPixels = px; best = tags; }
    }
    if (best.isEmpty()) return false;

    const int W = int(r.scalar(best[ImageWidth]));
    const int H = int(r.scalar(best[ImageLength]));
    QVector<Seg> segs;
    int segW, segH;
    QString err;
    if (!segments(r, best, W, H, segs, segW, segH, err)) return false;

    QImage out(W, H, QImage::Format_RGB888);
    if (out.isNull()) return false;
    out.fill(Qt::black);
    std::vector<float> f;
    for (const Seg &s : segs) {
        if (!file.seek(s.off)) return false;
        const QByteArray buf = file.read(int(s.len));
        int tw = 0, th = 0;
        if (!DngJxl::Decode(reinterpret_cast<const uint8_t *>(buf.constData()),
                            size_t(buf.size()), 3, f, tw, th)) return false;
        const int rows = qMin(th, H - s.gy), cols = qMin(tw, W - s.gx);
        for (int y = 0; y < rows; ++y) {
            const float *src = f.data() + size_t(y) * tw * 3;
            uchar *dst = out.scanLine(s.gy + y) + size_t(s.gx) * 3;
            for (int x = 0; x < cols * 3; ++x)
                dst[x] = uchar(qBound(0.0f, src[x] * 255.0f + 0.5f, 255.0f));
        }
    }
    image = out;
    return true;
}
