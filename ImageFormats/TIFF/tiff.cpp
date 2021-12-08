#include "tiff.h"

bool showDebug = false;

class parseInStream
{
/*
    The incoming stream is read 8 bits at a time into a bit buffer called pending. The buffer
    is consumed (most significant) n bits at a time (inCode), where n = codesize. currCode
    starts at 258 and is incremented each time an inCode is consumed. codeSize starts at 9
    bits, and is incremented each time currCode exceeds the bit capacity. codeSize maximum is
    12 bits.

*/
public :
    parseInStream(QDataStream &in)
        :
          codeSize(9),
          input( in ),
          availBits(0),
          pending(0),
          currCode(256),
          nextBump(512)
    {}
    bool operator>>(quint32 &inCode)
    {
        /*
            pending = 32bit buffer                    00000000 00000000 0XXXXXXX XXXXXXXX
            availBits ie 15                                              XXXXXXX XXXXXXXX
            codeSize  ie 9 bits                                          XXXXXXX XX
            availBits -= codeSize                                                  XXXXXX
            mask (1 << availBits) - 1                                              111111
        */
        /* debugging
        QDebug debug = qDebug();
        debug.noquote();
        //*/
        while (availBits < codeSize)
        {
            char c;                                 // 00111011 (example)
            if (!input.readRawData(&c, 1))          // 00000000 00000000 00000000 00XXXXXX
                return false;
            /* debugging
            if (showDebug) {
            debug << "\n" << n << ":";
            debug << "  Incoming char =" ;
            debug << QString::number((quint8)c, 16).toUpper().rightJustified(2, '0') << "h"
                  << QString::number(quint8(c), 10).rightJustified(3)
                  << QString::number((quint8)c, 2).rightJustified(8, '0')
                     ;
            }
                  //*/
            pending <<= 8;                          // 00000000 00000000 00XXXXXX 00000000
            pending |= (c & 0xff);                  // 00000000 00000000 00XXXXXX 00111011
            availBits += 8;
            n++;
        }
        inCode = pending >> (availBits - codeSize);
        /* debugging
        if (showDebug) {
        debug
            << "\npending:" << QString::number(pending, 2).rightJustified(32, '0')
            << "availBits =" << availBits
            << "codeSize =" << codeSize
            << "code =" <<  QString::number(inCode, 2).rightJustified(codeSize, '0') << inCode
            << "\n         ^32     ^24     ^16     ^8              code =" << inCode
            ;
        }
            //*/
        availBits -= codeSize;
        pending &= (1 << availBits) - 1;     // apply mask
        if (inCode == clearCode) {
            codeSize = 9;
            currCode = 256;
            nextBump = 512;
        }
        if (currCode < maxCode) {
            currCode++;
            if (currCode == nextBump - 1) {
                nextBump *= 2;
                codeSize++;
            }
        }
        if (inCode == EOFCode)
            return false;
        else
            return true;
    }
private :
    int codeSize;
    QDataStream & input;
    int availBits;
    quint32 pending;
    quint32 currCode;
    quint32 nextBump;
    quint32 n = 0;
    const unsigned int clearCode = 256;
    const unsigned int EOFCode = 257;
    const unsigned int maxCode = 4093;      // 12 bit max
};

Tiff::Tiff()
{
}

bool Tiff::parse(MetadataParameters &p,
           ImageMetadata &m,
           IFD *ifd,
           IRB *irb,
           IPTC *iptc,
           Exif *exif,
           GPS *gps)
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
    quint32 nextIFDOffset = ifd->readIFD(p, isBigEnd);
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
//        int xmpSegmentLength = static_cast<int>(ifd->ifdDataHash.value(700).tagCount);
//        m.xmpSegmentLength = m.xmpSegmentOffset + static_cast<uint>(xmpSegmentLength);
        m.xmpSegmentLength = static_cast<quint32>(ifd->ifdDataHash.value(700).tagCount);
    }

    p.offset = m.ifd0Offset;
    if (p.report) parseForDecoding(p, ifd);

    /* Search for thumbnail in chained IFDs, IRB and subIFDs.
       - if no thumbnail found then:    m.offsetThumb == m.offsetFull
       - if IRB Jpg thumb found then:   m.lengthThumb > 0
       - if IFD tiff thumb found then:  m.offsetThumb != m.offsetFull
    */
    // IRB:  Get embedded JPG thumbnail if available
    bool foundJpgThumb = false;
    if (ifdPhotoshopOffset) {
        if (p.report) {
            p.hdr = "IRB";
            MetaReport::header(p.hdr, p.rpt);
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
            nextIFDOffset = ifd->readIFD(p, isBigEnd);
            parseForDecoding(p, ifd);
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
            nextIFDOffset = ifd->readIFD(p, isBigEnd);
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
            nextIFDOffset = ifd->readIFD(p, isBigEnd);
            parseForDecoding(p, ifd);
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
            nextIFDOffset = ifd->readIFD(p, isBigEnd);
            // check if IFD is a reduced resolution main image ie maybe the thumbnail
            // SubFileType == 1 (tagid == 254)
            if (!ifd->ifdDataHash.contains(254)) continue;
            if (ifd->ifdDataHash.value(254).tagValue != 1) continue;
            // is image width smaller then full size and less than 640 pixels?
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
        p.offset = m.offsetThumb;        // Smallest preview to use
        encodeThumbnail(p, m, ifd);
        // write thumbnail added to xmp sidecar if writing sidecars
        if (G::useSidecar) {
            Xmp xmp(p.file);
            QByteArray val = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toLatin1();
            xmp.setItem("WinnowAddThumb", val);
            xmp.writeSidecar();
        }
    }

    // EXIF: *******************************************************************

    p.hdr = "IFD Exif";
    p.offset = ifdEXIFOffset;
    if (ifdEXIFOffset) ifd->readIFD(p, isBigEnd);

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
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength);
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
        m._rating = m.rating;
        m._label = m.label;
        m._title = m.title;
        m._creator = m.creator;
        m._copyright = m.copyright;
        m._email  = m.email ;
        m._url = m.url;

        if (p.report) p.xmpString = xmp.metaAsString();
    }

    return true;
}

bool Tiff::isBigEndian(MetadataParameters &p)
{
/*
    Used for decoding.
*/
    // first two bytes is the endian order
    p.file.seek(0);
    quint16 order = Utilities::get16(p.file.read(2));
    if (order == 0x4D4D) return true;
    else return false;
}

bool Tiff::parseForDecoding(MetadataParameters &p, /*ImageMetadata &m, */IFD *ifd)
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

    // endianess
    isBigEnd = isBigEndian(p);
    qDebug() << __FUNCTION__ << p.fPath << "isBigEnd =" << isBigEnd;

    bool isReport = p.report;
    p.report = false;
    ifd->readIFD(p, isBigEnd);
    p.report = isReport;
    err = "";

    // strip offsets
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
                stripOffsets[i] = Utilities::get32(p.file.read(4), isBigEnd);
            }
        }
    }
    else {
        err = "No strip offsets.  \n";
    }

    // strip byte counts
    if (ifd->ifdDataHash.contains(279)) {
        int stripCount = ifd->ifdDataHash.value(279).tagCount;
        stripByteCounts.resize(stripCount);
        if (stripCount == 1) {
            stripByteCounts[0] = static_cast<uint>(ifd->ifdDataHash.value(279).tagValue);
        }
        else {
            quint32 offset = ifd->ifdDataHash.value(279).tagValue;
            p.file.seek(offset);
            for (int i = 0; i < stripCount; ++i) {
                stripByteCounts[i] = Utilities::get32(p.file.read(4), isBigEnd);
            }
        }
    }
    else {
        err = "No StripByteCounts.  \n";
    }

    // IFD: bitsPerSample
    if (ifd->ifdDataHash.contains(258)) {
        p.file.seek(static_cast<int>(ifd->ifdDataHash.value(258).tagValue));
        bitsPerSample = Utilities::get16(p.file.read(2), isBigEnd);
    }
    else {
       err = "No BitsPerSample.  \n";
    }
    if (bitsPerSample != 8 && bitsPerSample != 16) {
        err = "BitsPerSample = " + QString::number(bitsPerSample) + ". Only 8 and 16 supported.";
    }

    // IFD: predictor
    (ifd->ifdDataHash.contains(317))
            ? predictor = ifd->ifdDataHash.value(317).tagValue
            : predictor = 0;

    // IFD: compression
    (ifd->ifdDataHash.contains(259))
            ? compression = static_cast<int>(ifd->ifdDataHash.value(259).tagValue)
            : compression = 1;
    if (!(compression == 1  || compression == 5)) {
        err = "Compression != 1 or 5.  \n";
    }
    switch (compression) {
    case 1:
        compressionType = NoCompression;
        break;
    case 5:
        if (predictor == 2) compressionType = LzwPredictorCompression;
        else compressionType = LzwCompression;
        break;
    case 8:
        compressionType = ZipCompression;
    }

    // IFD: width
    (ifd->ifdDataHash.contains(256))
        ? width = static_cast<int>(ifd->ifdDataHash.value(256).tagValue)
        : width = 0;
    if (width == 0) {
        err = "Width = 0.  \n";
    }

    // IFD: height
    (ifd->ifdDataHash.contains(257))
        ? height = static_cast<int>(ifd->ifdDataHash.value(257).tagValue)
        : height = 0;
    if (height == 0) {
        err = "Height = 0.  \n";
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
        err = "SamplesPerPixel is undefined.  \n";
    }

    // IFD: rowsPerStrip
    (ifd->ifdDataHash.contains(278))
            ? rowsPerStrip = static_cast<int>(ifd->ifdDataHash.value(278).tagValue)
            : rowsPerStrip = height;

    // IFD: planarConfiguration
    (ifd->ifdDataHash.contains(284))
            ? planarConfiguration = ifd->ifdDataHash.value(284).tagValue
            : planarConfiguration = 1;
    if (planarConfiguration == 2 && compression == 5) {
        err = "LZW compression not supported for per channel planar configuration.  \n";
    }
    if (err != "") qDebug() << __FUNCTION__ /*<< fPath*/ << err;
    if (err != "" && !isReport) return false;

    // rgh used for debugging - req'd?
    G::tiffData = "BitsPerSample:" + QString::number(bitsPerSample) +
                  " Compression:" + QString::number(compression) +
                  " Predictor:" + QString::number(predictor) +
                  " PlanarConfiguration:" + QString::number(planarConfiguration)
                    ;

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
        p.rpt.setFieldWidth(w1); p.rpt << "Predictor" << predictor; p.rpt.setFieldWidth(0); p.rpt << "\n";
    }

    return true;
}

/*  DECODING is required to produce a QImage from the tiff file.

    The QImage may be the full scale or a reduced scale for a thumbnail.  If the tiff is
    uncompressed the thumbnail scale can be sampled directly, which is the fastest.  If the
    tiff is compressed then the entire full scale QImage must be produced and then scaled
    down to the thumbnail size.

    If the "add missing thumbnails" preference is set, then the scaled QImage is added to the
    tiff to facilitate faster thumb loading in the future.
*/

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
        G::error(__FUNCTION__, fPath, "Unable to open file.");
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
    return decode(/*m,*/ p, image, newSize);
}

bool Tiff::decode(QString fPath, quint32 offset, QImage &image)
{
/*
   The version is used by decoders in image cache.
*/
    if (G::isLogger) G::log(__FUNCTION__, " load file from fPath");
    QFileInfo fileInfo(fPath);
    if (!fileInfo.exists()) return false;                 // guard for usb drive ejection

    MetadataParameters p;
    p.fPath = fPath;
    p.file.setFileName(fPath);
    if (!p.file.open(QIODevice::ReadOnly)) {
        G::error(__FUNCTION__, fPath, "Unable to open file.");
        return false;
    }
    p.offset = offset;
    qDebug() << __FUNCTION__ << fPath << "offset =" << offset;
    return decode(p, image);
}

bool Tiff::decode(/*ImageMetadata &m,*/ MetadataParameters &p, QImage &image, int newSize)
{
/*
    Decode tiff into QImage &image.

    p.file must be set and opened. If called from ImageDecoder then p.file will be mapped
    to memory.

    p.offset must be set to ifdOffset that describes the image to be decoded within the
    tiff before calling this function (either m.offsetFull or m.offsetThumb).

    newSize is the resized long side in pixels.  If newSize = 0 then no resizing.

*/
    if (G::isLogger) G::log(__FUNCTION__, "Main decode with p.file assigned");
    IFD *ifd = new IFD;
    p.report = false;
    if (!parseForDecoding(p, /*m, */ifd)) {
        return false;
    }

    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerRow = (uint)(bytesPerPixel * width);
    scanBytesAvail = (uint)(width * height * bytesPerPixel);

    if (bitsPerSample == 16) im = new QImage(width, height, QImage::Format_RGBX64);
    if (bitsPerSample == 8)  im = new QImage(width, height, QImage::Format_RGB888);

    if (compression == 1) decodeBase(/*m, */p, image);
    if (compression == 5) decodeLZW(p, image);

    p.file.close();

    if (bitsPerSample == 16) {
        if (isBigEnd) invertEndian16(im);
        toRRGGBBAA(im);
    }

    // if per channel convert to interleaved
    if (planarConfiguration == 2) {
        im->convertTo(QImage::Format_RGB888);
        perChannelToInterleave(im);
    }

    // convert to standard QImage format for display in Winnow
    im->convertTo(QImage::Format_RGB32);

    // scale
    if (newSize) {
        *im = im->scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    image.operator=(*im);
    return true;
}

void Tiff::decodeBase(MetadataParameters &p, QImage &image)
{
    qDebug() << __FUNCTION__ << p.fPath;
    int strips = stripOffsets.count();
    int line = 0;
    quint32 scanBytes = 0;
    for (int strip = 0; strip < strips; ++strip) {
        quint32 stripBytes = stripByteCounts.at(strip);
        QByteArray ba;
        p.file.seek(stripOffsets.at(strip));
        ba = p.file.read(stripBytes);
        QBuffer buf(&ba);
        buf.open(QBuffer::ReadOnly);
        for (int row = 0; row < rowsPerStrip; row++) {
            /*
            qDebug() << __FUNCTION__
                     << "strip =" << strip
                     << "row =" << row
                     << "buf.pos() =" << buf.pos()
                     << "bytesPerRow =" << bytesPerRow
                     << "buf.pos() + bytesPerRow =" << buf.pos() + bytesPerRow
                     << "ba.length() =" << ba.length()
                     << "scanBytes =" << scanBytes
                     << "scanBytesAvail =" << scanBytesAvail
                        ;
                        //*/
            // last strip may have less than rowsPerStrip rows
            if ((buf.pos() + bytesPerRow) > ba.length()) break;
            if (scanBytes + bytesPerRow > scanBytesAvail) break;
            std::memcpy(im->scanLine(line++),
                        buf.read(bytesPerRow),
                        static_cast<size_t>(bytesPerRow));
            scanBytes += bytesPerRow;
        }
    }
}

bool Tiff::decodeLZW(MetadataParameters &p, QImage &image)
{
    int strips = stripOffsets.count();
    TiffStrips tiffStrips;
    QFuture<void> future;
    // pass info to concurrent threads via TiffStrip for each strip
    TiffStrip tiffStrip;
    for (int strip = 0; strip < strips; ++strip) {
        tiffStrip.strip = strip;
        tiffStrip.bitsPerSample = bitsPerSample;
        tiffStrip.bytesPerRow = bytesPerRow;
        tiffStrip.stripBytes = stripByteCounts.at(strip);
        if (predictor == 2) tiffStrip.predictor = true;
        // read tiff strip into QByteArray
        p.file.seek(stripOffsets.at(strip));
        inBa.append(p.file.read(stripByteCounts.at(strip)));
        // ptr to start of strip QByteArray
        tiffStrip.in = inBa[strip].data();
        // length of incoming strip in bytes
        tiffStrip.incoming = inBa[strip].size();
        tiffStrip.out = im->scanLine(strip * rowsPerStrip);
        /* debugging
        tiffStrip.fName = p.file.fileName();
        tiffStrip.rowsPerStrip = rowsPerStrip;
        */
        tiffStrips.append(tiffStrip);
    }
    future = QtConcurrent::map(tiffStrips, lzwDecompress);
    future.waitForFinished();
    return true;
}

bool Tiff::encodeThumbnail(MetadataParameters &p, ImageMetadata &m, IFD *ifd)
{
/*
    If an image preview (thumbnail) with a longside <= 512px does not exist, then add a
    thumbnail with a longside of G::maxIconSize to the tiff file.  This involves appending an
    IFD to the end of the IFD chain and sampling the smallest existing IFD preview (source) to
    the new thumbnail.

    Steps:
        - replace last nextIFDOffset with offset to current EOF, where the thumbnail IFD will
          be appended.
        - add new IFD at end of chain, starting with the number of IFD items (15).
        - add IFD items:
            Num  tagId tagType  tagCount    tagValue   tagDescription
              0    254       4         1           1   SubfileType
              1    256       3         1               ImageWidth (thumbnail)
              2    257       3         1               ImageHeight (thumbnail)
              3    258       3         3      offset   BitsPerSample
              4    259       3         1           1   Compression
              5    262       3         1           2   PhotometricInterpretation
              6    273       4         1       value   StripOffsets
              7    274       3         1           1   Orientation
              8    277       3         1           3   SamplesPerPixel
              9    278       3         1      height   RowsPerStrip
             10    279       4         1       value   StripByteCounts
             11    282       5         1      offset   XResolution
             12    283       5         1      offset   YResolution
             13    284       3         1           1   PlanarConfiguration
             14    296       3         1           2   ResolutionUnit
        - subsample image down to thumbnail resolution
        - write strip pixels rgb at offset StripOffsets

    p.offset must be set to ifdOffset that describes the source image to be sampled to create
    the thumbnail before calling this function.
*/
    // get decoding parameters from source IFD (p.offset must be preset)
    if (!parseForDecoding(p, ifd)) {
        return false;
    }

    // go to last main IFD offset and replace with offset to new IFD at end of file
    m.offsetThumb = p.file.size();
    p.file.seek(lastIFDOffsetPosition);
    p.file.write(Utilities::put32(m.offsetThumb, m.isBigEnd));

    // Go to new thumbIFDOffset and write the count of IFD entries (15)
    p.offset = m.offsetThumb;
    ifd->writeIFDCount(p, m, 15);

    // thumbnail size and sample increment (nth)
    int w, h, nth;
    sample(m, G::maxIconSize, nth, w, h);

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

    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerRow = (uint)(bytesPerPixel * width);
    scanBytesAvail = (uint)(width * height * bytesPerPixel);

    if (bitsPerSample == 16) im = new QImage(width, height, QImage::Format_RGBX64);
    if (bitsPerSample == 8)  im = new QImage(width, height, QImage::Format_RGB888);

    if (compression == 1) decodeBase(/*m, */p, *im);
    if (compression == 5) decodeLZW(p, *im);

    if (bitsPerSample == 16) {
        if (m.isBigEnd) invertEndian16(im);
        toRRGGBBAA(im);
        im->convertTo(QImage::Format_RGB888);
    }

    QImage image = im->scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio, Qt::FastTransformation);

    // byte array thumb
    QByteArray bat;
    bat.resize(image.width() * image.height() * 3);
    int scanWidth = image.width() * 3;
    int batPos = 0;
    // copy QImage (scanline 32-bit aligned) to bat line by line
    for (int y = 0; y < image.height(); ++y) {
        std::memcpy(bat.data() + batPos, image.scanLine(y), (size_t)scanWidth);
        batPos += scanWidth;
    }

    delete im;

    // write thumb at EOF
    p.file.seek(p.file.size());
    p.file.write(bat);

    return true;
}

void Tiff::perChannelToInterleave(QImage *im1)
{
    // copy image
    QImage im2;
    im2.operator=(*im1);
    // ptrs to both images
    uchar *nn = im2.scanLine(0);
    uchar *n = im2.bits();
    uchar *o = im1->bits();
    int bc = im2.byteCount();
    int a = im2.bitPlaneCount();
    const int p = im2.height() * im2.width();
    for (int i = 0; i < p; ++i) {
        o[i*3+0] = n[i+0*p];
        o[i*3+1] = n[i+1*p];
        o[i*3+2] = n[i+2*p];
    }
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
    int nthW = width / w;
    int nthH = height / h;
    nthW < nthH ? nth = nthW : nth = nthH;
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

void Tiff::scaleFromQImage(QImage &im, QByteArray &bas, int newLongSide)
{
    int nth;
    bas.clear();
    for (int y = 0; y < im.height(); ++y) {
        if (y % nth == 0) {

        }
    }
}

Tiff::TiffType Tiff::getTiffType()
/*
    Not used.  Left as example for using enum
*/
{
    if (bitsPerSample == 16) return TiffType::tiff16bit;
    else if (bitsPerSample == 8) return TiffType::tiff16bit;
    else return TiffType::unknown;
}

void Tiff::lzwReset(QHash<quint32, QByteArray> &dictionary,
                    QByteArray &prevString,
                    quint32 &nextCode
                    )
{
    // initialize dictionary
    dictionary.clear();
    for (uint i = 0 ; i < 256 ; i++ ) {
        dictionary[i] = QByteArray(1, (char)i);
    }
    nextCode = 258;
    prevString.clear();
}

Tiff::TiffStrips Tiff::lzwDecompress(TiffStrip t)
{
/*
    Decompress a tiff strip that has LZW Prediction compression.

    The algorithm to decompress LZW (from TIFF6 Specification):

    while ((Code = GetNextCode()) != EoiCode) {
        if (Code == ClearCode) {
            InitializeTable();
            Code = GetNextCode();
            if (Code == EoiCode)
                break;
            WriteString(StringFromCode(Code));
            OldCode = Code;
        } // end of ClearCode case
        else {
            if (IsInTable(Code)) {
                WriteString(StringFromCode(Code));
                AddStringToTable(StringFromCode(OldCode)+FirstChar(StringFromCode(Code)));
                OldCode = Code;
            } else {
                OutString = StringFromCode(OldCode) + FirstChar(StringFromCode(OldCode));
                WriteString(OutString);
                AddStringToTable(OutString);
                OldCode = Code;
            }
        } // end of not-ClearCode case
    } // end of while loop

    The prediction variant uses the difference between row pixels of the same color channel
    for the code value.

    Alternatives to std::memcpy to improve performance.

        // option for byte-by-byte

        char* pDst = s[nextCode];
        char* pSrc = (char*)&ps;
        for (size_t i = 0; i != psLen; ++i) pDst[i] = pSrc[i];

        // or

        char* pDst = s[nextCode];
        char* pSrc = (char*)&ps;
        size_t len = psLen;
        while (len--)
        {
            *pDst++ = *pSrc++;
        }

        // option for 4 bytes at a time

        uint32_t* pDst = (uint32_t*)s[nextCode];
        uint32_t* pSrc = (uint32_t*)&ps;
        size_t len = psLen / 4 + 1;
        for (size_t i = 0; i != len; ++i) pDst[i] = pSrc[i];
*/
    TiffStrips tiffStrips;
    int alphaRowComponent = t.bytesPerRow / 3;
    /*
    qDebug() << __FUNCTION__
             << "t.predictor =" << t.predictor
             << "t.bytesPerRow =" << t.bytesPerRow
             << "t.fName =" << t.fName
                ;
                //*/

    const unsigned int clearCode = 256;
    const unsigned int EOFCode = 257;
    const unsigned int maxCode = 4095;              // 12 bit max

    // input and output pointers
    char* c = t.in;
    uchar* out = t.out;

    char* s[4096];                                  // ptrs in strings for each possible code
    int8_t sLen[4096];                              // code string length
    std::memset(&sLen, 1, 256);                     // 0-255 one char strings

    char strings[128000];
    // initialize first 256 code strings
    for (int i = 0 ; i != 256 ; i++ ) {
        strings[i] = (char)i;
        s[i] = &strings[i];
    }
    strings[256] = 0;  s[256] = &strings[256];      // Clear code
    strings[257] = 0;  s[257] = &strings[257];      // EOF code
    char* sEnd;                                     // ptr to current end of strings

    char ps[256];                                   // previous string
    size_t psLen = 0;                               // length of prevString
    uint32_t code;                                  // key to string for code
    uint32_t nextCode = 258;                        // used to preset string for next
    uint n = 0;                                     // output byte counter
    uint32_t iBuf = 0;                              // incoming bit buffer
    int32_t nBits = 0;                              // incoming bits in the buffer
    int32_t codeBits = 9;                           // number of bits to make code (9-12)
    uint32_t nextBump = 511;                        // when to increment code size 1st time
    uint32_t pBuf = 0;                              // previous out bit buffer
    uint32_t mask = (1 << codeBits) - 1;            // extract code from iBuf

    uint32_t* pSrc;                                 // ptr to src for word copies
    uint32_t* pDst;                                 // ptr to dst for word copies


    // read incoming bytes into the bit buffer (iBuf) using the char pointer c
    while (t.incoming) {
        /*
        The incoming stream is read 8 bits at a time into a bit buffer called iBuf. The buffer
        is consumed (most significant) codeBits at a time (code). codeBits starts at 9 bits,
        and is incremented each time nextCode exceeds the bit capacity. codeSize maximum is 12
        bits. nextCode starts at 258 and is incremented each time an code is consumed.
        */
        // GetNextCode
        iBuf = (iBuf << 8) | (uint8_t)*c++;         // make room in bit buf for char
        nBits += 8;
        --t.incoming;
        if (nBits < codeBits) {
            iBuf = (iBuf << 8) | (uint8_t)*c++;     // make room in bit buf for char
            nBits += 8;
            --t.incoming;
        }
        code = (iBuf >> (nBits - codeBits)) & mask; // extract code from buffer
        nBits -= codeBits;                          // update available bits to process

        // reset at start and when codes = max ~ 4094
        if (code == clearCode) {
            codeBits = 9;
            mask = (1 << codeBits) - 1;
            nextBump = 511;
            sEnd = s[257];
            nextCode = 258;
            psLen = 0;
            continue;
        }

        // finished (should not need as incoming counts down to zero)
        if (code == EOFCode) {
            return tiffStrips;
        }

        // new code then add prevString + prevString[0]
        // copy prevString
        if (code == nextCode) {
            s[code] = sEnd;
            switch(psLen) {
            case 1:
                *s[code] = ps[0];
                break;
            case 2:
                *s[code] = ps[0];
                *(s[code]+1) = ps[1];
                break;
            case 4:
                pDst = (uint32_t*)s[code];
                pSrc = (uint32_t*)&ps;
                *pDst = *pSrc;
                break;
            case 5:
            case 6:
            case 7:
            case 8:
                pDst = (uint32_t*)s[nextCode];
                pSrc = (uint32_t*)&ps;
                *pDst = *pSrc;
                *(pDst+1) = *(pSrc+1);
                break;
            default:
                std::memcpy(s[code], &ps, psLen);
            }

            // copy prevString[0]
            *(s[code] + psLen) = ps[0];
            sLen[code] = (int8_t)psLen + 1;
            sEnd = s[code] + psLen + 1;
        }

        if (t.predictor) {
            // output char string for code (add from left)
            // pBuf   00000000 11111111 22222222 33333333
            for (uint32_t i = 0; i != (uint32_t)sLen[code]; i++) {
                if (n % t.bytesPerRow < 3) *out++ = *(s[code] + i);
                else *out++ = (*(s[code] + i) + *(out - 3));
                ++n;
            }
        }
        else if (t.bitsPerSample == 8) {
            for (uint32_t i = 0; i != (uint32_t)sLen[code]; i++) {
                *out++ = (uchar)*(s[code] + i);
            }
        }
        else if (t.bitsPerSample == 16) {
            for (uint32_t i = 0; i != (uint32_t)sLen[code]; i++) {
//                if (n > 0 && n % t.bytesPerRow == 0) out += alphaRowComponent;
                *out++ = (uchar)*(s[code] + i);
                ++n;
                if (n % t.bytesPerRow == 0) out += alphaRowComponent;
            }
        }

        // add string to nextCode (prevString + strings[code][0])
        // copy prevString
        if (psLen/* && nextCode <= MAXCODE*/) {
            s[nextCode] = sEnd;
            switch(psLen) {
            case 1:
                *s[nextCode] = ps[0];
                break;
            case 2:
                *s[nextCode] = ps[0];
                *(s[nextCode]+1) = ps[1];
                break;
            case 4:
                pDst = (uint32_t*)s[nextCode];
                pSrc = (uint32_t*)&ps;
                *pDst = *pSrc;
                break;
            case 5:
            case 6:
            case 7:
            case 8:
                pDst = (uint32_t*)s[nextCode];
                pSrc = (uint32_t*)&ps;
                *pDst = *pSrc;
                *(pDst+1) = *(pSrc+1);
                break;
            default:
                std::memcpy(s[nextCode], &ps, psLen);
            }

            // copy strings[code][0]
            *(s[nextCode] + psLen) = *s[code];

            sLen[nextCode] = (int8_t)(psLen + 1);
            sEnd = s[nextCode] + psLen + 1;
            ++nextCode;
        }

        // strings[code][0] copy
        switch(sLen[code]) {
        case 1:
            ps[0] = *s[code];
            break;
        case 2:
            ps[0] = *s[code];
            ps[1] = *(s[code]+1);
            break;
        case 4:
            pSrc = (uint32_t*)s[code];
            pDst = (uint32_t*)&ps;
            *pDst = *pSrc;
            break;
        case 5:
        case 6:
        case 7:
        case 8:
            pSrc = (uint32_t*)s[code];
            pDst = (uint32_t*)&ps;
            *pDst = *pSrc;
            *(pDst+1) = *(pSrc+1);
            break;
        default:
            memcpy(&ps, s[code], (size_t)sLen[code]);
        }

        psLen = (size_t)sLen[code];

        // codeBits change
        if (nextCode == nextBump) {
            if (nextCode < maxCode) {
                nextBump = (nextBump << 1) + 1;
                ++codeBits;
                mask = (1 << codeBits) - 1;
            }
            else if (nextCode == maxCode) continue;
            else {
                codeBits = 9;
                mask = (1 << codeBits) - 1;
                nextBump = 511;
                sEnd = s[257];
                nextCode = 258;
                psLen = 0;
            }
        }

    } // end while
    qDebug() << __FUNCTION__ << "finish thread" << QThread::currentThreadId();

    return tiffStrips;
}
