#include "canon.h"

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
    canonMakerHash[147] = "FileInfo tags";
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
    if (G::isLogger) G::log(CLASSFUNCTION); 
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

    // get the offset for GPSIFD
    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue;

    // xmp offset
    m.xmpSegmentOffset = ifd->ifdDataHash.value(700).tagValue;
    // xmpNextSegmentOffset used to later calc available room in xmp
    m.xmpSegmentLength = ifd->ifdDataHash.value(700).tagCount/* + m.xmpSegmentOffset*/;
    if (m.xmpSegmentOffset) m.isXmp = true;
    else m.isXmp = false;

    // EXIF IFD offset (to be used in a little)
    quint32 offsetEXIF = 0;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;

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

    if (ifd->ifdDataHash.contains(37500)) {
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
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength);
        if (xmp.isValid) {
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
