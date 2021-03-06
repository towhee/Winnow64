#include "dng.h"

DNG::DNG()
{
}

bool DNG::parse(MetadataParameters &p,
                ImageMetadata &m,
                IFD *ifd,
                IPTC *iptc,
                Exif *exif,
                Jpeg *jpeg)
{
    //file.open happens in readMetadata

    quint32 startOffset = 0;

    // first two bytes is the endian order
    quint16 order = Utilities::get16(p.file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // should be magic number 42 next
    if (Utilities::get16(p.file.read(2), isBigEnd) != 42) return false;

    // read offset to first IFD
    quint32 ifdOffset = Utilities::get32(p.file.read(4), isBigEnd);
    p.hdr = "IFD0";
    p.offset = ifdOffset;
    p.hash = &exif->hash;
    ifd->readIFD(p, m);

    m.lengthFull = 1;  // set arbitrary length to avoid error msg as tif do not
                          // have full size embedded jpg

    // IFD0: *******************************************************************

    // IFD0: Model
    (ifd->ifdDataHash.contains(272))
        ? m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount)
        : m.model = "";

    // IFD0: Make
    (ifd->ifdDataHash.contains(271))
        ? m.make = Utilities::getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
        ifd->ifdDataHash.value(271).tagCount)
        : m.make = "";

    // IFD0: Title (ImageDescription)
    (ifd->ifdDataHash.contains(270))
        ? m.title = Utilities::getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(270).tagCount)
        : m.title = "";

    // IFD0: Creator (artist)
    (ifd->ifdDataHash.contains(315))
        ? m.creator = Utilities::getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(315).tagCount)
        : m.creator = "";

    // IFD0: Copyright
    (ifd->ifdDataHash.contains(33432))
            ? m.copyright = Utilities::getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
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

    p.offset = 0;
    if (!m.width || !m.height) jpeg->getDimensions(p, m);

    // IFD0: EXIF offset
    quint32 ifdEXIFOffset = 0;
    if (ifd->ifdDataHash.contains(34665))
        ifdEXIFOffset = ifd->ifdDataHash.value(34665).tagValue;

    // IFD0: Photoshop offset
    quint32 ifdPhotoshopOffset = 0;
    if (ifd->ifdDataHash.contains(34377))
        ifdPhotoshopOffset = ifd->ifdDataHash.value(34377).tagValue;

    // IFD0: IPTC offset
    quint32 ifdIPTCOffset = 0;
    if (ifd->ifdDataHash.contains(33723))
        ifdIPTCOffset = ifd->ifdDataHash.value(33723).tagValue;

    // IFD0: XMP offset
    quint32 ifdXMPOffset = 0;
    if (ifd->ifdDataHash.contains(700)) {
        m.isXmp = true;
        ifdXMPOffset = ifd->ifdDataHash.value(700).tagValue;
        m.xmpSegmentOffset = ifdXMPOffset;
        int xmpSegmentLength = static_cast<int>(ifd->ifdDataHash.value(700).tagCount);
        m.xmpNextSegmentOffset = m.xmpSegmentOffset + static_cast<uint>(xmpSegmentLength);
    }

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
    QList<quint32> ifdOffsets;
    if(ifd->ifdDataHash.contains(330)) {
        int count = static_cast<int>(ifd->ifdDataHash.value(330).tagCount);
        if (count > 1) {
            quint32 addr = ifd->ifdDataHash.value(330).tagValue;
            ifdOffsets = ifd->getSubIfdOffsets(p.file, addr, count);
        }
        else ifdOffsets.append(ifd->ifdDataHash.value(330).tagValue);

        QString hdr;
        count = 0;
        quint32 smallest = 999999;
        quint32 largest = 0;
        int smallJpg = 0;
        int largeJpg = 0;

        // iterate subIFDs looking for embedded JPGs
        for (int i = 0; i < ifdOffsets.length(); ++i) {
            p.hdr = "SubIFD" + QString::number(i + 1);
            p.offset = ifdOffsets[i];
            p.hash = &exif->hash;
            ifd->readIFD(p, m);
            if (ifd->ifdDataHash.contains(273)) {
                // is it a JPG
                quint32 offset = ifd->ifdDataHash.value(273).tagValue;
                p.file.seek(offset);
//                quint32 x = get2(file.read(2));
                if (Utilities::get16(p.file.read(2), true) != 0xFFD8) break;  // order = 4949 so reverse
                // yes it is a JPG
                jpgInfo.offset = offset;
                jpgInfo.length = ifd->ifdDataHash.value(279).tagValue;
                jpgInfo.width = ifd->ifdDataHash.value(256).tagValue;
                jpgInfo.height = ifd->ifdDataHash.value(257).tagValue;
                /*
                qDebug() << __FUNCTION__ << "i =" << i
                         << "jpgInfo.offset =" << jpgInfo.offset
                         << "jpgInfo.length" << jpgInfo.length
                         << "jpgInfo.width" << jpgInfo.width
                         << "jpgInfo.height" << jpgInfo.height;
//                */
                jpgs.append(jpgInfo);
                // find smallest and largest
                if (jpgInfo.width < smallest) {
                    smallest = jpgInfo.width;
                    smallJpg = count;
                }
                if (jpgInfo.width > largest) {
                    largest = jpgInfo.width;
                    largeJpg = count;
                }
                count++;
            }
        }
        if (jpgs.length() > 0) {
            m.width = static_cast<uint>(jpgs.at(largeJpg).width);
            m.height = static_cast<uint>(jpgs.at(largeJpg).height);
            m.offsetFull = static_cast<uint>(jpgs.at(largeJpg).offset);
            m.lengthFull = static_cast<uint>(jpgs.at(largeJpg).length);
            m.widthFull = m.width;
            m.heightFull = m.height;
//            m.offsetSmall = static_cast<uint>(jpgs.at(smallJpg).offset);
//            m.lengthSmall = static_cast<uint>(jpgs.at(smallJpg).length);
            m.offsetThumb = static_cast<uint>(jpgs.at(smallJpg).offset);
            m.lengthThumb = static_cast<uint>(jpgs.at(smallJpg).length);
        }
    }


    // EXIF: *******************************************************************

    if (ifdEXIFOffset) {
        p.hdr = "IFD Exif";
        p.offset = ifdEXIFOffset;
        p.hash = &exif->hash;
        ifd->readIFD(p, m);
    }

    // EXIF: created datetime
    QString createdExif;
    (ifd->ifdDataHash.contains(36868))
        ? createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount)
        : createdExif = "";
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.file,
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
        double x = Utilities::getReal(p.file,
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
        double x = Utilities::getReal_s(p.file,
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
        double x = Utilities::getReal(p.file,
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
            ? m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
                                  ifd->ifdDataHash.value(42036).tagCount)
            : m.lens = "";

    // Photoshop: **************************************************************
    // Get embedded JPG if available

    bool foundTifThumb = false;
//    if (ifdPhotoshopOffset) readIRB(ifdPhotoshopOffset);

    // IPTC: *******************************************************************
    // Get title if available

//    if (ifdIPTCOffset) readIPTC(ifdIPTCOffset);

    // read XMP - no XMP in fuji raw files
    bool okToReadXmp = true;
    if (m.isXmp && okToReadXmp) {
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpNextSegmentOffset);
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

        if (p.report) p.xmpString = xmp.metaAsString();
    }

    return true;
}
