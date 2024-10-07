#include "tiff.h"

/*

The G::useMyTiff flag uses this TIFF class when true.  Creating missing thumbnails is
also enabled.

Some LZW compressed files crash in /Users/roryhill/Pictures/_test2.

To do:

    • debug crash issues
    • add support for ZIP compression.
    • or use libtiff library

LibTiff

    • https://libtiff.gitlab.io/libtiff/libtiff.html
    • /Users/roryhill/Qt/6.6.0/Src/qtimageformats/src/3rdparty/libtiff/libtiff

    Copilot: How embed jpg thumbnail in existing till file?

    1. Open the Existing TIFF File: Begin by opening your existing TIFF file using the
       TIFFOpen function in write mode.

    2. Create the Thumbnail Data: Generate your JPEG thumbnail data. This thumbnail
       should be a valid JPEG image extracted from the main image. You can resize the
       main image or create a separate thumbnail image.

    3. Set the TIFFTAG_JPEGTABLES Field: Use the TIFFSetField function to set the
       TIFFTAG_JPEGTABLES field. Pass the JPEG thumbnail data as the value for this field.

    4. Write the Main IFD: Finally, write your main IFD (Image File Directory)
       using TIFFWriteDirectory. Here’s an updated example in C++:

    Example:
    // Assume you've opened an existing TIFF (TIFF* existing_TIFF) in write mode.
    // Create a valid JPEG thumbnail (replace this with your actual thumbnail data).
    const unsigned char jpeg_thumbnail_data[] = { Your JPEG data here };

    // Set the TIFFTAG_JPEGTABLES field:
    if (!TIFFSetField(existing_TIFF, TIFFTAG_JPEGTABLES, sizeof(jpeg_thumbnail_data), jpeg_thumbnail_data)) {
        // Handle the error if setting the field fails.
    }

    // Write the main IFD:
    TIFFWriteDirectory(existing_TIFF);

    Remember to replace the placeholder jpeg_thumbnail_data with your actual JPEG thumbnail
    data extracted from the main image. This approach will embed the thumbnail within your
    existing TIFF file.

    End Copilot answer

*/

bool showDebug = false;

class parseInStream
{
/*
    The incoming stream is read 8 bits at a time into a bit buffer called pending. The
    buffer is consumed (most significant) n bits at a time (inCode), where n = codesize.
    currCode starts at 258 and is incremented each time an inCode is consumed. codeSize
    starts at 9 bits, and is incremented each time currCode exceeds the bit capacity.
    codeSize maximum is 12 bits.

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

Tiff::Tiff(QString src)
{
    source = src;
    isDebug = false;
}

bool Tiff::parse(ImageMetadata &m, QString fPath)
{
/*
    Called from MW::embedThumbnails
*/
    MetadataParameters p;
    p.file.setFileName(fPath);
    if (p.file.open(QIODevice::ReadOnly)) {
        IFD *ifd = new IFD;
        IRB *irb = new IRB;
        IPTC *iptc = new IPTC;
        Exif *exif = new Exif;
        GPS *gps = new GPS;
        return parse(p, m, ifd,irb, iptc, exif, gps);
    }
    return false;
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
    Called from Metadata.

    This function reads the metadata from a tiff file. If the tiff file does not contain
    a thumbnail, either in an IRB or IFB, and G::embedTifThumb == true, then a thumbnail
    will be added at the end of file.
*/
    if (G::isLogger || isDebug) G::log("Tiff::parse", p.fPath);
    //file.open happens in readMetadata
    //qDebug() << "Tiff::parse" << p.file.fileName();

    Utilities u;
    quint32 startOffset = 0;

    // first two bytes is the endian order
    quint16 order = u.get16(p.file.read(2));
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
    if (u.get16(p.file.read(2), isBigEnd) != 42) return false;

    // read offset to first IFD
    quint32 ifdOffset = u.get32(p.file.read(4), isBigEnd);
    m.ifd0Offset = ifdOffset;
    m.ifdOffsets.append(ifdOffset); // added offset list
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

    // IFD0 Offsets
    quint32 ifdEXIFOffset = 0;
    if (ifd->ifdDataHash.contains(34665))
        ifdEXIFOffset = ifd->ifdDataHash.value(34665).tagValue;

    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue;

    quint32 ifdPhotoshopOffset = 0;
    if (ifd->ifdDataHash.contains(34377))
        ifdPhotoshopOffset = ifd->ifdDataHash.value(34377).tagValue;

    quint32 ifdsubIFDOffset = 0;
    if (ifd->ifdDataHash.contains(330))
        ifdsubIFDOffset = ifd->ifdDataHash.value(330).tagValue;

    quint32 ifdIPTCOffset = 0;
    if (ifd->ifdDataHash.contains(33723))
        ifdIPTCOffset = ifd->ifdDataHash.value(33723).tagValue;

    if (ifd->ifdDataHash.contains(34675)) {
        m.iccSegmentOffset = ifd->ifdDataHash.value(34675).tagValue;
        m.iccSegmentLength = ifd->ifdDataHash.value(34675).tagCount;
    }

    quint32 ifdXMPOffset = 0;
    if (ifd->ifdDataHash.contains(700)) {
        m.isXmp = true;
        ifdXMPOffset = ifd->ifdDataHash.value(700).tagValue;
        m.xmpSegmentOffset = static_cast<uint>(ifdXMPOffset);
        m.xmpSegmentLength = static_cast<quint32>(ifd->ifdDataHash.value(700).tagCount);
    }

    // IFD0: Model
    (ifd->ifdDataHash.contains(272))
        ? m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue, ifd->ifdDataHash.value(272).tagCount)
        : m.model = "";

    // IFD0: Make
    (ifd->ifdDataHash.contains(271))
        ? m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
        ifd->ifdDataHash.value(271).tagCount)
        : m.make = "";

    // IFD0: Title (ImageDescription)
    (ifd->ifdDataHash.contains(270))
        ? m.title = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(270).tagCount)
        : m.title = "";

    // IFD0: Creator (artist)
    (ifd->ifdDataHash.contains(315))
        ? m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
        ifd->ifdDataHash.value(315).tagCount)
        : m.creator = "";

    // IFD0: Copyright
    (ifd->ifdDataHash.contains(33432))
            ? m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
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
    m.widthPreview = m.width;
    m.heightPreview = m.height;

    // start thumbnail dimensions
    int thumbLongside;
    m.widthPreview > m.heightPreview ? thumbLongside = m.widthPreview : thumbLongside = m.heightPreview;

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

    p.offset = m.ifd0Offset;
    if (p.report) parseForDecoding(p, ifd);

    // IFD0: Jpeg table
    if (ifd->ifdDataHash.contains(347)) {
        p.offset = ifd->ifdDataHash.value(347).tagValue + 2;
        if (p.report) parseJpgTables(p, m);
    }


    /* Search for thumbnail in chained IFDs, IRB and subIFDs.
       - if no thumbnail found then:    m.offsetThumb == m.offsetFull
       - if IRB Jpg thumb found then:   m.lengthThumb > 0
       - if IFD tiff thumb found then:  m.offsetThumb != m.offsetFull
    */
    // IRB:  Get embedded JPG thumbnail if available
    if (ifdPhotoshopOffset) {
        if (p.report) {
            p.hdr = "IRB";
            MetaReport::header(p.hdr, p.rpt);
            p.rpt.reset();
            p.rpt << "Offset        IRB Id   (1036 = embedded jpg thumbnail)\n";
        }
        p.offset = ifdPhotoshopOffset;
        // set m.offsetThumb, m.lengthThumb, m.thumbFormat
        irb->readThumb(p, m);    // rgh need to add IRB for DNG
        if (m.lengthThumb) {
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
            if (thumbLong > G::maxIconSize) continue;
            m.offsetThumb = p.offset;
        }
    }
    else {
        while (nextIFDOffset > 0 && !m.isEmbeddedThumbMissing) {
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
            if (thumbLong < G::maxIconSize) break;
            // qDebug() << "Tiff::parse  thumbLong =" << thumbLong;
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
    if (!p.report && ifdsubIFDOffset && !m.isEmbeddedThumbMissing) {
        while (nextIFDOffset && count < 10) {
            count ++;
            p.hdr = "SubIFD " + QString::number(count);
            p.offset = nextIFDOffset;
            m.ifdOffsets.append(nextIFDOffset);
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

    if (G::modifySourceFiles &&
        G::autoAddMissingThumbnails &&
        m.offsetThumb == m.offsetFull &&
        thumbLongside > 512)
    {
        p.offset = m.offsetThumb;        // Smallest preview to use
        encodeThumbnail(p, m, ifd);
        // write thumbnail added to xmp sidecar if writing sidecars
        if (G::useSidecar) {
            Xmp xmp(p.file, p.instance);
            if (xmp.isValid) {
                QByteArray val = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss").toLatin1();
                xmp.setItem("WinnowAddThumb", val);
                // xmp.writeSidecar(p.file);
            }
        }
    }

    // Identify thumbnail is missing
    m.isEmbeddedThumbMissing = (m.offsetThumb == m.offsetFull);

    // EXIF: *******************************************************************

    p.hdr = "IFD Exif";
    p.offset = ifdEXIFOffset;
    if (ifdEXIFOffset) ifd->readIFD(p, isBigEnd);

    // EXIF: created datetime
    QString createdExif;
    createdExif = u.getString(p.file, ifd->ifdDataHash.value(36868).tagValue,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = u.getReal(p.file,
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
        double x = u.getReal(p.file,
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
        double x = u.getReal_s(p.file,
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
        double x = u.getReal(p.file,
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
            ? m.lens = u.getString(p.file, ifd->ifdDataHash.value(42036).tagValue,
                                  ifd->ifdDataHash.value(42036).tagCount)
            : m.lens = "";

    /* Read embedded ICC. The default color space is sRGB. If there is an embedded icc profile
    and it is sRGB then no point in saving the byte array of the profile since we already have
    it and it will take up space in the datamodel. If iccBuf is null then sRGB is assumed. */
    if (m.iccSegmentOffset && m.iccSegmentLength) {
        m.iccSpace = u.getString(p.file, m.iccSegmentOffset + 16, 4);
        if (m.iccSpace != "sRGB") {
            p.file.seek(m.iccSegmentOffset);
            m.iccBuf = p.file.read(m.iccSegmentLength);
        }
    }

    // GPSIFD: *******************************************************************
    if (offsetGPS) {
        p.file.seek(offsetGPS);
        p.hdr = "IFD GPS";
        p.hash = &gps->hash;
        p.offset = offsetGPS;
        ifd->readIFD(p, isBigEnd);

        if (ifd->ifdDataHash.contains(1)) {  // 1 = GPSLatitudeRef
            // process GPS info
            QString gpsCoord = gps->decode(p.file, ifd->ifdDataHash, isBigEnd);
            m.gpsCoord = gpsCoord;
        }
    }

    // IPTC: *******************************************************************
    // Get title if available

    if (ifdIPTCOffset) iptc->read(p.file, ifdIPTCOffset, m);

    // read XMP
    bool okToReadXmp = true;
    if (m.isXmp && okToReadXmp && !G::stop) {
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength, p.instance);
        if (xmp.isValid) {
            m.rating = xmp.getItem("Rating");
            m.label = xmp.getItem("Label");
            m.title = xmp.getItem("title");
            m.cameraSN = xmp.getItem("SerialNumber");
            if (m.lens.isEmpty()) m.lens = xmp.getItem("Lens");
            m.lensSN = xmp.getItem("LensSerialNumber");
            if (m.creator.isEmpty()) m.creator = xmp.getItem("creator");
            m.copyright = xmp.getItem("rights");
            m.email = xmp.getItem("email");
            m.url = xmp.getItem("url");
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

bool Tiff::isBigEndian(MetadataParameters &p)
{
/*
    Used for decoding.
*/
    if (G::isLogger || isDebug) G::log("Tiff::isBigEndian");
    // first two bytes is the endian order
    p.file.seek(0);
    quint16 order = u.get16(p.file.read(2));
    if (order == 0x4D4D) return true;
    else return false;
}

bool Tiff::parseForDecoding(MetadataParameters &p, /*ImageMetadata &m, */IFD *ifd)
{
/*
    Get IFD parameters required for TIF decoding: stripOffsets, stripByteCounts,
    bitsPerSample, photoInterp, samplesPerPixel, rowsPerStrip, compression and
    planarConfiguration.

    This function can be called from the main thread (for reporting) or from the metadata
    generation thread or from any of the image decoder threads. The reporting parameter
    p.rpt can only be used from the main thread.

    p.offset must be set to start of IFD before calling parseForDecoding.

    If reporting, then p.hash must be set to Exif::hash.
*/
    if (G::isLogger || isDebug) G::log("Tiff::parseForDecoding", p.fPath);

    // IFD has already been read once, so if reporting do not want to report again

    // endianess
    isBigEnd = isBigEndian(p);
//    qDebug() << "Tiff::parseForDecoding" << p.fPath << "isBigEnd =" << isBigEnd;

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
                stripOffsets[i] = u.get32(p.file.read(4), isBigEnd);
            }
        }
    }
    else {
        err = "No strip offsets.  \n";
    }

    // strip byte counts
    if (ifd->ifdDataHash.contains(279)) {
        int stripCount = ifd->ifdDataHash.value(279).tagCount;
//        if (stripCount < 100000) {
            stripByteCounts.resize(stripCount);
            if (stripCount == 1) {
                stripByteCounts[0] = static_cast<uint>(ifd->ifdDataHash.value(279).tagValue);
            }
            else {
                quint32 offset = ifd->ifdDataHash.value(279).tagValue;
                p.file.seek(offset);
                for (int i = 0; i < stripCount; ++i) {
                    stripByteCounts[i] = u.get32(p.file.read(4), isBigEnd);
                }
            }
//        }
//        else {
//            err = "stripCount > 100000.  \n";
//        }
    }
    else {
        err = "No StripByteCounts.  \n";
    }

    // IFD: bitsPerSample
    if (ifd->ifdDataHash.contains(258)) {
        p.file.seek(static_cast<int>(ifd->ifdDataHash.value(258).tagValue));
        bitsPerSample = u.get16(p.file.read(2), isBigEnd);
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
    QList validCompressions{1,5,7,8};
    // if (!(compression == 1  || compression == 5)) {
    if (!(validCompressions.contains(compression))) {
        err = "Compression " + QString::number(compression) + " is not supported\n";
    }
    switch (compression) {
    case 1:
        compressionType = NoCompression;
        break;
    case 5:
        if (predictor == 2) compressionType = LzwPredictorCompression;
        else compressionType = LzwCompression;
        break;
    case 7:
        compressionType = JpgCompression;
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
    if (err != "") G::issue("Error", err, "Tiff::decode_2", -1, p.fPath);
    if (err != "") qDebug() << "TIFF::parseForDecoding  err =" << err;

    if (err != "" && !isReport) return false;


    /* rgh used for debugging
    G::tiffData = "BitsPerSample:" + QString::number(bitsPerSample) +
                  " Compression:" + QString::number(compression) +
                  " Predictor:" + QString::number(predictor) +
                  " PlanarConfiguration:" + QString::number(planarConfiguration)
                    ; //*/

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
        p.rpt.setFieldWidth(w1); p.rpt << "Compression code" << compression; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "Compression type" << compressionString.at(compressionType); p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "PlanarConfiguration" << planarConfiguration; p.rpt.setFieldWidth(0); p.rpt << "\n";
        p.rpt.setFieldWidth(w1); p.rpt << "Predictor" << predictor; p.rpt.setFieldWidth(0); p.rpt << "\n";
    }

    return true;
}

void Tiff::parseJpgTables(MetadataParameters &p, ImageMetadata &m)
{
/*
    If the tiff file has been encoded with jpg compression then the tiff exif will contain
    JPEGTables in IFD0.  Use the Jpeg class to parse this.
*/
    Jpeg jpg;
    jpg.initSegCodeHash();
    jpg.getJpgSegments(p, m);
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
    Decode_2 (used in Issue)
    Decode using unmapped QFile.  Set p.file, p.offset and call main decode.

*/
    if (G::isLogger || isDebug) G::log("Tiff::decode", fPath + " Source = " + source);
    //qDebug() << "Tiff::decode1" << fPath;
    QFileInfo fileInfo(fPath);
    if (!fileInfo.exists()) return false;                 // guard for usb drive ejection

    MetadataParameters p;
    p.fPath = fPath;
    p.file.setFileName(fPath);
    if (!p.file.open(QIODevice::ReadOnly)) {
        QString msg = "Unable to open file.";
        G::issue("Error", msg, "Tiff::decode_2", m.row, fPath);
        return false;
    }

    /* qDebug() << "Tiff::decode"
             << "isThumb =" << thumb
             << "newSize =" << newSize
             << "m.offsetThumb =" << m.offsetThumb
                ;
                //*/

    if (thumb && m.offsetThumb != m.offsetFull) p.offset = m.offsetThumb;
    else p.offset = m.offsetFull;
    return decode(p, image, newSize);
}

bool Tiff::decode(QString fPath, quint32 offset, QImage &image)
{
/*
    Decode_1 (used in Issue)
    The version is used by ImageDecoders in image cache.
*/
    if (G::isLogger || isDebug) G::log("Tiff::decode_1", fPath + " Source = " + source);
    // qDebug() << "Tiff::decode(fPath,offset,image)" << fPath;

    QFileInfo fileInfo(fPath);
    if (!fileInfo.exists()) return false;                 // guard for usb drive ejection

    MetadataParameters p;
    p.fPath = fPath;
    p.file.setFileName(fPath);
    if (!p.file.open(QIODevice::ReadOnly)) {
        QString msg = "Unable to open file.";
        G::issue("Error", msg, "Tiff::decode", -1, fPath);
        return false;
    }
    p.offset = offset;
    /*
    qDebug() << "Tiff::decode" << fPath << "offset =" << offset;
    //*/
    return decode(p, image);
}

bool Tiff::decode(MetadataParameters &p, QImage &image, int newSize)
{
/*
    Decode_0 (used in Issue)
    Decode tiff into QImage &image.

    p.file must be set and opened. If called from ImageDecoder then p.file will be mapped
    to memory.

    p.offset must be set to ifdOffset that describes the image to be decoded within the
    tiff before calling this function (either m.offsetFull or m.offsetThumb).

    newSize is the resized long side in pixels.  If newSize = 0 then no resizing.

*/
    if (G::isLogger || isDebug) G::log("Tiff::decode2", p.fPath + " Source = " + source);
    // qDebug() << "Tiff::decode2" << p.fPath;

    IFD *ifd = new IFD;
    p.report = false;
    if (!parseForDecoding(p, ifd)) {
        qDebug() << "Tiff::decode(p,image,newsize)  parseForDecoding failed"
                 << "src:" << source << p.fPath;
        return false;
    }

    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerRow = (uint)(bytesPerPixel * width);
    scanBytesAvail = (uint)(width * height * bytesPerPixel);

    if (bitsPerSample == 16) im = new QImage(width, height, QImage::Format_RGBX64);
    if (bitsPerSample == 8)  im = new QImage(width, height, QImage::Format_RGB888);
    /*
    qDebug() << "Tiff::decode(p,image,newsize)  compression =" << compression
             << "src:" << source << p.fPath; //*/
    bool decoded = true;
    if (compression == 1) decoded = decodeBase(p);
    if (compression == 5 /*&& predictor == 0*/) decoded = decodeLZW(p);
    if (compression == 8) decoded = decodeZip(p);
    if (compression == 7) decoded = decodeJpg(p);
    p.file.close();
    if (!decoded) return false;

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

    image.operator = (*im);
    return true;
}

bool Tiff::decodeBase(MetadataParameters &p)
{
/*
    Decodes TIFF image data with no compression (compression type 1) and loads it into
    a QImage object. It operates by reading the image data strip by strip and copying it
    into the QImage object row by row.
*/
    if (G::isLogger || isDebug) G::log("Tiff::decodeBase", p.fPath + " Source = " + source);
    /*
    qDebug() << "Tiff::decodeBase"
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
    int strips = stripOffsets.count();
    int line = 0;
    quint32 scanBytes = 0;
    const quint32 totalScanBytes = width * height * bytesPerPixel;

    // Ensure the file is open and ready for reading
    if (!p.file.isOpen()) {
        if (!p.file.open(QIODevice::ReadOnly)) {
            return false;
        }
    }

    for (int strip = 0; strip < strips; ++strip) {
        const quint32 stripBytes = stripByteCounts.at(strip);
        QByteArray ba;
        // Preallocate memory to avoid reallocations
        ba.reserve(stripBytes);

        // Seek to the strip offset and read the strip data
        if (!p.file.seek(stripOffsets.at(strip))) {
            return false;
        }
        ba = p.file.read(stripBytes);
        if (ba.size() != stripBytes) {
            return false;
        }

        QBuffer buf(&ba);
        buf.open(QBuffer::ReadOnly);

        // Process each row in the strip
        for (int row = 0; row < rowsPerStrip; ++row) {
            if ((buf.pos() + bytesPerRow) > ba.size()) {
                break; // Last strip may have fewer rows than rowsPerStrip
            }
            if (scanBytes + bytesPerRow > totalScanBytes) {
                break; // Avoid reading beyond the available scan bytes
            }

            // ptr to pixel row in QImage
            uchar *dest = im->scanLine(line++);
            const QByteArray rowData = buf.read(bytesPerRow);
            if (rowData.size() != bytesPerRow) {
                return false;
            }
            // write the row of pixels
            std::memcpy(dest, rowData.constData(), static_cast<size_t>(bytesPerRow));

            scanBytes += bytesPerRow;
        }
    }

    return true;

}

//bool Tiff::decodeLZW(MetadataParameters &p, QImage &image)
bool Tiff::decodeLZW(MetadataParameters &p)
{
    if (G::isLogger || isDebug) G::log("Tiff::decodeLZW", p.fPath + " Source = " + source);
    qDebug() << "Tiff::decodeLZW" << p.fPath;
    int strips = stripOffsets.count();
    // TiffStrips tiffStrips;
    // QFuture<void> future;
    // pass info to concurrent threads via TiffStrip for each strip
    TiffStrip tiffStrip;
    for (int strip = 0; strip < strips; ++strip) {
        tiffStrip.strip = strip;
        tiffStrip.bitsPerSample = bitsPerSample;
        tiffStrip.bytesPerRow = bytesPerRow;
        tiffStrip.stripBytes = stripByteCounts.at(strip);
        if (predictor == 2) {
            tiffStrip.predictor = true;
            /*
            this is causing a crash when decoding
            /Users/roryhill/Photos/_misc/_Calibration/TripleMoireTarget.tif
            in Tiff::TiffStrips Tiff::lzwDecompress:

            if (t.predictor) {
                // output char string for code (add from left)
                // pBuf   00000000 11111111 22222222 33333333
                for (uint32_t i = 0; i != (uint32_t)sLen[code]; i++) {
                    if (n % t.bytesPerRow < 3) *out++ = *(s[code] + i);
                    else *out++ = (*(s[code] + i) + *(out - 3));  // crash here
                    ++n;
                }
            }

            So, return and use default tif decoder for now.
            */
            // return false;
        }
        // read tiff strip into QByteArray
        p.file.seek(stripOffsets.at(strip));
        inBa.append(p.file.read(stripByteCounts.at(strip)));
        // ptr to start of strip QByteArray
        tiffStrip.in = inBa[strip].data();
        // length of incoming strip in bytes
        tiffStrip.incoming = inBa[strip].size();
        // ptr to start of pixel row in QImage
        tiffStrip.out = im->scanLine(strip * rowsPerStrip);

        // decompress strip and write to QImage im
        lzwDecompress(tiffStrip, p);   // no future
        // lzwDecompressOld(tiffStrip, p);   // no future
    }
    // future = QtConcurrent::map(tiffStrips, lzwDecompress);
    // future.waitForFinished();
    return true;
}

bool Tiff::decodeZip(MetadataParameters &p)
{
    if (G::isLogger || isDebug) G::log("Tiff::decodeLZW", p.fPath + " Source = " + source);
    //qDebug() << "Tiff::decodeLZW" << p.fPath;
    int strips = stripOffsets.count();
    TiffStrips tiffStrips;
    //    QFuture<void> future;
    // pass info to concurrent threads via TiffStrip for each strip
    TiffStrip tiffStrip;
    for (int strip = 0; strip < strips; ++strip) {
        tiffStrip.strip = strip;
        tiffStrip.bitsPerSample = bitsPerSample;
        tiffStrip.bytesPerRow = bytesPerRow;
        tiffStrip.stripBytes = stripByteCounts.at(strip);
        if (predictor == 2) {
            tiffStrip.predictor = true;
        //     /*
        //     this is causing a crash when decoding
        //     /Users/roryhill/Photos/_misc/_Calibration/TripleMoireTarget.tif
        //     in Tiff::TiffStrips Tiff::lzwDecompress:

        //     if (t.predictor) {
        //         // output char string for code (add from left)
        //         // pBuf   00000000 11111111 22222222 33333333
        //         for (uint32_t i = 0; i != (uint32_t)sLen[code]; i++) {
        //             if (n % t.bytesPerRow < 3) *out++ = *(s[code] + i);
        //             else *out++ = (*(s[code] + i) + *(out - 3));  // crash here
        //             ++n;
        //         }
        //     }

        //     So, return and use default tif decoder for now.
        //     */
        //     return false;
        }
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

        //lzwDecompress(tiffStrip);   // no future
        // lzwDecompress2(tiffStrip, p);   // no future
        zipDecompress(tiffStrip, p);   // no future  (ChatGPT tweaaks)
    }
    //    future = QtConcurrent::map(tiffStrips, lzwDecompress);
    //    future.waitForFinished();
    return true;
}

bool Tiff::decodeJpg(MetadataParameters &p)
{
    if (G::isLogger || isDebug) G::log("Tiff::decodeLZW", p.fPath + " Source = " + source);
    qDebug() << "Tiff::decodeJpg" << p.fPath;
    int strips = stripOffsets.count();
    TiffStrips tiffStrips;
    //    QFuture<void> future;
    // pass info to concurrent threads via TiffStrip for each strip
    TiffStrip tiffStrip;
    for (int strip = 0; strip < strips; ++strip) {
        tiffStrip.strip = strip;
        tiffStrip.bitsPerSample = bitsPerSample;
        tiffStrip.bytesPerRow = bytesPerRow;
        tiffStrip.stripBytes = stripByteCounts.at(strip);
        if (predictor == 2) {
            tiffStrip.predictor = true;
        //     /*
        //     this is causing a crash when decoding
        //     /Users/roryhill/Photos/_misc/_Calibration/TripleMoireTarget.tif
        //     in Tiff::TiffStrips Tiff::lzwDecompress:

        //     if (t.predictor) {
        //         // output char string for code (add from left)
        //         // pBuf   00000000 11111111 22222222 33333333
        //         for (uint32_t i = 0; i != (uint32_t)sLen[code]; i++) {
        //             if (n % t.bytesPerRow < 3) *out++ = *(s[code] + i);
        //             else *out++ = (*(s[code] + i) + *(out - 3));  // crash here
        //             ++n;
        //         }
        //     }

        //     So, return and use default tif decoder for now.
        //     */
        //     return false;
        }
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

        //lzwDecompress(tiffStrip);   // no future
        // lzwDecompress2(tiffStrip, p);   // no future
        jpgDecompress(tiffStrip, p);   // no future  (ChatGPT tweaaks)
    }
    //    future = QtConcurrent::map(tiffStrips, lzwDecompress);
    //    future.waitForFinished();
    return true;
}

bool Tiff::encodeThumbnail(MetadataParameters &p, ImageMetadata &m, IFD *ifd)
{
/*
    If an image preview (thumbnail) with a longside <= 512px does not exist, then add a
    thumbnail with a longside of G::maxIconSize to the tiff file. This involves appending
    an IFD to the end of the IFD chain and sampling the smallest existing IFD preview
    (source) to the new thumbnail.

    Steps:
        - replace last nextIFDOffset with offset to current EOF, where the thumbnail IFD
          will be appended.
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

    p.offset must be set to ifdOffset that describes the source image to be sampled to
    create the thumbnail before calling this function.
*/
    if (G::isLogger || isDebug) G::log("Tiff::encodeThumbnail", p.fPath + " Source = " + source);
    // get decoding parameters from source IFD (p.offset must be preset)
    if (!parseForDecoding(p, ifd)) {
        return false;
    }

    qDebug() << "Tiff::encodeThumbnail" << p.fPath;

    // change p.file from QIODevice::ReadOnly to QIODevice::ReadWrite
    p.file.close();
    if (G::backupBeforeModifying) Utilities::backup(m.fPath, "backup");
    p.file.open(QIODevice::ReadWrite);

    // go to last main IFD offset and replace with offset to new IFD at end of file
    m.offsetThumb = p.file.size();
    p.file.seek(lastIFDOffsetPosition);
    p.file.write(u.put32(m.offsetThumb, m.isBigEnd));

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
    p.file.write(u.put32(0, m.isBigEnd));              // Next IFD = zero

    // write values at offset locations
    p.file.write(u.put16(8, m.isBigEnd));              // BitsPerSample R
    p.file.write(u.put16(8, m.isBigEnd));              // BitsPerSample G
    p.file.write(u.put16(8, m.isBigEnd));              // BitsPerSample B
    p.file.write(u.put32(1200000, m.isBigEnd));        // XResolution Numerator
    p.file.write(u.put32(10000, m.isBigEnd));          // XResolution Denominator
    p.file.write(u.put32(1200000, m.isBigEnd));        // YResolution Numerator
    p.file.write(u.put32(10000, m.isBigEnd));          // YResolution Denominator

    bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    bytesPerRow = (uint)(bytesPerPixel * width);
    scanBytesAvail = (uint)(width * height * bytesPerPixel);

    if (bitsPerSample == 16) im = new QImage(width, height, QImage::Format_RGBX64);
    if (bitsPerSample == 8)  im = new QImage(width, height, QImage::Format_RGB888);

    bool decoded;
    qDebug() << "Tiff::encodeThumbnail" << p.fPath;
    if (compression == 1) decoded = decodeBase(/*m, */p/*, *im*/);
    if (compression == 5) decoded = decodeLZW(p/*, *im*/);
    if (!decoded) return false;

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
    int bc = im2.sizeInBytes();
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
    qDebug() << "Tiff::sample"
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

// void Tiff::lzwDecompress(TiffStrip &t, MetadataParameters &p)
// {
//     /*
//         Receives a tiff strip, decompresses it, and writes the data to QImage Tiff::im
//     */
//     if (G::isLogger || isDebug)
//         G::log("Tiff::lzwDecompress3", "strip = " + QString::number(t.strip) + " " + p.fPath + " Source = " + source);

//     TiffStrips tiffStrips;
//     int alphaRowComponent = t.bytesPerRow / 3;
//     const unsigned int clearCode = 256;
//     const unsigned int EOFCode = 257;
//     const unsigned int maxCode = 4095;  // 12-bit max

//     // Input and output pointers
//     char* c = t.in;
//     std::vector<uchar> tempBuffer(t.bytesPerRow * rowsPerStrip);  // Temporary buffer for decompressed data
//     uchar* tempOut = tempBuffer.data();

//     // String table initialization
//     char* s[4096] = {nullptr};  // Pointers to strings for each possible code
//     int8_t sLen[4096] = {0};  // Code string length
//     std::memset(sLen, 1, 256);  // 0-255 one char strings

//     char strings[128000];
//     for (int i = 0; i < 256; i++) {
//         strings[i] = (char)i;
//         s[i] = &strings[i];
//     }
//     strings[256] = 0;  s[256] = &strings[256];  // Clear code
//     strings[257] = 0;  s[257] = &strings[257];  // EOF code
//     char* sEnd = s[257];  // Pointer to current end of strings

//     char ps[256];  // Previous string
//     size_t psLen = 0;  // Length of previous string
//     uint32_t code;  // Key to string for code
//     uint32_t nextCode = 258;  // Used to preset string for next
//     uint n = 0;  // Output byte counter
//     uint32_t iBuf = 0;  // Incoming bit buffer
//     int32_t nBits = 0;  // Incoming bits in the buffer
//     int32_t codeBits = 9;  // Number of bits to make code (9-12)
//     uint32_t nextBump = 511;  // When to increment code size first time
//     uint32_t mask = (1 << codeBits) - 1;  // Extract code from iBuf

//     // Read incoming bytes into the bit buffer (iBuf) using the char pointer c
//     while (t.incoming) {
//         // GetNextCode: Read bits to form the next code
//         while (nBits < codeBits && t.incoming) {
//             iBuf = (iBuf << 8) | (uint8_t)*c++;
//             nBits += 8;
//             --t.incoming;
//         }
//         code = (iBuf >> (nBits - codeBits)) & mask;  // Extract code from buffer
//         nBits -= codeBits;

//         // Handle special codes
//         if (code == clearCode) {
//             codeBits = 9;
//             mask = (1 << codeBits) - 1;
//             nextBump = 511;
//             sEnd = s[257];
//             nextCode = 258;
//             psLen = 0;
//             continue;
//         }

//         if (code == EOFCode) {
//             break;
//         }

//         // Handle new code then add prevString + prevString[0]
//         if (code == nextCode) {
//             s[code] = sEnd;
//             switch(psLen) {
//             case 1:
//                 *s[code] = ps[0];
//                 break;
//             case 2:
//                 std::memcpy(s[code], ps, 2);
//                 break;
//             case 4:
//                 std::memcpy(s[code], ps, 4);
//                 break;
//             default:
//                 std::memcpy(s[code], ps, psLen);
//             }
//             *(s[code] + psLen) = ps[0];
//             sLen[code] = (int8_t)psLen + 1;
//             sEnd = s[code] + psLen + 1;
//         }

//         // Output the decoded string for the current code
//         // for (uint32_t i = 0; i < (uint32_t)sLen[code]; i++) {
//         //     if (s[code] != nullptr) {
//         //         *tempOut++ = (uchar)*(s[code] + i);
//         //     }
//         // }
//         // rgh replace
//         if (t.bitsPerSample == 8) {
//             for (uint32_t i = 0; i < (uint32_t)sLen[code]; i++) {
//                 if (s[code] != nullptr)
//                     *tempOut++ = (uchar)*(s[code] + i);
//             }
//         }
//         if (t.bitsPerSample == 16) {
//             for (uint32_t i = 0; i < (uint32_t)sLen[code]; i++) {
//                 *tempOut++ = (uchar)*(s[code] + i);
//                 ++n;
//                 if (n % t.bytesPerRow == 0)
//                     tempOut += alphaRowComponent;
//             }
//         }


//         // Add string to nextCode (prevString + strings[code][0])
//         if (psLen && nextCode <= maxCode) {
//             s[nextCode] = sEnd;
//             switch(psLen) {
//             case 1:
//                 *s[nextCode] = ps[0];
//                 break;
//             case 2:
//                 std::memcpy(s[nextCode], ps, 2);
//                 break;
//             case 4:
//                 std::memcpy(s[nextCode], ps, 4);
//                 break;
//             default:
//                 std::memcpy(s[nextCode], ps, psLen);
//             }
//             *(s[nextCode] + psLen) = *s[code];
//             sLen[nextCode] = (int8_t)(psLen + 1);
//             sEnd = s[nextCode] + psLen + 1;
//             ++nextCode;
//         }

//         // Copy strings[code] to ps
//         switch(sLen[code]) {
//         case 1:
//             ps[0] = *s[code];
//             break;
//         case 2:
//             ps[0] = *s[code];
//             ps[1] = *(s[code] + 1);
//             break;
//         case 4:
//             std::memcpy(ps, s[code], 4);
//             break;
//         default:
//             std::memcpy(ps, s[code], sLen[code]);
//         }

//         psLen = (size_t)sLen[code];

//         // Code size management
//         if (nextCode == nextBump) {
//             if (nextCode < maxCode) {
//                 nextBump = (nextBump << 1) + 1;
//                 ++codeBits;
//                 mask = (1 << codeBits) - 1;
//             } else if (nextCode == maxCode) {
//                 continue;
//             } else {
//                 codeBits = 9;
//                 mask = (1 << codeBits) - 1;
//                 nextBump = 511;
//                 sEnd = s[257];
//                 nextCode = 258;
//                 psLen = 0;
//             }
//         }
//     }

//     // Apply the predictor if necessary
//     if (t.predictor) {
//         uchar* tempIn = tempBuffer.data();
//         uchar* out = t.out;
//         for (int row = 0; row < rowsPerStrip; ++row) {
//             for (uint32_t col = 0; col < t.bytesPerRow; ++col) {
//                 if (col < 3) {
//                     *out++ = *tempIn++;
//                 } else {
//                     *out++ = (*tempIn++ + *(out - 3));
//                 }
//             }
//         }
//     } else {
//         // Copy the data from the temporary buffer to the QImage if no predictor is used
//         std::memcpy(t.out, tempBuffer.data(), tempBuffer.size());
//     }

//     return;
// }

void Tiff::lzwDecompress(TiffStrip &t, MetadataParameters &p)
{
    if (G::isLogger || isDebug)
        G::log("Tiff::lzwDecompress3", "strip = " + QString::number(t.strip) + " " + p.fPath + " Source = " + source);

    TiffStrips tiffStrips;
    int alphaRowComponent = t.bytesPerRow / 3;
    const unsigned int clearCode = 256;
    const unsigned int EOFCode = 257;
    const unsigned int maxCode = 4095;  // 12-bit max

    // Input and output pointers
    char* c = t.in;
    std::vector<uchar> tempBuffer(t.bytesPerRow * rowsPerStrip);  // Temporary buffer for decompressed data
    uchar* tempOut = tempBuffer.data();

    // String table initialization
    char* s[4096] = {nullptr};  // Pointers to strings for each possible code
    int8_t sLen[4096] = {0};  // Code string length
    std::memset(sLen, 1, 256);  // 0-255 one char strings

    char strings[128000];
    for (int i = 0; i < 256; i++) {
        strings[i] = (char)i;
        s[i] = &strings[i];
    }
    strings[256] = 0;  s[256] = &strings[256];  // Clear code
    strings[257] = 0;  s[257] = &strings[257];  // EOF code
    char* sEnd = s[257];  // Pointer to current end of strings

    char ps[256];  // Previous string
    size_t psLen = 0;  // Length of previous string
    uint32_t code;  // Key to string for code
    uint32_t nextCode = 258;  // Used to preset string for next
    uint n = 0;  // Output byte counter
    uint32_t iBuf = 0;  // Incoming bit buffer
    int32_t nBits = 0;  // Incoming bits in the buffer
    int32_t codeBits = 9;  // Number of bits to make code (9-12)
    uint32_t nextBump = 511;  // When to increment code size first time
    uint32_t mask = (1 << codeBits) - 1;  // Extract code from iBuf

    // Read incoming bytes into the bit buffer (iBuf) using the char pointer c
    while (t.incoming) {
        // GetNextCode: Read bits to form the next code
        while (nBits < codeBits && t.incoming) {
            iBuf = (iBuf << 8) | (uint8_t)*c++;
            nBits += 8;
            --t.incoming;
        }
        code = (iBuf >> (nBits - codeBits)) & mask;  // Extract code from buffer
        nBits -= codeBits;

        // Handle special codes
        if (code == clearCode) {
            codeBits = 9;
            mask = (1 << codeBits) - 1;
            nextBump = 511;
            sEnd = s[257];
            nextCode = 258;
            psLen = 0;
            continue;
        }

        if (code == EOFCode) {
            break;
        }

        // Handle new code then add prevString + prevString[0]
        if (code == nextCode) {
            s[code] = sEnd;
            switch(psLen) {
            case 1:
                *s[code] = ps[0];
                break;
            case 2:
                std::memcpy(s[code], ps, 2);
                break;
            case 4:
                std::memcpy(s[code], ps, 4);
                break;
            default:
                std::memcpy(s[code], ps, psLen);
            }
            *(s[code] + psLen) = ps[0];
            sLen[code] = (int8_t)psLen + 1;
            sEnd = s[code] + psLen + 1;
        }

        // Output the decoded string for the current code
        for (uint32_t i = 0; i < (uint32_t)sLen[code]; i++) {
            if (s[code] != nullptr) {
                *tempOut++ = (uchar)*(s[code] + i);
            }
        }

        // Add string to nextCode (prevString + strings[code][0])
        if (psLen && nextCode <= maxCode) {
            s[nextCode] = sEnd;
            switch(psLen) {
            case 1:
                *s[nextCode] = ps[0];
                break;
            case 2:
                std::memcpy(s[nextCode], ps, 2);
                break;
            case 4:
                std::memcpy(s[nextCode], ps, 4);
                break;
            default:
                std::memcpy(s[nextCode], ps, psLen);
            }
            *(s[nextCode] + psLen) = *s[code];
            sLen[nextCode] = (int8_t)(psLen + 1);
            sEnd = s[nextCode] + psLen + 1;
            ++nextCode;
        }

        // Copy strings[code] to ps
        switch(sLen[code]) {
        case 1:
            ps[0] = *s[code];
            break;
        case 2:
            ps[0] = *s[code];
            ps[1] = *(s[code] + 1);
            break;
        case 4:
            std::memcpy(ps, s[code], 4);
            break;
        default:
            std::memcpy(ps, s[code], sLen[code]);
        }

        psLen = (size_t)sLen[code];

        // Code size management
        if (nextCode == nextBump) {
            if (nextCode < maxCode) {
                nextBump = (nextBump << 1) + 1;
                ++codeBits;
                mask = (1 << codeBits) - 1;
            } else if (nextCode == maxCode) {
                continue;
            } else {
                codeBits = 9;
                mask = (1 << codeBits) - 1;
                nextBump = 511;
                sEnd = s[257];
                nextCode = 258;
                psLen = 0;
            }
        }
    }

    // Apply the predictor if necessary
    uchar* tempIn = tempBuffer.data();
    uchar* out = t.out;
    if (t.predictor) {
        if (t.bitsPerSample == 8) {
            for (int row = 0; row < rowsPerStrip; ++row) {
                for (uint32_t col = 0; col < t.bytesPerRow; ++col) {
                    if (col < 3) {
                        *out++ = *tempIn++;
                    } else {
                        *out++ = (*tempIn++ + *(out - 3));
                    }
                }
            }
        } else if (t.bitsPerSample == 16) {
            for (int row = 0; row < rowsPerStrip; ++row) {
                for (uint32_t col = 0; col < t.bytesPerRow; col += 2) {
                    if (col < 6) {
                        *out++ = *tempIn++;
                        *out++ = *tempIn++;
                    } else {
                        *out++ = (*tempIn++ + *(out - 6));
                        *out++ = (*tempIn++ + *(out - 6));
                    }
                }
                out += alphaRowComponent;  // Handle alpha component
            }
        }
    } else {
        // Copy the data from the temporary buffer to the QImage, considering the alphaRowComponent
        if (t.bitsPerSample == 8) {
            for (int row = 0; row < rowsPerStrip; ++row) {
                std::memcpy(out, tempIn, t.bytesPerRow);
                out += t.bytesPerRow;
                tempIn += t.bytesPerRow;
            }
        } else if (t.bitsPerSample == 16) {
            for (int row = 0; row < rowsPerStrip; ++row) {
                std::memcpy(out, tempIn, t.bytesPerRow);
                out += t.bytesPerRow + alphaRowComponent;
                tempIn += t.bytesPerRow;
            }
        }
    }

    return;
}

void Tiff::lzwDecompressOld(TiffStrip &t, MetadataParameters &p)
{
/*
    Receives a tiff strip, decompresses it, and writes the data to QImage Tiff::im
*/
    if (G::isLogger || isDebug)
        G::log("Tiff::lzwDecompress3", "strip = " + QString::number(t.strip) + " " + p.fPath + " Source = " + source);

    // qDebug() << "Tiff::lzwDecompress3  strip = " << t.strip << p.fPath;
    TiffStrips tiffStrips;
    int alphaRowComponent = t.bytesPerRow / 3;
    const unsigned int clearCode = 256;
    const unsigned int EOFCode = 257;
    const unsigned int maxCode = 4095;  // 12-bit max

    // Input and output pointers
    char* c = t.in;
    uchar* out = t.out;

    // String table initialization
    char* s[4096] = {nullptr};  // Pointers to strings for each possible code
    int8_t sLen[4096] = {0};  // Code string length
    std::memset(sLen, 1, 256);  // 0-255 one char strings

    char strings[128000];
    for (int i = 0; i < 256; i++) {
        strings[i] = (char)i;
        s[i] = &strings[i];
    }
    strings[256] = 0;  s[256] = &strings[256];  // Clear code
    strings[257] = 0;  s[257] = &strings[257];  // EOF code
    char* sEnd = s[257];  // Pointer to current end of strings

    char ps[256];  // Previous string
    size_t psLen = 0;  // Length of previous string
    uint32_t code;  // Key to string for code
    uint32_t nextCode = 258;  // Used to preset string for next
    uint n = 0;  // Output byte counter
    uint32_t iBuf = 0;  // Incoming bit buffer
    int32_t nBits = 0;  // Incoming bits in the buffer
    int32_t codeBits = 9;  // Number of bits to make code (9-12)
    uint32_t nextBump = 511;  // When to increment code size first time
    uint32_t mask = (1 << codeBits) - 1;  // Extract code from iBuf

    // Read incoming bytes into the bit buffer (iBuf) using the char pointer c
    while (t.incoming) {
        // GetNextCode: Read bits to form the next code
        while (nBits < codeBits && t.incoming) {
            iBuf = (iBuf << 8) | (uint8_t)*c++;
            nBits += 8;
            --t.incoming;
        }
        code = (iBuf >> (nBits - codeBits)) & mask;  // Extract code from buffer
        nBits -= codeBits;

        // Handle special codes
        if (code == clearCode) {
            codeBits = 9;
            mask = (1 << codeBits) - 1;
            nextBump = 511;
            sEnd = s[257];
            nextCode = 258;
            psLen = 0;
            continue;
        }

        if (code == EOFCode) {
            return;
        }

        // Handle new code then add prevString + prevString[0]
        if (code == nextCode) {
            s[code] = sEnd;
            switch(psLen) {
            case 1:
                *s[code] = ps[0];
                break;
            case 2:
                std::memcpy(s[code], ps, 2);
                break;
            case 4:
                std::memcpy(s[code], ps, 4);
                break;
            default:
                std::memcpy(s[code], ps, psLen);
            }
            *(s[code] + psLen) = ps[0];
            sLen[code] = (int8_t)psLen + 1;
            sEnd = s[code] + psLen + 1;
        }

        // Output the decoded string for the current code
        if (t.predictor) {
            for (uint32_t i = 0; i < (uint32_t)sLen[code]; i++) {
                if (s[code] != nullptr) {
                    if (n % t.bytesPerRow < 3)
                        *out++ = *(s[code] + i);
                    else
                        *out++ = (*(s[code] + i) + *(out - 3));
                }
                ++n;
            }
        } else if (t.bitsPerSample == 8) {
            for (uint32_t i = 0; i < (uint32_t)sLen[code]; i++) {
                if (s[code] != nullptr)
                    *out++ = (uchar)*(s[code] + i);
            }
        } else if (t.bitsPerSample == 16) {
            for (uint32_t i = 0; i < (uint32_t)sLen[code]; i++) {
                *out++ = (uchar)*(s[code] + i);
                ++n;
                if (n % t.bytesPerRow == 0)
                    out += alphaRowComponent;
            }
        }

        // Add string to nextCode (prevString + strings[code][0])
        if (psLen && nextCode <= maxCode) {
            s[nextCode] = sEnd;
            switch(psLen) {
            case 1:
                *s[nextCode] = ps[0];
                break;
            case 2:
                std::memcpy(s[nextCode], ps, 2);
                break;
            case 4:
                std::memcpy(s[nextCode], ps, 4);
                break;
            default:
                std::memcpy(s[nextCode], ps, psLen);
            }
            *(s[nextCode] + psLen) = *s[code];
            sLen[nextCode] = (int8_t)(psLen + 1);
            sEnd = s[nextCode] + psLen + 1;
            ++nextCode;
        }

        // Copy strings[code] to ps
        switch(sLen[code]) {
        case 1:
            ps[0] = *s[code];
            break;
        case 2:
            ps[0] = *s[code];
            ps[1] = *(s[code] + 1);
            break;
        case 4:
            std::memcpy(ps, s[code], 4);
            break;
        default:
            std::memcpy(ps, s[code], sLen[code]);
        }

        psLen = (size_t)sLen[code];

        // Code size management
        if (nextCode == nextBump) {
            if (nextCode < maxCode) {
                nextBump = (nextBump << 1) + 1;
                ++codeBits;
                mask = (1 << codeBits) - 1;
            } else if (nextCode == maxCode) {
                continue;
            } else {
                codeBits = 9;
                mask = (1 << codeBits) - 1;
                nextBump = 511;
                sEnd = s[257];
                nextCode = 258;
                psLen = 0;
            }
        }
    }

    return;
}

/* Created by ChatGPT
#include <vector> */

Tiff::TiffStrips Tiff::zipDecompress(TiffStrip &t, MetadataParameters &p)
{
    if (G::isLogger || isDebug)
        G::log("Tiff::zipDecompress", "strip = " + QString::number(t.strip) + " " + p.fPath + " Source = " + source);

    qDebug() << "Tiff::zipDecompress" << p.fPath;
    TiffStrips tiffStrips;

    // Prepare zlib structures
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        return tiffStrips; // Initialization failed
    }

    strm.next_in = reinterpret_cast<Bytef*>(t.in);
    strm.avail_in = t.incoming;
    strm.next_out = t.out;
    strm.avail_out = t.stripBytes;

    // Decompress the data
    ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        inflateEnd(&strm);
        return tiffStrips; // Decompression failed
    }

    inflateEnd(&strm);

    // If predictor is used, handle it
    if (t.predictor) {
        uchar* row = t.out;
        for (uint rowIndex = 0; rowIndex < t.stripBytes / t.bytesPerRow; ++rowIndex) {
            uchar* prevRow = row;
            row += t.bytesPerRow;
            for (uint colIndex = 0; colIndex < t.bytesPerRow; ++colIndex) {
                row[colIndex] += prevRow[colIndex];
            }
        }
    }

    tiffStrips.push_back(t);
    return tiffStrips;
}

#include <QImage>
#include <QBuffer>

bool Tiff::jpgDecompress(TiffStrip &t, MetadataParameters &p)
{
    if (G::isLogger || isDebug)
        G::log("Tiff::jpgDecompress", "strip = " + QString::number(t.strip) + " " + p.fPath + " Source = " + source);
    qDebug() << "Tiff::jpgDecompress";
    // Read the JPEG data from the file
    p.file.seek(stripOffsets.at(t.strip));
    QByteArray jpegData = p.file.read(t.stripBytes);

    // Load the JPEG data into a QImage
    QImage tempImage;
    if (!tempImage.loadFromData(jpegData, "JPEG")) {
        return false;
    }

    // Handle 8-bit and 16-bit data
    if (t.bitsPerSample == 8) {
        for (int y = 0; y < tempImage.height(); ++y) {
            uchar* scanLine = tempImage.scanLine(y);
            uchar* out = t.out + y * t.bytesPerRow;
            if (t.predictor) {
                for (int x = 0; x < t.bytesPerRow; ++x) {
                    if (x < 3) {
                        *out++ = *scanLine++;
                    } else {
                        *out++ = (*scanLine++ + *(out - 3));
                    }
                }
            } else {
                std::memcpy(out, scanLine, t.bytesPerRow);
            }
        }
    } else if (t.bitsPerSample == 16) {
        for (int y = 0; y < tempImage.height(); ++y) {
            uchar* scanLine = tempImage.scanLine(y);
            uchar* out = t.out + y * (t.bytesPerRow + t.bytesPerRow / 3);
            if (t.predictor) {
                for (int x = 0; x < t.bytesPerRow; x += 2) {
                    if (x < 6) {
                        *out++ = *scanLine++;
                        *out++ = *scanLine++;
                    } else {
                        *out++ = (*scanLine++ + *(out - 6));
                        *out++ = (*scanLine++ + *(out - 6));
                    }
                }
            } else {
                std::memcpy(out, scanLine, t.bytesPerRow);
                out += t.bytesPerRow / 3;  // Handle alpha component
            }
        }
    }

    return true;
}


/*************************************************************************************************
QTTIFFHANDLER SOURCE
https://github.com/GarageGames/Qt/blob/master/qt-5/qtimageformats/src/plugins/imageformats/tiff/qtiffhandler.cpp
Using this instead of QImage::load works for all types of TIFF files (incl jpeg compressed)
*************************************************************************************************/

QImageIOHandler::Transformations Tiff::exif2Qt(int exifOrientation)
{
    switch (exifOrientation) {
    case 1: // normal
        return QImageIOHandler::TransformationNone;
    case 2: // mirror horizontal
        return QImageIOHandler::TransformationMirror;
    case 3: // rotate 180
        return QImageIOHandler::TransformationRotate180;
    case 4: // mirror vertical
        return QImageIOHandler::TransformationFlip;
    case 5: // mirror horizontal and rotate 270 CW
        return QImageIOHandler::TransformationFlipAndRotate90;
    case 6: // rotate 90 CW
        return QImageIOHandler::TransformationRotate90;
    case 7: // mirror horizontal and rotate 90 CW
        return QImageIOHandler::TransformationMirrorAndRotate90;
    case 8: // rotate 270 CW
        return QImageIOHandler::TransformationRotate270;
    }
    qWarning("Invalid EXIF orientation");
    return QImageIOHandler::TransformationNone;
}

int Tiff::qt2Exif(QImageIOHandler::Transformations transformation)
{
    switch (transformation) {
    case QImageIOHandler::TransformationNone:
        return 1;
    case QImageIOHandler::TransformationMirror:
        return 2;
    case QImageIOHandler::TransformationRotate180:
        return 3;
    case QImageIOHandler::TransformationFlip:
        return 4;
    case QImageIOHandler::TransformationFlipAndRotate90:
        return 5;
    case QImageIOHandler::TransformationRotate90:
        return 6;
    case QImageIOHandler::TransformationMirrorAndRotate90:
        return 7;
    case QImageIOHandler::TransformationRotate270:
        return 8;
    }
    qWarning("Invalid Qt image transformation");
    return 1;
}

void Tiff::convert32BitOrder(void *buffer, int width)
{
    uint32_t *target = reinterpret_cast<uint32_t *>(buffer);
    for (int32_t x=0; x<width; ++x) {
        uint32_t p = target[x];
        // convert between ARGB and ABGR
        target[x] = (p & 0xff000000)
                    | ((p & 0x00ff0000) >> 16)
                    | (p & 0x0000ff00)
                    | ((p & 0x000000ff) << 16);
    }
}

void Tiff::rgb48fixup(QImage *image, bool floatingPoint)
{
    Q_ASSERT(image->depth() == 64);
    const int h = image->height();
    const int w = image->width();
    uchar *scanline = image->bits();
    const qsizetype bpl = image->bytesPerLine();
    quint16 mask = 0xffff;
    const qfloat16 fp_mask = qfloat16(1.0f);
    if (floatingPoint)
        memcpy(&mask, &fp_mask, 2);
    for (int y = 0; y < h; ++y) {
        quint16 *dst = reinterpret_cast<uint16_t *>(scanline);
        for (int x = w - 1; x >= 0; --x) {
            dst[x * 4 + 3] = mask;
            dst[x * 4 + 2] = dst[x * 3 + 2];
            dst[x * 4 + 1] = dst[x * 3 + 1];
            dst[x * 4 + 0] = dst[x * 3 + 0];
        }
        scanline += bpl;
    }
}

void Tiff::rgb96fixup(QImage *image)
{
    Q_ASSERT(image->depth() == 128);
    const int h = image->height();
    const int w = image->width();
    uchar *scanline = image->bits();
    const qsizetype bpl = image->bytesPerLine();
    for (int y = 0; y < h; ++y) {
        float *dst = reinterpret_cast<float *>(scanline);
        for (int x = w - 1; x >= 0; --x) {
            dst[x * 4 + 3] = 1.0f;
            dst[x * 4 + 2] = dst[x * 3 + 2];
            dst[x * 4 + 1] = dst[x * 3 + 1];
            dst[x * 4 + 0] = dst[x * 3 + 0];
        }
        scanline += bpl;
    }
}

void Tiff::rgbFixup(QImage *image)
{
    // Q_ASSERT(floatingPoint);
    if (image->depth() == 64) {
        const int h = image->height();
        const int w = image->width();
        uchar *scanline = image->bits();
        const qsizetype bpl = image->bytesPerLine();
        for (int y = 0; y < h; ++y) {
            qfloat16 *dst = reinterpret_cast<qfloat16 *>(scanline);
            for (int x = w - 1; x >= 0; --x) {
                dst[x * 4 + 3] = qfloat16(1.0f);
                dst[x * 4 + 2] = dst[x];
                dst[x * 4 + 1] = dst[x];
                dst[x * 4 + 0] = dst[x];
            }
            scanline += bpl;
        }
    } else {
        const int h = image->height();
        const int w = image->width();
        uchar *scanline = image->bits();
        const qsizetype bpl = image->bytesPerLine();
        for (int y = 0; y < h; ++y) {
            float *dst = reinterpret_cast<float *>(scanline);
            for (int x = w - 1; x >= 0; --x) {
                dst[x * 4 + 3] = 1.0f;
                dst[x * 4 + 2] = dst[x];
                dst[x * 4 + 1] = dst[x];
                dst[x * 4 + 0] = dst[x];
            }
            scanline += bpl;
        }
    }
}

bool Tiff::readHeaders(TIFF *tiff, QSize &size, QImage::Format &format,
                       uint16_t &photometric, bool &grayscale, bool &floatingPoint,
                       QImageIOHandler::Transformations transformation)
{
/*
    From QTiffHandler.cpp to assign key tiff fields and assign the right QImage format
*/
    uint32_t width;
    uint32_t height;
    if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width)
        || !TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height)
        || !TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photometric)) {
        return false;
    }
    size = QSize(width, height);

    uint16_t orientationTag;
    if (TIFFGetField(tiff, TIFFTAG_ORIENTATION, &orientationTag))
        transformation = exif2Qt(orientationTag);

    // BitsPerSample defaults to 1 according to the TIFF spec.
    uint16_t bitPerSample;
    if (!TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitPerSample))
        bitPerSample = 1;
    uint16_t samplesPerPixel; // they may be e.g. grayscale with 2 samples per pixel
    if (!TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel))
        samplesPerPixel = 1;
    uint16_t sampleFormat;
    if (!TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat))
        sampleFormat = SAMPLEFORMAT_VOID;
    floatingPoint = (sampleFormat == SAMPLEFORMAT_IEEEFP);

    grayscale = photometric == PHOTOMETRIC_MINISBLACK || photometric == PHOTOMETRIC_MINISWHITE;

    if (grayscale && bitPerSample == 1 && samplesPerPixel == 1)
        format = QImage::Format_Mono;
    else if (photometric == PHOTOMETRIC_MINISBLACK && bitPerSample == 8 && samplesPerPixel == 1)
        format = QImage::Format_Grayscale8;
    else if (photometric == PHOTOMETRIC_MINISBLACK && bitPerSample == 16 && samplesPerPixel == 1 && !floatingPoint)
        format = QImage::Format_Grayscale16;
    else if ((grayscale || photometric == PHOTOMETRIC_PALETTE) && bitPerSample == 8 && samplesPerPixel == 1)
        format = QImage::Format_Indexed8;
    else if (samplesPerPixel < 4)
        if (bitPerSample == 16 && (photometric == PHOTOMETRIC_RGB || photometric == PHOTOMETRIC_MINISBLACK))
            format = floatingPoint ? QImage::Format_RGBX16FPx4 : QImage::Format_RGBX64;
        else if (bitPerSample == 32 && floatingPoint && (photometric == PHOTOMETRIC_RGB || photometric == PHOTOMETRIC_MINISBLACK))
            format = QImage::Format_RGBX32FPx4;
        else
            format = QImage::Format_RGB32;
    else {
        uint16_t count;
        uint16_t *extrasamples;
        // If there is any definition of the alpha-channel, libtiff will return premultiplied
        // data to us. If there is none, libtiff will not touch it and  we assume it to be
        // non-premultiplied, matching behavior of tested image editors, and how older Qt
        // versions used to save it.
        bool premultiplied = true;
        bool gotField = TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &count, &extrasamples);
        if (!gotField || !count || extrasamples[0] == EXTRASAMPLE_UNSPECIFIED)
            premultiplied = false;

        if (bitPerSample == 16 && photometric == PHOTOMETRIC_RGB) {
            // We read 64-bit raw, so unassoc remains unpremultiplied.
            if (gotField && count && extrasamples[0] == EXTRASAMPLE_UNASSALPHA)
                premultiplied = false;
            if (premultiplied)
                format = floatingPoint ? QImage::Format_RGBA16FPx4_Premultiplied : QImage::Format_RGBA64_Premultiplied;
            else
                format = floatingPoint ? QImage::Format_RGBA16FPx4 : QImage::Format_RGBA64;
        } else if (bitPerSample == 32 && floatingPoint && photometric == PHOTOMETRIC_RGB) {
            if (gotField && count && extrasamples[0] == EXTRASAMPLE_UNASSALPHA)
                premultiplied = false;
            if (premultiplied)
                format = QImage::Format_RGBA32FPx4_Premultiplied;
            else
                format = QImage::Format_RGBA32FPx4;
        } else {
            if (premultiplied)
                format = QImage::Format_ARGB32_Premultiplied;
            else
                format = QImage::Format_ARGB32;
        }
    }

    return true;
}

bool Tiff::read(QString fPath, QImage *image, quint32 ifdOffset)
{
    TIFF *tiff = TIFFOpen(fPath.toStdString().c_str(), "r");
    if (!tiff) {
        qDebug() << "Tiff::read Failed to open TIFF file." << filePath;
        return false;
    }

    // check if another directory offset
    if (ifdOffset) {
        TIFFSetSubDirectory(tiff, ifdOffset);
    }

    QSize size;
    QImage::Format format;
    QImageIOHandler::Transformations transformation;
    uint16_t photometric;
    bool grayscale;
    bool floatingPoint;

    if (!readHeaders(tiff, size, format, photometric, grayscale, floatingPoint, transformation)) {
        qDebug() << "Tiff::read Failed to read headers." << filePath;
        return false;
    }

    if (!QImageIOHandler::allocateImage(size, format, image)) {
        TIFFClose(tiff); tiff = nullptr;
        qDebug() << "Tiff::read Failed to QImageIOHandler::allocateImage." << filePath;
        return false;
    }

    if (TIFFIsTiled(tiff) && TIFFTileSize64(tiff) > uint64_t(image->sizeInBytes())) {// Corrupt image
        TIFFClose(tiff); tiff = nullptr;
        qDebug() << "Tiff::read Corrupt image." << filePath;
        return false;
    }

    const quint32 width = size.width();
    const quint32 height = size.height();

    // Setup color tables
    if (format == QImage::Format_Mono || format == QImage::Format_Indexed8) {
        if (format == QImage::Format_Mono) {
            QList<QRgb> colortable(2);
            if (photometric == PHOTOMETRIC_MINISBLACK) {
                colortable[0] = 0xff000000;
                colortable[1] = 0xffffffff;
            } else {
                colortable[0] = 0xffffffff;
                colortable[1] = 0xff000000;
            }
            image->setColorTable(colortable);
        } else if (format == QImage::Format_Indexed8) {
            const uint16_t tableSize = 256;
            QList<QRgb> qtColorTable(tableSize);
            if (grayscale) {
                for (int i = 0; i<tableSize; ++i) {
                    const int c = (photometric == PHOTOMETRIC_MINISBLACK) ? i : (255 - i);
                    qtColorTable[i] = qRgb(c, c, c);
                }
            } else {
                // create the color table
                uint16_t *redTable = 0;
                uint16_t *greenTable = 0;
                uint16_t *blueTable = 0;
                if (!TIFFGetField(tiff, TIFFTAG_COLORMAP, &redTable, &greenTable, &blueTable)) {
                    TIFFClose(tiff); tiff = nullptr;
                    qDebug() << "Tiff::read Failed to get field TIFFTAG_COLORMAP." << filePath;
                    return false;
                }
                if (!redTable || !greenTable || !blueTable) {
                    TIFFClose(tiff); tiff = nullptr;
                    return false;
                }

                for (int i = 0; i<tableSize ;++i) {
                    // emulate libtiff behavior for 16->8 bit color map conversion: just ignore the lower 8 bits
                    const int red = redTable[i] >> 8;
                    const int green = greenTable[i] >> 8;
                    const int blue = blueTable[i] >> 8;
                    qtColorTable[i] = qRgb(red, green, blue);
                }
            }
            image->setColorTable(qtColorTable);
            // free redTable, greenTable and greenTable done by libtiff
        }
    }
    bool format8bit = (format == QImage::Format_Mono || format == QImage::Format_Indexed8 || format == QImage::Format_Grayscale8);
    bool format16bit = (format == QImage::Format_Grayscale16);
    bool format64bit = (format == QImage::Format_RGBX64 || format == QImage::Format_RGBA64 || format == QImage::Format_RGBA64_Premultiplied);
    bool format64fp = (format == QImage::Format_RGBX16FPx4 || format == QImage::Format_RGBA16FPx4 || format == QImage::Format_RGBA16FPx4_Premultiplied);
    bool format128fp = (format == QImage::Format_RGBX32FPx4 || format == QImage::Format_RGBA32FPx4 || format == QImage::Format_RGBA32FPx4_Premultiplied);

    // Formats we read directly, instead of over RGBA32:
    if (format8bit || format16bit || format64bit || format64fp || format128fp) {
        int bytesPerPixel = image->depth() / 8;
        if (format == QImage::Format_RGBX64 || format == QImage::Format_RGBX16FPx4)
            bytesPerPixel = photometric == PHOTOMETRIC_RGB ? 6 : 2;
        else if (format == QImage::Format_RGBX32FPx4)
            bytesPerPixel = photometric == PHOTOMETRIC_RGB ? 12 : 4;
        if (TIFFIsTiled(tiff)) {
            quint32 tileWidth, tileLength;
            TIFFGetField(tiff, TIFFTAG_TILEWIDTH, &tileWidth);
            TIFFGetField(tiff, TIFFTAG_TILELENGTH, &tileLength);
            if (!tileWidth || !tileLength || tileWidth % 16 || tileLength % 16) {
                TIFFClose(tiff); tiff = nullptr;
                return false;
            }
            quint32 byteWidth = (format == QImage::Format_Mono) ? (width + 7)/8 : (width * bytesPerPixel);
            quint32 byteTileWidth = (format == QImage::Format_Mono) ? tileWidth/8 : (tileWidth * bytesPerPixel);
            tmsize_t byteTileSize = TIFFTileSize(tiff);
            if (byteTileSize > image->sizeInBytes() || byteTileSize / tileLength < byteTileWidth) {
                TIFFClose(tiff); tiff = nullptr;
                return false;
            }
            uchar *buf = (uchar *)_TIFFmalloc(byteTileSize);
            if (!buf) {
                TIFFClose(tiff); tiff = nullptr;
                return false;
            }
            for (quint32 y = 0; y < height; y += tileLength) {
                for (quint32 x = 0; x < width; x += tileWidth) {
                    if (TIFFReadTile(tiff, buf, x, y, 0, 0) < 0) {
                        _TIFFfree(buf);
                        TIFFClose(tiff); tiff = nullptr;
                        return false;
                    }
                    quint32 linesToCopy = qMin(tileLength, height - y);
                    quint32 byteOffset = (format == QImage::Format_Mono) ? x/8 : (x * bytesPerPixel);
                    quint32 widthToCopy = qMin(byteTileWidth, byteWidth - byteOffset);
                    for (quint32 i = 0; i < linesToCopy; i++) {
                        ::memcpy(image->scanLine(y + i) + byteOffset, buf + (i * byteTileWidth), widthToCopy);
                    }
                }
            }
            _TIFFfree(buf);
        } else {
            if (image->bytesPerLine() < TIFFScanlineSize(tiff)) {
                TIFFClose(tiff); tiff = nullptr;
                return false;
            }
            for (uint32_t y=0; y<height; ++y) {
                if (TIFFReadScanline(tiff, image->scanLine(y), y, 0) < 0) {
                    TIFFClose(tiff); tiff = nullptr;
                    return false;
                }
            }
        }
        if (format == QImage::Format_RGBX64 || format == QImage::Format_RGBX16FPx4) {
            if (photometric == PHOTOMETRIC_RGB)
                rgb48fixup(image, floatingPoint);
            else if (floatingPoint)
                rgbFixup(image);
        } else if (format == QImage::Format_RGBX32FPx4) {
            if (photometric == PHOTOMETRIC_RGB)
                rgb96fixup(image);
            else if (floatingPoint)
                rgbFixup(image);
        }
    } else {
        const int stopOnError = 1;
        if (TIFFReadRGBAImageOriented(tiff, width, height, reinterpret_cast<uint32_t *>(image->bits()), qt2Exif(transformation), stopOnError)) {
            for (uint32_t y=0; y<height; ++y)
                convert32BitOrder(image->scanLine(y), width);
            // qDebug() << "Tiff::read Succeeded: TIFFReadRGBAImageOriented." << filePath;
        } else {
            TIFFClose(tiff); tiff = nullptr;
            // qDebug() << "Tiff::read Failed to TIFFReadRGBAImageOriented." << filePath;
            return false;
        }
    }


    float resX = 0;
    float resY = 0;
    uint16_t resUnit;
    if (!TIFFGetField(tiff, TIFFTAG_RESOLUTIONUNIT, &resUnit))
        resUnit = RESUNIT_INCH;

    if (TIFFGetField(tiff, TIFFTAG_XRESOLUTION, &resX)
        && TIFFGetField(tiff, TIFFTAG_YRESOLUTION, &resY)) {

        switch(resUnit) {
        case RESUNIT_CENTIMETER:
            image->setDotsPerMeterX(qRound(resX * 100));
            image->setDotsPerMeterY(qRound(resY * 100));
            break;
        case RESUNIT_INCH:
            image->setDotsPerMeterX(qRound(resX * (100 / 2.54)));
            image->setDotsPerMeterY(qRound(resY * (100 / 2.54)));
            break;
        default:
            // do nothing as defaults have already
            // been set within the QImage class
            break;
        }
    }

    uint32_t count;
    void *profile;
    if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &count, &profile)) {
        QByteArray iccProfile(reinterpret_cast<const char *>(profile), count);
        image->setColorSpace(QColorSpace::fromIccProfile(iccProfile));
    }
    // We do not handle colorimetric metadat not on ICC profile form, it seems to be a lot
    // less common, and would need additional API in QColorSpace.

    return true;
}
