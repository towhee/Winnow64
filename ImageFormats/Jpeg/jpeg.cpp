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
    decodeScan(p);

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
//            qDebug() << "\n" << __FUNCTION__ << "restartInterval" << restartInterval;
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
        QMap<uint, uint> dhtCodeMap;  // code value, value width
        QMap<uint, QMap<uint, uint>> dhtLengthMap;
        int dhtType;
//        dht.codeLengths.resize(16);
        QByteArray c = p.file.read(1);
        dhtType = Utilities::get8(c);
        dht.classID = Utilities::get4_1st(c);
        dht.tableID = Utilities::get4_2nd(c);

        // get count of codes with bit length i
        QVector<int> counts;
        counts.resize(16);
        quint16 huffCode = 0;
        // get count of codes for each bit length i
        for (int i = 0; i < 16; i++) {
            counts[i] = Utilities::get8(p.file.read(1));
//            qDebug() << "Table =" << dhtType << "Length =" << i << "Count =" << counts[i];
        }
        /*
         qDebug() << __FUNCTION__
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
                dhtCodeMap[huffCode] = Utilities::get8(p.file.read(1));

                /*qDebug() << __FUNCTION__
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
    // rgh can we only build once?
    buildMask();

    qDebug();
//    qDebug() << "REPORT HUFFMAN TABLES";
//    rptHuff();

    qDebug() << "START OF SCAN   Offset =" << scanDataOffset;
    qDebug();

    // bit buffer
    uint buf = 0;

    // end of scan = 0xFFD9
    bool eos = false;
    // buffer is empty
    uint consumed = 32;
    // start of scanned data
    p.file.seek(scanDataOffset);

    // decode MCU, first initialize to zero
    for (int component = 0; component != 3; ++component) {
        for (int m = 0; m != 64; ++m) {
            mcu[component][m] = 0;
        }
    }
    // Luminance Y, Cb, Cr
    for (int c = 0; c != 3; ++c) {
        int qTbl;
        c == 0 ?qTbl = 0 : qTbl = 1;
        // each entry in MCU
        for (int m = 0; m != 64; ++m) {
            // backfill buffer while there is room
            while (consumed > 7) {
                // load another byte
                if (!eos) {
                    quint8 nextByte = Utilities::get8(p.file.read(1));
                    /*qDebug() << "nextByte =" << QString::number(nextByte, 16).toUpper().rightJustified(0, '0')
                             << "offset =" << p.file.pos()
                             << "consumed =" << consumed;*/
                    bool isMarkerByte = false;
                    if (nextByte == 0xFF) {
                        uint markerByte = Utilities::get8(p.file.read(1));
                        if (markerByte != 0) {
                            qDebug().noquote() << "Marker byte = " << QString::number(markerByte, 16);
                            isMarkerByte = true;
                            if (markerByte == 0xD9) {
                                // End of scan
                                eos = true;
                            }
                        }
                    }
                    if (!eos && !isMarkerByte) {
                        bufAppend(buf, nextByte, consumed);
                        /*qDebug() << __FUNCTION__
                                 << "Appended nextByte =" << QString::number(nextByte, 16)
                                 << "Consumed =" << consumed;*/

                    }
                }
            }
            /*
            qDebug() << __FUNCTION__
                     << "initial buf ="
                     << QString::number(buf, 16)
                     << QString::number(buf, 2);*/
            // which huffTable to use?'
            int tbl = 0;
            if      (c == 0 && m == 0) tbl = 0x00;      // DC component of Luminance (Y)
            else if (c == 0 && m != 0) tbl = 0x10;      // AC component of Luminance (Y)
            else if (c != 0 && m == 0) tbl = 0x01;      // DC component of Chrominance (Cb & Cr)
            else if (c != 0 && m != 0) tbl = 0x11;      // AC component of Chrominance (Cb & Cr)

            // iterate bit buffer until huffman code found
            bool huffFound = false;
            bool allZero = false;
            for (uint huffLength = 1; huffLength < 17; huffLength++) {
                if (dhtMap[tbl][huffLength].count()) {
                    uint huffCode = bufPeek(buf, huffLength);
                    /*qDebug() << QString::number(buf, 2)
                             << "Peeking for code = "
                             << QString::number(code, 2)
                             << "length =" << huffLength;*/
                    if (dhtMap[tbl][huffLength].contains(huffCode)) {
                        QString binCode = QString::number(huffCode, 2).rightJustified(huffLength, '0');
                        /*qDebug().noquote()
                             << "c =" << c
                             << "m =" << QString::number(m).rightJustified(2)
                             << QString::number(buf, 2).rightJustified(32, '0')
                             << "table =" << QString::number(tbl, 16).rightJustified(2)
                             << "consumed =" << QString::number(consumed).rightJustified(2)
                             << "huffLength =" << QString::number(huffLength).rightJustified(2)
                             << "huffCode =" << QString::number(huffCode).rightJustified(3)
                             << binCode.leftJustified(16)
                             << "huffVal =" << dhtMap[tbl][huffLength][huffCode]
                             << "Signed =" << huff2Signed(dhtMap[tbl][huffLength][huffCode], huffLength);*/
                        // check if valueLength (dhtMap[tbl][huffLength][code]) = zero
                        // if so, all AC are zero, so break out of MCU loop
                        if (m == 1 && dhtMap[tbl][huffLength][huffCode] == 0) {
                            allZero = true;
                            // extract code from buffer (not used as zero huff lookup value)
                            bufExtract(buf, huffLength, consumed);
                            break;
                        }

                        uint huffVal;
                        QString bufSnapShot = QString::number(buf, 2).rightJustified(32, '0');
                        // extract huffCode from buffer so can access next bits = value
                        bufExtract(buf, huffLength, consumed);
                        // extract value from buffer
                        huffVal = dhtMap[tbl][huffLength][huffCode] & 0xF;
                        int huffRpt = dhtMap[tbl][huffLength][huffCode] / 0xF;
                        uint huffResult = bufExtract(buf, huffVal, consumed);
                        QString binHuffResult = QString::number(huffResult, 2).rightJustified(huffVal, '0');
                        int huffSignedResult = huff2Signed(huffResult, huffVal);

                        mcu[c][zzmcu[m]] = huffSignedResult * dqt[qTbl][m];
                        if (huffRpt) {
                            for (int i = 0; i != huffRpt; ++i) {
                                m++;
                                mcu[c][zzmcu[m]] = huffSignedResult * dqt[qTbl][m];
                            }
                        }

                        QString sHuffCode = bufSnapShot.left(huffLength) + " ";
                        QString sHuffResult = bufSnapShot.mid(huffLength, huffVal) + " ";
                        QString sBufRemainder = bufSnapShot.right(32 - huffLength - huffVal);
                        QString sBuf = sHuffCode + sHuffResult + sBufRemainder;
//                        int huffValAndF = huffVal & 0xF;
                        qDebug().noquote()
                             << "c =" << c
                             << "m =" << QString::number(m).rightJustified(2)
                             << "zigzag =" << QString::number(zzmcu[m]).rightJustified(2)
                             << "    " << bufSnapShot
                             << "table =" << QString::number(tbl, 16).rightJustified(2)
                             << "consumed =" << QString::number(consumed).rightJustified(2)
                             << "huffLength =" << QString::number(huffLength).rightJustified(2)
                             << "huffCode =" << QString::number(huffCode).rightJustified(3)
                             << binCode.leftJustified(12)
                             << "huffVal =" << QString::number(huffVal).rightJustified(3)
                             << "rpt =" << QString::number(huffRpt).leftJustified(2)
                             << "bits =" << binHuffResult.leftJustified(10)
                             << "huffResult =" << QString::number(huffResult).rightJustified(4)
                             << "huffSignedResult =" << QString::number(huffSignedResult).rightJustified(4)
                             << "Q =" << QString::number(dqt[qTbl][m]).leftJustified(2)
                             << "   " << sBuf;
                        huffFound = true;
                        break;
                    }
                }
            }
//            qDebug() << __FUNCTION__ << "c =" << c << "m =" << m << "allZero =" << allZero;
            if (allZero) break;
            if (!huffFound) {
                // err
                qDebug() << __FUNCTION__ << "HUFF CODE NOT FOUND";
            }
        }
    }
    qDebug() << "REPORT MCU";
    rptMCU();

//    qDebug() << __FUNCTION__ << dhtMap[0];
}

void Jpeg::bufAppend(uint &buf, quint8 byte, uint &consumed)
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

uint Jpeg::bufExtract(uint &buf, uint nBits, uint &consumed)
{
    uint val = (buf & mask[nBits]) >> (32 - nBits);
//    if (val) {
        buf <<= nBits;
        consumed += nBits;
//    }
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

void Jpeg::rptHuff()
{
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

void Jpeg::rptMCU()
{
    QString row = "";
    for (int c = 0; c != 3; ++c) {
        qDebug().noquote() << componentDescription[c];
        for (int m = 0; m != 64; ++m) {
            QString item = QString::number(mcu[c][m]).rightJustified(6);
            row += item;
//            qDebug() << "c =" << c << "m =" << m << "m % 8 =" << m % 8 << "row =" << row;
            if (m > 0 && ((m + 1) % 8) == 0) {
                qDebug().noquote() << row;
                row = "";
            }
        }
    }
}
