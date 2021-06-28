#include "tiff.h"

Tiff::Tiff()
{
}

bool Tiff::parse(MetadataParameters &p,
           ImageMetadata &m,
           IFD *ifd,
           IRB *irb,
           IPTC *iptc,
           Exif *exif,
           GPS *gps,
           Jpeg *jpeg)
{
/*
    This function reads the metadata from a tiff file.  If the tiff file does not contains a
    thumbnail, either in an IRB or IFB, and G::embedTifThumb == true, then a thumbnail will be
    added at the end of file.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    //file.open happens in readMetadata

    quint32 startOffset = 0;

    // first two bytes is the endian order
    quint16 order = Utilities::get16(p.file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;

    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
    m.isBigEnd = isBigEnd;      // for reporting
    if (p.report) {
        QString byteOrder;
        isBigEnd ? byteOrder = "Big Endian MM" : byteOrder = "Little Endian II";
        p.rpt << "\nFile byte order = " << byteOrder << "\n";
    }

    // should be magic number 42 next
    if (Utilities::get16(p.file.read(2), isBigEnd) != 42) return false;

    // read offset to first IFD
    quint32 ifdOffset = Utilities::get32(p.file.read(4), isBigEnd);
    m.ifd0Offset = ifdOffset;
    m.offsetFull = ifdOffset;
    m.offsetThumb = ifdOffset;
    p.hdr = "IFD0";
    p.offset = ifdOffset;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
    // position of the final nextIFDOffset in the chain
    lastIFDOffsetPosition = 0;
    if (!nextIFDOffset) lastIFDOffsetPosition = p.file.pos() - 4;

    m.lengthFull = 0;   // not relevent for tif files
    m.lengthThumb = 0;  // reset to length if jpg embedded in IRB for the tiff


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
        ? m.width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue)
        : m.width = 0;

    // IFD0: height
    (ifd->ifdDataHash.contains(257))
        ? m.height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue)
        : m.height = 0;
    m.widthFull = m.width;
    m.heightFull = m.height;
    // start thumbnail dimensions
    int thumbLongside;
    m.widthFull > m.heightFull ? thumbLongside = m.widthFull : thumbLongside = m.heightFull;

    /*
     Moved to Tiff::parseForDecoding:
         IFD0: bitsPerSampleFull
         IFD0: photoInterpFull
         IFD0: compressionFull
         IFD0: stripByteCountsFull
         IFD0: planarConfigurationFull
    //*/

    // IFD0: samplesPerPixelFull (cannot decode if > 3)
    (ifd->ifdDataHash.contains(277))
        ? m.samplesPerPixel = static_cast<int>(ifd->ifdDataHash.value(277).tagValue)
        : m.samplesPerPixel = 0;

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

    // IFD0: ICC offset
    if (ifd->ifdDataHash.contains(34675)) {
        m.iccSegmentOffset = ifd->ifdDataHash.value(34675).tagValue;
        m.iccSegmentLength = ifd->ifdDataHash.value(34675).tagCount;
    }

    // IFD0: XMP offset
    quint32 ifdXMPOffset = 0;
    if (ifd->ifdDataHash.contains(700)) {
        m.isXmp = true;
        ifdXMPOffset = ifd->ifdDataHash.value(700).tagValue;
        m.xmpSegmentOffset = static_cast<uint>(ifdXMPOffset);
        int xmpSegmentLength = static_cast<int>(ifd->ifdDataHash.value(700).tagCount);
        m.xmpNextSegmentOffset = m.xmpSegmentOffset + static_cast<uint>(xmpSegmentLength);
    }

    p.offset = m.ifd0Offset;
    if (p.report) parseForDecoding(p, m, ifd);

    // search for thumbnail in chained IFDs, IRB and subIFDs

    // IRB:  Get embedded JPG thumbnail if available
    bool foundJpgThumb = false;
    if (ifdPhotoshopOffset) {
        if (p.report) {
            p.hdr = "IRB";
            MetaReport::header(p.hdr, p.rpt);
//            p.rpt.setFieldAlignment(QTextStream::AlignLeft);
            p.rpt.reset();
            p.rpt << "Offset        IRB Id   (1036 = embedded jpg thumbnail)\n";
        }
        p.offset = ifdPhotoshopOffset;
        irb->readThumb(p, m);    // rgh need to add IRB for DNG
        if (m.lengthThumb) {
            foundJpgThumb = true;
            // assign arbitrary value to thumbLongside less than 512
            thumbLongside = 256;
            if (p.report) {
                p.offset = m.offsetThumb;
                Jpeg jpg;
                jpg.parse(p, m, ifd, iptc, exif, gps);
            }
        }
    }

    // Chained IFDs ************************************************************
    // search for lastIFDOffset and existing thumbnail
    int count = 0;
    if (p.report) {
        while (nextIFDOffset) {
            count++;
            p.hdr = "IFD" + QString::number(count);
            p.offset = nextIFDOffset;
            p.hash = &exif->hash;
            nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
            parseForDecoding(p, m, ifd);
            if (!ifd->ifdDataHash.contains(256)) continue;
            int w = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
            int h = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
            int thumbLong;
            w > h ? thumbLong = w : thumbLong = h;
            if (thumbLong >= thumbLongside) continue;
            thumbLongside = thumbLong;
            m.offsetThumb = p.offset;
        }
    }
    else {
        while (nextIFDOffset > 0 && !foundJpgThumb) {
            count++;
            p.hdr = "IFD" + QString::number(count);
            p.offset = nextIFDOffset;
            p.hash = &exif->hash;
            nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
//            // Embedded JPG (513)
//            if (ifd->ifdDataHash.contains(513) && ifd->ifdDataHash.contains(513)) {
//                m.offsetThumb = ifd->ifdDataHash.value(513).tagValue;
//                m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;
//                foundJpgThumb = true;
//                // assign arbitrary value to thumbLongside less than 512
//                thumbLongside = 256;
//                break;
//            }
            // check if IFD is a reduced resolution main image ie maybe the thumbnail
            // SubFileType == 1 (tagid == 254)
            if (!ifd->ifdDataHash.contains(254)) continue;
            if (ifd->ifdDataHash.value(254).tagValue != 1) continue;
            // is image width small then full size and less than 640 pixels?
            if (!ifd->ifdDataHash.contains(256)) continue;
            int w = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
            int h = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
            int thumbLong;
            w > h ? thumbLong = w : thumbLong = h;
            if (thumbLong >= thumbLongside) continue;
            thumbLongside = thumbLong;
            m.offsetThumb = p.offset;
        }
        if (!lastIFDOffsetPosition) lastIFDOffsetPosition = p.file.pos() - 4;
    }

    // subIFDs: ****************************************************************
    /* If save tiff with save pyramid in photoshop then subIFDs created. Iterate to report and
       possibly read smallest for thumbnail. */
    nextIFDOffset = ifdsubIFDOffset;
    count = 0;
    if (p.report && ifdsubIFDOffset) {
        while (nextIFDOffset && count < 10) {
            count ++;
            p.hdr = "SubIFD " + QString::number(count);
            p.offset = nextIFDOffset;
            nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
            parseForDecoding(p, m, ifd);
            if (!ifd->ifdDataHash.contains(256)) continue;
            int w = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
            int h = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
            int thumbLong;
            w > h ? thumbLong = w : thumbLong = h;
            if (thumbLong >= thumbLongside) continue;
            thumbLongside = thumbLong;
            m.offsetThumb = p.offset;
        }

    }
    if (!p.report && ifdsubIFDOffset && !foundJpgThumb) {
        while (nextIFDOffset && count < 10) {
            count ++;
            p.hdr = "SubIFD " + QString::number(count);
            p.offset = nextIFDOffset;
            nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
            // check if IFD is a reduced resolution main image ie maybe the thumbnail
            // SubFileType == 1 (tagid == 254)
            if (!ifd->ifdDataHash.contains(254)) continue;
            if (ifd->ifdDataHash.value(254).tagValue != 1) continue;
            // is image width small then full size and less than 640 pixels?
            if (!ifd->ifdDataHash.contains(256)) continue;
            int w = static_cast<int>(ifd->ifdDataHash.value(256).tagValue);
            int h = static_cast<int>(ifd->ifdDataHash.value(257).tagValue);
            int thumbLong;
            w > h ? thumbLong = w : thumbLong = h;
            if (thumbLong >= thumbLongside) continue;
            thumbLongside = thumbLong;
            m.offsetThumb = p.offset;
        }
    }

    // Add a thumbnail if missing
    if (G::embedTifThumb && m.offsetThumb == m.offsetFull && thumbLongside > 512) {
//        /*
        qDebug() << __FUNCTION__ << "encodeThumbnail " << m.fPath
                 << "lastIFDOffsetPosition =" << lastIFDOffsetPosition
                 << "thumbLongside =" << thumbLongside
                    ;
                    //*/
        p.offset = m.offsetFull;        // change to best offset if smaller preview to use
        encodeThumbnail(p, m, ifd);
    }

    // EXIF: *******************************************************************

    p.hdr = "IFD Exif";
    p.offset = ifdEXIFOffset;
    if (ifdEXIFOffset) ifd->readIFD(p, m, isBigEnd);

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue,
                                      isBigEnd);
        if (x < 1 ) {
            int t = qRound(1 / x);
            m.exposureTime = "1/" + QString::number(t);
            m.exposureTimeNum = x;
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
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(33437).tagValue,
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
        double x = Utilities::getReal_s(p.file,
                                      ifd->ifdDataHash.value(37380).tagValue,
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
            ? m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
                                  ifd->ifdDataHash.value(42036).tagCount)
            : m.lens = "";

    /* Read embedded ICC. The default color space is sRGB. If there is an embedded icc profile
    and it is sRGB then no point in saving the byte array of the profile since we already have
    it and it will take up space in the datamodel. If iccBuf is null then sRGB is assumed. */
    if (m.iccSegmentOffset && m.iccSegmentLength) {
        m.iccSpace = Utilities::getString(p.file, m.iccSegmentOffset + 16, 4);
        if (m.iccSpace != "sRGB") {
            p.file.seek(m.iccSegmentOffset);
            m.iccBuf = p.file.read(m.iccSegmentLength);
        }
    }

    // IPTC: *******************************************************************
    // Get title if available

    if (ifdIPTCOffset) iptc->read(p.file, ifdIPTCOffset, m);

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

    return true;
}

bool Tiff::parseForDecoding(MetadataParameters &p, ImageMetadata &m, IFD *ifd)
{
/*
    Get IFD parameters required for TIF decoding: stripOffsets, stripByteCounts, bitsPerSample,
    photoInterp, samplesPerPixel, rowsPerStrip, compression and planarConfiguration.

    This function can be called from the main thread (for reporting) or from the metadata
    generation thread or from any of the image decoder threads.  The reporting parameter
    p.rpt can only be used from the main thread.

    p.offset must be set to start of IFD before calling parseForDecoding.

    If reporting, then p.hash must be set to Exif::hash.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    // IFD has already been read once, so if reporting do not want to report again
    bool isReport = p.report;
    p.report = false;
    ifd->readIFD(p, m, m.isBigEnd);
    p.report = isReport;
    err = "";

    if (ifd->ifdDataHash.contains(273)) {
        int offsetCount = ifd->ifdDataHash.value(273).tagCount;
        stripOffsets.resize(offsetCount);
        if (offsetCount == 1) {
            stripOffsets[0] = static_cast<uint>(ifd->ifdDataHash.value(273).tagValue);
        }
        else {
            quint32 offset = ifd->ifdDataHash.value(273).tagValue;
            p.file.seek(offset);
            for (int i = 0; i < offsetCount; ++i) {
                stripOffsets[i] = Utilities::get32(p.file.read(4), m.isBigEnd);
            }
        }
    }
    else {
        err += "No strip offsets.  ";
//        qDebug() << __FUNCTION__
//                 << "No strip offsets for " + m.fPath + ". "
//                 << "IFD offset =" << p.offset
//                    ;
    }

    if (ifd->ifdDataHash.contains(279)) {
        int offsetCount = ifd->ifdDataHash.value(279).tagCount;
        stripByteCounts.resize(offsetCount);
        if (offsetCount == 1) {
            stripByteCounts[0] = static_cast<uint>(ifd->ifdDataHash.value(279).tagValue);
        }
        else {
            quint32 offset = ifd->ifdDataHash.value(279).tagValue;
            p.file.seek(offset);
            for (int i = 0; i < offsetCount; ++i) {
                stripByteCounts[i] = Utilities::get32(p.file.read(4), m.isBigEnd);
            }
        }
    }
    else {
        err += "No StripByteCounts.  ";
    }

    // IFD: bitsPerSample
    if (ifd->ifdDataHash.contains(258)) {
        p.file.seek(static_cast<int>(ifd->ifdDataHash.value(258).tagValue));
        bitsPerSample = Utilities::get16(p.file.read(2), m.isBigEnd);
    }
    else {
       err += "No BitsPerSample.  ";
    }

    // IFD: compression
    (ifd->ifdDataHash.contains(259))
            ? compression = static_cast<int>(ifd->ifdDataHash.value(259).tagValue)
            : compression = 1;
    if (compression !=1) {
        err += "Compression != 1.  ";
    }

    // IFD: width
    (ifd->ifdDataHash.contains(256))
        ? width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue)
        : width = 0;
    if (width == 0) {
        err += "Width = 0.  ";
    }

    // IFD: height
    (ifd->ifdDataHash.contains(257))
        ? height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue)
        : height = 0;
    if (height == 0) {
        err += "Height = 0.  ";
    }

    // IFD: photoInterp
    (ifd->ifdDataHash.contains(262))
            ? photoInterp = static_cast<int>(ifd->ifdDataHash.value(262).tagValue)
            : photoInterp = 0;

    // IFD: samplesPerPixel
    (ifd->ifdDataHash.contains(277))
            ? samplesPerPixel = static_cast<int>(ifd->ifdDataHash.value(277).tagValue)
            : samplesPerPixel = 0;
    if (samplesPerPixel == 0) {
        err += "SamplesPerPixel is undefined.  ";
    }

    // IFD: rowsPerStrip
    (ifd->ifdDataHash.contains(278))
            ? rowsPerStrip = static_cast<int>(ifd->ifdDataHash.value(278).tagValue)
            : rowsPerStrip = m.height;

    // IFD: planarConfiguration
    (ifd->ifdDataHash.contains(284))
            ? planarConfiguration = ifd->ifdDataHash.value(284).tagValue
            : planarConfiguration = 1;

    if (err != "") return false;

    if (p.report) {
        int w1 = 25;
        p.rpt << "\n" << "Decode TIFF parameter values:" << "\n";
        p.rpt.setFieldAlignment(QTextStream::AlignLeft);
        p.rpt.setFieldWidth(w1); p.rpt << "1st StripOffset" << stripOffsets[0]; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "1st StripByteCount" << stripByteCounts[0]; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "BitsPerSample" << bitsPerSample; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "PhotometricInterpation" << photoInterp; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "SamplesPerPixel" << samplesPerPixel; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "RowsPerStrip" << rowsPerStrip; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "Compression" << compression; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "PlanarConfiguration" << planarConfiguration; p.rpt.setFieldWidth(0); p.rpt << "\n";
    }

    return true;
}

//void Tiff::reportDecodingParamters(MetadataParameters &p)
//{
//    int w1 = 25;
//    p.rpt << "\n" << "Decode TIFF parameter values:" << "\n";
//    p.rpt.setFieldAlignment(QTextStream::AlignLeft);
//    p.rpt.setFieldWidth(w1); p.rpt << "1st StripOffset" << stripOffsets[0]; p.rpt.setFieldWidth(0); p.rpt << "\n";
//    p.rpt.setFieldWidth(w1); p.rpt << "1st StripByteCount" << stripByteCounts[0]; p.rpt.setFieldWidth(0); p.rpt << "\n";
//    p.rpt.setFieldWidth(w1); p.rpt << "BitsPerSample" << bitsPerSample; p.rpt.setFieldWidth(0); p.rpt << "\n";
//    p.rpt.setFieldWidth(w1); p.rpt << "PhotometricInterpation" << photoInterp; p.rpt.setFieldWidth(0); p.rpt << "\n";
//    p.rpt.setFieldWidth(w1); p.rpt << "SamplesPerPixel" << samplesPerPixel; p.rpt.setFieldWidth(0); p.rpt << "\n";
//    p.rpt.setFieldWidth(w1); p.rpt << "RowsPerStrip" << rowsPerStrip; p.rpt.setFieldWidth(0); p.rpt << "\n";
//    p.rpt.setFieldWidth(w1); p.rpt << "Compression" << compression; p.rpt.setFieldWidth(0); p.rpt << "\n";
//    p.rpt.setFieldWidth(w1); p.rpt << "PlanarConfiguration" << planarConfiguration; p.rpt.setFieldWidth(0); p.rpt << "\n";
//}

bool Tiff::decode(ImageMetadata &m, QString &fPath, QImage &image, bool thumb, int newSize)
{
/*
    Decode using unmapped QFile.  Set p.file, p.offset and call main decode.
*/
    if (G::isLogger) G::log(__FUNCTION__, " load file from fPath");
    QFileInfo fileInfo(fPath);
    if (!fileInfo.exists()) return false;                 // guard for usb drive ejection

    MetadataParameters p;
    p.file.setFileName(fPath);
    if (!p.file.open(QIODevice::ReadOnly)) {
        m.err += "Unable to open file " + fPath + ". ";
        qDebug() << __FUNCTION__ << m.err;
        return false;
    }

    /*
    qDebug() << __FUNCTION__
             << "isThumb =" << thumb
             << "newSize =" << newSize
             << "m.offsetThumb =" << m.offsetThumb
                ;
                //*/

    if (thumb && m.offsetThumb != m.offsetFull) p.offset = m.offsetThumb;
    else p.offset = m.offsetFull;
    return decode(m, p, image, newSize);
}

bool Tiff::decode(ImageMetadata &m, MetadataParameters &p, QImage &image, int newSize)
{
/*
    Decoding the Tif occurs here.

    p.file must be set and opened. If called from ImageDecoder then p.file will be mapped
    to memory.

    p.offset must be set to ifdOffset that describes the image to be decoded within the
    tif before calling this function (either m.offsetFull or m.offsetThumb).

*/
    if (G::isLogger) G::log(__FUNCTION__, "Main decode with p.file assigned");
//    QElapsedTimer t;
//    t.restart();

    IFD *ifd = new IFD;
    p.report = false;
    if (!parseForDecoding(p, m, ifd)) {
        qWarning() << err + m.fPath;
        return false;
    }
//    if (unableToDecode()) return false;

    /*
    qDebug() << __FUNCTION__
             << "IFD width =" << width
             << "IFD height =" << height
                ;
                //*/

    // width and height are for the IFD (not necessarily the full image) from parseForDecoding
    int w = width;
    int h = height;
    int nth;
    sample(m, newSize, nth, w, h);

    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerLine = bytesPerPixel * width;
    quint32 fSize = static_cast<quint32>(p.file.size());

    QImage *im;
    if (bitsPerSample == 16) im = new QImage(w, h, QImage::Format_RGBX64);
    if (bitsPerSample == 8)  im = new QImage(w, h, QImage::Format_RGB888);

    /*
    qDebug() << __FUNCTION__
             << "w =" << w
             << "h =" << h
             << "nth =" << nth
             << "bytesPerPixel =" << bytesPerPixel
                ;
                //*/

    // read every nth pixel
    if (newSize) {
        int newBytesPerLine = w * bytesPerPixel;
        QByteArray ba;
        quint32 fOffset = 0;
        // read every nth pixel to create new smaller image (thumbnail)
        int y = 0;
        int sampleY = 0;
        for (int strip = 0; strip < stripOffsets.count();++strip) {
            for (int row = 0; row < rowsPerStrip; row++) {
                if (y % nth == 0) {
                    fOffset = stripOffsets.at(strip) + static_cast<quint32>(row * bytesPerLine);
                    ba.clear();
                    for (int x = 0; x < w; x++) {
                        p.file.seek(fOffset);
                        if (fOffset + static_cast<size_t>(bytesPerPixel) > fSize) {
                            qDebug() << __FUNCTION__
                                     << "Read nth pixel - ATTEMPTING TO READ PAST END OF FILE"
                                     << "row =" << row
                                     << "x =" << x
                                     << "sampleY =" << sampleY
                                     << "startOffset =" << stripOffsets.at(0)
                                     << "fOffset =" << fOffset
                                     << "Bytes =" << fOffset - stripOffsets.at(0)
                                     << m.fPath
                                        ;
                            break;
                        }
                        ba += p.file.read(bytesPerPixel);
                        // jump to next nth sample
                        fOffset += static_cast<uint>(nth * bytesPerPixel);
                    }
                    if (sampleY < h) {
                        if (fOffset + static_cast<size_t>(newBytesPerLine) > fSize) {
                            qDebug() << __FUNCTION__
                                     << "Read scanline - ATTEMPTING TO READ PAST END OF FILE"
                                     << "row =" << row
                                     << "sampleY =" << sampleY
                                     << "startOffset =" << stripOffsets.at(0)
                                     << "fOffset =" << fOffset
                                     << "Bytes =" << fOffset - stripOffsets.at(0)
                                     << m.fPath
                                        ;
                            break;
                        }
//                        std::memcpy(im->scanLine(sampleY), ba, static_cast<size_t>(ba.length()));
                        std::memcpy(im->scanLine(sampleY), ba, static_cast<size_t>(newBytesPerLine));
                    }
                    sampleY++;
                }
                y++;
                if (sampleY >= h) break;
            }
            if (sampleY >= h) break;
        }
        if (bitsPerSample == 16) {
            if (m.isBigEnd) invertEndian16(im);
            toRRGGBBAA(im);
        }
        p.file.close();
        // convert to standard QImage format for display in Winnow
        im->convertTo(QImage::Format_RGB32);
        image.operator=(*im);
        return true;
    }

    // read entire image
    quint32 fOffset = 0;
    int line = 0;
    for (int strip = 0; strip < stripOffsets.count(); ++strip) {
        for (int row = 0; row < rowsPerStrip; row++) {
            fOffset = stripOffsets.at(strip) + static_cast<quint32>(row * bytesPerLine);
            if (fOffset + static_cast<size_t>(bytesPerLine) > fSize) {
                qDebug() << __FUNCTION__ << "Read entire image - ATTEMPTING TO READ PAST END OF FILE";
                break;
            }
            p.file.seek(fOffset);
            std::memcpy(im->scanLine(line),
                        p.file.read(bytesPerLine),
                        static_cast<size_t>(bytesPerLine));
            line++;
        }
    }
    if (bitsPerSample == 16) {
        if (m.isBigEnd) invertEndian16(im);
        toRRGGBBAA(im);
    }
    p.file.close();
    // convert to standard QImage format for display in Winnow
    im->convertTo(QImage::Format_RGB32);
    image.operator=(*im);
    return true;
}

bool Tiff::encodeThumbnail(MetadataParameters &p, ImageMetadata &m, IFD *ifd)
{
/*
    - little/big endian put functions to write 8, 16, 24, 32, 64, real, CString
    - add new IFD at end of chain
        - replace last nextIFDOffset with offset to current EOF
        - add IFD items:
            Num  tagId tagType  tagCount    tagValue   tagDescription
              0    254       4         1           1   SubfileType
              1    256       3         1               ImageWidth
              2    257       3         1               ImageHeight
              3    258       3         3      offset   BitsPerSample
              4    259       3         1           1   Compression
              5    262       3         1           2   PhotometricInterpretation
              6    273       4         1       value   StripOffsets
              7    274       3         1           1   Orientation
              8    277       3         1           3   SamplesPerPixel
              9    278       3         1        1728   RowsPerStrip
             10    279       4         1       value   StripByteCounts
             11    282       5         1      offset   XResolution
             12    283       5         1      offset   YResolution
             13    284       3         1           1   PlanarConfiguration
             14    296       3         1           2   ResolutionUnit
    - subsample image down to thumbnail resolution
    - write strip pixels rgb at offset StripOffsets
*/
    // p.offset = offset to the last IFD nextIFDOffset and will be changed in parseForDecoding
//    quint32 lastIFDOffset = p.offset;
//    if (m.height == 0) return false;

    // get decoding parameters from IFD0 (the main image)
//    p.offset = m.ifd0Offset;
    parseForDecoding(p, m, ifd);
    if (unableToDecode()) {
        qDebug() << __FUNCTION__ << "Unable to decode";
        return false;
    }

    // go to last main IFD offset and replace with offset to new IFD at end of file
    m.offsetThumb = p.file.size();
    p.file.seek(lastIFDOffsetPosition);
    p.file.write(Utilities::put32(m.offsetThumb, m.isBigEnd));
    qDebug() << __FUNCTION__ << "1";

    // Go to new thumbIFDOffset and write the count of IFD entries (15)
    p.offset = m.offsetThumb;
    ifd->writeIFDCount(p, m, 15);

    // thumbnail size and sample increment (nth)
    int w, h, nth;
    sample(m, G::maxIconSize, nth, w, h);      // sample to thumbnail with longside = 240

    /* First offset in IFD is BitsPerSample (bpsOffset) located at end of IFD.
       Length of IFD = 15 entries * 12 bytes per entry + 4 bytes nextIFDOffset.
       Since StripOffsets count = 1, value is in the IFD, not an offset from the IFD, and same
       thing for StripByteCounts. */
    quint32 bpsOffset = p.file.pos() + 15 * 12 + 4;             // write 2 bytes * 3
    quint32 xresOffset = bpsOffset + 6;                         // write 8 bytes
    quint32 yresOffset = xresOffset + 8;                        // write 8 bytes
    quint32 pixelsOffset = yresOffset + 8;

    // write IFD items (15)
    ifd->writeIFDItem(p, m, 254, 4, 1, 1);                      // SubfileType
    ifd->writeIFDItem(p, m, 256, 3, 1, w);                      // ImageWidth
    ifd->writeIFDItem(p, m, 257, 3, 1, h);                      // ImageHeight
    ifd->writeIFDItem(p, m, 258, 3, 3, bpsOffset);              // BitsPerSample
    ifd->writeIFDItem(p, m, 259, 3, 1, 1);                      // Compression
    ifd->writeIFDItem(p, m, 262, 3, 1, 2);                      // PhotometricInterpretation
    ifd->writeIFDItem(p, m, 273, 4, 1, pixelsOffset);           // StripOffsets
    ifd->writeIFDItem(p, m, 274, 3, 1, 1);                      // Orientation
    ifd->writeIFDItem(p, m, 277, 3, 1, 3);                      // SamplesPerPixel
    ifd->writeIFDItem(p, m, 278, 4, 1, h);                      // RowsPerStrip
    ifd->writeIFDItem(p, m, 279, 4, 1, w*h*3);                  // StripByteCounts
    ifd->writeIFDItem(p, m, 282, 5, 1, xresOffset);             // XResolution
    ifd->writeIFDItem(p, m, 283, 5, 1, yresOffset);             // YResolution
    ifd->writeIFDItem(p, m, 284, 3, 1, 1);                      // PlanarConfiguration
    ifd->writeIFDItem(p, m, 296, 3, 1, 2);                      // ResolutionUnit
    p.file.write(Utilities::put32(0, m.isBigEnd));              // Next IFD = zero

    // write values at offset locations
    p.file.write(Utilities::put16(8, m.isBigEnd));              // BitsPerSample R
    p.file.write(Utilities::put16(8, m.isBigEnd));              // BitsPerSample G
    p.file.write(Utilities::put16(8, m.isBigEnd));              // BitsPerSample B
    p.file.write(Utilities::put32(1200000, m.isBigEnd));        // XResolution Numerator
    p.file.write(Utilities::put32(10000, m.isBigEnd));          // XResolution Denominator
    p.file.write(Utilities::put32(1200000, m.isBigEnd));        // YResolution Numerator
    p.file.write(Utilities::put32(10000, m.isBigEnd));          // YResolution Denominator

    quint32 sopOffset = p.file.pos();   // for debugging

    // create sampled full size rgb to thumbnail
    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerLine = bytesPerPixel * m.width;
    qDebug() << __FUNCTION__ << "Adding thumbnail for" << m.fPath;
    int newBytesPerLine = w * bytesPerPixel;
    quint32 fOffset = 0;
    // read every nth pixel to create new smaller image (thumbnail)
    int y = 0;          // every line in source image scan
    int line = 0;       // lines sampled and written to new image scan
    QByteArray ba;
    for (int strip = 0; strip < stripOffsets.count(); ++strip) {
        for (int row = 0; row < rowsPerStrip; row++) {
            if (y % nth == 0) {
                fOffset = stripOffsets.at(strip) + static_cast<quint32>(row * bytesPerLine);
                ba.clear();
                for (int x = 0; x < w; x++) {
//                    if (x==0&&row==0&&strip==0) qDebug() << __FUNCTION__ << "1st read at" << fOffset;
                    p.file.seek(fOffset);
                    if (fOffset + static_cast<size_t>(bytesPerPixel) > m.offsetThumb) {
                        qDebug() << __FUNCTION__ << "Read nth pixel - ATTEMPTING TO READ PAST END OF SCAN";
                        break;
                    }
                    if (bitsPerSample == 8)
                        ba += p.file.read(bytesPerPixel);
                    else {
                        if (m.isBigEnd) {
                            ba += p.file.read(1);  p.file.skip(1);
                            ba += p.file.read(1);  p.file.skip(1);
                            ba += p.file.read(1);  p.file.skip(1);
                        }
                        else {
                            p.file.skip(1);  ba += p.file.read(1);
                            p.file.skip(1);  ba += p.file.read(1);
                            p.file.skip(1);  ba += p.file.read(1);
                        }
                    }
//                    if (x == 0 && row == 0 && strip == 0) qDebug() << ba.toHex();
                    fOffset += static_cast<uint>(nth * bytesPerPixel);
                }
                // write thumb at EOF
                qint64 eof = p.file.size();
//                qDebug() << __FUNCTION__ << "eof =" << eof << "line =" << line;
                p.file.seek(eof);
                p.file.write(ba);
//                if (line < h) {
//                    if (fOffset + static_cast<size_t>(newBytesPerLine) > m.offsetThumb) {
//                        qDebug() << __FUNCTION__ << "Read scanline - ATTEMPTING TO READ PAST END OF FILE";
//                        break;
//                    }
//                }
                line++;
            }
            if (line > h) break;
            y++;
        }
        if (line > h) break;
    }
    quint32 eopOffset = p.file.pos();

    /*
    qDebug() << __FUNCTION__
             << "m.offsetThumb =" << m.offsetThumb
             << "line =" << line
             << "h =" << h
             << "bytesPerPixel =" << bytesPerPixel
             << "bytesPerLine =" << bytesPerLine
             << "rowsPerStrip =" << rowsPerStrip
             << "stripOffsets.count() =" << stripOffsets.count()
             << "pixelsOffset =" << pixelsOffset
             << "sopOffset =" << sopOffset
             << "eopOffset =" << eopOffset
                ;
                //*/

    return true;
}

void Tiff::toRRGGBBAA(QImage *im)
{
    uchar *scanline = im->bits();
    const int h = im->height();
    const int w = im->width();
    const qsizetype bpl = im->bytesPerLine();
    for (int y = 0; y < h; ++y) {
        quint16 *n = reinterpret_cast<quint16 *>(scanline);
        for (int x = w - 1; x >= 0; --x) {
            n[x * 4 + 3] = 0xffff;
            n[x * 4 + 2] = n[x * 3 + 2];
            n[x * 4 + 1] = n[x * 3 + 1];
            n[x * 4 + 0] = n[x * 3 + 0];
        }
        scanline += bpl;
    }
}

void Tiff::invertEndian16(QImage *im)
{
    uchar *scanline = im->bits();
    const int h = im->height();
    const int w = im->width();
    const qsizetype bpl = im->bytesPerLine();
    for (int y = 0; y < h; ++y) {
        quint16 *n = reinterpret_cast<quint16 *>(scanline);
        for (int x = 0; x < w; ++x) {
            for (int b = 0; b < 3; ++b) {
                int o = x * 3 + b;
                n[o] = (n[o] >> 8) | (n[o] << 8);
            }
        }
        scanline += bpl;
    }
}

void Tiff::sample(ImageMetadata &m, int newLongside, int &nth, int &w, int &h)
{
/*
    Define nth = sample every nth horizontal pixel.

    width       = width of the IFD image chosen
    height      = height of the IFD image chosen
    newLongside = length of the long side of the thumb
    w           = width of the thumbnail
    h           = height of the thumbnail
*/
    w = width;                      // width of thumb to create
    h = height;                     // height of thumb to create
    nth = 1;
    if (!newLongside) return;
    if (width > height) {
        if (newLongside > width) newLongside = width;
        w = newLongside;
        h = w * height / width;
    }
    else {
        if (newLongside > height) newLongside = height;
        h = newLongside;
        w = h * width / height;
    }
    nth = width / w;
    /*
    qDebug() << __FUNCTION__
             << "width =" << width
             << "height =" << height
             << "w =" << w
             << "h =" << h
             << "newLongside" << newLongside
             << "nth =" << nth
                ;
                //*/
}

bool Tiff::unableToDecode()
{
    // cancel if there is compression or planar format
    if (compression > 1 || planarConfiguration > 1 /*|| samplesPerPixel > 3*/) {
        qWarning() << __FUNCTION__ << "compression > 1 || planarConfiguration > 1";
        return true;
    }
    if (bitsPerSample != 16 && bitsPerSample != 8) {
        qWarning() << __FUNCTION__ << "bitsPerSample =" << bitsPerSample;
        return true;
    }
    return false;
}

Tiff::TiffType Tiff::getTiffType()
{
    if (bitsPerSample == 16) return TiffType::tiff16bit;
    else if (bitsPerSample == 8) return TiffType::tiff16bit;
    else return TiffType::unknown;
}
