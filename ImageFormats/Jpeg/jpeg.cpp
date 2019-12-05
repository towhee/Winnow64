#include "jpeg.h"
#include <QDebug>

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

bool Jpeg::getDimensions(MetadataParameters &p, ImageMetadata &m)
{
    bool isBigEnd = true;                  // only IFD/EXIF can be little endian
    quint32 marker = 0;
    p.offset += 2;
    while (marker != 0xFFC0) {
        p.file.seek(p.offset);                  // APP1 FFE*
        marker = Utilities::get16(p.file.read(2), isBigEnd);
        if (marker < 0xFF01) {
            return false;
        }
        p.offset = Utilities::get16(p.file.peek(2), isBigEnd) + static_cast<quint32>(p.file.pos());
    }
    p.file.seek(p.file.pos()+3);
    m.height = Utilities::get16(p.file.read(2), isBigEnd);
    m.width = Utilities::get16(p.file.read(2), isBigEnd);
    return true;
}

bool Jpeg::parse(MetadataParameters &p,
                 ImageMetadata &m,
                 IFD *ifd,
                 IPTC *iptc,
                 Exif *exif)
{
    // init
    m.iccSegmentOffset = 0;

    //file.open happens in readMetadata
    bool isBigEnd = true;

    if (Utilities::get16(p.file.read(2), isBigEnd) != 0xFFD8) {
        err = "JPG does not start with 0xFFD8";
        qDebug() << __FUNCTION__ << err;
        return false;
    }

    // build a hash of jpg segment offsets
    p.offset = static_cast<quint32>(p.file.pos());
    getJpgSegments(p, m);

    // TEST: read scan data
    //Scan data bitBuf = "FCFF00E2" "1111110 0111111110 000000011100010"
    uint val = 510; uint bits = 10; // -897

    int x = huff2Signed(val, bits);
    qDebug() << __FUNCTION__
             << "twosComplement()" << val << bits << "=" << x;

    // check if JFIF
    if (segmentHash.contains("JFIF")) {
        // it's a jpg so the whole thing is the full length jpg and no other
        // metadata available
        m.offsetFullJPG = 0;
        m.lengthFullJPG = static_cast<uint>(p.file.size());
        return true;
    }

    // test
//    readAppSegments(p);
//    readFrameHeader(p);  // qDebug() << __FUNCTION__ << subFormat << losslessFormat;

    // read the EXIF data
    if (segmentHash.contains("EXIF")) p.file.seek(segmentHash["EXIF"]);
    else {
        err = "JPG does not contain EXIF information";
        qDebug() << __FUNCTION__ << err;
        return false;
    }

    quint32 startOffset;
    bool foundEndian = false;
    int count = 0;
    while (!foundEndian) {
        quint32 order = Utilities::get16(p.file.read(2));
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
            return false;
        }
    }

    if (p.report) p.rpt << "\n startOffset = " << startOffset;

    quint32 a = Utilities::get16(p.file.read(2), isBigEnd);  // magic 42
    a = Utilities::get32(p.file.read(4), isBigEnd);
    quint32 offsetIfd0 = a + startOffset;

    // it's a jpg so the whole thing is the full length jpg
    m.offsetFullJPG = 0;
    m.lengthFullJPG = static_cast<uint>(p.file.size());

    // read IFD0
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p, m) + startOffset;
    quint32 offsetEXIF;
    offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;
    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);

    m.make = Utilities::getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
                     ifd->ifdDataHash.value(271).tagCount);
    m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue + startOffset,
                      ifd->ifdDataHash.value(272).tagCount);
    m.creator = Utilities::getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
                        ifd->ifdDataHash.value(315).tagCount);
    m.copyright = Utilities::getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                          ifd->ifdDataHash.value(33432).tagCount);

    // read IFD1
//    if (nextIFDOffset) nextIFDOffset = readIFD("IFD1", nextIFDOffset);
    if (nextIFDOffset) {
        p.hdr = "IFD1";
        p.offset = nextIFDOffset;
        nextIFDOffset = ifd->readIFD(p, m);
    }
    // IFD1: thumbnail offset and length
    m.offsetThumbJPG = ifd->ifdDataHash.value(513).tagValue + 12;
    m.lengthThumbJPG = ifd->ifdDataHash.value(514).tagValue;

    // read EXIF
//    readIFD("IFD Exif", offsetEXIF);
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p, m);

    m.width = ifd->ifdDataHash.value(40962).tagValue;
    m.height = ifd->ifdDataHash.value(40963).tagValue;
    p.offset = 0;
    if (!m.width || !m.height) getDimensions(p, m);

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue + startOffset,
        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
    // try DateTimeOriginal
    if (createdExif.length() == 0) {
        createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36867).tagValue + startOffset,
            ifd->ifdDataHash.value(36867).tagCount);
        if (createdExif.length() > 0) {
            m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
        }
//            if(!createdDate.isValid())
//                createdDate = QDateTime::fromString("2017:10:10 17:26:08", "yyyy:MM:dd hh:mm:ss");
    }

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.file,
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
    m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(42036).tagValue + startOffset,
                     ifd->ifdDataHash.value(42036).tagCount);

    /* Read embedded ICC. The default color space is sRGB. If there is an embedded icc profile
    and it is sRGB then no point in saving the byte array of the profile since we already have
    it and it will take up space in the datamodel. If iccBuf is null then sRGB is assumed. */
    if (segmentHash.contains("ICC")) {
        if (m.iccSegmentOffset && m.iccSegmentLength) {
            m.iccSpace = Utilities::getString(p.file, m.iccSegmentOffset + 52, 4);
            if (m.iccSpace != "sRGB") {
                p.file.seek(m.iccSegmentOffset);
                m.iccBuf = p.file.read(m.iccSegmentLength);
            }
        }
    }

    // read IPTC
    if (segmentHash.contains("IPTC")) iptc->read(p.file, segmentHash["IPTC"], m);

    // read XMP
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
}
    
void Jpeg::getJpgSegments(MetadataParameters &p, ImageMetadata &m)
{
/*
The JPG file structure is based around a series of file segments.  The marker at
the start of each segment is FFC0 to FFFE (see initSegCodeHash). The next two
bytes are the incremental offset to the next segment.

This function walks through all the segments and records their global offsets in
segmentHash.  segmentHash is used by parseJPG to access the EXIF, IPTC and XMP
segments.

In addition, the XMP offset and nextOffset are set to facilitate editing XMP data.
*/
    segmentHash.clear();
    isXmp = false;
    // big endian by default in Utilities: only IFD/EXIF can be little endian
    uint marker = 0xFFFF;
    while (marker > 0xFFBF) {
        p.file.seek(p.offset);           // APP1 FFE*
        marker = static_cast<uint>(Utilities::get16(p.file.read(2)));
        if (marker < 0xFFC0) break;
        quint32 pos = static_cast<quint32>(p.file.pos());
        quint16 len = Utilities::get16(p.file.read(2));
        quint32 nextOffset = pos + len;

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
            restartInterval = Utilities::get16(p.file.read(2));
            qDebug() << "\n" << __FUNCTION__ << "restartInterval" << restartInterval;
            break;
        case 0xFFE1: {
            QString segName = p.file.read(4);
            if (segName == "Exif") segmentHash["EXIF"] = static_cast<quint32>(p.offset);
            if (segName == "http") segName += p.file.read(15);
            if (segName == "http://ns.adobe.com") {
                segmentHash["XMP"] = static_cast<quint32>(p.offset);
                m.xmpSegmentOffset = static_cast<quint32>(p.offset);
                m.xmpNextSegmentOffset = static_cast<quint32>(nextOffset);
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
//        case 0xFFE1:
//        case 0xFFE2:
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
        p.rpt << "Segment\t\tOffset\t\tHex\n";

        QHashIterator<QString, quint32> i(segmentHash);
        while (i.hasNext()) {
            i.next();
            p.rpt << i.key()
                  << ":\t\t" << i.value() << "\t\t"
                  << QString::number(i.value(), 16).toUpper() << "\n";
        }
    }
}

void Jpeg::readAppSegments(MetadataParameters &p)
{
    int len;
    if (segmentHash.contains("APP14")) {
        p.file.seek(segmentHash["APP14"] + 2);
        len = Utilities::get16(p.file.read(2));
        if (len > 6) {
            p.file.seek(p.file.pos() + len - 4);
            colorModel =  Utilities::get16(p.file.read(2));
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
    precision = Utilities::get8(p.file.read(1));
    lines = Utilities::get16(p.file.read(2));
    samplesPerLine = Utilities::get16(p.file.read(2));
    componentsInFrame = Utilities::get8(p.file.read(1));
    components.resize(componentsInFrame);
    Component component;
    if (componentsInFrame) {
        for (int i = 0; i < componentsInFrame; i++) {
            component.Id = Utilities::get8(p.file.read(1));
            QByteArray c = p.file.read(1);
            component.horSampleFactor = Utilities::get4_1st(c);
            component.verSampleFactor = Utilities::get4_2nd(c);
            component.QTableSel = Utilities::get8(p.file.read(1));
            components[i] = component;
         }
    }
    qDebug() << "\n" << __FUNCTION__ << "Marker"
             << "0x" + QString::number(marker, 16).toUpper().rightJustified(4, '0');
    qDebug() << __FUNCTION__
             << "marker" << QString::number(marker, 16)
             << "segment" << segCodeHash[marker]
             << "length" << len
             << "precision" << precision
             << "lines" << lines
             << "samplesPerLine" << samplesPerLine
             << "componentsInFrame" << componentsInFrame;
    for (int i = 0; i < componentsInFrame; i++) {
        qDebug() << __FUNCTION__
                 << componentDescription[i]
                 << i
                 << "Id" << components.at(i).Id
                 << "horSampleFactor" << components.at(i).horSampleFactor
                 << "verSampleFactor" << components.at(i).verSampleFactor
                 << "QTableSel" << components.at(i).QTableSel;
    }
}

void Jpeg::parseHuffmanTable(MetadataParameters &p, quint16 len)
{
    quint32 pos = static_cast<quint32>(p.file.pos());
    quint32 endOffset = pos + len - 2;

    qDebug() << "\n";
    qDebug() << __FUNCTION__ << "DEBUG HUFFMAN DECODING";
    while (p.file.pos() < endOffset) {
        DHT dht;
        QMap<quint16, quint16> dhtCodeMap;  // code value, value width
        QMap<int, QMap<quint16, quint16>> dhtLengthMap;
        int dhtType;
        dht.codeLengths.resize(16);
        QByteArray c = p.file.read(1);
        dhtType = Utilities::get8(c);
        dht.classID = Utilities::get4_1st(c);
        dht.tableID = Utilities::get4_2nd(c);

        // get count of codes with bit length i
        QVector<int> counts;
        counts.resize(16);
        quint16 huffVal = 0;
        // get count of codes for each bit length i
        for (int i = 0; i < 16; i++) {
            counts[i] = Utilities::get8(p.file.read(1));
//            qDebug() << __FUNCTION__ << "Length =" << i << "Count =" << counts[i];
        }
//        int total = 0;
        // store the code:value for each bit length
        /*
Jpeg::parseHuffmanTable   Table type = " 0" Class = " 0" TableID = " 0"
Jpeg::parseHuffmanTable      j = "  0" HuffCodeCount "  7" HuffBitLength " 3" HuffVal = "    0" "               0" HuffCode = "   4" " 4"
Jpeg::parseHuffmanTable      j = "  1" HuffCodeCount "  7" HuffBitLength " 3" HuffVal = "    1" "               1" HuffCode = "   5" " 5"
Jpeg::parseHuffmanTable      j = "  2" HuffCodeCount "  7" HuffBitLength " 3" HuffVal = "    2" "              10" HuffCode = "   3" " 3"
Jpeg::parseHuffmanTable      j = "  3" HuffCodeCount "  7" HuffBitLength " 3" HuffVal = "    3" "              11" HuffCode = "   2" " 2"
Jpeg::parseHuffmanTable      j = "  4" HuffCodeCount "  7" HuffBitLength " 3" HuffVal = "    4" "             100" HuffCode = "   6" " 6"
Jpeg::parseHuffmanTable      j = "  5" HuffCodeCount "  7" HuffBitLength " 3" HuffVal = "    5" "             101" HuffCode = "   1" " 1"
Jpeg::parseHuffmanTable      j = "  6" HuffCodeCount "  7" HuffBitLength " 3" HuffVal = "    6" "             110" HuffCode = "   0" " 0"
Jpeg::parseHuffmanTable      j = "  0" HuffCodeCount "  1" HuffBitLength " 4" HuffVal = "   14" "            1110" HuffCode = "   7" " 7"
Jpeg::parseHuffmanTable      j = "  0" HuffCodeCount "  1" HuffBitLength " 5" HuffVal = "   30" "           11110" HuffCode = "   8" " 8"
Jpeg::parseHuffmanTable      j = "  0" HuffCodeCount "  1" HuffBitLength " 6" HuffVal = "   62" "          111110" HuffCode = "   9" " 9"
Jpeg::parseHuffmanTable      j = "  0" HuffCodeCount "  1" HuffBitLength " 7" HuffVal = "  126" "         1111110" HuffCode = "  10" " A"
Jpeg::parseHuffmanTable      j = "  0" HuffCodeCount "  1" HuffBitLength " 8" HuffVal = "  254" "        11111110" HuffCode = "  11" " B"
*/
                qDebug() << __FUNCTION__
                 << "  Table type =" << QString::number(dhtType).rightJustified(2)
                 << "Class =" << QString::number(dht.classID).rightJustified(2)
                 << "TableID =" << QString::number(dht.tableID).rightJustified(2)
                 << "counts =" << counts;
//                     << "Bit Length =" << i;
        // iterate bit widths
        for (int i = 0; i < 16; i++) {
            if (counts.at(i) == 0) continue;
            dhtCodeMap.clear();
            // iterate huffman codes for bit width
            for  (int j = 0; j < counts[i]; j++) {
                dhtCodeMap[huffVal] = Utilities::get8(p.file.read(1));

                qDebug() << __FUNCTION__
                         << "     j (item) =" << QString::number(j).rightJustified(3)
                         << "HuffItemCount" << QString::number(counts[i]).rightJustified(3)
                         << "HuffBitLength" << QString::number(i+1).rightJustified(2)
                         << "HuffCode =" << QString::number(huffVal).rightJustified(5)
                         << QString::number(huffVal, 2).rightJustified(16)
                         << "HuffValLength ="
                         << QString::number(dhtCodeMap[huffVal]).rightJustified(4)
                         << QString::number(dhtCodeMap[huffVal], 16).toUpper().rightJustified(2);

                huffVal++;
            }
            if (counts[i]) dhtLengthMap[i + 1] = dhtCodeMap;
            huffVal <<= 1;
        }
        // Value = dhtMap[dhtType][length][code]
        dhtMap[dhtType] = dhtLengthMap;

    }

    return;

    /* Report */
//    qDebug() << "\n" << __FUNCTION__;
//    // table, length, code : value
//    QMapIterator<int, QMap<int, QMap<quint16, quint16>>> i(dhtMap);
//    // iterate tables
//    while (i.hasNext()) {
//        i.next();
//        qDebug() << __FUNCTION__ << "Table =" << i.key();
//        // each value from dhtMap = dhtLengthMap
//        // length, code : value
//        QMapIterator<int, QMap<quint16, quint16>> ii(i.value());
//        // iterate lengths for table
//        while (ii.hasNext()) {
//            ii.next();
//            auto length = ii.value();
//            // code : value
//            QMapIterator<quint16, quint16> iii(ii.value());
//            // iterate codes for length
//            while (iii.hasNext()) {
//                iii.next();
//                qDebug() << __FUNCTION__
////                     << "    Bit Length =" << QString::number(ii.value()).rightJustified(5)
//                     << "Code =" << QString::number(iii.key()).rightJustified(5)
//                     << QString::number(iii.key(), 2).rightJustified(18)
//                     << "Value =" << QString::number(iii.value()).rightJustified(3)
//                     << QString::number(iii.value(), 16).toUpper().rightJustified(2, '0');
////                     << QString::number(ii.value(), 2).rightJustified(8);
//            }
//        }
//    }

}

void Jpeg::parseQuantizationTable(MetadataParameters &p, quint16 len)
{
    quint32 pos = static_cast<quint32>(p.file.pos());
    quint32 endOffset = pos + len - 2;

    while (p.file.pos() < endOffset) {
        QByteArray c = p.file.read(1);
//        quint8 table = Utilities::get8(c);
        int precision = Utilities::get4_1st(c);
        int table = Utilities::get4_2nd(c);
        QVector<int> q(64);
        if (precision == 0) {
            for (int i = 0; i < 64; i++) q[i] = Utilities::get8(p.file.read(1));
        }
        else {
            for (int i = 0; i < 64; i++) q[i] = Utilities::get16(p.file.read(2));
        }
        dqt[table] = q;
    }

    // report
    qDebug() << "\n" << __FUNCTION__;
    QMapIterator<int, QVector<int>> table(dqt);
    while (table.hasNext()) {
        table.next();
        qDebug() << __FUNCTION__ << "Quantization Table" << table.key()
                 << dqtDescription[table.key()];
        int i = 0;
        for (int r = 0; r < 8; r++) {
            QString rStr = "";
            for (int c = 0; c < 8; c++) {
                rStr += QString::number(dqt[table.key()][i]).rightJustified(4);
                i++;
            }
            qDebug() << __FUNCTION__ << "  "
                     << "Row" << r << " " << rStr;
        }
    }

}

void Jpeg::parseSOSHeader(MetadataParameters &p, quint16 len)
{
    scanDataOffset = p.file.pos() + len - 2;
    sosComponentCount = Utilities::get8(p.file.read(1));
    for (int i = 0; i < sosComponentCount; i++) {
        int componentID = Utilities::get8(p.file.read(1));
        huffTblToUse[componentID] = Utilities::get8(p.file.read(1));
    }

    // report
    qDebug() << "\n" << __FUNCTION__
             << "Scan data offset =" << scanDataOffset
             << "Number of components =" << sosComponentCount;
    for (int i = 0; i < sosComponentCount; i++) {
        qDebug() << __FUNCTION__ << "  "
        << "ComponentID =" << i
        << componentDescription[i]
        << "Huffman table ID =" << huffTblToUse[i];
//        << dqtDescription[huffTblToUse[i]];
    }

    p.file.seek(scanDataOffset);
    quint32 bitBuf = Utilities::get32(p.file.read(4));
    qDebug() << "\n";
    qDebug() << __FUNCTION__ << "Scan data bitBuf ="
             << QString::number(bitBuf, 16).toUpper().rightJustified(8, '0')
             << QString::number(bitBuf, 2).rightJustified(32, '0');

    p.file.seek(scanDataOffset);
    QByteArray bytes = p.file.read(4);
    QBitArray bits = Utilities::bytes2Bits(bytes);

    qDebug() << "\n";
    int dhtType = 0;
    // iterate bits until huffman code found
    for (int huffLength = 1; huffLength < 17; huffLength++) {
        if (dhtMap[dhtType][huffLength].count()) {
            auto code = Utilities::bits2Int(bits, huffLength);
            if (dhtMap[dhtType][huffLength].contains(code)) {
                qDebug() << __FUNCTION__
                         << "bit code =" << QString::number(code, 2)
                         << "value bit length =" << dhtMap[dhtType][huffLength][code];
//                bits = bits << huffLength;
                break;
            }
        }
    }
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
