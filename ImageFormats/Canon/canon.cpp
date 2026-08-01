#include "canon.h"
#include "Main/global.h"
#include "ImageFormats/Raw/tiffwalk.h"
#include "ImageFormats/Raw/losslessjpeg.h"
#include "ImageFormats/Raw/rawimage.h"
#include "ImageFormats/Raw/cameramatrix.h"

Canon::Canon()
{
    canonMakerHash[0] = "Unknown";
    canonMakerHash[1] = "Various camera settings";
    canonMakerHash[2] = "Focal length";
    canonMakerHash[3] = "Unknown";
    canonMakerHash[4] = "Shot information";
    canonMakerHash[5] = "Panorama";
    canonMakerHash[6] = "Image type";
    canonMakerHash[7] = "Firmware version";
    canonMakerHash[8] = "File number";
    canonMakerHash[9] = "Owner Name";
    canonMakerHash[12] = "Camera serial number";
    canonMakerHash[13] = "Camera info";
    canonMakerHash[15] = "Custom Functions";
    canonMakerHash[16] = "Model ID";
    canonMakerHash[18] = "Picture info";
    canonMakerHash[19] = "Thumbnail image valid area";
    canonMakerHash[21] = "Serial number format";
    canonMakerHash[26] = "Super macro";
    canonMakerHash[38] = "AF info";
    canonMakerHash[131] = "Original decision data offset";
    canonMakerHash[147] = "FileInfo tags";  // ChatGPT says shutter count
    canonMakerHash[149] = "Lens model";
    canonMakerHash[150] = "Internal serial number";
    canonMakerHash[151] = "Dust removal data";
    canonMakerHash[153] = "Custom functions";
    canonMakerHash[160] = "Processing info";
    canonMakerHash[164] = "White balance table";
    canonMakerHash[170] = "Measured color";
    canonMakerHash[180] = "ColorSpace";
    canonMakerHash[181] = "Unknown";
    canonMakerHash[192] = "Unknown";
    canonMakerHash[193] = "Unknown";
    canonMakerHash[208] = "VRD offset";
    canonMakerHash[224] = "Sensor info";
    canonMakerHash[16385] = "Color data";
    canonMakerHash[16386] = "CRWParam?";
    canonMakerHash[16387] = "ColorInfo Tags";
    canonMakerHash[16389] = "(unknown 49kB block, not copied to JPEG images)";
    canonMakerHash[16392] = "PictureStyleUserDef";
    canonMakerHash[16393] = "PictureStylePC";
    canonMakerHash[16400] = "CustomPictureStyleFileName";
    canonMakerHash[16403] = "AFMicroAdj";
    canonMakerHash[16405] = "VignettingCorr ";
    canonMakerHash[16406] = "Canon VignettingCorr2 Tags";
    canonMakerHash[16408] = "Canon LightingOpt Tags";
    canonMakerHash[16409] = "Canon LensInfo Tags";
    canonMakerHash[16416] = "Canon Ambience Tags";
    canonMakerHash[16417] = "Canon MultiExp Tags";
    canonMakerHash[16420] = "Canon FilterInfo Tags";
    canonMakerHash[16421] = "Canon HDRInfo Tags";
    canonMakerHash[16424] = "Canon AFConfig Tags";

    canonFileInfoHash[1] = "File number / Shutter count";
    canonFileInfoHash[3] = "Bracket mode";
    canonFileInfoHash[4] = "Bracket value";
    canonFileInfoHash[5] = "Bracket shot number";
    canonFileInfoHash[6] = "Raw JPG quality";
    canonFileInfoHash[7] = "Raw JPG size";
    canonFileInfoHash[8] = "Long exposure noise reduction";
    canonFileInfoHash[9] = "WB bracket mode";
    canonFileInfoHash[12] = "WB bracket value AB";
    canonFileInfoHash[13] = "WB bracket value GM";
    canonFileInfoHash[14] = "Filter effect";
    canonFileInfoHash[15] = "Toning effect";
    canonFileInfoHash[16] = "Macro magnification";
    canonFileInfoHash[19] = "Live view shooting";
    canonFileInfoHash[20] = "Focus distance upper";
    canonFileInfoHash[21] = "Focus distance lower";
    canonFileInfoHash[25] = "Flash exposure lock";
}

bool Canon::parse(MetadataParameters &p,
                  ImageMetadata &m,
                  IFD *ifd,
                  Exif *exif,
                  Jpeg *jpeg,
                  GPS *gps)
{
    if (G::isLogger) G::log("Canon::parse");
    //p.file.open in Metadata::readMetadata
    quint32 startOffset = 0;

    // first two bytes is the endian order
    quint16 order = u.get16(p.file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // is canon always offset 16 to IFD0 ???
    p.hdr = "IFD0";
    p.offset = 16;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p) + startOffset;

    // IFD0 Offsets

    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue;

    quint32 offsetEXIF = 0;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;

    m.xmpSegmentOffset = ifd->ifdDataHash.value(700).tagValue;
    // xmpNextSegmentOffset used to later calc available room in xmp
    m.xmpSegmentLength = ifd->ifdDataHash.value(700).tagCount/* + m.xmpSegmentOffset*/;
    if (m.xmpSegmentOffset) m.isXmp = true;
    else m.isXmp = false;

    // pull data reqd from IFD0
    m.offsetFull = ifd->ifdDataHash.value(273).tagValue;
    m.lengthFull = ifd->ifdDataHash.value(279).tagValue;
    // default values for thumbnail
    m.offsetThumb = m.offsetFull;
    m.lengthThumb =  m.lengthFull;

    m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue, ifd->ifdDataHash.value(271).tagCount);
    m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount);
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue, ifd->ifdDataHash.value(315).tagCount);
    m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue, ifd->ifdDataHash.value(33432).tagCount);

    if (nextIFDOffset) {
        p.hdr = "IFD1";
        p.offset = nextIFDOffset;
        nextIFDOffset = ifd->readIFD(p);
    }

    // pull data reqd from IFD1
    m.offsetThumb = ifd->ifdDataHash.value(513).tagValue;
    m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;
    p.offset = m.offsetFull;
    jpeg->getWidthHeight(p, m.widthPreview, m.heightPreview);
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    if (nextIFDOffset) {
        p.hdr = "IFD2";
        p.offset = nextIFDOffset;
        nextIFDOffset = ifd->readIFD(p);
    }

    // pull small size jpg from IFD2
//    m.offsetSmall = ifd->ifdDataHash.value(273).tagValue;
//    m.lengthSmall = ifd->ifdDataHash.value(279).tagValue;

    // IFD3 not used at present, but does contain details on embedded jpg
    if (nextIFDOffset && p.report) {
        p.hdr = "IFD3";
        p.offset = nextIFDOffset;
        ifd->readIFD(p);
    }

    // read ExifIFD
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p);

    // ExifIFD Offsets
    quint32 makerOffset = 0;
    if (ifd->ifdDataHash.contains(37500))
        makerOffset = ifd->ifdDataHash.value(37500).tagValue;

    // EXIF: created datetime
    QString createdExif;
    createdExif = u.getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      isBigEnd);
        if (x < 1 ) {
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

    // width
    if (ifd->ifdDataHash.contains(40962)) {
        m.width = ifd->ifdDataHash.value(40962).tagValue;
    } else {
        m.width = 0;
    }
    // height
    m.height = ifd->ifdDataHash.value(40963).tagValue;
    // aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33437).tagValue + startOffset,
                                      isBigEnd);
        m.aperture = "f/" + QString::number(x, 'f', 1);
        m.apertureNum = (qRound(x * 10) / 10.0);
    } else {
        m.aperture = "";
        m.apertureNum = 0;
    }
    //ISO
    if (ifd->ifdDataHash.contains(34855)) {
        quint32 x = ifd->ifdDataHash.value(34855).tagValue;
        m.ISONum = static_cast<int>(x);
        m.ISO = QString::number(m.ISONum);
//        ISO = "ISO " + QString::number(x);
    } else {
        m.ISO = "";
        m.ISONum = 0;
    }
    // Exposure compensation
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
    // focal length
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
    // IFD Exif: lens
    m.lens = u.getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
            ifd->ifdDataHash.value(42036).tagCount);

    // IFD Exif: camera serial number
    m.cameraSN = u.getString(p.file, ifd->ifdDataHash.value(42033).tagValue,
            ifd->ifdDataHash.value(42033).tagCount);

    // IFD Exif: lens serial nember
    m.lensSN = u.getString(p.file, ifd->ifdDataHash.value(42037).tagValue,
            ifd->ifdDataHash.value(42037).tagCount);

    // Exif: read makernoteIFD

    if (makerOffset) {
        quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue;
        p.hdr = "IFD Canon Maker Note";
        p.offset = makerOffset;
        p.hash = &canonMakerHash;
        ifd->readIFD(p);
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
   CanonRaw::UnpackCfa  --  Canon CR2 sensor unpack (lossless JPEG + slices + crop)
   ------------------------------------------------------------------------------------------ */

bool CanonRaw::UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw)
{
    Q_UNUSED(m)
    using namespace TiffWalk;

    Reader r;
    if (!r.init(&file)) { errMsg = "CR2: not a TIFF file."; return false; }

    /* CR2 IFDs are a next-IFD chain (IFD0 preview .. IFD3 raw). The raw IFD is the one carrying
       the Canon slice tag 0xC640 (and a strip). */
    QList<quint32> queue { r.firstIfd() };
    QSet<quint32> seen;
    Ifd ifd0, rawIfd;
    bool haveIfd0 = false, haveRaw = false;

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
        if (tags.contains(0xC640) && tags.contains(273) && tags.contains(279)) {
            rawIfd = tags; haveRaw = true;
        }
    }
    if (!haveRaw) { errMsg = "CR2: no raw IFD (slice tag absent)."; return false; }

    /* Decode the lossless-JPEG raw. */
    if (!file.seek(r.scalar(rawIfd[273]))) { errMsg = "CR2: seek to raw failed."; return false; }
    const QByteArray buf = file.read(int(r.scalar(rawIfd[279])));
    LosslessJpeg::Image im;
    QString lerr;
    if (!LosslessJpeg::Decode(reinterpret_cast<const uint8_t *>(buf.constData()),
                              size_t(buf.size()), im, &lerr)) {
        errMsg = "CR2: lossless JPEG decode failed (" + lerr + ")."; return false;
    }
    const int clrs = im.components;
    const int jwide = im.width * clrs;
    const int H = im.height;

    /* Canon vertical slices (tag 0xC640 = [nFullSlices, sliceWidth, lastSliceWidth]). Map the
       component-interleaved JPEG raster back to sensor (row,col). dcraw's reassembly. */
    const QVector<quint32> sl = r.u32s(rawIfd[0xC640]);
    const int s0 = sl.size() > 0 ? int(sl[0]) : 0;
    const int s1 = sl.size() > 1 ? int(sl[1]) : 0;
    const int s2 = sl.size() > 2 ? int(sl[2]) : 0;
    const int W = s0 ? s0 * s1 + s2 : jwide;
    if (W <= 0 || H <= 0) { errMsg = "CR2: bad raw dimensions."; return false; }

    std::vector<uint16_t> full(size_t(W) * H, 0);
    const size_t total = size_t(jwide) * H;
    const size_t slcw = size_t(s1) * H;
    for (size_t jidx = 0; jidx < total && jidx < im.samples.size(); ++jidx) {
        int row, col;
        if (s0 && slcw) {
            size_t i = jidx / slcw;
            const int j = (i >= size_t(s0)) ? 1 : 0;
            if (j) i = size_t(s0);
            const size_t k = jidx - i * slcw;
            const int wsl = j ? s2 : s1;
            row = int(k / wsl);
            col = int(k % wsl) + int(i) * s1;
        } else {
            row = int(jidx / jwide);
            col = int(jidx % jwide);
        }
        if (row < H && col < W) full[size_t(row) * W + col] = im.samples[jidx];
    }

    /* Canon makernote: IFD0 -> ExifIFD (0x8769) -> MakerNote (0x927C). Read SensorInfo (0xE0)
       for the active-area crop and ColorData (0x4001) for the as-shot white balance. */
    int left = 0, top = 0, right = W - 1, bottom = H - 1;
    QVector<quint32> wb;
    if (haveIfd0 && ifd0.contains(0x8769)) {
        Ifd exif; QList<quint32> es; quint32 en = 0;
        if (r.readIfd(r.ifdPointer(ifd0[0x8769]), exif, es, en) && exif.contains(0x927C)) {
            Ifd mn; QList<quint32> ms; quint32 mnn = 0;
            if (r.readIfd(r.ifdPointer(exif[0x927C]), mn, ms, mnn)) {
                if (mn.contains(0xE0)) {                    // SensorInfo
                    const QVector<quint32> si = r.u32s(mn[0xE0]);
                    if (si.size() >= 9) { left = int(si[5]); top = int(si[6]);
                                          right = int(si[7]); bottom = int(si[8]); }
                }
                if (mn.contains(0x4001)) {                  // ColorData -> WB_RGGBLevelsAsShot
                    const QVector<quint32> cd = r.u32s(mn[0x4001]);
                    const int n = cd.size();
                    const int o = n == 582 ? 25 : n == 653 ? 24 : n == 5120 ? 142 : 63;
                    if (o + 3 < n) wb = { cd[o], cd[o + 1], cd[o + 2], cd[o + 3] };
                }
            }
        }
    }

    /* Black level from the masked optical-black columns (x < left) of the full sensor, before
       cropping them away. Self-calibrating, so no per-model black table is needed. */
    uint16_t black = 0;
    if (left > 2) {
        double sum = 0; size_t cnt = 0;
        const int mb = left - 2;                            // skip the transition columns
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < mb; ++x) { sum += full[size_t(y) * W + x]; ++cnt; }
        if (cnt) black = uint16_t(sum / double(cnt) + 0.5);
    }

    /* Crop to the active area. */
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right >= W) right = W - 1;
    if (bottom >= H) bottom = H - 1;
    int cw = right - left + 1, ch = bottom - top + 1;
    if (cw <= 0 || ch <= 0) { left = top = 0; cw = W; ch = H; }
    /* Keep the crop origin even so the RGGB phase is preserved. */
    if (left & 1) { ++left; --cw; }
    if (top & 1)  { ++top;  --ch; }

    raw.width = cw;
    raw.height = ch;
    raw.cfa.assign(size_t(cw) * ch, 0);
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x)
            raw.cfa[size_t(y) * cw + x] = full[size_t(top + y) * W + (left + x)];

    raw.pattern = CfaPattern::RGGB;                         // Canon EOS sensors are RGGB
    raw.white = uint16_t((1u << im.precision) - 1);
    for (int i = 0; i < 4; ++i) raw.black[i] = black;

    if (wb.size() == 4 && wb[0] > 0 && wb[1] > 0 && wb[3] > 0) {
        raw.camMul[0] = wb[0]; raw.camMul[1] = wb[1];       // R, G1
        raw.camMul[2] = wb[3]; raw.camMul[3] = wb[2];       // B, G2
    }

    QString model = (haveIfd0 && ifd0.contains(272)) ? r.ascii(ifd0[272]) : QString();
    xyzToCamForModel(model, raw.xyzToCam);                  // identity fallback if unknown

    /* Canon sensors saturate BELOW the bit-depth maximum (e.g. the 7D Mark II clips at 13584,
       not 16383), so the bit-depth white above is too high and washes the highlights. Use the
       per-model saturation from the adobe_coeff levels table when it has one. */
    {
        int tblack = 0, tmax = 0;
        if (cameraLevelsForModel(model, tblack, tmax) && tmax > 0)
            raw.white = uint16_t(tmax);
    }

    return true;
}
