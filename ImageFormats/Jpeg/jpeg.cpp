#include "jpeg.h"
#include <QDebug>

Jpeg::Jpeg()
{
//    initSegCodeHash();
}

void Jpeg::initSegCodeHash()
{
    segCodeHash[0xFFC0] = "SOF0";    //Start of Frame 0
    segCodeHash[0xFFC1] = "SOF1";
    segCodeHash[0xFFC2] = "SOF2";
    segCodeHash[0xFFC3] = "SOF3";
    segCodeHash[0xFFC4] = "DHT";    // FOF4 ?
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

bool Jpeg::getWidthHeight(MetadataParameters &p, int &w, int &h)
{
    bool isBigEnd = true;                  // only IFD/EXIF can be little endian
    quint32 marker = 0;
    p.offset += 2;
    while (marker != 0xFFC0) {
        p.file.seek(p.offset);                  // APP1 FFE*
        marker = u.get16(p.file.read(2), isBigEnd);
        if (marker < 0xFF01) {
            return false;
        }
        p.offset = u.get16(p.file.peek(2), isBigEnd) + static_cast<quint32>(p.file.pos());
    }
    p.file.seek(p.file.pos()+3);
    h = u.get16(p.file.read(2), isBigEnd);
    w = u.get16(p.file.read(2), isBigEnd);
    return true;
}

bool Jpeg::getDimensions(MetadataParameters &p, ImageMetadata &m)
{
    bool isBigEnd = true;                  // only IFD/EXIF can be little endian
    quint32 marker = 0;
    p.offset += 2;
    while (marker != 0xFFC0) {
        p.file.seek(p.offset);                  // APP1 FFE*
        marker = u.get16(p.file.read(2), isBigEnd);
        if (marker < 0xFF01) {
            qWarning() << "WARNING" << "Jpeg::getDimensions"
                     << "FAIL: MARKER < 0xFFC0"
                     << "m.fPath =" << m.fPath
                        ;
            return false;
        }
        p.offset = u.get16(p.file.peek(2), isBigEnd) + static_cast<quint32>(p.file.pos());
    }
    p.file.seek(p.file.pos()+3);
    m.height = u.get16(p.file.read(2), isBigEnd);
    m.width = u.get16(p.file.read(2), isBigEnd);
    return true;
}

bool Jpeg::getDimensions2(MetadataParameters &p, ImageMetadata &m)
{
    if (!segmentHash.contains("SOF0")) return false;
    p.offset = segmentHash.value("SOF0") + 5;
    bool isBigEnd = true;                  // only IFD/EXIF can be little endian
    p.file.seek(p.offset);
    m.height = u.get16(p.file.read(2), isBigEnd);
    m.width = u.get16(p.file.read(2), isBigEnd);
    return true;
}

bool Jpeg::parse(MetadataParameters &p,
                 ImageMetadata &m,
                 IFD *ifd,
                 IPTC *iptc,
                 Exif *exif,
                 GPS *gps)
{
    // init
    m.parseSource = "Jpeg::parse";
    initSegCodeHash();
    m.iccSegmentOffset = 0;

    //file.open happens in readMetadata
    bool isBigEnd = true;

    if (u.get16(p.file.read(2), isBigEnd) != 0xFFD8) {
        G::error("JPG does not start with 0xFFD8.", "Jpeg::parse", m.fPath);
        qWarning() << "WARNING" << "Jpeg::parse FAILED JPG does not start with 0xFFD8."
                 << m.fPath
                    ;
        return false;
    }

    // build a hash of jpg segment offsets
    p.offset = static_cast<quint32>(p.file.pos());
    getJpgSegments(p, m);


    // read the EXIF data
    if (segmentHash.contains("EXIF")) p.file.seek(segmentHash["EXIF"]);
    // check if JFIF
    else if (segmentHash.contains("JFIF")) {
        // it's a jpg so the whole thing is the full length jpg and no other
        // metadata available
        m.offsetFull = 0;
        m.lengthFull = static_cast<uint>(p.file.size());
        getDimensions2(p, m);
        return true;
    }
    else {
        G::error("JPG does not contain EXIF information.", "Jpeg::parse", m.fPath);
        qDebug() << "Jpeg::parse JPG does not contain EXIF information."
                 << m.fPath
                    ;
        return false;
    }

    quint32 startOffset;
    bool foundEndian = false;
    int count = 0;
    while (!foundEndian) {
        quint32 order = u.get16(p.file.read(2));
        if (order == 0x4949 || order == 0x4D4D) {
            order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
            // offsets are from the start of endian position in JPEGs
            // therefore must adjust all offsets found in tagValue
            startOffset = static_cast<quint32>(p.file.pos()) - 2;
            foundEndian = true;
            /*
            qDebug() << "Jpeg::parse" << p.file.fileName()
                     << "isBigEnd =" << isBigEnd
                     << "startOffset =" << startOffset;
            //*/
        }
        // add condition to check for EOF
        count++;
        if (count > 100) {
            // err endian order not found
            G::error("Endian order not found.", "Jpeg::parse", m.fPath);
            return false;
        }
    }

    if (p.report) p.rpt << "\nstartOffset = " << startOffset;

    quint32 a = u.get16(p.file.read(2), isBigEnd);  // magic 42
    a = u.get32(p.file.read(4), isBigEnd);
    quint32 offsetIfd0 = a + startOffset;

    // it's a jpg so the whole thing is the full length jpg
    m.offsetFull = 0;
    m.lengthFull = static_cast<uint>(p.file.size());
    // default values for thumbnail
    m.offsetThumb = m.offsetFull;
    m.lengthThumb =  m.lengthFull;

    // read IFD0
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p, isBigEnd);
    if (nextIFDOffset) nextIFDOffset += startOffset;

    // IFD0 Offsets
    quint32 offsetEXIF = 0;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;

    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue + startOffset;

    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
                     ifd->ifdDataHash.value(271).tagCount);
    m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue + startOffset,
                      ifd->ifdDataHash.value(272).tagCount);
    m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
                        ifd->ifdDataHash.value(315).tagCount);
    m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                          ifd->ifdDataHash.value(33432).tagCount);

    // read IFD1
    ifd->ifdDataHash.clear();
    if (nextIFDOffset) {
        p.hdr = "IFD1";
        p.offset = nextIFDOffset;
        nextIFDOffset = ifd->readIFD(p, isBigEnd);
    }
    // IFD1: thumbnail offset and length
    if (ifd->ifdDataHash.contains(513)) {
        m.offsetThumb = ifd->ifdDataHash.value(513).tagValue + startOffset/*12*/;
        m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;
    }
    m.isEmbeddedThumbMissing = (m.offsetThumb == 0);

    // read EXIF
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p, isBigEnd);

    m.width = static_cast<int>(ifd->ifdDataHash.value(40962).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(40963).tagValue);
    p.offset = 0;
    if (!m.width || !m.height) getDimensions2(p, m);
    m.widthPreview = m.width;
    m.heightPreview = m.height;

    // EXIF: created datetime
    QString createdExif;
    // try DateTimeOriginal
    createdExif = u.getString(p.file, ifd->ifdDataHash.value(36867).tagValue + startOffset,
                              ifd->ifdDataHash.value(36867).tagCount);
    if (createdExif.length() > 0)
        m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
    // try CreateData
    else {
        createdExif = u.getString(p.file, ifd->ifdDataHash.value(36868).tagValue + startOffset,
                                  ifd->ifdDataHash.value(36868).tagCount).left(19);
        if (createdExif.length() > 0)
            m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
    }

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      isBigEnd);
        if (x < 1) {
            double recip = static_cast<double>(1 / x);
            if (recip >= 2) m.exposureTime = "1/" + QString::number(qRound(recip));
            else m.exposureTime = "1/" + QString::number(recip, 'g', 2);
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

    // EXIF: aperture
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
        double x = u.getReal_s(p.file,
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

    /* Read embedded ICC. The default color space is sRGB. If there is an embedded icc profile
    and it is sRGB then no point in saving the byte array of the profile since we already have
    it and it will take up space in the datamodel. If iccBuf is null then sRGB is assumed. */
    if (segmentHash.contains("ICC")) {
        if (m.iccSegmentOffset && m.iccSegmentLength) {
            m.iccSpace = u.getString(p.file, m.iccSegmentOffset + 52, 4);
            if (m.iccSpace != "sRGB") {
                p.file.seek(m.iccSegmentOffset);
                m.iccBuf = p.file.read(m.iccSegmentLength);
            }
        }
    }

    // read GPS
    if (offsetGPS) {
        p.hdr = "IFD GPS";
        p.offset = offsetGPS;
        p.hash = &gps->hash;
        ifd->readIFD(p, isBigEnd);

        if (ifd->ifdDataHash.contains(1)) {  // 1 = GPSLatitudeRef
            // process GPS info
            QString gpsCoord = gps->decode(p.file, ifd->ifdDataHash, isBigEnd, 12);
            m.gpsCoord = gpsCoord;
        }
    }

    // read IPTC
    if (segmentHash.contains("IPTC")) iptc->read(p.file, segmentHash["IPTC"], m);

    // read XMP
    bool okToReadXmp = true;
    if (m.isXmp && okToReadXmp && !G::stop) {
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength, p.instance);
        if (xmp.isValid) {
            m.rating = xmp.getItem("Rating");     // case is important "Rating"
            m.label = xmp.getItem("Label");       // case is important "Label"
            m.title = xmp.getItem("title");       // case is important "title"
            m.cameraSN = xmp.getItem("SerialNumber");
            if (m.lens.isEmpty()) m.lens = xmp.getItem("Lens");
            m.lensSN = xmp.getItem("LensSerialNumber");
            if (m.creator.isEmpty()) m.creator = xmp.getItem("creator");
            m.copyright = xmp.getItem("rights");
            m.email = xmp.getItem("email");
            m.url = xmp.getItem("url");
            m.keywords  = xmp.getItemList("subject");
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
    
void Jpeg::getJpgSegments(MetadataParameters &p, ImageMetadata &m)
{
/*
    The JPG file structure is based around a series of file segments.  The marker at
    the start of each segment is FFC0 to FFFE (see initSegCodeHash). The next two
    bytes are the incremental offset to the next segment.

    This function walks through all the segments and records their global offsets in
    segmentHash.  segmentHash is used by parseJPG to access the EXIF, IPTC, ICC and XMP
    segments.

    In addition, the XMP offset and nextOffset are set to facilitate editing XMP data.
*/
    /*
    qDebug().noquote()
            << "Jpeg::getJpgSegments"
            << "fPath =" << m.fPath
            ; //*/
    segmentHash.clear();
    isXmp = false;
    // big endian by default in Utilities: only IFD/EXIF can be little endian
    uint marker = 0xFFFF;
    while (marker > 0xFFBF) {
        p.file.seek(p.offset);           // APP1 FFE*
        marker = static_cast<uint>(u.get16(p.file.read(2)));
        if (marker < 0xFFC0) break;
        quint32 pos = static_cast<quint32>(p.file.pos());
        quint16 len = u.get16(p.file.read(2));
        quint32 nextOffset = pos + len;

        /*
        qDebug().noquote()
            << "Jpeg::getJpgSegments"
            << "marker =" << QString::number(marker, 16).toUpper().leftJustified(8)
            << "pos =" << QString::number(pos).leftJustified(8)
            << "len =" << QString::number(len).leftJustified(8)
            << "nextOffset =" << QString::number(nextOffset).leftJustified(8)
                ; //*/

        // populate segmentCodeHash
        if (segCodeHash.contains(marker) && marker != 0xFFE1) {
            segmentHash[segCodeHash[marker]] = static_cast<quint32>(p.offset);
        }

        switch (marker) {
        case 0xFFD8:      // SOI = Start Of Image
            break;
        case 0xFFD9:      // EOI = End Of Image
            break;
        // Start of Frame
        case 0xFFC0:      // SOF0
        case 0xFFC1:
        case 0xFFC2:
        case 0xFFC3:
        case 0xFFC5:
        case 0xFFC6:
        case 0xFFC7:
        case 0xFFC8:
        case 0xFFC9:
        case 0xFFCA:
        case 0xFFCB:
        case 0xFFCC:
        case 0xFFCE:
        // Start of scan segment
        case 0xFFCF:
            parseFrameHeader(p, marker, len);
            break;
        // Huffman table segment
        case 0xFFC4:
            parseHuffmanTable(p, len);
            break;
        // Start of scan segment
        case 0xFFDA:
            parseSOSHeader(p, len);
            break;
        // Quantization table segment
        case 0xFFDB:
            parseQuantizationTable(p, len);
            break;
        // Define restart interval
        case 0xFFDD:
            restartInterval = u.get16(p.file.read(2));
            break;
        case 0xFFE1: {
            QString segName = p.file.read(4);
            if (segName == "Exif") segmentHash["EXIF"] = static_cast<quint32>(p.offset);
            if (segName == "http") segName += p.file.read(15);
            if (segName == "http://ns.adobe.com") {
                segmentHash["XMP"] = static_cast<quint32>(p.offset);
                m.xmpSegmentOffset = static_cast<quint32>(p.offset);
                m.xmpSegmentLength = static_cast<quint32>(nextOffset - m.xmpSegmentOffset);
                m.isXmp = true;
            }
            break;
        }
        case 0xFFE2: {
            segmentHash["ICC"] = static_cast<quint32>(p.offset);
            m.iccSegmentOffset = static_cast<quint32>(p.offset) + 18;
            m.iccSegmentLength = static_cast<quint32>(nextOffset) - m.iccSegmentOffset;
            break;
        }
        case 0xFFE0:
        case 0xFFE3:
        case 0xFFE4:
        case 0xFFE5:
        case 0xFFE6:
        case 0xFFE7:
        case 0xFFE8:
        case 0xFFE9:
        case 0xFFEA:
        case 0xFFEB:
        case 0xFFEC:
        case 0xFFED:
        case 0xFFEE:
        case 0xFFEF:
            readAppSegments(p);
            break;
        default: {
        }

        } // end switch

        /*
        if (marker == 0xFFE1) {
            QString segName = p.file.read(4);
            if (segName == "Exif") segmentHash["EXIF"] = static_cast<quint32>(p.offset);
            if (segName == "http") segName += p.file.read(15);
            if (segName == "http://ns.adobe.com") {
                segmentHash["XMP"] = static_cast<quint32>(p.offset);
                m.xmpSegmentOffset = static_cast<quint32>(p.offset);
                m.xmpNextSegmentOffset = static_cast<quint32>(nextOffset);
                m.isXmp = true;
            }
        }
        // icc
        else if (marker == 0xFFE2) {
            segmentHash["ICC"] = static_cast<quint32>(p.offset);
            m.iccSegmentOffset = static_cast<quint32>(p.offset) + 18;
            m.iccSegmentLength = static_cast<quint32>(nextOffset) - m.iccSegmentOffset;
        }
        else if (segCodeHash.contains(marker)) {
            segmentHash[segCodeHash[marker]] = static_cast<quint32>(p.offset);
        }*/

        p.offset = nextOffset;
    }
    if (p.report) {
        MetaReport::header("JPG Segment Hash", p.rpt);
        p.rpt.setFieldWidth(12);
        p.rpt.setFieldAlignment(QTextStream::AlignLeft);
        p.rpt << "Segment";
        p.rpt.setFieldAlignment(QTextStream::AlignRight);
        p.rpt << "Offset"<< "Hex" << "\n";

        QHashIterator<QString, quint32> i(segmentHash);
        while (i.hasNext()) {
            i.next();
            p.rpt.setFieldAlignment(QTextStream::AlignLeft);
            p.rpt << i.key();
            p.rpt.setFieldAlignment(QTextStream::AlignRight);
            p.rpt << QString::number(i.value());
            p.rpt << QString::number(i.value(), 16).toUpper() << "\n";
        }
        p.rpt.setFieldAlignment(QTextStream::AlignLeft);
    }
}

void Jpeg::readAppSegments(MetadataParameters &p)
{
    int len;
    if (segmentHash.contains("APP14")) {
        p.file.seek(segmentHash["APP14"] + 2);
        len = u.get16(p.file.read(2));
        if (len > 6) {
            p.file.seek(p.file.pos() + len - 4);
            colorModel =  u.get16(p.file.read(2));
        }
    }
}

void Jpeg::parseFrameHeader(MetadataParameters &p, uint marker, quint16 len)
{
    losslessFormat = false;
    switch (marker) {
    case 0xFFC0:      // SOF0
        subFormat = SubFormat::Baseline_DCT;
        break;
    case 0xFFC1:
        subFormat = SubFormat::Extended_DCT;
        break;
    case 0xFFC2:
        subFormat = SubFormat::Progressive_DCT;
        break;
    case 0xFFC3:
        subFormat = SubFormat::Lossless;
        losslessFormat = true;
        break;
    case 0xFFC9:
        subFormat = SubFormat::Arithmetric_DCT;
        break;
    case 0xFFCA:
        subFormat = SubFormat::ArithmetricProgressive_DCT;
        break;
    case 0xFFCB:
        subFormat = SubFormat::ArithmetricLossless;
        losslessFormat = true;
        break;
    default:
        subFormat = SubFormat::UnknownJPEGSubformat;
    } // end switch

    // get frame header length
    precision = u.get8(p.file.read(1));
    lines = u.get16(p.file.read(2));
    iHeight = lines;
    mcuRows = (lines / 8) + (lines % 8);
    samplesPerLine = u.get16(p.file.read(2));
    iWidth = samplesPerLine;
    mcuCols = (samplesPerLine / 8) + (samplesPerLine % 8);
    componentsInFrame = u.get8(p.file.read(1));
    components.resize(componentsInFrame);
    Component component;
    if (componentsInFrame) {
        for (int i = 0; i < componentsInFrame; i++) {
            component.Id = u.get8(p.file.read(1));
            QByteArray c = p.file.read(1);
            component.horSampleFactor = u.get4_1st(c);
            component.verSampleFactor = u.get4_2nd(c);
            component.QTableSel = u.get8(p.file.read(1));
            components[i] = component;
         }
    }
/*    qDebug() << "\n" << "Jpeg::parseFrameHeader" << "Marker"
             << "0x" + QString::number(marker, 16).toUpper().rightJustified(4, '0');
    qDebug() << "Jpeg::parseFrameHeader"
             << "marker" << QString::number(marker, 16)
             << "segment" << segCodeHash[marker]
             << "length" << len
             << "precision" << precision
             << "lines" << lines
             << "samplesPerLine" << samplesPerLine
             << "mcuCols =" << mcuCols << "mcuRows =" << mcuRows
             << "componentsInFrame" << componentsInFrame;
    for (int i = 0; i < componentsInFrame; i++) {
        qDebug() << "Jpeg::parseFrameHeader"
                 << componentDescription[i]
                 << i
                 << "Id" << components.at(i).Id
                 << "horSampleFactor" << components.at(i).horSampleFactor
                 << "verSampleFactor" << components.at(i).verSampleFactor
                 << "QTableSel" << components.at(i).QTableSel;
    }*/
}

void Jpeg::parseHuffmanTable(MetadataParameters &p, quint16 len)
{
    quint32 pos = static_cast<quint32>(p.file.pos());
    quint32 endOffset = pos + len - 2;

//    qDebug() << "\n";
//    qDebug() << "Jpeg::parseHuffmanTable" << "DEBUG HUFFMAN DECODING";
    while (p.file.pos() < endOffset) {
        DHT dht;
        QMap<uint, uint> dhtCodeMap;  // code value, value width
        QMap<uint, QMap<uint, uint>> dhtLengthMap;
        int dhtType;
//        dht.codeLengths.resize(16);
        QByteArray c = p.file.read(1);
        dhtType = u.get8(c);
        dht.classID = u.get4_1st(c);
        dht.tableID = u.get4_2nd(c);

        // get count of codes with bit length i
        QVector<int> counts;
        counts.resize(16);
        quint16 huffCode = 0;
        // get count of codes for each bit length i
        for (int i = 0; i < 16; i++) {
            counts[i] = u.get8(p.file.read(1));
//            qDebug() << "Table =" << dhtType << "Length =" << i << "Count =" << counts[i];
        }
        /*
         qDebug() << "Jpeg::parseHuffmanTable"
         << "  Table type =" << QString::number(dhtType, 16).rightJustified(2)
         << "Class =" << QString::number(dht.classID).rightJustified(2)
         << "TableID =" << QString::number(dht.tableID).rightJustified(2)
         << "counts =" << counts;*/
        // iterate bit widths
        for (int i = 0; i < 16; i++) {
            if (counts.at(i) == 0) continue;
            dhtCodeMap.clear();
            // iterate huffman codes for bit width
            for  (int j = 0; j < counts[i]; j++) {
                dhtCodeMap[huffCode] = u.get8(p.file.read(1));

                /*qDebug() << "Jpeg::parseHuffmanTable"
                         << "HuffBitLength" << QString::number(i+1).rightJustified(2)
                         << "HuffCode =" << QString::number(huffVal).rightJustified(5)
                         << QString::number(huffVal, 2).rightJustified(16)
                         << "HuffValLength ="
                         << QString::number(dhtCodeMap[huffVal]).rightJustified(4);*/

                huffCode++;
            }
            ulong len = static_cast<ulong>(i + 1);
            if (counts[i]) dhtLengthMap[len] = dhtCodeMap;
            huffCode <<= 1;
        }
        // Value = dhtMap[dhtType][length][code]
        dhtMap[dhtType] = dhtLengthMap;

    }

    return;
}

void Jpeg::parseQuantizationTable(MetadataParameters &p, quint16 len)
{
    quint32 pos = static_cast<quint32>(p.file.pos());
    quint32 endOffset = pos + len - 2;

    while (p.file.pos() < endOffset) {
        QByteArray c = p.file.read(1);
//        quint8 table = u.get8(c);
        int precision = u.get4_1st(c);
        int table = u.get4_2nd(c);
        QVector<int> q(64);
        if (precision == 0) {
            for (int i = 0; i < 64; i++) q[i] = u.get8(p.file.read(1));
        }
        else {
            for (int i = 0; i < 64; i++) q[i] = u.get16(p.file.read(2));
        }
        dqt[table] = q;
    }

    // report
//    qDebug() << "\n" << "Jpeg::parseQuantizationTable";
    QMapIterator<int, QVector<int>> table(dqt);
    while (table.hasNext()) {
        table.next();
//        qDebug() << "Jpeg::parseQuantizationTable" << "Quantization Table" << table.key()
//                 << dqtDescription[table.key()];
        int i = 0;
        for (int r = 0; r < 8; r++) {
            QString rStr = "";
            for (int c = 0; c < 8; c++) {
                rStr += QString::number(dqt[table.key()][i]).rightJustified(4);
                i++;
            }
//            qDebug() << "Jpeg::parseQuantizationTable" << "  "
//                     << "Row" << r << " " << rStr;
        }
    }

}

void Jpeg::parseSOSHeader(MetadataParameters &p, quint16 len)
{
    scanDataOffset = p.file.pos() + len - 2;
    sosComponentCount = u.get8(p.file.read(1));
    for (int i = 0; i < sosComponentCount; i++) {
        int componentID = u.get8(p.file.read(1));
        huffTblToUse[componentID] = u.get8(p.file.read(1));
    }

    // report
//    qDebug() << "\n" << "Jpeg::parseSOSHeader"
//             << "Scan data offset =" << scanDataOffset
//             << "Number of components =" << sosComponentCount;
//    for (int i = 0; i < sosComponentCount; i++) {
//        qDebug() << "Jpeg::parseSOSHeader" << "  "
//        << "ComponentID =" << i
//        << componentDescription[i]
//        << "Huffman table ID =" << huffTblToUse[i];
//    }

}

int Jpeg::huff2Signed(uint val, uint bits)
{
    if (val >= static_cast<uint>(1 << (bits - 1))) {
        return static_cast<int>(val);
    }
    else {
        return static_cast<int>(val - ((1 << bits) - 1) );
    }
}

void Jpeg::decodeScan(MetadataParameters &p)
{
    p.file.seek(0);
    QByteArray ba = p.file.readAll();
//    decodeScan(p, ba);
}

void Jpeg::decodeScan(QFile &file, QImage &image)
{
//    qDebug() << "Jpeg::decodeScan" << file.fileName();
    if (!file.exists()) return;                 // guard for usb drive ejection

    MetadataParameters p;
    p.file.setFileName(file.fileName());
    if (p.file.isOpen()) return;    //rgh_isOpenCheck
//    if (p.file.isOpen()) p.file.close();
//    qDebug() << "Jpeg::decodeScan" << "Close" << p.file.fileName();
    if (p.file.open(QIODevice::ReadOnly)) {
//        qDebug() << "Jpeg::decodeScan" << "Open " << p.file.fileName();
        bool isBigEnd = true;
        if (u.get16(p.file.read(2), isBigEnd) != 0xFFD8) {
            G::error("JPG does not start with 0xFFD8.", "Jpeg::decodeScan", file.fileName());
            p.file.close();
//            qDebug() << "Jpeg::decodeScan" << "Close" << p.file.fileName();
            return;
        }
        p.offset = static_cast<quint32>(p.file.pos());
        ImageMetadata m;
        getJpgSegments(p, m);

        p.file.seek(0);
        QByteArray ba = p.file.readAll();
        p.file.close();
//        qDebug() << "Jpeg::decodeScan" << "Close" << p.file.fileName();
        decodeScan(ba, image);
    }
}

void Jpeg::decodeScan(QByteArray &ba, QImage &image)
{
/*  Decoding steps:

    - Decode huffman encoding for each component of each MCU
    - Transform to zigzag order
    - Account for ZRL markers
    - Convert to signed twos complement
    - Multiply by quantization table
    - Invert Discrete Cosine Transform
    - Level shift
    - Convert to RGB

    */
    qDebug() << "Jpeg::decodeScan";

    QBuffer buffer(&ba);
    if (buffer.isOpen()) return;  //rgh_isOpenCheck
    buffer.open(QIODevice::ReadOnly);

    // rgh can we only build once?
    buildMask();
    buildIdctLookup();

//    MetadataParameters p;
//    ImageMetadata m;
//    getJpgSegments(p, m);
    //    rptHuff();

    QImage im(iWidth, iHeight, QImage::Format_ARGB32);

    // bit buffer
    uint buf = 0;

    // end of scan = 0xFFD9
    bool eos = false;

    // buffer is empty
    int consumed = 32;
    // the number of bits from the start of the bit buffer to the reset marker, which is on
    // a byte boundary, with filler "111..." bits to the byte boundary
    int bitsToResetMarker = 0;

    // For reporting, the incoming bytes to the bit buffer
    QString sBytes;

    // Set up for image size
    mcuRows = lines / 8;
    mcuCols = samplesPerLine / 8;


    // for testing only
//    mcuRows = 4;
//    mcuCols = 150;
    bool isReport = false;
    int reportMCU0 = -1;   // row, column
    int reportMCU1 = -1;   // row, column
//    QImage image(samplesPerLine, lines, QImage::Format_RGB32);

    if (isReport) {
        qDebug() << "START OF SCAN   Offset =" << scanDataOffset;
        qDebug();
    }

    // start of scanned data
    buffer.seek(scanDataOffset);
    quint32 offset = scanDataOffset;

    G::log("Jpeg::decodeScan", "Starting to decode scan data");

    // Iterate through scan data MCU blocks row by row
    int mcuCount = 0;
    for (int mcuRow = 0; mcuRow < mcuRows; ++mcuRow) {

        // dcDiff initialize
        for (int i = 0; i < 3; i++) dcDiff[i] = 0;

        for (int mcuCol = 0; mcuCol < mcuCols; ++mcuCol) {

            // decode MCU, first initialize to zero
            for (int c = 0; c != 3; ++c) {
                for (int v = 0; v != 8; ++v) {
                    for (int u = 0; u != 8; ++u) {
                        mcu[c][v][u] = 0;
                    }
                }
            }

            // Luminance Y, Cb, Cr
            for (int c = 0; c != 3; ++c) {
                int qTbl;   // quantization table
                c == 0 ? qTbl = 0 : qTbl = 1;

                // debug reporting
                if (mcuCount >= reportMCU0 && mcuCount <= reportMCU1 && isReport) {
                    qDebug();
                    qDebug() << "MCU" << mcuCount << "COMPONENT" << componentDescription[c];
                }

                // each entry in MCU
                for (int m = 0; m < 64; ++m) {

                    // backfill buffer while there is room
                    sBytes = "";
                    while (consumed > 7 && !eos) {
                        // load another byte
                        if (!eos) {
//                            quint8 nextByte = u.get8(buffer.read(1));
//                            quint8 nextByte = buffer.read(1)[0]&0xFF;
                            quint8 nextByte = ba[offset++]&0xFF;
                            sBytes += QString::number(nextByte, 2).rightJustified(8, '0') + " ";
                            /*qDebug() << "nextByte =" << QString::number(nextByte, 16).toUpper().rightJustified(0, '0')
                                     << "offset =" << buffer.pos()
                                     << "consumed =" << consumed;*/
                            bool isMarkerByte = false;
                            if (nextByte == 0xFF) {
//                                uint markerByte = u.get8(buffer.read(1));
                                uint markerByte = ba[offset++]&0xFF;
                                if (markerByte != 0) {
                                    isMarkerByte = true;
                                    // adjust bit buf to byte boundary
                                    bitsToResetMarker = 32 - consumed;
                                    if (isReport) qDebug().noquote()
                                        << QString::number(buf, 2).rightJustified(32, '0')
                                        << "Reset Marker byte = " << QString::number(markerByte, 16)
                                        << "mcuRow =" << mcuRow << "mcuCol =" << mcuCol
                                        << "bitsToMarker =" << bitsToResetMarker;
                                    if (markerByte == 0xD9) {
                                        // End of scan
                                        eos = true;
                                    }
                                }
                            }
                            if (!eos && !isMarkerByte) {
                                bufAppend(buf, nextByte, consumed);
                                /*qDebug() << "Jpeg::decodeScan"
                                         << "Appended nextByte =" << QString::number(nextByte, 16)
                                         << "Consumed =" << consumed;*/
                            }
                        }
                    }
                    /*
                    qDebug() << "Jpeg::decodeScan"
                             << "initial buf ="
                             << QString::number(buf, 16)
                             << QString::number(buf, 2);*/

                    // which huffTable to use?'
                    int hTbl = 0;
                    if      (c == 0 && m == 0) hTbl = 0x00;      // DC component of Luminance (Y)
                    else if (c == 0 && m != 0) hTbl = 0x10;      // AC component of Luminance (Y)
                    else if (c != 0 && m == 0) hTbl = 0x01;      // DC component of Chrominance (Cb & Cr)
                    else if (c != 0 && m != 0) hTbl = 0x11;      // AC component of Chrominance (Cb & Cr)

                    // iterate bit buffer until huffman code found
                    bool huffFound = false;
                    bool endOfBlock = false;
                    for (uint huffLength = 1; huffLength < 17; huffLength++) {
                        if (dhtMap[hTbl][huffLength].count()) {
                            uint huffCode = bufPeek(buf, huffLength);
                            if (dhtMap[hTbl][huffLength].contains(huffCode)) {
                                QString binCode = QString::number(huffCode, 2).rightJustified(huffLength, '0');

                                // check if first huffResult (dhtMap[tbl][huffLength][code]) = zero
                                // if so, all AC for component are zero, so break out of MCU component loop

                                uint huffVal;
                                QString bufSnapShot = QString::number(buf, 2).rightJustified(32, '0');

                                /* Extract huffCode and huffResult
                                   huffCode   = binary string                   ie 11001 = 25
                                   huffLength = bit length of huffCodecode      ie 11001 length = 5
                                   huffVal    = lookup table value for huffCode (usually used to define next #bits)
                                   huffResult = value of next huffVal bits
                                   huffSignedResult = Twos complement of huffResult
                                */

                                // extract huffCode from buffer so can access next bits = result
                                bufExtract(buf, huffLength, consumed);
                                int huffNextBits = dhtMap[hTbl][huffLength][huffCode];
                                huffVal = dhtMap[hTbl][huffLength][huffCode] & 0xF;
                                int huffRepeat = dhtMap[hTbl][huffLength][huffCode] / 16;
                                uint huffResult = bufExtract(buf, huffVal, consumed);
                                QString binHuffResult = QString::number(huffResult, 2).rightJustified(huffVal, '0');
                                int huffSignedResult = huff2Signed(huffResult, huffVal);

                                // reset marker ahead?  Remove 1 bit place markers to byte boundary
                                if (bitsToResetMarker) {
                                    bitsToResetMarker -= huffLength + huffVal;
                                    if (bitsToResetMarker < 8) {
                                        // check if next bitsToMarker are all ones ie 1111...
                                        uint markerBuf = buf;
                                        markerBuf = markerBuf >> (32 - bitsToResetMarker);
                                        bool alignToByteMarker = false;
                                        switch (bitsToResetMarker) {
                                        case 1:
                                            if (markerBuf == 0b1)  alignToByteMarker = true;
                                            break;
                                        case 2:
                                            if (markerBuf == 0b11)  alignToByteMarker = true;
                                            break;
                                        case 3:
                                            if (markerBuf == 0b111)  alignToByteMarker = true;
                                            break;
                                        case 4:
                                            if (markerBuf == 0b1111)  alignToByteMarker = true;
                                            break;
                                        case 5:
                                            if (markerBuf == 0b11111)  alignToByteMarker = true;
                                            break;
                                        case 6:
                                            if (markerBuf == 0b111111)  alignToByteMarker = true;
                                            break;
                                        case 7:
                                            if (markerBuf == 0b1111111)  alignToByteMarker = true;
                                        }
                                        if (alignToByteMarker) {
                                            buf = buf << bitsToResetMarker;
                                            consumed += bitsToResetMarker;
                                            /*
                                            qDebug() << "Encountered reset marker.  "
                                                     << "bitsToMarker =" << bitsToMarker
                                                     << "buf:"
                                                     << QString::number(huffCode, 2).rightJustified(huffLength, '0');
                                                     */
                                            bitsToResetMarker = 0;
                                        }
                                    }
                                }

                                // Repeats
                                if (huffRepeat) {
                                    for (int i = 0; i != huffRepeat; ++i) {
                                        int y = zz[m][0];
                                        int x = zz[m][1];
                                        mcu[c][y][x] = 0;
                                        m++;
                                    }
                                }
                                if (m > 63) break;  // rgh why needed??
                                int y = zz[m][0];
                                int x = zz[m][1];
                                mcu[c][y][x] = huffSignedResult * dqt[qTbl][m];

                                // if huffVal == 0 (the bit length to read to get value) then EOB
                                QString sEOB = "";
                                if (huffNextBits == 0 && m > 0) {
                                    endOfBlock = true;
                                    sEOB = "EOB";
                                    // make sure rest of mcu block = zero
                                    for (int i = m+1; i < 64; ++i) {
                                        int y = zz[m][0];
                                        int x = zz[m][1];
                                        mcu[c][y][x] = 0;
                                    }
                                }

                                // Report MCU coefficient
                                if (mcuCount >= reportMCU0 && mcuCount <= reportMCU1 && isReport) {
                                    int prevConsumed = consumed - huffLength - huffVal;
                                    QString sHuffCode = bufSnapShot.left(huffLength) + " ";
                                    QString sHuffResult = bufSnapShot.mid(huffLength, huffVal) + " ";
                                    QString sBufRemainder = bufSnapShot.mid(huffLength + huffVal, 32 - prevConsumed - huffLength - huffVal);
                                    QString sBufConsumed;
                                    sBufConsumed.fill('_', prevConsumed);
                                    QString sBuf = sHuffCode + sHuffResult + sBufRemainder + sBufConsumed;
                                    QString sRpt;
                                    huffRepeat == 0 ? sRpt = QString::number(huffRepeat).leftJustified(2) : "--";
                                    QString sBitsToAlign;
                                    if (bitsToResetMarker) sBitsToAlign = "bitsToReset =" + QString::number(bitsToResetMarker);
                                    QString szz =  QString::number(zz[m][0]) + "," + QString::number(zz[m][1]);
                                    qDebug().noquote()
                                     << "c =" << c
                                     << "m =" << QString::number(m).rightJustified(2)
                                     << "zz =" << szz.leftJustified(2)
                                     << "table =" << QString::number(hTbl, 16).leftJustified(2)
                                     << "consumed =" << QString::number(consumed).leftJustified(2)
                                     << "huffLength =" << QString::number(huffLength).leftJustified(2)
                                     << "huffCode =" << QString::number(huffCode).leftJustified(5)
                                     << binCode.leftJustified(12)
                                     << "huffVal =" << QString::number(dhtMap[hTbl][huffLength][huffCode]).leftJustified(3)
                                     << "repeat =" << QString::number(huffRepeat).leftJustified(2)
                                     << "bits =" << binHuffResult.leftJustified(10)
                                     << "huffResult =" << QString::number(huffResult).leftJustified(4)
                                     << "huffSignedResult =" << QString::number(huffSignedResult).leftJustified(4)
                                     << "Q =" << QString::number(dqt[qTbl][m]).leftJustified(2)
                                     << "DCT =" << QString::number(dqt[qTbl][m] * huffResult).leftJustified(4)
                                     << sBuf
                                     << sBytes
                                     << sBitsToAlign
                                     << sEOB;
                                }

                                huffFound = true;
                                break;
                            }
                        }
                    }
                    if (endOfBlock || m > 63) break;
                    if (!huffFound) {
                        // err
                        if (isReport) qDebug()
                            << "Jpeg::decodeScan" << "HUFF CODE NOT FOUND"
                            << "buf =" << QString::number(buf, 2).rightJustified(32, '0')
                            << "mcuRow =" << mcuRow << "mcuCol =" << mcuCol
                            << "c =" << c << "m =" << m;
                    }
                } // end mcu component (Y,Cb,Cr)

//                qDebug() << "Processed MCU" << mcuCol << mcuRow << "c =" << c;

            } // end components and MCU

//            if (mcuCount == 10) G::log("Jpeg::decodeScan", "Loaded DCU");

//            if (mcuCount >= reportMCU0 && mcuCount <= reportMCU1) {
//                rptMCU(mcuCol, mcuRow);
//            }

            // DC records difference from previous MCU
            for (int c = 0; c != 3; ++c) {
                mcu[c][0][0] += dcDiff[c];
                dcDiff[c] = mcu[c][0][0];
            }

//            // IDCT transform and level shift
            for (int c = 0; c != 3; ++c) {
                for (uint y = 0; y != 8; ++y) {
                    for (uint x = 0; x != 8; ++x) {
                        int sum = 0;
                        for (uint v = 0; v != 8; ++v) {
                            for (uint u = 0; u != 8; ++u) {
                                sum += iIdctLookup[c][y][x][v][u] * mcu[c][v][u];
                            }
                        }
                        // All coefficients are multiplied by 1024 since they are int
                        sum = sum >> 10;
                        idct[c][y][x] = sum + 128.0;        // rgh also precision 12 then add 2048 instead of 128
                    }
                }
            }
            /*  Another version of IDCT transform and level shift
            float pi = static_cast<float>(3.141592654);
            float pi16 = static_cast<float>(3.141592654/16.0);
            float sqrtHalf	= static_cast<float>(0.707106781);
            for (int c = 0; c != 3; ++c) {
                for (uint y = 0; y != 8; ++y) {
                    for (uint x = 0; x != 8; ++x) {
                        double sum = 0;
                        for (uint v = 0; v != 8; ++v) {
                            for (uint u = 0; u != 8; ++u) {
                                sum += fIdctLookup[c][y][x][v][u] * mcu[c][v][u];
                                float cu = (u == 0) ? sqrtHalf : 1;
                                float cv = (v == 0) ? sqrtHalf : 1;
                                float cosProd = std::cos((2*x+1)*u*pi/16.0) * std::cos((2*y+1)*v*pi/16.0);
                                sum += cu * cv * cosProd * mcu[c][v][u];
                            }
                        }
                        idct[c][y][x] = sum * 0.25 + 128.0;
                    }
                }
            }*/
//            if (mcuCount == 10) G::log("Jpeg::decodeScan", "IDCT transform and level shift");

            // RGB transform
            for (int y = 0; y != 8; ++y) {
                for (int x = 0; x != 8; ++x) {
                    double Y =  idct[0][y][x];
                    double Cb = idct[1][y][x];
                    double Cr = idct[2][y][x];

                    int r = static_cast<int>(floor(Y + 1.402 * (Cr - 128.0)));
                    int g = static_cast<int>(floor(Y - 0.34414 * (Cb - 128.0) - 0.71414 * (Cr - 128)));
                    int b = static_cast<int>(floor(Y + 1.772 * (Cb - 128.0)));

                    r = std::max(0, std::min(r, 255));
                    g = std::max(0, std::min(g, 255));
                    b = std::max(0, std::min(b, 255));

                    rgb[0][y][x] = static_cast<uint>(r);
                    rgb[1][y][x] = static_cast<uint>(g);
                    rgb[2][y][x] = static_cast<uint>(b);
                }
            }

//            if (mcuCount == 10) G::log("Jpeg::decodeScan", "RGB transform");

            /* testing: add mcu rgb to QImage image using setPixel
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    int X = mcuCol * 8 + x;
                    int Y = mcuRow * 8 + y;
                    QRgb px = QColor(rgb[0][y][x], rgb[1][y][x], rgb[2][y][x]).rgb();
                    if (X > iWidth || Y > iHeight) {
                        if (isReport) qDebug() << "Problem: "
                            << "image width =" << iWidth
                            << "height = " << iHeight
                            << "mcuCol =" << mcuCol
                            << "mcuRow =" << mcuRow
                            << "px =" << X
                            << "py =" << Y;
                    }
                    im.setPixel(X, Y, px);
                }
            }
//            */

            // testing: add mcu rgb to QImage image using memcpy scalLone
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    uint px = (rgb[2][y][x] << 24) +
                              (rgb[1][y][x] << 16) +
                              (rgb[0][y][x] << 8) + 255;
                    scanLine[y].append(px);
                }
            }

//            if (mcuCount == 10) G::log("Jpeg::decodeScan", "Add mcu pixels to image");

            // Debug reporting
            if (mcuCount >= reportMCU0 && mcuCount <= reportMCU1 && isReport) {
                rptIDCT(mcuCol, mcuRow);
                rptRGB(mcuCol, mcuRow);
            }

            mcuCount++;

            if (mcuCount == 10) G::t.restart();
//            QString s = QString::number(mcuCount);
//            if (G::isLogger) G::log("Jpeg::decodeScan", s);

//            qDebug() << "Processed MCU" << mcuCol << mcuRow;

        } // end row of MCUs

        // add scanLines for this row of mcus
        for (int y = 0; y < 8; y++) {
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            for (auto x : scanLine[y])
                stream << x;
            int line = mcuRow * 8 + y;
//            if (line < 40)
            qDebug() << "Jpeg::decodeScan"
                     << "mcuRow =" << mcuRow
                     << "y =" << y
                     << "line =" << line
                     << "scanLines[y].length() =" << scanLine[y].length()
                     << "data.length() =" << data.length()
                     << "im.bytesPerLine() =" << im.bytesPerLine();
            std::memcpy(im.scanLine(line), data, im.bytesPerLine());
            scanLine[y].clear();
        }
//        qDebug() << "Jpeg::decodeScan" << mcuRow;

    } // end all rows of MCUs

    buffer.close();

    // assign im to image
    image.operator=(im);

    // write image for review
    qDebug() << "Jpeg::decodeScan" << *image.scanLine(0);
    image.save("D:/Pictures/_Jpg/test/test.jpg", "JPG");
}

void Jpeg::bufAppend(uint &buf, quint8 byte, int &consumed)
{
    if (consumed > 7) {
        uint shift = consumed - 8;
        buf >>= shift;
//        qDebug() << QString::number(buf, 2) << "shifted right" << shift;
        buf = buf | byte;
//        qDebug() << QString::number(buf, 2) << "buf = buf | byte   byte =" << byte;
        buf <<= shift;
        consumed -= 8;
    }
}

uint Jpeg::bufExtract(uint &buf, uint nBits, int &consumed)
{
    uint val = (buf & mask[nBits]) >> (32 - nBits);
    buf <<= nBits;
    consumed += nBits;
    return val;
}

uint Jpeg::bufPeek(uint &buf, uint nBits)
{
    return (buf & mask[nBits]) >> (32 - nBits);
}

void Jpeg::buildMask()
{
    for (uint i = 0; i != 32; ++i) {
        uint iMask = (1 << i) - 1;
        iMask <<= 32 - i;
        mask[i] = iMask;
    }
}

void Jpeg::buildIdctLookup()
{
    double pi = 3.141592654;
    /*
    double pi16 = 3.141592654/16.0;          // 0.196350
    double sqrtHalf	= 0.707106781;
*/
    for (int i = 0; i != 106; i++) cosine[i] = cos(i*pi/16);
    for (int c = 0; c != 3; ++c) {
        for (uint y = 0; y != 8; ++y) {
            for (uint x = 0; x != 8; ++x) {
                for (uint v = 0; v != 8; ++v) {
                    for (uint u = 0; u != 8; ++u) {
                        double tmp = 1024 * cosine[(2*x+1)*u] * cosine[(2*y+1)*v];
                        if (u==0 && v==0)
                            tmp /= 8;
                        else if (u>0 && v>0)
                            tmp /= 4;
                        else
                            tmp *= 0.1767766952966368811;
                        iIdctLookup[c][y][x][v][u] = static_cast<int>(tmp);
                        /*
                        fIdctLookup[c][y][x][v][u] = tmp;
                        double cu = (u == 0) ? sqrtHalf : 1;
                        double cv = (v == 0) ? sqrtHalf : 1;
                        double cosProd = std::cos((2*x+1)*u*M_PI/16.0) * std::cos((2*y+1)*v*M_PI/16.0);
                        double insideProd = cu * cv * cosProd;
                        fIdctLookup[c][y][x][v][u] = insideProd;
                        iIdctLookup[c][y][x][v][u] = static_cast<int>(insideProd * (1 << 10));
                        */
                    }
                }
            }
        }
    }
}

void Jpeg::rptHuff()
{
    qDebug();
    qDebug() << "REPORT HUFFMAN TABLES";
    QMapIterator<int, QMap<uint, QMap<uint, uint>>> i(dhtMap);
    // iterate tables
    while (i.hasNext()) {
        i.next();
        qDebug();
        qDebug() << "Table =" << "0x" + QString::number(i.key(), 16).rightJustified(2, '0') << i.key();
        QMapIterator<uint, QMap<uint, uint>> ii(i.value());
        while (ii.hasNext()) {
            ii.next();
            QMapIterator<uint, uint> iii(ii.value());
            while (iii.hasNext()) {
                iii.next();
                QString binCode = QString::number(iii.key(), 2).rightJustified(ii.key(), '0');
                qDebug().noquote()
                   << "    Bit Length =" << QString::number(ii.key()).leftJustified(2)
                   << "Code =" << QString::number(iii.key()).leftJustified(5)
                   << binCode.leftJustified(18)
//                   << QString::number(iii.key(), 2).rightJustified(18)
                   << "Huff Lookup =" << QString::number(iii.value()).rightJustified(3)
                   << QString::number(iii.value(), 16).toUpper().rightJustified(2, '0');
            }
        }
    }
}

void Jpeg::rptMCU(int col, int row)
{
    qDebug();
    QString mcuCoord = "[" + QString::number(col) + "," + QString::number(row) + "]";
    qDebug().noquote() << "REPORT MCU" << mcuCoord;
    for (int c = 0; c != 3; ++c) {
        qDebug().noquote() << componentDescription[c];
        for (int v = 0; v != 8; ++v) {
            QString srow = "";
            for (int u = 0; u != 8; ++u) {
                QString item = QString::number(mcu[c][v][u]).rightJustified(8);
                srow += item;
            }
            qDebug().noquote() << srow;
            srow = "";
        }
    }
}

void Jpeg::rptIDCT(int col, int row)
{
    qDebug();
    QString mcuCoord = "[" + QString::number(col) + "," + QString::number(row) + "]";
    qDebug().noquote() << "REPORT IDCT" << mcuCoord;
    for (int c = 0; c != 3; ++c) {
        qDebug().noquote() << componentDescription[c];
        for (int v = 0; v != 8; ++v) {
            QString srow = "";
            for (int u = 0; u != 8; ++u) {
                QString item = QString::number(idct[c][v][u]/*, 'f', 2*/).rightJustified(8);
                srow += item;
            }
            qDebug().noquote() << srow;
            srow = "";
        }
    }
}

void Jpeg::rptRGB(int col, int row)
{
    qDebug();
    QString mcuCoord = "[" + QString::number(col) + "," + QString::number(row) + "]";
    qDebug().noquote() << "REPORT RGB" << mcuCoord;
    QString srow = "";
    for (int y = 0; y != 8; ++y) {
        for (int x = 0; x != 8; ++x) {
            QString sR = QString::number(rgb[0][y][x], 16).toUpper().leftJustified(2, '0');
            QString sG = QString::number(rgb[1][y][x], 16).toUpper().leftJustified(2, '0');
            QString sB = QString::number(rgb[2][y][x], 16).toUpper().leftJustified(2, '0');
            QString xRGB = sR + sG + sB + "  ";
            srow += xRGB;
        }
        srow += "   ";
        for (int x = 0; x != 8; ++x) {
            QString sR = QString::number(rgb[0][y][x]).toUpper().leftJustified(3);
            QString sG = QString::number(rgb[1][y][x]).toUpper().leftJustified(3);
            QString sB = QString::number(rgb[2][y][x]).toUpper().leftJustified(3);
            QString sRGB = sR + "," + sG + "," + sB + "  ";
            srow += sRGB;
        }
        qDebug().noquote() << " " << srow;
        srow = "";
    }
    /*
    qDebug();
    srow = "";
    for (int y = 0; y != 8; ++y) {
        for (int x = 0; x != 8; ++x) {
            QString sR = QString::number(rgb[0][y][x]).toUpper().leftJustified(3);
            QString sG = QString::number(rgb[1][y][x]).toUpper().leftJustified(3);
            QString sB = QString::number(rgb[2][y][x]).toUpper().leftJustified(3);
            QString sRGB = sR + "," + sG + "," + sB + "  ";
            srow += sRGB;
        }
        qDebug().noquote() << " " << srow;
        srow = "";
    }
    //*/
}

void Jpeg::embedThumbnail(ImageMetadata &m)
{
    if (G::isLogger) G::log("Jpeg::embedThumbnail", m.fPath);

    if (G::backupBeforeModifying) Utilities::backup(m.fPath, "backup");

    ExifTool et;
    et.setOverWrite(true);

    // create path for temp jpg thumbnail file = thumbPath
    QFileInfo info(m.fPath);
    QString folder = info.dir().path();
    QString base = info.baseName();
    QString thumbPath = folder + "/" + base + "_thumb.jpg";

    // create a thumbnail size jpg
    QImage thumb = QImage(m.fPath).scaled(160, 160, Qt::KeepAspectRatio);
    thumb.save(thumbPath, "JPG", 60);

    // add the thumb.jpg to the source file
    et.addThumb(thumbPath, m.fPath);

    et.close();

    // delete the temp thumbnail file
    QFile::remove(thumbPath);

    // update datamodel thumb information
    m.isEmbeddedThumbMissing = false;
}
