#include "jpeg.h"
//#include "Main/global.h"
//#include "Utilities/utilities.h"

Jpeg::Jpeg()
{
    initSegCodeHash();
}

void Jpeg::initSegCodeHash()
{
    segCodeHash[0xFFC0] = "SOF0";    //Start of Frame 0
    segCodeHash[0xFFC1] = "SOF1";
    segCodeHash[0xFFC2] = "SOF2";
    segCodeHash[0xFFC3] = "SOF3";
    segCodeHash[0xFFC4] = "SOF4";
    segCodeHash[0xFFC5] = "SOF5";
    segCodeHash[0xFFC6] = "SOF6";
    segCodeHash[0xFFC7] = "SOF7";
    segCodeHash[0xFFC8] = "JPG";     //JPEG extensions
    segCodeHash[0xFFC9] = "SOF9";
    segCodeHash[0xFFCA] = "SOF10";
    segCodeHash[0xFFCB] = "SOF11";
    segCodeHash[0xFFCC] = "DAC";     // Define arithmetic coding
    segCodeHash[0xFFCD] = "SOF13";
    segCodeHash[0xFFCE] = "SOF14";
    segCodeHash[0xFFCF] = "SOF15";
    segCodeHash[0xFFD0] = "RST0";    // Restart Marker 0
    segCodeHash[0xFFD1] = "RST1";
    segCodeHash[0xFFD2] = "RST2";
    segCodeHash[0xFFD3] = "RST3";
    segCodeHash[0xFFD4] = "RST4";
    segCodeHash[0xFFD5] = "RST5";
    segCodeHash[0xFFD6] = "RST6";
    segCodeHash[0xFFD7] = "RST7";
    segCodeHash[0xFFD8] = "SOI";     // Start of Image
    segCodeHash[0xFFD9] = "EOI";     // End of Image
    segCodeHash[0xFFDA] = "SOS";     // Start of Scan
    segCodeHash[0xFFDB] = "DQT";     // Define Quantization Table
    segCodeHash[0xFFDC] = "DNL";     // Define Number of Lines
    segCodeHash[0xFFDD] = "DRI";     // Define Restart Interval
    segCodeHash[0xFFDE] = "DHP";     // Define Hierarchical Progression
    segCodeHash[0xFFDF] = "EXP";     // Expand Reference Component
    segCodeHash[0xFFE0] = "JFIF";
    // FFE1 can be EXIF or XMP.  Defined in getSegments()
    segCodeHash[0xFFE2] = "ICC";
    segCodeHash[0xFFE3] = "APP3";
    segCodeHash[0xFFE4] = "APP4";
    segCodeHash[0xFFE5] = "APP5";
    segCodeHash[0xFFE6] = "APP6";
    segCodeHash[0xFFE7] = "APP7";
    segCodeHash[0xFFE8] = "APP8";
    segCodeHash[0xFFE9] = "APP9";
    segCodeHash[0xFFEA] = "APP10";
    segCodeHash[0xFFEB] = "APP11";
    segCodeHash[0xFFEC] = "APP12";
    segCodeHash[0xFFED] = "IPTC";
    segCodeHash[0xFFEE] = "APP14";
    segCodeHash[0xFFEF] = "APP15";
    segCodeHash[0xFFF0] = "JPG0";    // JPEG Extension 0
    segCodeHash[0xFFF1] = "JPG2";
    segCodeHash[0xFFF2] = "JPG3";
    segCodeHash[0xFFF3] = "JPG4";
    segCodeHash[0xFFF4] = "JPG5";
    segCodeHash[0xFFF5] = "JPG6";
    segCodeHash[0xFFF6] = "JPG7";
    segCodeHash[0xFFF7] = "JPG8";
    segCodeHash[0xFFF8] = "JPG9";
    segCodeHash[0xFFF9] = "JPG10";
    segCodeHash[0xFFFA] = "JPG11";
    segCodeHash[0xFFFB] = "JPG12";
    segCodeHash[0xFFFC] = "JPG13";
    segCodeHash[0xFFFD] = "JPG14";
    segCodeHash[0xFFFE] = "JPG15";
}

bool Jpeg::getDimensions(QFile &file, quint32 offset, ImageMetadata &m)
{
    bool isBigEnd = true;                  // only IFD/EXIF can be little endian
    quint32 marker = 0;
    offset += 2;
    while (marker != 0xFFC0) {
        file.seek(offset);                  // APP1 FFE*
        marker = Utilities::get16(file.read(2), isBigEnd);
        if (marker < 0xFF01) {
            return false;
        }
        offset = Utilities::get16(file.peek(2), isBigEnd) + static_cast<quint32>(file.pos());
    }
    file.seek(file.pos()+3);
    m.height = Utilities::get16(file.read(2), isBigEnd);
    m.width = Utilities::get16(file.read(2), isBigEnd);
    return true;
}

bool Jpeg::parse(QFile &file,
                 quint32 startOffset,
                 ImageMetadata &m,
                 IFD *ifd,
                 IPTC *iptc,
                 Exif *exif,
                 bool report,
                 QTextStream &rpt,
                 QString &xmpString)
{
    // init
    m.iccSegmentOffset = 0;

    //file.open happens in readMetadata
    bool isBigEnd = true;

    if (Utilities::get16(file.read(2), isBigEnd) != 0xFFD8) {
        err = "JPG does not start with 0xFFD8";
        qDebug() << __FUNCTION__ << err;
        return false;
    }

    // build a hash of jpg segment offsets
    getJpgSegments(file, file.pos(), m, report, rpt);

    // check if JFIF
    if (segmentHash.contains("JFIF")) {
        // it's a jpg so the whole thing is the full length jpg and no other
        // metadata available
        m.offsetFullJPG = 0;
        m.lengthFullJPG = static_cast<uint>(file.size());
        return true;
    }

    // read the EXIF data
    if (segmentHash.contains("EXIF")) file.seek(segmentHash["EXIF"]);
    else {
        err = "JPG does not contain EXIF information";
        qDebug() << __FUNCTION__ << err;
        return false;
    }

    bool foundEndian = false;
    int count = 0;
    while (!foundEndian) {
        quint32 order = Utilities::get16(file.read(2));
        if (order == 0x4949 || order == 0x4D4D) {
            order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
            // offsets are from the endian position in JPEGs
            // therefore must adjust all offsets found in tagValue
            startOffset = static_cast<quint32>(file.pos()) - 2;
            foundEndian = true;
        }
        // add condition to check for EOF
        count++;
        if (count > 100) {
            // err endian order not found
            return false;
        }
    }

    if (report) rpt << "\n startOffset = " << startOffset;

    quint32 a = Utilities::get16(file.read(2), isBigEnd);  // magic 42
    a = Utilities::get32(file.read(4), isBigEnd);
    quint32 offsetIfd0 = a + startOffset;

    // it's a jpg so the whole thing is the full length jpg
    m.offsetFullJPG = 0;
    m.lengthFullJPG = static_cast<uint>(file.size());

    // read IFD0
    QString hdr = "IFD0";
    quint32 nextIFDOffset = ifd->readIFD(file,
                                         offsetIfd0,
                                         m,
                                         exif->hash,
                                         report, rpt, hdr) + startOffset;
    quint32 offsetEXIF;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);

    m.make = Utilities::getString(file, ifd->ifdDataHash.value(271).tagValue + startOffset,
                     ifd->ifdDataHash.value(271).tagCount);
    m.model = Utilities::getString(file, ifd->ifdDataHash.value(272).tagValue + startOffset,
                      ifd->ifdDataHash.value(272).tagCount);
    m.creator = Utilities::getString(file, ifd->ifdDataHash.value(315).tagValue + startOffset,
                        ifd->ifdDataHash.value(315).tagCount);
    m.copyright = Utilities::getString(file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                          ifd->ifdDataHash.value(33432).tagCount);

    // read IFD1
//    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);
    if (nextIFDOffset) {
        hdr = "IFD1";
        nextIFDOffset = ifd->readIFD(file,
                                     nextIFDOffset,
                                     m,
                                     exif->hash,
                                     report, rpt, hdr);
    }
    // IFD1: thumbnail offset and length
    m.offsetThumbJPG = ifd->ifdDataHash.value(513).tagValue + 12;
    m.lengthThumbJPG = ifd->ifdDataHash.value(514).tagValue;

    // read EXIF
//    readIFD("IFD Exif", offsetEXIF);
    hdr = "IFD Exif";
    ifd->readIFD(file,
                 offsetEXIF,
                 m,
                 exif->hash,
                 report, rpt, hdr);

    m.width = ifd->ifdDataHash.value(40962).tagValue;
    m.height = ifd->ifdDataHash.value(40963).tagValue;
    if (!m.width || !m.height) getDimensions(file, 0, m);

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(file, ifd->ifdDataHash.value(36868).tagValue + startOffset,
        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
    // try DateTimeOriginal
    if (createdExif.length() == 0) {
        createdExif = Utilities::getString(file, ifd->ifdDataHash.value(36867).tagValue + startOffset,
            ifd->ifdDataHash.value(36867).tagCount);
        if (createdExif.length() > 0) {
            m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
        }
//            if(!createdDate.isValid())
//                createdDate = QDateTime::fromString("2017:10:10 17:26:08", "yyyy:MM:dd hh:mm:ss");
    }

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(file,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      isBigEnd);
        if (x < 1 ) {
            int t = qRound(1 / x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = static_cast<float>(x);
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
        double x = Utilities::getReal(file,
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
        double x = Utilities::getReal(file,
                                      ifd->ifdDataHash.value(37386).tagValue + startOffset,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // EXIF: lens model
    m.lens = Utilities::getString(file, ifd->ifdDataHash.value(42036).tagValue + startOffset,
                     ifd->ifdDataHash.value(42036).tagCount);

    /* Read embedded ICC. The default color space is sRGB. If there is an embedded icc profile
    and it is sRGB then no point in saving the byte array of the profile since we already have
    it and it will take up space in the datamodel. If iccBuf is null then sRGB is assumed. */
    if (segmentHash.contains("ICC")) {
        if (m.iccSegmentOffset && m.iccSegmentLength) {
            m.iccSpace = Utilities::getString(file, m.iccSegmentOffset + 52, 4);
            if (m.iccSpace != "sRGB") {
                file.seek(m.iccSegmentOffset);
                m.iccBuf = file.read(m.iccSegmentLength);
            }
        }
    }

    // read IPTC
    if (segmentHash.contains("IPTC")) iptc->read(file, segmentHash["IPTC"], m);

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
}
    
void Jpeg::getJpgSegments(QFile &file, qint64 offset, ImageMetadata &m,
                          bool &report, QTextStream &rpt)
{
/*
The JPG file structure is based around a series of file segments.  The marker at
the start of each segment is FFC0 to FFFE (see initSegCodeHash). The next two
bytes are the incremental offset to the next segment.

This function walks through all the segments and records their global offsets in
segmentHash.  segmentHash is used by formatJPG to access the EXIF, IPTC and XMP
segments.

In addition, the XMP offset and nextOffset are set to facilitate editing XMP data.
*/
    segmentHash.clear();
    isXmp = false;
    // big endian by default in Utilities: only IFD/EXIF can be little endian
    uint marker = 0xFFFF;
    while (marker > 0xFFBF) {
        file.seek(offset);           // APP1 FFE*
        marker = static_cast<uint>(Utilities::get16(file.read(2)));
        if (marker < 0xFFC0) break;
        quint32 pos = static_cast<quint32>(file.pos());
        quint16 nex = Utilities::get16(file.read(2));
        quint32 nextOffset = pos + nex;
        if (marker == 0xFFE1) {
            QString segName = file.read(4);
            if (segName == "Exif") segmentHash["EXIF"] = static_cast<quint32>(offset);
            if (segName == "http") segName += file.read(15);
            if (segName == "http://ns.adobe.com") {
                segmentHash["XMP"] = static_cast<quint32>(offset);
                m.xmpSegmentOffset = static_cast<quint32>(offset);
                m.xmpNextSegmentOffset = static_cast<quint32>(nextOffset);
                m.isXmp = true;
            }
        }
        // icc
        else if (marker == 0xFFE2) {
            segmentHash["ICC"] = static_cast<quint32>(offset);
            m.iccSegmentOffset = static_cast<quint32>(offset) + 18;
            m.iccSegmentLength = static_cast<quint32>(nextOffset) - m.iccSegmentOffset;
            /*qDebug() << __FUNCTION__ << fPath
                     << "offset =" << offset
                     << "pos = " << pos
                     << "length =" << nex
                     << "next offset =" << nextOffset
                     << "iccSegmentOffset" << iccSegmentOffset
                     << "iccSegmentLength" << iccSegmentLength;*/
        }
        else if (segCodeHash.contains(marker)) {
            segmentHash[segCodeHash[marker]] = static_cast<quint32>(offset);
        }
        offset = nextOffset;
    }
    if (report) {
        MetaReport::header("JPG Segment Hash", rpt);
        rpt << "Segment\t\tOffset\t\tHex\n";

        QHashIterator<QString, quint32> i(segmentHash);
        while (i.hasNext()) {
            i.next();
            rpt << i.key() << ":\t\t" << i.value() << "\t\t"
                << QString::number(i.value(), 16).toUpper() << "\n";
        }
    }
}

