#include "tiff.h"

Tiff::Tiff()
{

}

bool Tiff::parse(QFile &file,
           ImageMetadata &m,
           IFD *ifd,
           IPTC *iptc,
           Exif *exif,
           Jpeg *jpeg,
           bool report,
           QTextStream &rpt,
           QString &xmpString)
{
    //file.open happens in readMetadata

    quint32 startOffset = 0;

    // first two bytes is the endian order
    quint16 order = Utilities::get16(file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;

    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // should be magic number 42 next
    if (Utilities::get16(file.read(2), isBigEnd) != 42) return false;

    // read offset to first IFD
    QString hdr = "IFD0";
    quint32 ifdOffset = Utilities::get32(file.read(4), isBigEnd);
    ifd->readIFD(file, ifdOffset, m, exif->hash, report, rpt, hdr, isBigEnd);

    m.lengthFullJPG = 1;  // set arbitrary length to avoid error msg as tif do not
                         // have full size embedded jpg

    // IFD0: *******************************************************************

    // IFD0: Model
    (ifd->ifdDataHash.contains(272))
        ? m.model = Utilities::getString(file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount)
        : m.model = "";

    // IFD0: Make
    (ifd->ifdDataHash.contains(271))
        ? m.make = Utilities::getString(file, ifd->ifdDataHash.value(271).tagValue + startOffset,
        ifd->ifdDataHash.value(271).tagCount)
        : m.make = "";

    // IFD0: Title (ImageDescription)
    (ifd->ifdDataHash.contains(270))
        ? m.title = Utilities::getString(file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(270).tagCount)
        : m.title = "";

    // IFD0: Creator (artist)
    (ifd->ifdDataHash.contains(315))
        ? m.creator = Utilities::getString(file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(315).tagCount)
        : m.creator = "";

    // IFD0: Copyright
    (ifd->ifdDataHash.contains(33432))
            ? m.copyright = Utilities::getString(file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                                  ifd->ifdDataHash.value(33432).tagCount)
            : m.copyright = "";

    // IFD0: width
    (ifd->ifdDataHash.contains(256))
        ? m.width = static_cast<uint>(ifd->ifdDataHash.value(256).tagValue)
        : m.width = 0;

    // IFD0: height
    (ifd->ifdDataHash.contains(257))
        ? m.height = static_cast<uint>(ifd->ifdDataHash.value(257).tagValue)
        : m.height = 0;

    if (!m.width || !m.height) jpeg->getDimensions(file, 0, m);  // rgh does this work??

    // IFD0: EXIF offset
    quint32 ifdEXIFOffset = 0;
    if (ifd->ifdDataHash.contains(34665))
        ifdEXIFOffset = ifd->ifdDataHash.value(34665).tagValue;

    // IFD0: Photoshop offset
    quint32 ifdPhotoshopOffset = 0;
    if (ifd->ifdDataHash.contains(34377))
        ifdPhotoshopOffset = ifd->ifdDataHash.value(34377).tagValue;

    // IFD0: subIFD offset
    quint32 ifdsubIFDOffset = 0;
    if (ifd->ifdDataHash.contains(330))
        ifdsubIFDOffset = ifd->ifdDataHash.value(330).tagValue;

    // IFD0: IPTC offset
    quint32 ifdIPTCOffset = 0;
    if (ifd->ifdDataHash.contains(33723))
        ifdIPTCOffset = ifd->ifdDataHash.value(33723).tagValue;

    // IFD0: XMP offset
    quint32 ifdXMPOffset = 0;
    if (ifd->ifdDataHash.contains(700)) {
        ifdXMPOffset = ifd->ifdDataHash.value(700).tagValue;
        m.xmpSegmentOffset = static_cast<uint>(ifdXMPOffset);
        int xmpSegmentLength = static_cast<int>(ifd->ifdDataHash.value(700).tagCount);
        m.xmpNextSegmentOffset = m.xmpSegmentOffset + static_cast<uint>(xmpSegmentLength);
    }

    // subIFDs: ****************************************************************

    /* If save tiff with save pyramid in photoshop then subIFDs created. Iterate to report and
    possibly read smallest for thumbnail if not a thumb in the photoshop IRB. */
    uint thumbWidth = m.width;
    if (ifdsubIFDOffset) {
//        startOffset = 4;
        quint32 nextIFDOffset;
        hdr = "SubIFD 1";
        nextIFDOffset = ifd->readIFD(file, ifdsubIFDOffset, m, exif->hash, report, rpt, hdr, isBigEnd) + startOffset;
        if(ifd->ifdDataHash.contains(256)) {
            if (ifd->ifdDataHash.value(256).tagValue < thumbWidth) {
                thumbWidth = ifd->ifdDataHash.value(256).tagValue;
            }
        }
        int count = 2;
        while (nextIFDOffset && count < 5) {
            QString hdr = "SubIFD " + QString::number(count);
            nextIFDOffset = ifd->readIFD(file, nextIFDOffset, m, exif->hash, report, rpt, hdr, isBigEnd) + startOffset;
            if(ifd->ifdDataHash.contains(256)) {
                if (ifd->ifdDataHash.value(256).tagValue < thumbWidth) {
                    thumbWidth = ifd->ifdDataHash.value(256).tagValue;
                }
            }
            count ++;
        }
    }

    // EXIF: *******************************************************************

    hdr = "IFD Exif";
    if (ifdEXIFOffset) ifd->readIFD(file, ifdEXIFOffset, m, exif->hash, report, rpt, hdr, isBigEnd);

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(file,
                                      ifd->ifdDataHash.value(33434).tagValue,
                                      isBigEnd);
        if (x < 1 ) {
            int t = qRound(1 / x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = static_cast<float>(x);
        } else {
            int t = static_cast<int>(x);
            m.exposureTime = QString::number(t);
            m.exposureTimeNum = t;
        }
        m.exposureTime += " sec";
    } else {
        m.exposureTime = "";
    }

    // EXIF: aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = Utilities::getReal(file,
                                      ifd->ifdDataHash.value(33437).tagValue,
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
        double x = Utilities::getReal(file,
                                      ifd->ifdDataHash.value(37386).tagValue,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // EXIF: lens model
    (ifd->ifdDataHash.contains(42036))
            ? m.lens = Utilities::getString(file, ifd->ifdDataHash.value(42036).tagValue,
                                  ifd->ifdDataHash.value(42036).tagCount)
            : m.lens = "";

    // Photoshop: **************************************************************
    // Get embedded JPG if available

//    foundTifThumb = false;
//    if (ifdPhotoshopOffset) readIRB(ifdPhotoshopOffset);    // rgh need to add IRB for this and DNG

    // IPTC: *******************************************************************
    // Get title if available

    if (ifdIPTCOffset) iptc->read(file, ifdIPTCOffset, m);

    // read XMP
    bool okToReadXmp = true;
    if (m.isXmp && okToReadXmp) {
        Xmp xmp(file, m.xmpSegmentOffset, m.xmpNextSegmentOffset);
        m.rating = xmp.getItem("Rating");     // case is important "Rating"
        m.label = xmp.getItem("Label");       // case is important "Label"
        m.title = xmp.getItem("title");       // case is important "title"
        m.cameraSN = xmp.getItem("SerialNumber");
        if (m.lens.isEmpty()) m.lens = xmp.getItem("Lens");
        m.lensSN = xmp.getItem("LensSerialNumber");
        if (m.creator.isEmpty()) m.creator = xmp.getItem("creator");
        m.copyright = xmp.getItem("rights");
        m.email = xmp.getItem("CiEmailWork");
        m.url = xmp.getItem("CiUrlWork");

        // save original values so can determine if edited when writing changes
        m._title = m.title;
        m._rating = m.rating;
        m._label = m.label;

        if (report) xmpString = xmp.metaAsString();
    }

    return true;
}
