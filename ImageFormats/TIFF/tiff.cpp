#include "tiff.h"

const unsigned int CLEAR_CODE = 256;
const unsigned int EOF_CODE = 257;
const unsigned int MAXCODE = 4093;      // 12 bit max

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
        if (inCode == CLEAR_CODE) {
            codeSize = 9;
            currCode = 256;
            nextBump = 512;
        }
        if (currCode < MAXCODE) {
            currCode++;
            if (currCode == nextBump - 1) {
                nextBump *= 2;
                codeSize++;
            }
        }
        if (inCode == EOF_CODE)
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
        p.offset = m.offsetThumb;        // Smallest preview to use
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
                stripOffsets[i] = Utilities::get32(p.file.read(4), m.isBigEnd);
            }
        }
    }
    else {
        err += "No strip offsets.  ";
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

    // IFD: predictor
    (ifd->ifdDataHash.contains(317))
            ? predictor = ifd->ifdDataHash.value(317).tagValue
            : predictor = 0;

    // IFD: compression
    (ifd->ifdDataHash.contains(259))
            ? compression = static_cast<int>(ifd->ifdDataHash.value(259).tagValue)
            : compression = 1;
    if (!(compression == 1  || compression == 5)) {
        err += "Compression != 1 0r 5.  ";
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

    if (err != "" && !isReport) return false;

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

void Tiff::decompressStrip(int strip, MetadataParameters &p, QByteArray &ba)
{
    QByteArray inBa;
    quint32 stripBytes = stripByteCounts.at(strip);
    p.file.seek(stripOffsets.at(strip));
//    qDebug() << __FUNCTION__ << "compressionType" << compressionType;
    switch (compressionType) {
    case NoCompression:
//        qDebug() << __FUNCTION__ << "using no compression";
        ba = p.file.read(stripBytes);
        break;
    case LzwCompression:
//        qDebug() << __FUNCTION__ << "using LZW compression";
        inBa = p.file.read(stripBytes);
        lzwDecompress(inBa, ba);
        break;
    case LzwPredictorCompression:
//        qDebug() << __FUNCTION__ << "using LZW Prediction compression";
        inBa = p.file.read(stripBytes);
        lzwPredictorDecompress(inBa, ba);
        break;
    }
}

bool Tiff::decode(ImageMetadata &m, QString &fPath, QImage &image, bool thumb, int newSize)
{
/*
    Decode using unmapped QFile.  Set p.file, p.offset and call main decode.
*/
//    qDebug() << __FUNCTION__ << fPath << "unmapped";
    if (G::isLogger) G::log(__FUNCTION__, " load file from fPath");
    QFileInfo fileInfo(fPath);
    if (!fileInfo.exists()) return false;                 // guard for usb drive ejection

    MetadataParameters p;
    p.file.setFileName(fPath);
    if (!p.file.open(QIODevice::ReadOnly)) {
        m.err += "Unable to open file " + fPath + ". ";
        qWarning() << __FUNCTION__ << m.err;
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
    Decode tiff into QImage &image.

    p.file must be set and opened. If called from ImageDecoder then p.file will be mapped
    to memory.

    p.offset must be set to ifdOffset that describes the image to be decoded within the
    tiff before calling this function (either m.offsetFull or m.offsetThumb).

    newSize is the resized long side in pixels.  If newSize = 0 then no resizing.

*/
    if (G::isLogger) G::log(__FUNCTION__, "Main decode with p.file assigned");
    /* timer
    QElapsedTimer t;
    t.restart();
    //*/
//    qDebug() << __FUNCTION__ << m.fPath;
    IFD *ifd = new IFD;
    p.report = false;
    if (!parseForDecoding(p, m, ifd)) {
        qWarning() << err + m.fPath;
        return false;
    }

    /*
    qDebug() << __FUNCTION__
             << "IFD width =" << width
             << "IFD height =" << height
                ;
                //*/

    // width and height are for the IFD (not necessarily the full image) from parseForDecoding
    int w = width;
    int h = height;
    int nth = 1;
    // if newSize <= thumbnail then do not sample and resize
    if (newSize > G::maxIconSize)
        sample(m, newSize, nth, w, h);
    else
        newSize = 0;

    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerRow = bytesPerPixel * w;
    scanBytesAvail = w * h * bytesPerPixel;

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

    // read every nth pixel to create new smaller image (thumbnail)
    if (newSize) {
        qDebug() << __FUNCTION__ << m.fPath << "Resampling tiff";
        int newBytesPerLine = w * bytesPerPixel;
        QByteArray newLine;
        // y is every line in the image (row = every line in the strip)
        int y = 0;
        // sampleY is every nth line in the image
        int sampleY = 0;
        // sampleX is every nth pixel in a row (less one just read)
        int sampleX = static_cast<int>((nth - 1) * bytesPerPixel);
        int strips = stripOffsets.count();
        for (int strip = 0; strip < strips; ++strip) {
//            qDebug() << __FUNCTION__ << "Processing strip" << strip;
            QByteArray ba;      // strip byte array after decompression
            decompressStrip(strip, p, ba);
            QBuffer bufStrip(&ba);
            bufStrip.open(QBuffer::ReadOnly);
            // iterate rows in this strip
            for (int row = 0; row < rowsPerStrip; row++) {
                // last strip may have less than rowsPerStrip rows
                if ((row + 1) * bytesPerRow > ba.length()) break;

                // sample row
                if (y % nth == 0) {
                    bufStrip.seek(row * bytesPerRow);
                    newLine.clear();
                    // sample nth pixel in row, where w = new width of image
                    for (int x = 0; x < w; x++) {
                        newLine += bufStrip.read(bytesPerPixel);
                        bufStrip.skip(sampleX);
                    }
                    if (sampleY < h) {
                        std::memcpy(im->scanLine(sampleY),
                                    newLine,
                                    static_cast<size_t>(newBytesPerLine));
                    }
                    sampleY++;
                }
                y++;
                if (sampleY >= h) break;
            }
            if (sampleY >= h) break;
        }
        p.file.close();
        if (bitsPerSample == 16) {
            if (m.isBigEnd) invertEndian16(im);
            toRRGGBBAA(im);
        }
        // convert to standard QImage format for display in Winnow
        im->convertTo(QImage::Format_RGB32);
        image.operator=(*im);
        qDebug() << __FUNCTION__ << m.fPath << "Resampling entire tiff";
        return true;
    }

    /*
    // read every nth pixel (old code)
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
                    fOffset = stripOffsets.at(strip) + static_cast<quint32>(row * bytesPerRow);
                    ba.clear();
                    for (int x = 0; x < w; x++) {
                        p.file.seek(fOffset);
                        if (fOffset + static_cast<size_t>(bytesPerPixel) > fSize) {
                            qDebug() << __FUNCTION__
                                     << "Read nth pixel - ATTEMPTING TO READ PAST END OF FILE"
                                     << "nth =" << nth
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
    */

    // read entire image
//    qDebug() << __FUNCTION__ << m.fPath << "Decoding entire tiff";
    int line = 0;
    quint32 scanBytes = 0;
    int strips = stripOffsets.count();
//    G::track("Start stipts");
    for (int strip = 0; strip < strips; ++strip) {
//        G::track("Strip " + QString::number(strip));
//        if (strip > 3) continue;
//        qDebug() << __FUNCTION__ << "Processing strip" << strip;
        QByteArray ba;      // strip byte array after decompression
        decompressStrip(strip, p, ba);
//        G::track("decompressStrip");
//        qDebug() << __FUNCTION__
//                 << "Strip #" << strip
////                 << Utilities::hexFromByteArray(ba, 50, 0, 500)
//                    ;
//        if (strip == 0) Utilities::hexFromByteArray(ba, 50, 16250, 16300);
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
//            if ((row + 1) * bytesPerRow > ba.length()) break;
            if ((buf.pos() + bytesPerRow) > ba.length()) break;
            if (scanBytes + bytesPerRow > scanBytesAvail) break;
            std::memcpy(im->scanLine(line++),
                        buf.read(bytesPerRow),
                        static_cast<size_t>(bytesPerRow));
            scanBytes += bytesPerRow;
        }
//        G::track("scan strip");
    }
//    qDebug() << __FUNCTION__ << "Processed all strips";
    p.file.close();

    /*
    // read entire image old code
    int line = 0;
    int fOffset = 0;
    p.file.seek(stripOffsets.at(0));
    for (int strip = 0; strip < stripOffsets.count(); ++strip) {
        for (int row = 0; row < rowsPerStrip; row++) {
            fOffset = stripOffsets.at(strip) + static_cast<quint32>(row * bytesPerLine);
            if (fOffset + static_cast<size_t>(bytesPerLine) > fSize) {
                qWarning() << __FUNCTION__
                           << "Read entire image - ATTEMPTING TO READ PAST END OF FILE"
                         << "fOffset =" << fOffset
                         << "strip =" << strip
                         << "row =" << row
                         << "stripOffsets.at(strip) =" << stripOffsets.at(strip)
                         << "bytesPerLine =" << bytesPerLine
                         << "w =" << w
                         << m.fPath
                            ;
                break;
            }
            p.file.seek(fOffset);
            std::memcpy(im->scanLine(line),
                        p.file.read(bytesPerLine),
                        static_cast<size_t>(bytesPerLine));
            line++;
        }
    }
    */
    if (bitsPerSample == 16) {
        if (m.isBigEnd) invertEndian16(im);
        toRRGGBBAA(im);
    }
    // convert to standard QImage format for display in Winnow
    im->convertTo(QImage::Format_RGB32);
    image.operator=(*im);
//    qDebug() << __FUNCTION__ << m.fPath << "Decoded entire tiff";
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
    if (!parseForDecoding(p, m, ifd)) {
        qWarning() << err + m.fPath;
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

    quint32 sopOffset = p.file.pos();   // for debugging

    // sample existing tiff in IFD to create new thumbnail IFD
    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerRow = bytesPerPixel * m.width;
    quint32 fOffset = 0;
    // read every nth pixel to create new smaller image (thumbnail)
    int y = 0;          // every line in source image scan
    int line = 0;       // lines sampled and written to new image scan
    QByteArray bat;     // byte array thumb
    for (int strip = 0; strip < stripOffsets.count(); ++strip) {
        for (int row = 0; row < rowsPerStrip; row++) {
            if (y % nth == 0) {
                fOffset = stripOffsets.at(strip) + static_cast<quint32>(row * bytesPerRow);
                bat.clear();
                for (int x = 0; x < w; x++) {
                    /*
                    if (x==0&&row==0&&strip==0) qDebug() << __FUNCTION__ << "1st read at" << fOffset;
                    //*/
                    p.file.seek(fOffset);
                    if (fOffset + static_cast<size_t>(bytesPerPixel) > m.offsetThumb) {
                        qWarning() << __FUNCTION__ << "Read nth pixel - ATTEMPTING TO READ PAST END OF SCAN";
                        break;
                    }
                    if (bitsPerSample == 8)
                        bat += p.file.read(bytesPerPixel);
                    else {
                        if (m.isBigEnd) {
                            bat += p.file.read(1);  p.file.skip(1);
                            bat += p.file.read(1);  p.file.skip(1);
                            bat += p.file.read(1);  p.file.skip(1);
                        }
                        else {
                            p.file.skip(1);  bat += p.file.read(1);
                            p.file.skip(1);  bat += p.file.read(1);
                            p.file.skip(1);  bat += p.file.read(1);
                        }
                    }
                    /*
                    if (x == 0 && row == 0 && strip == 0) qDebug() << bat.toHex();
                    //*/
                    fOffset += static_cast<uint>(nth * bytesPerPixel);
                }
                // write thumb at EOF
                qint64 eof = p.file.size();
                p.file.seek(eof);
                p.file.write(bat);
                line++;
            }
            if (line > h) break;
            y++;
        }
        if (line > h) break;
    }
    quint32 eopOffset = p.file.pos();   // for debugging

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

void Tiff::lzwDecompress(QByteArray &inBa, QByteArray &outBa)
{
//    qDebug() << __FUNCTION__;
    QBuffer inBuf(&inBa);
    QBuffer outBuf(&outBa);
    inBuf.open(QIODevice::ReadOnly);
    outBuf.open(QIODevice::WriteOnly);
    QDataStream inStream(&inBuf);
    QDataStream out(&outBuf);

    parseInStream in(inStream);
    /* debugging
    //
        QFile f1("D:/Pictures/_TIFF_lzw1/ps_8_big_800px.tif");
        f1.open(QIODevice::ReadOnly);
        f1.seek(34296);
        QByteArray baPs = f1.read(1080000);
        f1.close();
    //*/
    int n = 0;      // for debugging

    // hash of code => byte array (string of char)
    QHash<quint32,QByteArray> dictionary;
    // the value of the prior code in the in stream
    QByteArray prevString;
    // code is the index (key) in the dictionary hash
    quint32 code;
    quint32 nextCode = 258;
    while (in >> code) {
        // reset if clear code = 256. Occurs at start and before dictionary code 4094 (<13 bits)
        if (code == CLEAR_CODE) {
            lzwReset(dictionary, prevString, nextCode);
            continue;
        }
        // if code not found then add to dictionary
        /*
        if (dictionary.find(code) == dictionary.end()) {
        //*/
        if (!dictionary.contains(code)) {
            dictionary[code] = prevString + prevString[0];
        }

        // output entire dictionary code value
        for (auto& ch : dictionary[code])
            out << ch;
        //
        if (prevString.size() && nextCode <= MAXCODE)
            dictionary[nextCode++] = prevString + dictionary[code][0];
        /*
        // debug
//        if (debug) {
//            quint8 baPsX = (0xff & (unsigned int)baPs.at(n));
//    //        bool isOkay = dictionary[code].at(0) == baPs.at(n);
//    //        if (!isOkay) {
//                qDebug().noquote()
//                << "n =" << QString::number(n).leftJustified(5)
//                << "code =" << QString::number(code).leftJustified(5)
//                         << QString::number(code, 16).toUpper().leftJustified(5)
//                << "dict[code] =" << dictionary[code].toHex().toUpper().leftJustified(12)
//                << "dictionary[code][0] =" << QString::number(quint8(dictionary[code][0]), 16)
//                << "baPsX = " << QString::number(baPsX, 16).toUpper().leftJustified(5)
//                << "nextCode =" << QString::number(nextCode).leftJustified(4)
//                << "dict[nextCode] =" << dictionary[nextCode].toHex().toUpper().leftJustified(12)
//                << "prevString =" << prevString.toHex().toUpper().leftJustified(12)
//                << "prevString.size() =" << QString::number(prevString.size()).leftJustified(6)
//                << "dictionary[code][0] =" << QString::number(0xff & (unsigned int)dictionary[code][0], 16).toUpper().leftJustified(5)
//    //            << isOkay
//                ;
//    //            break;
//    //        }
        // end debug
        //*/

//        n += dictionary[code].length();
//        if (n > 1000000) break;

        // update prevString
        prevString = dictionary[code];
    }
}

void Tiff::lzwPredictorDecompress(QByteArray &inBa, QByteArray &outBa)
{
//    qDebug() << __FUNCTION__;
    QBuffer inBuf(&inBa);
    QBuffer outBuf(&outBa);
    inBuf.open(QIODevice::ReadOnly);
    outBuf.open(QIODevice::WriteOnly);
    QDataStream inStream(&inBuf);
    QDataStream out(&outBuf);

    parseInStream in(inStream);
    /* debugging
        QFile f1("D:/Pictures/_TIFF_lzw1/ps_8_big_800px.tif");
        f1.open(QIODevice::ReadOnly);
        f1.seek(34296);
        QByteArray baPs = f1.read(1080000);
        f1.close();
    //*/
    int n = 0;      // for debugging

    // bytes per row (reset predictor at end of row)
    int rowLength = width * samplesPerPixel;
    // hash of code => byte array (string of char)
    QHash<quint32,QByteArray> dictionary;
    // the value of the prior code in the in stream
    QByteArray prevString;
    // code is the index (key) in the dictionary hash
    quint32 code;
    quint32 nextCode = 258;
    // rgb iterator: 0,1,2,0,1,2...
    int m = 0;
    rgb[0] = rgb[1] = rgb[2] = 0;
    while (in >> code) {
//        G::track(QString::number(n), "Start of loop");
        // reset if clear code = 256. Occurs at start and before dictionary code 4094 (<13 bits)
        if (code == CLEAR_CODE) {
//            lzwReset(dictionary, prevString, nextCode);
            dictionary.clear();
            for (uint i = 0 ; i < 256 ; i++ ) {
                dictionary[i] = QByteArray(1, (char)i);
            }
            nextCode = 258;
            prevString.clear();
            continue;
        }
        // if code not found then add to dictionary
        if (!dictionary.contains(code)) {
            dictionary[code] = prevString + prevString[0];
        }

        // if predictor add to previous RGB value
        for (auto& ch : dictionary[code]) {
            if (n % rowLength == 0) rgb[0] = rgb[1] = rgb[2] = 0;
            quint8 b = (quint8)ch + rgb[m];
                /* debugging
            bool ok = b == (quint8)baPs[n];
//                if (!ok) qDebug().noquote()
            if (showDebug) qDebug().noquote()
                 << "n =" << QString::number(n).leftJustified(10)
                 << "x =" << QString::number(n % rowLength).leftJustified(4)
                 << "y =" << QString::number(n / rowLength).leftJustified(4)
                 << "nextCode =" << QString::number(nextCode).leftJustified(4)
                 << "code =" << QString::number(code).leftJustified(4)
                 << "string =" << dictionary[code].toHex().toUpper().leftJustified(24)
                 << "ch =" << QString::number((quint8)ch).leftJustified(4)
                 << "m =" << m
                 << "prev =" << QString::number(rgb[m],16).toUpper().leftJustified(2)
                 << "out =" << QString::number(b,16).toUpper().leftJustified(2)
                 << QString::number((quint8)baPs[n],16).toUpper()
                 << "diff = " << QString::number(b - (quint8)baPs[n]).leftJustified(4)
                 << ok
                    ;
//            if (!ok) return;
                        //*/
            out << (quint8)b;
            rgb[m] = b;
            m++;
            n++;
            if (m > 2) m = 0;
        }

        if (prevString.size() && nextCode <= MAXCODE)
        dictionary[nextCode++] = prevString + dictionary[code][0];
        prevString = dictionary[code];

//        G::track(QString::number(n), "End of loop");
//        if (n > 100) break;

        /* debugging
        if (n > 16275 && n < 16300) showDebug = true;
        else showDebug = false;
        if (n > 16300) break;
        //*/
    }
}
