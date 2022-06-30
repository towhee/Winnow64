#include "olympus.h"

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
    olympusMakerHash[8208] = "Equipment";
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
}

bool Olympus::parse(MetadataParameters &p,
                    ImageMetadata &m,
                    IFD *ifd,
                    Exif *exif,
                    Jpeg *jpeg)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__); 
    //file.open in Metadata::readMetadata
    // first two bytes is the endian order (skip next 2 bytes)
    quint16 order = Utilities::get16(p.file.read(4));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // get offset to first IFD and read it
    quint32 offsetIfd0 = Utilities::get32(p.file.read(4), isBigEnd);

    // Olympus does not chain IFDs
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    ifd->readIFD(p);

    // pull data reqd from IFD0
    m.make = Utilities::getString(p.file, ifd->ifdDataHash.value(271).tagValue, ifd->ifdDataHash.value(271).tagCount).trimmed();
    m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount).trimmed();
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.creator = Utilities::getString(p.file, ifd->ifdDataHash.value(315).tagValue, ifd->ifdDataHash.value(315).tagCount);
    m.copyright = Utilities::getString(p.file, ifd->ifdDataHash.value(33432).tagValue, ifd->ifdDataHash.value(33432).tagCount);
    m.width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);

    // get the offset for ExifIFD and read it
    quint32 offsetEXIF;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p);

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // get shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.file,
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
        double x = Utilities::getReal(p.file,
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
        double x = Utilities::getReal_s(p.file,
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
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(37386).tagValue,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }
    // lens
    m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
                ifd->ifdDataHash.value(42036).tagCount);

    // read makernoteIFD

    /* the makernotes IFD is preceeded with either
       "OLYMPUS " + 49 49  03 00
       "OM SYSTEM   " + 49 49  04 00
     */
    quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue;
    p.file.seek(makerOffset);
    bool foundMakerNotes = false;
    if (ifd->ifdDataHash.contains(37500)) {
        int off;
        for (off = 0; off < 30; off++) {
            quint16 order = Utilities::get16(p.file.read(2));
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
            m.offsetThumb = ifd->ifdDataHash.value(256).tagValue + makerOffset;
            m.lengthThumb = ifd->ifdDataHash.value(256).tagCount;
    //        if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

            // read CameraSettingsIFD
            if (ifd->ifdDataHash.contains(8224)) {
    //            readIFD("IFD Olympus Maker Note: CameraSettingsIFD",
    //                    ifd->ifdDataHash.value(8224).tagValue + makerOffset);
                offset = ifd->ifdDataHash.value(8224).tagValue + makerOffset;
                p.hdr = "IFD Olympus Maker Note: CameraSettingsIFD";
                p.offset = offset;
                ifd->readIFD(p);
                m.offsetFull = ifd->ifdDataHash.value(257).tagValue + makerOffset;
                m.lengthFull = ifd->ifdDataHash.value(258).tagValue;
                p.offset = m.offsetFull;
                qDebug() << __PRETTY_FUNCTION__ << "p.offset =" << p.offset;
                jpeg->getWidthHeight(p, m.widthPreview, m.heightPreview);
//                jpeg->getDimensions(p, m);
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
