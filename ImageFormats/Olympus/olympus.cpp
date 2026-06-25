#include "olympus.h"
#include "Main/global.h"
#include "ImageFormats/Raw/tiffwalk.h"
#include "ImageFormats/Raw/rawimage.h"
#include "ImageFormats/Raw/cameramatrix.h"
#include <vector>

/*
    https://exiftool.org/TagNames/Olympus.html
    https://exiftool.org/TagNames/Olympus.html#Equipment
    http://www.ozhiker.com/electronics/pjmt/jpeg_info/olympus_mn.html
    https://exiv2.org/tags-olympus.html
*/

Olympus::Olympus()
{
    olympusMakerHash[0] = "MakerNoteVersion";
    olympusMakerHash[1] = "MinoltaCameraSettingsOld";
    olympusMakerHash[3] = "MinoltaCameraSettings";
    olympusMakerHash[64] = "CompressedImageSize";
    olympusMakerHash[129] = "PreviewImageData";
    olympusMakerHash[136] = "PreviewImageStart";
    olympusMakerHash[137] = "PreviewImageLength";
    olympusMakerHash[256] = "ThumbnailImage";
    olympusMakerHash[260] = "BodyFirmwareVersion";
    olympusMakerHash[512] = "SpecialMode";
    olympusMakerHash[513] = "QualitySetting";
    olympusMakerHash[514] = "MacroMode";
    olympusMakerHash[515] = "BWMode";
    olympusMakerHash[516] = "DigitalZoomRatio";
    olympusMakerHash[517] = "FocalPlaneDiagonal";
    olympusMakerHash[518] = "LensDistortionParams";
    olympusMakerHash[519] = "CameraType";
    olympusMakerHash[520] = "TextInfo";
    olympusMakerHash[521] = "CameraID";
    olympusMakerHash[523] = "EpsonImageWidth";
    olympusMakerHash[524] = "EpsonImageHeight";
    olympusMakerHash[525] = "EpsonSoftware";
    olympusMakerHash[640] = "PreviewImage";
    olympusMakerHash[768] = "PreCaptureFrames";
    olympusMakerHash[769] = "WhiteBoard";
    olympusMakerHash[770] = "OneTouchWB";
    olympusMakerHash[771] = "WhiteBalanceBracket";
    olympusMakerHash[772] = "WhiteBalanceBias";
    olympusMakerHash[1024] = "SensorArea";
    olympusMakerHash[1025] = "BlackLevel";
    olympusMakerHash[1027] = "SceneMode";
    olympusMakerHash[1028] = "SerialNumber";
    olympusMakerHash[1029] = "Firmware";
    olympusMakerHash[3584] = "PrintIM";
    olympusMakerHash[3840] = "DataDump";
    olympusMakerHash[3841] = "DataDump2";
    olympusMakerHash[3844] = "ZoomedPreviewStart";
    olympusMakerHash[3845] = "ZoomedPreviewLength";
    olympusMakerHash[3846] = "ZoomedPreviewSize";
    olympusMakerHash[4096] = "ShutterSpeedValue";
    olympusMakerHash[4097] = "ISOValue";
    olympusMakerHash[4098] = "ApertureValue";
    olympusMakerHash[4099] = "BrightnessValue";
    olympusMakerHash[4100] = "FlashMode";
    olympusMakerHash[4101] = "FlashDevice";
    olympusMakerHash[4102] = "ExposureCompensation";
    olympusMakerHash[4103] = "SensorTemperature";
    olympusMakerHash[4104] = "LensTemperature";
    olympusMakerHash[4105] = "LightCondition";
    olympusMakerHash[4106] = "FocusRange";
    olympusMakerHash[4107] = "FocusMode";
    olympusMakerHash[4108] = "ManualFocusDistance";
    olympusMakerHash[4109] = "ZoomStepCount";
    olympusMakerHash[4110] = "FocusStepCount";
    olympusMakerHash[4111] = "Sharpness";
    olympusMakerHash[4112] = "FlashChargeLevel";
    olympusMakerHash[4113] = "ColorMatrix";
    olympusMakerHash[4114] = "BlackLevel";
    olympusMakerHash[4115] = "ColorTemperatureBG?";
    olympusMakerHash[4116] = "ColorTemperatureRG?";
    olympusMakerHash[4117] = "WBMode";
    olympusMakerHash[4119] = "RedBalance";
    olympusMakerHash[4120] = "BlueBalance";
    olympusMakerHash[4121] = "ColorMatrixNumber";
    olympusMakerHash[4122] = "SerialNumber";
    olympusMakerHash[4123] = "ExternalFlashAE1_0?";
    olympusMakerHash[4124] = "ExternalFlashAE2_0?";
    olympusMakerHash[4125] = "InternalFlashAE1_0?";
    olympusMakerHash[4126] = "InternalFlashAE2_0?";
    olympusMakerHash[4127] = "ExternalFlashAE1?";
    olympusMakerHash[4128] = "ExternalFlashAE2?";
    olympusMakerHash[4129] = "InternalFlashAE1?";
    olympusMakerHash[4130] = "InternalFlashAE2?";
    olympusMakerHash[4131] = "FlashExposureComp";
    olympusMakerHash[4132] = "InternalFlashTable";
    olympusMakerHash[4133] = "ExternalFlashGValue";
    olympusMakerHash[4134] = "ExternalFlashBounce";
    olympusMakerHash[4135] = "ExternalFlashZoom";
    olympusMakerHash[4136] = "ExternalFlashMode";
    olympusMakerHash[4137] = "Contrast";
    olympusMakerHash[4138] = "SharpnessFactor";
    olympusMakerHash[4139] = "ColorControl";
    olympusMakerHash[4140] = "ValidBits";
    olympusMakerHash[4141] = "CoringFilter";
    olympusMakerHash[4142] = "OlympusImageWidth";
    olympusMakerHash[4143] = "OlympusImageHeight";
    olympusMakerHash[4144] = "SceneDetect";
    olympusMakerHash[4145] = "SceneArea?";
    olympusMakerHash[4147] = "SceneDetectData?";
    olympusMakerHash[4148] = "CompressionRatio";
    olympusMakerHash[4149] = "PreviewImageValid";
    olympusMakerHash[4150] = "PreviewImageStart";
    olympusMakerHash[4151] = "PreviewImageLength";
    olympusMakerHash[4152] = "AFResult";
    olympusMakerHash[4153] = "CCDScanMode";
    olympusMakerHash[4154] = "NoiseReduction";
    olympusMakerHash[4155] = "FocusStepInfinity";
    olympusMakerHash[4156] = "FocusStepNear";
    olympusMakerHash[4157] = "LightValueCenter";
    olympusMakerHash[4158] = "LightValuePeriphery";
    olympusMakerHash[4159] = "FieldCount?";
    olympusMakerHash[8208] = "Equipment IFDOffset";
    olympusMakerHash[8224] = "CameraSettings";
    olympusMakerHash[8240] = "RawDevelopment";
    olympusMakerHash[8241] = "RawDev2";
    olympusMakerHash[8256] = "ImageProcessing";
    olympusMakerHash[8272] = "FocusInfo";
    olympusMakerHash[8448] = "Olympus2100";
    olympusMakerHash[8704] = "Olympus2200";
    olympusMakerHash[8960] = "Olympus2300";
    olympusMakerHash[9216] = "Olympus2400";
    olympusMakerHash[9472] = "Olympus2500";
    olympusMakerHash[9728] = "Olympus2600";
    olympusMakerHash[9984] = "Olympus2700";
    olympusMakerHash[10240] = "Olympus2800";
    olympusMakerHash[10496] = "Olympus2900";
    olympusMakerHash[12288] = "RawInfo";
    olympusMakerHash[16384] = "MainInfo";
    olympusMakerHash[20480] = "UnknownInfo";

    olympusCameraSettingsHash[0] = "CameraSettingsVersion";
    olympusCameraSettingsHash[256] = "PreviewImageValid";
    olympusCameraSettingsHash[257] = "PreviewImageStart";
    olympusCameraSettingsHash[258] = "PreviewImageLength";
    olympusCameraSettingsHash[512] = "ExposureMode";
    olympusCameraSettingsHash[513] = "AELock";
    olympusCameraSettingsHash[514] = "MeteringMode";
    olympusCameraSettingsHash[515] = "ExposureShift";
    olympusCameraSettingsHash[516] = "NDFilter";
    olympusCameraSettingsHash[768] = "MacroMode";
    olympusCameraSettingsHash[769] = "FocusMode";
    olympusCameraSettingsHash[770] = "FocusProcess";
    olympusCameraSettingsHash[771] = "AFSearch";
    olympusCameraSettingsHash[772] = "AFAreas";
    olympusCameraSettingsHash[773] = "AFPointSelected";
    olympusCameraSettingsHash[774] = "AFFineTune";
    olympusCameraSettingsHash[775] = "AFFineTuneAdj";
    olympusCameraSettingsHash[1024] = "FlashMode";
    olympusCameraSettingsHash[1025] = "FlashExposureComp";
    olympusCameraSettingsHash[1027] = "FlashRemoteControl";
    olympusCameraSettingsHash[1028] = "FlashControlMode";
    olympusCameraSettingsHash[1029] = "FlashIntensity";
    olympusCameraSettingsHash[1030] = "ManualFlashStrength";
    olympusCameraSettingsHash[1280] = "WhiteBalance2";
    olympusCameraSettingsHash[1281] = "WhiteBalanceTemperature";
    olympusCameraSettingsHash[1282] = "WhiteBalanceBracket";
    olympusCameraSettingsHash[1283] = "CustomSaturation";
    olympusCameraSettingsHash[1284] = "ModifiedSaturation";
    olympusCameraSettingsHash[1285] = "ContrastSetting";
    olympusCameraSettingsHash[1286] = "SharpnessSetting";
    olympusCameraSettingsHash[1287] = "ColorSpace";
    olympusCameraSettingsHash[1289] = "SceneMode";
    olympusCameraSettingsHash[1290] = "NoiseReduction";
    olympusCameraSettingsHash[1291] = "DistortionCorrection";
    olympusCameraSettingsHash[1292] = "ShadingCompensation";
    olympusCameraSettingsHash[1293] = "CompressionFactor";
    olympusCameraSettingsHash[1295] = "Gradation";
    olympusCameraSettingsHash[1312] = "PictureMode";
    olympusCameraSettingsHash[1313] = "PictureModeSaturation";
    olympusCameraSettingsHash[1314] = "PictureModeHue?";
    olympusCameraSettingsHash[1315] = "PictureModeContrast";
    olympusCameraSettingsHash[1316] = "PictureModeSharpness";
    olympusCameraSettingsHash[1317] = "PictureModeBWFilter";
    olympusCameraSettingsHash[1318] = "PictureModeTone";
    olympusCameraSettingsHash[1319] = "NoiseFilter";
    olympusCameraSettingsHash[1321] = "ArtFilter";
    olympusCameraSettingsHash[1324] = "MagicFilter";
    olympusCameraSettingsHash[1325] = "PictureModeEffect";
    olympusCameraSettingsHash[1326] = "ToneLevel";
    olympusCameraSettingsHash[1327] = "ArtFilterEffect";
    olympusCameraSettingsHash[1330] = "ColorCreatorEffect";
    olympusCameraSettingsHash[1335] = "MonochromeProfileSettings";
    olympusCameraSettingsHash[1336] = "FilmGrainEffect";
    olympusCameraSettingsHash[1337] = "ColorProfileSettings";
    olympusCameraSettingsHash[1338] = "MonochromeVignetting";
    olympusCameraSettingsHash[1339] = "MonochromeColor";
    olympusCameraSettingsHash[1536] = "DriveMode";
    olympusCameraSettingsHash[1537] = "PanoramaMode";
    olympusCameraSettingsHash[1539] = "ImageQuality2";
    olympusCameraSettingsHash[1540] = "ImageStabilization";
    olympusCameraSettingsHash[2052] = "StackedImage";
    olympusCameraSettingsHash[2304] = "ManometerPressure";
    olympusCameraSettingsHash[2305] = "ManometerReading";
    olympusCameraSettingsHash[2306] = "ExtendedWBDetect";
    olympusCameraSettingsHash[2307] = "RollAngle";
    olympusCameraSettingsHash[2308] = "PitchAngle";
    olympusCameraSettingsHash[2312] = "DateTimeUTC";

    olympusEquipmentHash[0] = "EquipmentVersion";
    olympusEquipmentHash[256] = "CameraType";
    olympusEquipmentHash[257] = "SerialNumber";
    olympusEquipmentHash[258] = "InternalSerialNumber";
    olympusEquipmentHash[259] = "FocalPlaneDiagonal";
    olympusEquipmentHash[260] = "BodyFirmwareVersion";
    olympusEquipmentHash[513] = "LensType";
    olympusEquipmentHash[514] = "LensSerialNumber";
    olympusEquipmentHash[515] = "LensModel";
    olympusEquipmentHash[516] = "LensFirmwareVersion";
    olympusEquipmentHash[517] = "MaxApertureAtMinFocal";
    olympusEquipmentHash[518] = "MaxApertureAtMaxFocal";
    olympusEquipmentHash[519] = "MinFocalLength";
    olympusEquipmentHash[520] = "MaxFocalLength";
    olympusEquipmentHash[522] = "MaxApertureAtCurrentFocal";
    olympusEquipmentHash[523] = "LensProperties";
    olympusEquipmentHash[769] = "Extender";
    olympusEquipmentHash[770] = "ExtenderSerialNumber";
    olympusEquipmentHash[771] = "ExtenderModel";
    olympusEquipmentHash[772] = "ExtenderFirmwareVersion";
    olympusEquipmentHash[1027] = "ConversionLens";
    olympusEquipmentHash[4096] = "FlashType";
    olympusEquipmentHash[4097] = "FlashModel";
    olympusEquipmentHash[4098] = "FlashFirmwareVersion";
    olympusEquipmentHash[4099] = "FlashSerialNumber";
}

bool Olympus::parse(MetadataParameters &p,
                    ImageMetadata &m,
                    IFD *ifd,
                    Exif *exif,
                    Jpeg *jpeg,
                    GPS *gps)
{
    if (G::isLogger) G::log("Olympus::parse");
    //file.open in Metadata::readMetadata
    // first two bytes is the endian order (skip next 2 bytes)
    quint16 order = u.get16(p.file.read(4));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // get offset to first IFD and read it
    quint32 offsetIfd0 = u.get32(p.file.read(4), isBigEnd);

    // Olympus does not chain IFDs
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    ifd->readIFD(p);

    // pull data reqd from IFD0
    m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue, ifd->ifdDataHash.value(271).tagCount).trimmed();
    m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount).trimmed();
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue, ifd->ifdDataHash.value(315).tagCount);
    m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue, ifd->ifdDataHash.value(33432).tagCount);
    m.width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);

    // get the offset for GPSIFD
    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue;

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

    // get shutter speed
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
    // EXIF: Exposure compensation
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
    // lens
    m.lens = u.getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
                ifd->ifdDataHash.value(42036).tagCount);

    // maker note offset while still have EXIF IFD
    bool isMakerNotes = false;
    quint32 makerOffset = 0;
    if (ifd->ifdDataHash.contains(37500)) {
        isMakerNotes = true;
        makerOffset = ifd->ifdDataHash.value(37500).tagValue;
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

    // read makernoteIFD
    /* the makernotes IFD is preceeded with either
       "OLYMPUS " + 49 49  03 00
       "OM SYSTEM   " + 49 49  04 00
     */
    p.file.seek(makerOffset);
    bool foundMakerNotes = false;
    if (isMakerNotes) {
        int off;
        for (off = 0; off < 30; off++) {
            quint16 order = u.get16(p.file.read(2));
            off++;
            if (order == 0x4949) {
                foundMakerNotes = true;
                break;
            }
        }
        if (foundMakerNotes) {
            quint32 offset = makerOffset + off + 3;
            p.hdr = "IFD Olympus Maker Note";
            p.offset = offset;
            p.hash = &olympusMakerHash;
            ifd->readIFD(p);

            // IFD Offsets
            quint32 cameraSettingsIFDOffset = 0;
            if (ifd->ifdDataHash.contains(8224))
                cameraSettingsIFDOffset = ifd->ifdDataHash.value(8224).tagValue + makerOffset;

            quint32 equipmentIFDOffset = 0;
            if (ifd->ifdDataHash.contains(8208))
                    equipmentIFDOffset = ifd->ifdDataHash.value(8208).tagValue + makerOffset;

            m.offsetThumb = ifd->ifdDataHash.value(256).tagValue + makerOffset;
            m.lengthThumb = ifd->ifdDataHash.value(256).tagCount;
            // if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

            // read CameraSettingsIFD
            if (cameraSettingsIFDOffset) {
                p.hdr = "IFD Olympus Maker Note: CameraSettingsIFD";
                p.offset = cameraSettingsIFDOffset;
                p.hash = &olympusCameraSettingsHash;
                ifd->readIFD(p);
                m.offsetFull = ifd->ifdDataHash.value(257).tagValue + makerOffset;
                m.lengthFull = ifd->ifdDataHash.value(258).tagValue;
                p.offset = m.offsetFull;
                jpeg->getWidthHeight(p, m.widthPreview, m.heightPreview);
            }

            if (equipmentIFDOffset) {
                p.hdr = "IFD Olympus Maker Note: EquipmentSettingsIFD";
                p.offset = equipmentIFDOffset;
                p.hash = &olympusEquipmentHash;
                ifd->readIFD(p);
                m.cameraSN = u.getString(p.file, ifd->ifdDataHash.value(257).tagValue + makerOffset, ifd->ifdDataHash.value(257).tagCount).trimmed();
                m.lensSN = u.getString(p.file, ifd->ifdDataHash.value(514).tagValue + makerOffset, ifd->ifdDataHash.value(514).tagCount).trimmed();
            }
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

    // Olympus does not embed xmp in raw files
    m.isXmp = false;

    return true;

}

/* ------------------------------------------------------------------------------------------
   OlympusRaw::UnpackCfa  --  Olympus ORF proprietary 12-bit compression
   Ported from dcraw's olympus_load_raw; validated byte-identical to libraw on an E-M1 ORF.
   ------------------------------------------------------------------------------------------ */

bool OlympusRaw::UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw)
{
    Q_UNUSED(m)
    using namespace TiffWalk;

    Reader r;
    if (!r.init(&file)) { errMsg = "ORF: not a TIFF file."; return false; }

    /* The raw strip is in IFD0 (Olympus tags Compression 1 / 16-bit, but it is really the
       proprietary 12-bit compression). Pick the largest-strip IFD to be safe. */
    QList<quint32> queue { r.firstIfd() };
    QSet<quint32> seen;
    Ifd rawIfd, ifd0;
    bool haveIfd0 = false;
    quint64 bestPx = 0;
    while (!queue.isEmpty()) {
        const quint32 off = queue.takeFirst();
        if (off == 0 || seen.contains(off)) continue;
        seen.insert(off);
        Ifd tags; QList<quint32> subs; quint32 next = 0;
        if (!r.readIfd(off, tags, subs, next)) continue;
        if (!haveIfd0) { ifd0 = tags; haveIfd0 = true; }
        for (quint32 s : subs) queue << s;
        if (next) queue << next;
        if (tags.contains(256) && tags.contains(257) && tags.contains(273) && tags.contains(279)) {
            const quint64 px = quint64(r.scalar(tags[256])) * r.scalar(tags[257]);
            if (px > bestPx) { bestPx = px; rawIfd = tags; }
        }
    }
    if (rawIfd.isEmpty()) { errMsg = "ORF: no CFA strip."; return false; }

    const int W = int(r.scalar(rawIfd[256]));
    const int H = int(r.scalar(rawIfd[257]));
    const quint32 so = r.scalar(rawIfd[273]);
    const quint32 sl = r.scalar(rawIfd[279]);
    if (W <= 2 || H <= 0) { errMsg = "ORF: bad dimensions."; return false; }

    if (!file.seek(so)) { errMsg = "ORF: seek to strip failed."; return false; }
    const QByteArray strip = file.read(int(sl));
    if (strip.size() < int(sl)) { errMsg = "ORF: short strip read."; return false; }
    const uchar *d = reinterpret_cast<const uchar *>(strip.constData());
    const int dn = strip.size();

    /* dcraw Olympus Huffman: a direct 4096-entry table indexed by the top 12 bits ->
       (codeLength << 8) | magnitudeValue. */
    int huff[4096];
    huff[0] = 0xc0c;
    int hn = 0;
    for (int i = 11; i >= 0; --i)
        for (int c = 0; c < (2048 >> i); ++c) huff[++hn] = ((i + 1) << 8) | i;

    /* MSB bit reader over the strip, starting 7 bytes in (dcraw skips 7). */
    int pos = 7; quint64 bitbuf = 0; int vbits = 0;
    auto ensure = [&](int nb) {
        while (vbits < nb) {
            const int b = (pos < dn) ? d[pos] : 0; ++pos;
            bitbuf = (bitbuf << 8) | quint64(b); vbits += 8;
        }
    };
    auto getbits = [&](int nb) -> int {
        if (nb <= 0) return 0;
        ensure(nb);
        const int v = int((bitbuf >> (vbits - nb)) & ((1u << nb) - 1));
        vbits -= nb; return v;
    };
    auto huffdec = [&]() -> int {
        ensure(12);
        const int c = int((bitbuf >> (vbits - 12)) & 0xfff);
        vbits -= huff[c] >> 8;
        return huff[c] & 0xff;
    };

    raw.width = W;
    raw.height = H;
    raw.cfa.assign(size_t(W) * size_t(H), 0);
    std::vector<int> img(size_t(W) * size_t(H), 0);     // signed scratch for predictors

    for (int row = 0; row < H; ++row) {
        int acarry[2][3] = {{0, 0, 0}, {0, 0, 0}};
        for (int col = 0; col < W; ++col) {
            int *carry = acarry[col & 1];
            const int i = 2 * (carry[2] < 3);
            int nbits = 2 + i;
            while ((carry[0] & 0xffff) >> (nbits + i)) ++nbits;
            const int sign3 = getbits(3);
            const int low = sign3 & 3;
            const int sign = ((sign3 >> 2) & 1) ? -1 : 0;     // sign<<29>>31
            int high = huffdec();
            if (high == 12) high = getbits(16 - nbits) >> 1;
            carry[0] = (high << nbits) | getbits(nbits);
            const int diff = (carry[0] ^ sign) + carry[1];
            carry[1] = (diff * 3 + carry[1]) >> 5;
            carry[2] = carry[0] > 16 ? 0 : carry[2] + 1;

            int pred;
            if (row < 2 && col < 2) pred = 0;
            else if (row < 2) pred = img[size_t(row) * W + (col - 2)];
            else if (col < 2) pred = img[size_t(row - 2) * W + col];
            else {
                const int w  = img[size_t(row) * W + (col - 2)];
                const int n  = img[size_t(row - 2) * W + col];
                const int nw = img[size_t(row - 2) * W + (col - 2)];
                if ((w < nw && nw < n) || (n < nw && nw < w)) {
                    if (qAbs(w - nw) > 32 || qAbs(n - nw) > 32) pred = w + n - nw;
                    else pred = (w + n) >> 1;
                } else {
                    pred = (qAbs(w - nw) > qAbs(n - nw)) ? w : n;
                }
            }
            const int val = pred + ((diff << 2) | low);
            img[size_t(row) * W + col] = val;
            raw.cfa[size_t(row) * W + col] = uint16_t(val & 0xffff);
        }
    }

    /* Levels / pattern / colour. FIRST CUT: 12-bit Olympus scheme -> white 4095; E-M1-validated
       black 255 and BGGR pattern; matrix by model. As-shot WB (MakerNote 0x2040->0x0100) and
       per-model black/pattern are a refinement -> RawColor uses the matrix-derived neutral WB. */
    raw.pattern = CfaPattern::BGGR;
    raw.white = 4095;
    for (int i = 0; i < 4; ++i) raw.black[i] = 255;
    QString model;
    if (haveIfd0 && ifd0.contains(272)) model = "Olympus " + r.ascii(ifd0[272]);
    xyzToCamForModel(model, raw.xyzToCam);

    /* CFA phase. Olympus bodies differ (the older E-M1 is BGGR, the OM-1 is RGGB), so the BGGR
       default above is not safe to assume. Read the EXIF CFAPattern (0xA302): a 2x2 repeat-dim
       header (two SHORTs) followed by the plane-colour bytes (0=R,1=G,2=B). Fall back to BGGR
       when the tag is absent. Without this an OM-1 renders with red and blue swapped. */
    if (haveIfd0 && ifd0.contains(0x8769)) {
        Ifd exif; QList<quint32> es; quint32 en = 0;
        if (r.readIfd(r.ifdPointer(ifd0[0x8769]), exif, es, en) && exif.contains(0xA302)) {
            const QByteArray cfa = r.bytes(exif[0xA302]);
            if (cfa.size() >= 8) {
                const uint8_t pc[4] = { uint8_t(cfa[4]), uint8_t(cfa[5]),
                                        uint8_t(cfa[6]), uint8_t(cfa[7]) };
                const CfaPattern pat = cfaPatternFromPlaneColor(pc);
                if (pat != CfaPattern::Unknown) raw.pattern = pat;
            }
        }
    }

    /* As-shot white balance from the Olympus MakerNote (essential -- matrix-neutral renders
       badly green for Olympus): IFD0 -> ExifIFD (0x8769) -> MakerNote (0x927C). The MakerNote is
       "OLYMPUS\0II\3\0" (12-byte header, embedded IFD at +12, offsets relative to its start),
       then ImageProcessing (0x2040) -> WB_RBLevels (0x0100, 2 SHORTs R,B, with green == 256). */
    if (haveIfd0 && ifd0.contains(0x8769)) {
        Ifd exif; QList<quint32> es; quint32 en = 0;
        if (r.readIfd(r.ifdPointer(ifd0[0x8769]), exif, es, en) && exif.contains(0x927C)) {
            const quint32 mnAbs = r.ifdPointer(exif[0x927C]);
            if (file.seek(mnAbs)) {
                const QByteArray h = file.read(12);
                if (h.size() == 12 && h.startsWith("OLYMPUS")) {
                    const bool mbig = (uchar(h[8]) == 'M');
                    Reader mr;
                    mr.initEmbedded(&file, mnAbs, 12, mbig);
                    Ifd mn; QList<quint32> ms; quint32 mnn = 0;
                    if (mr.readIfd(mr.firstIfd(), mn, ms, mnn) && mn.contains(0x2040)) {
                        Ifd ip; QList<quint32> is2; quint32 in2 = 0;
                        if (mr.readIfd(mr.ifdPointer(mn[0x2040]), ip, is2, in2) &&
                            ip.contains(0x0100)) {
                            const QVector<quint32> wb = mr.u32s(ip[0x0100]);   // R, B
                            if (wb.size() >= 2 && wb[0] && wb[1]) {
                                raw.camMul[0] = wb[0];  raw.camMul[1] = 256;   // R, G(=256)
                                raw.camMul[2] = wb[1];  raw.camMul[3] = 256;   // B, G2
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}
