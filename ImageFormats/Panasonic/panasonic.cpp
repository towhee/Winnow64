#include "panasonic.h"
#include "Main/global.h"
#include "ImageFormats/Raw/tiffwalk.h"
#include "ImageFormats/Raw/rawimage.h"
#include "ImageFormats/Raw/cameramatrix.h"
#include <vector>

Panasonic::Panasonic()
{
    panasonicMakerHash[1] = "ImageQuality";
    panasonicMakerHash[2] = "FirmwareVersion";
    panasonicMakerHash[3] = "WhiteBalance";
    panasonicMakerHash[7] = "FocusMode";
    panasonicMakerHash[15] = "AFAreaMode";
    panasonicMakerHash[26] = "ImageStabilization";
    panasonicMakerHash[28] = "MacroMode";
    panasonicMakerHash[31] = "ShootingMode";
    panasonicMakerHash[32] = "Audio";
    panasonicMakerHash[33] = "DataDump";
    panasonicMakerHash[35] = "WhiteBalanceBias";
    panasonicMakerHash[36] = "FlashBias";
    panasonicMakerHash[37] = "InternalSerialNumber";
    panasonicMakerHash[38] = "PanasonicExifVersion";
    panasonicMakerHash[40] = "ColorEffect";
    panasonicMakerHash[41] = "TimeSincePowerOn";
    panasonicMakerHash[42] = "BurstMode";
    panasonicMakerHash[43] = "SequenceNumber";
    panasonicMakerHash[44] = "ContrastMode";
    panasonicMakerHash[45] = "NoiseReduction";
    panasonicMakerHash[46] = "SelfTimer";
    panasonicMakerHash[48] = "Rotation";
    panasonicMakerHash[49] = "AFAssistLamp";
    panasonicMakerHash[50] = "ColorMode";
    panasonicMakerHash[51] = "BabyAge";
    panasonicMakerHash[52] = "OpticalZoomMode";
    panasonicMakerHash[53] = "ConversionLens";
    panasonicMakerHash[54] = "TravelDay";
    panasonicMakerHash[57] = "Contrast";
    panasonicMakerHash[58] = "WorldTimeLocation";
    panasonicMakerHash[59] = "TextStamp";
    panasonicMakerHash[60] = "ProgramISO";
    panasonicMakerHash[61] = "AdvancedSceneType";
    panasonicMakerHash[62] = "TextStamp";
    panasonicMakerHash[63] = "FacesDetected";
    panasonicMakerHash[64] = "Saturation";
    panasonicMakerHash[65] = "Sharpness";
    panasonicMakerHash[66] = "FilmMode";
    panasonicMakerHash[67] = "JPEGQuality";
    panasonicMakerHash[68] = "ColorTempKelvin";
    panasonicMakerHash[69] = "BracketSettings";
    panasonicMakerHash[70] = "WBShiftAB";
    panasonicMakerHash[71] = "WBShiftGM";
    panasonicMakerHash[72] = "FlashCurtain";
    panasonicMakerHash[73] = "LongExposureNoiseReduction";
    panasonicMakerHash[75] = "PanasonicImageWidth";
    panasonicMakerHash[76] = "PanasonicImageHeight";
    panasonicMakerHash[77] = "AFPointPosition";
    panasonicMakerHash[78] = "FaceDetInfo";
    panasonicMakerHash[81] = "LensType";
    panasonicMakerHash[82] = "LensSerialNumber";
    panasonicMakerHash[83] = "AccessoryType";
    panasonicMakerHash[84] = "AccessorySerialNumber";
    panasonicMakerHash[89] = "Transform";
    panasonicMakerHash[93] = "IntelligentExposure";
    panasonicMakerHash[96] = "LensFirmwareVersion";
    panasonicMakerHash[97] = "FaceRecInfo";
    panasonicMakerHash[98] = "FlashWarning";
    panasonicMakerHash[99] = "RecognizedFaceFlags?";
    panasonicMakerHash[101] = "Title";
    panasonicMakerHash[102] = "BabyName";
    panasonicMakerHash[103] = "Location";
    panasonicMakerHash[105] = "Country";
    panasonicMakerHash[107] = "State";
    panasonicMakerHash[109] = "City";
    panasonicMakerHash[111] = "Landmark";
    panasonicMakerHash[112] = "IntelligentResolution";
    panasonicMakerHash[118] = "HDRShot";
    panasonicMakerHash[119] = "BurstSpeed";
    panasonicMakerHash[121] = "IntelligentD-Range";
    panasonicMakerHash[124] = "ClearRetouch";
    panasonicMakerHash[128] = "City2";
    panasonicMakerHash[134] = "ManometerPressure";
    panasonicMakerHash[137] = "PhotoStyle";
    panasonicMakerHash[138] = "ShadingCompensation";
    panasonicMakerHash[139] = "WBShiftIntelligentAuto";
    panasonicMakerHash[140] = "AccelerometerZ";
    panasonicMakerHash[141] = "AccelerometerX";
    panasonicMakerHash[142] = "AccelerometerY";
    panasonicMakerHash[143] = "CameraOrientation";
    panasonicMakerHash[144] = "RollAngle";
    panasonicMakerHash[145] = "PitchAngle";
    panasonicMakerHash[146] = "WBShiftCreativeControl";
    panasonicMakerHash[147] = "SweepPanoramaDirection";
    panasonicMakerHash[148] = "SweepPanoramaFieldOfView";
    panasonicMakerHash[150] = "TimerRecording";
    panasonicMakerHash[157] = "InternalNDFilter";
    panasonicMakerHash[158] = "HDR";
    panasonicMakerHash[159] = "ShutterType";
    panasonicMakerHash[163] = "ClearRetouchValue";
    panasonicMakerHash[167] = "OutputLUT";
    panasonicMakerHash[171] = "TouchAE";
    panasonicMakerHash[173] = "HighlightShadow";
    panasonicMakerHash[175] = "TimeStamp";
    panasonicMakerHash[180] = "MultiExposure";
    panasonicMakerHash[185] = "RedEyeRemoval";
    panasonicMakerHash[187] = "VideoBurstMode";
    panasonicMakerHash[188] = "DiffractionCorrection";
    panasonicMakerHash[3584] = "PrintIM";
    panasonicMakerHash[8195] = "TimeInfo";
    panasonicMakerHash[32768] = "MakerNoteVersion";
    panasonicMakerHash[32769] = "SceneMode";
}

bool Panasonic::parse(MetadataParameters &p,
                      ImageMetadata &m,
                      IFD *ifd,
                      Exif *exif,
                      Jpeg *jpeg)
{
    if (G::isLogger) G::log("Panasonic::parse");
    /* Panasonic RW2 files

      Note TIFF magic word is 0x55 instead of 0x2a.

    Offset          Bytes   Description
    0000 - 0004     4      Endian
    0005 - 0008     4      Offset to IFD0   // 18 from start of file

    Panasonic starts with an single unchained IFD, which includes offsets for an Exif IFD
    and an XMP segment.  It also has an offset to the embedded Jpg.  The Jpg has its own
    IFD0 and IFD1.  The IFD0 has an offset to another Exit IFD which, in turn, has an offset
    to the maker notes IFD.
    */

    //file.open in Metadata::readMetadata

    // first two bytes is the endian order (skip next 2 bytes)
    quint16 order = u.get16(p.file.read(4));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // get offset to first IFD and read it
    quint32 offsetIfd0 = u.get32(p.file.read(4), isBigEnd);

    // Panasonic does not chain IFDs
    p.hdr = "Panasonic IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    ifd->readIFD(p);

    // pull data reqd from main file IFD0
    m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue, ifd->ifdDataHash.value(271).tagCount).trimmed();
    m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount).trimmed();
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue, ifd->ifdDataHash.value(315).tagCount);
    m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue, ifd->ifdDataHash.value(33432).tagCount);
    m.offsetFull = ifd->ifdDataHash.value(46).tagValue;
    m.lengthFull = ifd->ifdDataHash.value(46).tagCount;
    // default values for thumbnail
    m.offsetThumb = m.offsetFull;
    m.lengthThumb =  m.lengthFull;
    p.offset = m.offsetFull;
    jpeg->getWidthHeight(p, m.widthPreview, m.heightPreview);
    m.xmpSegmentOffset = ifd->ifdDataHash.value(700).tagValue;
    m.xmpSegmentLength = ifd->ifdDataHash.value(700).tagCount /*+ m.xmpSegmentOffset*/;
    m.height = static_cast<int>(ifd->ifdDataHash.value(49).tagValue);
    m.width = static_cast<int>(ifd->ifdDataHash.value(50).tagValue);
    if (ifd->ifdDataHash.contains(23)) {
        quint32 x = ifd->ifdDataHash.value(23).tagValue;
        m.ISONum = static_cast<int>(x);
        m.ISO = QString::number(m.ISONum);
    } else {
        m.ISO = "";
        m.ISONum = 0;
    }

    // get the offset for ExifIFD and read it
    quint32 offsetEXIF;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p);

    // EXIF: created datetime
    QString createdExif;
    createdExif = u.getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue,
                                      isBigEnd);
        if (x <1 ) {
            int t = qRound(1/x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = x;
        } else {
            uint t = static_cast<uint>(x);
            m.exposureTime = QString::number(t);
            m.exposureTimeNum = t;
        }
        m.exposureTime += " sec";
    } else {
        m.exposureTime = "";
    }
    // aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33437).tagValue,
                                      isBigEnd);
        m.aperture = "f/" + QString::number(x, 'f', 1);
        m.apertureNum = (qRound(x * 10) / 10.0);
    } else {
        m.aperture = "";
        m.apertureNum = 0;
    }
    // exposure compensation
    if (ifd->ifdDataHash.contains(37380)) {
        // tagType = 10 signed rational
        double x = u.getReal_s(p.file,
                                      ifd->ifdDataHash.value(37380).tagValue,
                                      isBigEnd);
        m.exposureCompensation = QString::number(x, 'f', 1) + " EV";
        m.exposureCompensationNum = x;
    } else {
        m.exposureCompensation = "";
        m.exposureCompensationNum = 0;
    }
    // focal length
    if (ifd->ifdDataHash.contains(37386)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(37386).tagValue,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // check embedded JPG for more metadata (IFD0, IFD1, Exit IFD and Maker notes IFD)
    order = 0x4D4D;
    isBigEnd = true;
    quint32 startOffset = m.offsetFull;
    p.file.seek(m.offsetFull);

    if (u.get16(p.file.read(2), isBigEnd) != 0xFFD8) return 0;

    // build a hash of jpg segment offsets
    p.offset = static_cast<quint32>(p.file.pos());
    jpeg->getJpgSegments(p, m);

    // read the embedded JPG EXIF data
    if (jpeg->segmentHash.contains("EXIF")) {
        quint32 startOffset;
        p.file.seek(jpeg->segmentHash["EXIF"]);
        bool foundEndian = false;
        int counter = 0;
        while (!foundEndian) {
            quint32 a = u.get16(p.file.read(2), isBigEnd);
            if (a == 0x4949 || a == 0x4D4D) {
                a == 0x4D4D ? isBigEnd = true : isBigEnd = false;
                // offsets are from the endian position in JPEGs
                // therefore must adjust all offsets found in tagValue
                startOffset = static_cast<quint32>(p.file.pos()) - 2;
                foundEndian = true;
            }
            // break out if not finding endian
            if(++counter > 30) break;
        }

        if (p.report) p.rpt << "\n startOffset = " << startOffset;

        quint32 a = u.get16(p.file.read(2));  // skip magic 42
        a = u.get32(p.file.read(4), isBigEnd);
        quint32 offsetIfd0 = a + startOffset;

        // read JPG IFD0
        p.hdr = "IFD0";
        p.offset = offsetIfd0;
        p.hash = &exif->hash;
        quint32 nextIFDOffset = ifd->readIFD(p) + startOffset;
        offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;

        // read JPG IFD1
        if (nextIFDOffset) {
            p.hdr = "IFD1";
            p.offset = nextIFDOffset;
            ifd->readIFD(p);
        }

        m.offsetThumb = ifd->ifdDataHash.value(513).tagValue + startOffset;
        m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;

        // read JPG Exif IFD
        p.hdr = "IFD Exif";
        p.offset = offsetEXIF;
        ifd->readIFD(p);

        // maker note
        if (ifd->ifdDataHash.contains(37500)) {
            // get offset from the IFD EXIF in embedded JPG
//            quint32 makerOffset = 5948;
            quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue + startOffset + 12;
            p.hdr = "IFD Panasonic Maker Note";
            p.offset = makerOffset;
            p.hash = &panasonicMakerHash;
            ifd->readIFD(p);
            // get lens
            m.lens = u.getString(p.file, ifd->ifdDataHash.value(81).tagValue + startOffset, ifd->ifdDataHash.value(81).tagCount);
            // get lens serial number
            m.lensSN = u.getString(p.file, ifd->ifdDataHash.value(82).tagValue + startOffset, ifd->ifdDataHash.value(82).tagCount);
            // get camera serial number
            m.cameraSN = u.getString(p.file, ifd->ifdDataHash.value(37).tagValue + startOffset, ifd->ifdDataHash.value(37).tagCount);
        }
    }

    m.isXmp = true;       // Panasonic has an xmp segment

    // jpg preview metadata report information
    if (p.report) {
        p.offset = m.offsetFull;
        Jpeg jpeg;
        IPTC iptc;
        GPS gps;
        jpeg.parse(p, m, ifd, &iptc, exif, &gps);
    }

    // read XMP
    bool okToReadXmp = true;
    if (!G::stop && m.isXmp && okToReadXmp && m.xmpSegmentOffset > 0 && m.xmpSegmentLength > 0) {
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
   PanasonicRaw::UnpackCfa  --  Panasonic RW2 (RawFormat 4) decode
   Ported from dcraw's panasonic_load_raw / pana_bits; validated byte-identical to libraw (GX9).
   ------------------------------------------------------------------------------------------ */

bool PanasonicRaw::UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw)
{
    Q_UNUSED(m)
    using namespace TiffWalk;

    Reader r;
    if (!r.init(&file)) { errMsg = "RW2: not a TIFF/RW2 file."; return false; }
    Ifd t; QList<quint32> subs; quint32 next = 0;
    if (!r.readIfd(r.firstIfd(), t, subs, next)) { errMsg = "RW2: bad IFD0."; return false; }

    if (!t.contains(0x02) || !t.contains(0x03) || !t.contains(0x118)) {
        errMsg = "RW2: missing raw tags."; return false;
    }
    const int W = int(r.scalar(t[0x02]));               // SensorWidth (raw width incl. margin)
    const int H = int(r.scalar(t[0x03]));               // SensorHeight
    const int rawFormat = t.contains(0x2D) ? int(r.scalar(t[0x2D])) : 0;
    const quint32 strip = r.scalar(t[0x118]);           // RawDataOffset
    if (W <= 0 || H <= 0) { errMsg = "RW2: bad dimensions."; return false; }
    if (rawFormat != 4) { errMsg = "RW2: RawFormat != 4 (newer compression unsupported)."; return false; }
    const int loadFlags = 0x2008;                       // RawFormat 4

    if (!file.seek(strip)) { errMsg = "RW2: seek to raw data failed."; return false; }
    const QByteArray data = file.readAll();
    const uchar *dp = reinterpret_cast<const uchar *>(data.constData());
    const qint64 dn = data.size();

    /* Panasonic "pana_bits": bits come from a reversed 0x4000-byte buffer, refilled (rotated by
       loadFlags) each time it empties. */
    std::vector<uchar> buf(0x4000, 0);
    qint64 fp = 0; int vbits = 0;
    auto pana = [&](int nbits) -> int {
        if (nbits == 0) { vbits = 0; return 0; }
        if (vbits == 0) {
            for (int i = 0; i < 0x4000 - loadFlags; ++i)
                buf[loadFlags + i] = (fp + i < dn) ? dp[fp + i] : 0;
            for (int i = 0; i < loadFlags; ++i) {
                const qint64 src = fp + 0x4000 - loadFlags + i;
                buf[i] = (src < dn) ? dp[src] : 0;
            }
            fp += 0x4000;
        }
        vbits = (vbits - nbits) & 0x1ffff;
        const int byte = (vbits >> 3) ^ 0x3ff0;
        const int b = buf[byte] | (buf[byte + 1] << 8);
        return (b >> (vbits & 7)) & ((1 << nbits) - 1);
    };

    raw.width = W;
    raw.height = H;
    raw.cfa.assign(size_t(W) * size_t(H), 0);
    for (int row = 0; row < H; ++row) {
        int pred[2] = {0, 0}, nonz[2] = {0, 0}, sh = 0;
        for (int col = 0; col < W; ++col) {
            const int i = col % 14;
            if (i == 0) { pred[0] = pred[1] = 0; nonz[0] = nonz[1] = 0; }
            if (i % 3 == 2) sh = 4 >> (3 - pana(2));
            if (nonz[i & 1]) {
                const int j = pana(8);
                if (j) {
                    pred[i & 1] -= 0x80 << sh;
                    if (pred[i & 1] < 0 || sh == 4) pred[i & 1] &= (1 << sh) - 1;
                    pred[i & 1] += j << sh;
                }
            } else if ((nonz[i & 1] = pana(8)) || i > 11) {
                pred[i & 1] = (nonz[i & 1] << 4) | pana(4);
            }
            raw.cfa[size_t(row) * W + col] = uint16_t(pred[i & 1] & 0xffff);
        }
    }

    /* Pattern (tag 0x09: 1=RGGB 2=GRBG 3=GBRG 4=BGGR), levels, WB, matrix. */
    switch (t.contains(0x09) ? int(r.scalar(t[0x09])) : 4) {
    case 1:  raw.pattern = CfaPattern::RGGB; break;
    case 2:  raw.pattern = CfaPattern::GRBG; break;
    case 3:  raw.pattern = CfaPattern::GBRG; break;
    default: raw.pattern = CfaPattern::BGGR; break;
    }
    const int bps = t.contains(0x0A) ? int(r.scalar(t[0x0A])) : 12;
    raw.white = uint16_t((1u << bps) - 1);
    /* Panasonic stores BlackLevelRed/Green/Blue (0x1C..0x1E) 15 below the true black. */
    const int blk = (t.contains(0x1C) ? int(r.scalar(t[0x1C])) : 128) + 15;
    for (int i = 0; i < 4; ++i) raw.black[i] = uint16_t(blk);
    if (t.contains(0x24) && t.contains(0x25) && t.contains(0x26)) {
        raw.camMul[0] = r.scalar(t[0x24]);              // R
        raw.camMul[1] = r.scalar(t[0x25]);              // G (== 256)
        raw.camMul[2] = r.scalar(t[0x26]);              // B
        raw.camMul[3] = r.scalar(t[0x25]);
    }
    QString model;
    if (t.contains(272)) model = "Panasonic " + r.ascii(t[272]);
    xyzToCamForModel(model, raw.xyzToCam);

    return true;
}
