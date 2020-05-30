#include "sony.h"

Sony::Sony()
{
    sonyMakerHash[258] = "Image quality";
    sonyMakerHash[260] = "Flash exposure compensation in EV";
    sonyMakerHash[261] = "Teleconverter Model";
    sonyMakerHash[274] = "White Balance Fine Tune Value";
    sonyMakerHash[276] = "Camera Settings";
    sonyMakerHash[277] = "White balance";
    sonyMakerHash[278] = "Unknown";
    sonyMakerHash[3584] = "PrintIM information";
    sonyMakerHash[4096] = "Multi Burst Mode";
    sonyMakerHash[4097] = "Multi Burst Image Width";
    sonyMakerHash[4098] = "Multi Burst Image Height";
    sonyMakerHash[4099] = "Panorama";
    sonyMakerHash[8192] = "Unknown";
    sonyMakerHash[8193] = "JPEG preview image";
    sonyMakerHash[8194] = "Unknown";
    sonyMakerHash[8195] = "Unknown";
    sonyMakerHash[8196] = "Contrast";
    sonyMakerHash[8197] = "Saturation";
    sonyMakerHash[8198] = "Sharpness";
    sonyMakerHash[8199] = "Brightness";
    sonyMakerHash[8200] = "LongExposureNoiseReduction ";
    sonyMakerHash[8201] = "HighISONoiseReduction ";
    sonyMakerHash[8202] = "High Definition Range Mode";
    sonyMakerHash[8203] = "Multi Frame Noise Reduction";
    sonyMakerHash[8212] = "WBShiftAB_GM";
    sonyMakerHash[8220] = "AFAreaModeSetting";
    sonyMakerHash[12288] = "Shot Information IFD";
    sonyMakerHash[45056] = "File Format";
    sonyMakerHash[45057] = "Sony Model ID";
    sonyMakerHash[45088] = "Color Reproduction";
    sonyMakerHash[45089] = "Color Temperature";
    sonyMakerHash[45090] = "Color Compensation Filter";
    sonyMakerHash[45091] = "Scene Mode";
    sonyMakerHash[45092] = "Zone Matching";
    sonyMakerHash[45093] = "Dynamic Range Optimizer";
    sonyMakerHash[45094] = "Image stabilization";
    sonyMakerHash[45095] = "Lens identifier";
    sonyMakerHash[45096] = "Minolta MakerNote";
    sonyMakerHash[45097] = "Color Mode";
    sonyMakerHash[45099] = "Full Image Size";
    sonyMakerHash[45100] = "Preview image size";
    sonyMakerHash[45120] = "Macro";
    sonyMakerHash[45121] = "Exposure Mode";
    sonyMakerHash[45122] = "Focus Mode";
    sonyMakerHash[45123] = "AF Mode";
    sonyMakerHash[45124] = "AF Illuminator";
    sonyMakerHash[45127] = "JPEG Quality";
    sonyMakerHash[45128] = "Flash Level";
    sonyMakerHash[45129] = "Release Mode";
    sonyMakerHash[45130] = "Shot number in continuous burst mode";
    sonyMakerHash[45131] = "Anti-Blur";
    sonyMakerHash[45134] = "Long Exposure Noise Reduction";
    sonyMakerHash[45135] = "Dynamic Range Optimizer";
    sonyMakerHash[45138] = "Intelligent Auto";
    sonyMakerHash[45140] = "White balance 2";
}

bool Sony::parse(MetadataParameters &p,
                 ImageMetadata &m,
                 IFD *ifd,
                 Exif *exif,
                 Jpeg *jpeg)
{
    //file.open in Metadata::readMetadata
    // first two bytes is the endian order (skip next 2 bytes)
    quint16 order = Utilities::get16(p.file.read(4));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // get offset to first IFD and read it
    quint32 offsetIfd0 = Utilities::get32(p.file.read(4), isBigEnd);
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p, m);

    // pull data reqd from IFD0
    m.offsetFull = ifd->ifdDataHash.value(513).tagValue;
    m.lengthFull = ifd->ifdDataHash.value(514).tagValue;
    // get jpeg full size preview dimensions
    p.offset = m.offsetFull;
    jpeg->getWidthHeight(p, m.widthFull, m.heightFull);
//    if (lengthFullJPG) verifyEmbeddedJpg(offsetFull, lengthFull);
    m.offsetThumb = ifd->ifdDataHash.value(273).tagValue;
    m.lengthThumb = ifd->ifdDataHash.value(279).tagValue;

//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumb, lengthThumb);
    m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount);
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);

    quint32 offsetEXIF;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue;

    /* Sony provides an offset in IFD0 to subIFDs, but there is only one, which is
       at the offset ifd->ifdDataHash.value(330).tagValue */

    // SubIFD0
    quint32 offset = ifd->ifdDataHash.value(330).tagValue;
    p.hdr = "SubIFD0";
    p.offset = offset;
    ifd->readIFD(p, m);
    m.width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);

    // IFD 1:
    p.hdr = "IFD1";
    p.offset = nextIFDOffset;
    if (nextIFDOffset) ifd->readIFD(p, m);

    m.offsetThumb = ifd->ifdDataHash.value(513).tagValue;
    m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;
//    if (lengthThumbJPG) verifyEmbeddedJpg(offsetThumbJPG, lengthThumbJPG);

    // get the offset for ExifIFD and read it
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p, m);

    // IFD EXIF: dimensions
//    m.width = ifd->ifdDataHash.value(40962).tagValue;
//    m.height = ifd->ifdDataHash.value(40963).tagValue;

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // Exif: get shutter speed
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

    // Exif: aperture
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

    //Exif: ISO
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

    // Exif: focal length
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

    // Exif: lens
    m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
        ifd->ifdDataHash.value(42036).tagCount);

    // Exif: read makernoteIFD

    if (ifd->ifdDataHash.contains(37500)) {
        quint32 makerOffset = ifd->ifdDataHash.value(37500).tagValue;
        p.hdr = "IFD Sony Maker Note";
        p.offset = makerOffset;
        p.hash = &sonyMakerHash;
        ifd->readIFD(p, m);
    }

    // Sony does not embed xmp in raw files

    return true;

}
