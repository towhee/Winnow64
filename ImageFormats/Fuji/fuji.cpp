#include "fuji.h"

Fuji::Fuji()
{
    fujiMakerHash[0] = "Version";
    fujiMakerHash[16] = "InternalSerialNumber";
    fujiMakerHash[4096] = "Quality";
    fujiMakerHash[4097] = "Sharpness";
    fujiMakerHash[4098] = "WhiteBalance";
    fujiMakerHash[4099] = "Saturation";
    fujiMakerHash[4100] = "Contrast";
    fujiMakerHash[4101] = "ColorTemperature";
    fujiMakerHash[4102] = "Contrast";
    fujiMakerHash[4106] = "WhiteBalanceFineTune";
    fujiMakerHash[4107] = "NoiseReduction";
    fujiMakerHash[4110] = "HighISONoiseReduction";
    fujiMakerHash[4112] = "FujiFlashMode";
    fujiMakerHash[4113] = "FlashExposureComp";
    fujiMakerHash[4128] = "Macro";
    fujiMakerHash[4129] = "FocusMode";
    fujiMakerHash[4130] = "AFMode";
    fujiMakerHash[4131] = "FocusPixel";
    fujiMakerHash[4144] = "SlowSync";
    fujiMakerHash[4145] = "PictureMode";
    fujiMakerHash[4146] = "ExposureCount";
    fujiMakerHash[4147] = "EXRAuto";
    fujiMakerHash[4148] = "EXRMode";
    fujiMakerHash[4160] = "ShadowTone";
    fujiMakerHash[4161] = "HighlightTone";
    fujiMakerHash[4164] = "DigitalZoom";
    fujiMakerHash[4176] = "ShutterType";
    fujiMakerHash[4352] = "AutoBracketing";
    fujiMakerHash[4353] = "SequenceNumber";
    fujiMakerHash[4435] = "PanoramaAngle";
    fujiMakerHash[4436] = "PanoramaDirection";
    fujiMakerHash[4609] = "AdvancedFilter";
    fujiMakerHash[4624] = "ColorMode";
    fujiMakerHash[4864] = "BlurWarning";
    fujiMakerHash[4865] = "FocusWarning";
    fujiMakerHash[4866] = "ExposureWarning";
    fujiMakerHash[4868] = "GEImageSize";
    fujiMakerHash[5120] = "DynamicRange";
    fujiMakerHash[5121] = "FilmMode";
    fujiMakerHash[5122] = "DynamicRangeSetting";
    fujiMakerHash[5123] = "DevelopmentDynamicRange";
    fujiMakerHash[5124] = "MinFocalLength";
    fujiMakerHash[5125] = "MaxFocalLength";
    fujiMakerHash[5126] = "MaxApertureAtMinFocal";
    fujiMakerHash[5127] = "MaxApertureAtMaxFocal";
    fujiMakerHash[5131] = "AutoDynamicRange";
    fujiMakerHash[5154] = "ImageStabilization";
    fujiMakerHash[5157] = "SceneRecognition";
    fujiMakerHash[5169] = "Rating";
    fujiMakerHash[5174] = "ImageGeneration";
    fujiMakerHash[5176] = "ImageCount";
    fujiMakerHash[14368] = "FrameRate";
    fujiMakerHash[14369] = "FrameWidth";
    fujiMakerHash[14370] = "FrameHeight";
    fujiMakerHash[16640] = "FacesDetected";
    fujiMakerHash[16643] = "FacePositions";
    fujiMakerHash[16896] = "NumFaceElements";
    fujiMakerHash[16897] = "FaceElementTypes";
    fujiMakerHash[16899] = "FaceElementPositions";
    fujiMakerHash[17026] = "FaceRecInfo";
    fujiMakerHash[32768] = "FileSource";
    fujiMakerHash[32770] = "OrderNumber";
    fujiMakerHash[32771] = "FrameNumber";
    fujiMakerHash[45585] = "Parallax";
}

bool Fuji::parse(MetadataParameters &p,
                 ImageMetadata &m,
                 IFD *ifd,
                 Exif *exif,
                 Jpeg *jpeg)
{
    //file.open in Metadata::readMetadata

    /* Fuji File Format

    General structure:
    Fuji information
    Directory of offsets
    JPEG file
    CFA Header
    CFA

    Offset          Bytes   Description
    000 - 015       16      "FUJIFILMCCD-RAW "
    016 - 019       4       Format fersion ie "0201"
    020 - 027       8       Camera ID ie "FF129502"
    028 - 059       32      32 bytes for the camera string, \0 terminated
    Start of offset directory
    060 - 063       4       Directory version ie "0100", "0159", "0135"
    064 - 083       20      Unkonwn
    084 - 087       4       JPEG image offset
    088 - 091       4       JPEG image length
    092 - 095       4       CFA Header Offset (Color Filter Array)
    096 - 099       4       CFA Header Length
    100 - 103       4       CFA offset
    104 - 107       4       CFA length
    108 - (148 or JPG offset) Unknown
    148 - 151 or close = JPEG image offset FF D8 FF E1
    */

    // Fuji is big endian
    bool isBigEnd = true;
    quint32 startOffset = 0;
    p.file.seek(0);

    // read first 16 bytes to confirm it is a fuji file
    if (p.file.read(16) != "FUJIFILMCCD-RAW ") return false;

    // seek JPEG image offset
    p.file.seek(84);
    m.offsetFull = u.get32(p.file.read(4));
    m.lengthFull = u.get32(p.file.read(4));
    // default values for thumbnail
    m.offsetThumb = m.offsetFull;
    m.lengthThumb =  m.lengthFull;
    p.file.seek(m.offsetFull);

    // start on embedded JPEG
    if (u.get16(p.file.read(2)) != 0xFFD8) return false;

    // build a hash of jpg segment offsets
    p.offset = p.file.pos();
    jpeg->getJpgSegments(p, m);

    // read the EXIF data
    if (jpeg->segmentHash.contains("EXIF")) p.file.seek(jpeg->segmentHash["EXIF"]);
    else return false;

    bool foundEndian = false;
    int count = 0;
    while (!foundEndian) {
        quint32 order = u.get16(p.file.read(2));
        if (order == 0x4949 || order == 0x4D4D) {
            order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
            // offsets are from the endian position in JPEGs
            // therefore must adjust all offsets found in tagValue
            startOffset = static_cast<quint32>(p.file.pos()) - 2;
            foundEndian = true;
        }
        // add condition to check for EOF
        count++;
        if (count > 100) {
            // err endian order not found
            QString err = "Endian order not found.";
            G::error("Fuji::parse", m.fPath, err);
            return false;
        }
    }

    if (p.report) p.rpt << "\n startOffset = " << startOffset;

    quint32 a = u.get16(p.file.read(2), isBigEnd);  // magic 42
    a = u.get32(p.file.read(4), isBigEnd);
    quint32 offsetIfd0 = a + startOffset;

//    getDimensions(offsetFullJPG);

    // read IFD0:
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p) + startOffset;

    // IFD0 Offsets
    quint32 offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;

    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
                     ifd->ifdDataHash.value(271).tagCount);
    m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue + startOffset,
                      ifd->ifdDataHash.value(272).tagCount);
    m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                          ifd->ifdDataHash.value(33432).tagCount);

    // read IFD1
    if (nextIFDOffset) {
        p.hdr = "IFD1";
        p.offset = nextIFDOffset;
        nextIFDOffset = ifd->readIFD(p);
    }
    m.offsetThumb = ifd->ifdDataHash.value(513).tagValue + startOffset;
    m.lengthThumb = ifd->ifdDataHash.value(514).tagValue + startOffset;
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    // read EXIF IFD
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p);

    m.width = static_cast<int>(ifd->ifdDataHash.value(40962).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(40963).tagValue);
    p.offset = m.offsetFull;
    if (!m.width || !m.height) jpeg->getDimensions(p, m);
    jpeg->getWidthHeight(p, m.widthPreview, m.heightPreview);

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      isBigEnd);
        if (x < 1 ) {
            int t = qRound(1 / x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = static_cast<double>(x);
        } else {
            uint t = static_cast<uint>(x);
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
    m.lens = u.getString(p.file, ifd->ifdDataHash.value(42036).tagValue + startOffset,
                     ifd->ifdDataHash.value(42036).tagCount);

    // Exif: read makernoteIFD
    if (ifd->ifdDataHash.contains(37500)) {
        quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue + startOffset + 12;
        p.hdr = "IFD Fuji Maker Note";
        p.offset = makerOffset;
        p.hash = &fujiMakerHash;
        ifd->readIFD(p);
    }

    // jpg preview metadata report information
    if (p.report) {
        p.offset = m.offsetFull;
        Jpeg jpeg;
        IPTC iptc;
        GPS gps;
        jpeg.parse(p, m, ifd, &iptc, exif, &gps);
    }

    // Fuji files do not contain xmp

    return true;
}
