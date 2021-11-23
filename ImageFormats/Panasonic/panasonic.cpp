#include "panasonic.h"

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
    if (G::isLogger) G::log(__FUNCTION__); 
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
    quint16 order = Utilities::get16(p.file.read(4));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // get offset to first IFD and read it
    quint32 offsetIfd0 = Utilities::get32(p.file.read(4), isBigEnd);

    // Panasonic does not chain IFDs
    p.hdr = "Panasonic IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    ifd->readIFD(p);

    // pull data reqd from main file IFD0
    m.make = Utilities::getString(p.file, ifd->ifdDataHash.value(271).tagValue, ifd->ifdDataHash.value(271).tagCount).trimmed();
    m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount).trimmed();
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.creator = Utilities::getString(p.file, ifd->ifdDataHash.value(315).tagValue, ifd->ifdDataHash.value(315).tagCount);
    m.copyright = Utilities::getString(p.file, ifd->ifdDataHash.value(33432).tagValue, ifd->ifdDataHash.value(33432).tagCount);
    m.offsetFull = ifd->ifdDataHash.value(46).tagValue;
    m.lengthFull = ifd->ifdDataHash.value(46).tagCount;
    // default values for thumbnail
    m.offsetThumb = m.offsetFull;
    m.lengthThumb =  m.lengthFull;
    p.offset = m.offsetFull;
    jpeg->getWidthHeight(p, m.widthFull, m.heightFull);
    m.xmpSegmentOffset = ifd->ifdDataHash.value(700).tagValue;
    m.xmpNextSegmentOffset = ifd->ifdDataHash.value(700).tagCount + m.xmpSegmentOffset;
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
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // shutter speed
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
    // exposure compensation
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

    // check embedded JPG for more metadata (IFD0, IFD1, Exit IFD and Maker notes IFD)
    order = 0x4D4D;
    isBigEnd = true;
    quint32 startOffset = m.offsetFull;
    p.file.seek(m.offsetFull);

    if (Utilities::get16(p.file.read(2), isBigEnd) != 0xFFD8) return 0;

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
            quint32 a = Utilities::get16(p.file.read(2), isBigEnd);
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

        quint32 a = Utilities::get16(p.file.read(2));  // skip magic 42
        a = Utilities::get32(p.file.read(4), isBigEnd);
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
            m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(81).tagValue + startOffset, ifd->ifdDataHash.value(81).tagCount);
            // get lens serial number
            m.lensSN = Utilities::getString(p.file, ifd->ifdDataHash.value(82).tagValue + startOffset, ifd->ifdDataHash.value(82).tagCount);
            // get camera serial number
            m.cameraSN = Utilities::getString(p.file, ifd->ifdDataHash.value(37).tagValue + startOffset, ifd->ifdDataHash.value(37).tagCount);
        }
    }

    m.isXmp = true;       // Panasonic has an xmp segment

    // read XMP
    bool okToReadXmp = true;
    if (m.isXmp && okToReadXmp && m.xmpSegmentOffset > 0 && m.xmpNextSegmentOffset > 0) {
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpNextSegmentOffset);
        m.rating = xmp.getItem("Rating");     // case is important "Rating"
        m.label = xmp.getItem("Label");       // case is important "Label"
        m.title = xmp.getItem("title");       // case is important "title"
        m.cameraSN = xmp.getItem("SerialNumber");
        m.lens = xmp.getItem("Lens");
        m.lensSN = xmp.getItem("LensSerialNumber");
        m.creator = xmp.getItem("creator");
        m.copyright = xmp.getItem("rights");
        m.email = xmp.getItem("CiEmailWork");
        m.url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        m._rating = m.rating;
        m._label = m.label;
        m._title = m.title;
        m._creator = m.creator;
        m._copyright = m.copyright;
        m._email  = m.email ;
        m._url = m.url;

        if (p.report) p.xmpString = xmp.metaAsString();
    }

    return true;

}
