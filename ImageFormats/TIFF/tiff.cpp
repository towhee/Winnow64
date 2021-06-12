#include "tiff.h"

Tiff::Tiff()
{
}

bool Tiff::parse(MetadataParameters &p,
           ImageMetadata &m,
           IFD *ifd,
           IPTC *iptc,
           Exif *exif,
           Jpeg *jpeg)
{
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
    p.hdr = "IFD0";
    p.offset = ifdOffset;
    p.hash = &exif->hash;
    ifd->readIFD(p, m, isBigEnd);

    m.lengthFull = 1;  // set arbitrary length to avoid error msg as tif do not
                         // have full size embedded jpg

    // IFD0: *******************************************************************

    // IFD0: Offset to main image byte stream
    (ifd->ifdDataHash.contains(273))
        ? m.offsetFull = static_cast<uint>(ifd->ifdDataHash.value(273).tagValue)
        : m.offsetFull = 0;
    m.offsetThumb = m.offsetFull;       // For now.  Add search for smaller frame in tif

    // IFD0: Length of main image byte stream
    (ifd->ifdDataHash.contains(279))
        ? m.lengthFull = static_cast<uint>(ifd->ifdDataHash.value(279).tagValue)
        : m.lengthFull = 0;
    m.lengthThumb = m.lengthFull;       // For now.  Add search for smaller frame in tif

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

//    // IFD0: bitsPerSampleFull
//    m.bitsPerSample = 0;
//    if (ifd->ifdDataHash.contains(258)) {
//        p.file.seek(static_cast<int>(ifd->ifdDataHash.value(258).tagValue));
//        m.bitsPerSample = Utilities::get16(p.file.read(2), isBigEnd);
//    }

//    // IFD0: photoInterpFull
//    (ifd->ifdDataHash.contains(262))
//            ? m.photoInterp = static_cast<int>(ifd->ifdDataHash.value(262).tagValue)
//            : m.photoInterp = 0;

    // IFD0: samplesPerPixelFull
    (ifd->ifdDataHash.contains(277))
            ? m.samplesPerPixel = static_cast<int>(ifd->ifdDataHash.value(277).tagValue)
            : m.samplesPerPixel = 0;

//    // IFD0: compressionFull
//    (ifd->ifdDataHash.contains(259))
//            ? m.compression = static_cast<int>(ifd->ifdDataHash.value(259).tagValue)
//            : m.compression = 0;

//    // IFD0: stripByteCountsFull
//    (ifd->ifdDataHash.contains(279))
//            ? m.stripByteCounts = ifd->ifdDataHash.value(279).tagValue
//            : m.stripByteCounts = 0;

//    // IFD0: planarConfigurationFull
//    (ifd->ifdDataHash.contains(284))
//            ? m.stripByteCounts = ifd->ifdDataHash.value(284).tagValue
//            : m.stripByteCounts = 0;

    p.offset = 0;
    if (!m.width || !m.height) jpeg->getDimensions(p, m);  // rgh does this work??

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

    // subIFDs: ****************************************************************

    /* If save tiff with save pyramid in photoshop then subIFDs created. Iterate to report and
    possibly read smallest for thumbnail if not a thumb in the photoshop IRB. */
    uint thumbWidth = m.width;
    if (ifdsubIFDOffset) {
//        startOffset = 4;
        quint32 nextIFDOffset;
        p.hdr = "SubIFD 1";
        p.offset = ifdsubIFDOffset;
        nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
        if(ifd->ifdDataHash.contains(256)) {
            if (ifd->ifdDataHash.value(256).tagValue < thumbWidth) {
                thumbWidth = ifd->ifdDataHash.value(256).tagValue;
            }
        }
        int count = 2;
        while (nextIFDOffset && count < 5) {
            p.hdr = "SubIFD " + QString::number(count);
            p.offset = nextIFDOffset;
            nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
            if(ifd->ifdDataHash.contains(256)) {
                if (ifd->ifdDataHash.value(256).tagValue < thumbWidth) {
                    thumbWidth = ifd->ifdDataHash.value(256).tagValue;
                }
            }
            count ++;
        }
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

    // Photoshop: **************************************************************
    // Get embedded JPG if available

//    foundTifThumb = false;
//    if (ifdPhotoshopOffset) readIRB(ifdPhotoshopOffset);    // rgh need to add IRB for this and DNG

    // IPTC: *******************************************************************
    // Get title if available

    if (ifdIPTCOffset) iptc->read(p.file, ifdIPTCOffset, m);

    // read XMP
    bool okToReadXmp = true;
    qDebug() << __FUNCTION__ << "m.isXmp" << m.isXmp;
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
    if (G::isLogger) G::log(__FUNCTION__);
    ifd->readIFD(p, m, m.isBigEnd);
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
        m.err += "No strip offsets for " + m.fPath + ". ";
        qDebug() << __FUNCTION__ << m.err;
        return false;
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
        m.err += "No StripByteCounts for " + m.fPath + ". ";
        qDebug() << __FUNCTION__ << m.err;
        return false;
    }

    // IFD0: bitsPerSample
    if (ifd->ifdDataHash.contains(258)) {
        p.file.seek(static_cast<int>(ifd->ifdDataHash.value(258).tagValue));
        bitsPerSample = Utilities::get16(p.file.read(2), m.isBigEnd);
    }
    else {
        m.err += "No BitsPerSample for " + m.fPath + ". ";
        qDebug() << __FUNCTION__ << m.err;
        return false;
    }

    // IFD0: photoInterp
    (ifd->ifdDataHash.contains(262))
            ? photoInterp = static_cast<int>(ifd->ifdDataHash.value(262).tagValue)
            : photoInterp = 0;

    // IFD0: samplesPerPixel
    (ifd->ifdDataHash.contains(277))
            ? samplesPerPixel = static_cast<int>(ifd->ifdDataHash.value(277).tagValue)
            : samplesPerPixel = 0;

    // IFD0: rowsPerStrip
    (ifd->ifdDataHash.contains(278))
            ? rowsPerStrip = static_cast<int>(ifd->ifdDataHash.value(278).tagValue)
            : rowsPerStrip = m.height;

    // IFD0: compression
    (ifd->ifdDataHash.contains(259))
            ? compression = static_cast<int>(ifd->ifdDataHash.value(259).tagValue)
            : compression = 1;

    // IFD0: planarConfiguration
    (ifd->ifdDataHash.contains(284))
            ? planarConfiguration = ifd->ifdDataHash.value(284).tagValue
            : planarConfiguration = 1;

    return true;
}

bool Tiff::decode(ImageMetadata &m, QString &fPath, QImage &image, int newSize)
{
/*
    Decode using unmapped QFile.  Set p.file and call main decode.
*/
//    qDebug() << __FUNCTION__ << "using unmapped QFile";
    if (G::isLogger) G::log(__FUNCTION__, " load file from fPath");
    MetadataParameters p;
    p.file.setFileName(fPath);
    if (!p.file.open(QIODevice::ReadOnly)) {
        m.err += "Unable to open file " + fPath + ". ";
        qDebug() << __FUNCTION__ << m.err;
        return false;
    }
    return decode(m, p, image, newSize);
}

bool Tiff::decode(ImageMetadata &m, MetadataParameters &p, QImage &image, int newSize)
{
/*
    Decoding the Tif occurs here.  p.file must be set and opened.  If called from ImageDecoder
    then p.file will be mapped to memory
*/
    if (G::isLogger) G::log(__FUNCTION__, "Main decode with p.file assigned");
//    qDebug() << __FUNCTION__;
    QElapsedTimer t;
    t.restart();

    Exif *exif = new Exif;
    IFD *ifd = new IFD;
    p.report = false;
    p.hdr = "IFD0";
    p.offset = m.ifd0Offset;
    p.hash = &exif->hash;
    parseForDecoding(p, m, ifd);

    // cancel if there is compression or planar format
    if (compression > 1 || planarConfiguration > 1 /*|| samplesPerPixel > 3*/) {
        qDebug() << __FUNCTION__ << "compression > 1 || planarConfiguration > 1";
        return false;
    }

    // width and height of thumbnail to be created
    int w = m.width;                      // width of thumb to create
    int h = m.height;                     // height of thumb to create
    int nth = 1;

    // newSize = sample from every nth pixel ie to create thumbnail
    if (newSize) {
        if (m.width > m.height) {
            if (newSize > m.width) newSize = m.width;
            w = newSize;
            h = w * m.height / m.width;
        }
        else {
            if (newSize > m.height) newSize = m.height;
            h = newSize;
            w = h * m.width / m.height;
        }
        nth = m.width / w;                           // sample every nth horizontal pixel
    }

    int bytesPerPixel = bitsPerSample / 8 * samplesPerPixel;
    int bytesPerLine = bytesPerPixel * m.width;
    quint32 fSize = static_cast<quint32>(p.file.size());

    QImage *im;

    // define TIFF type
    TiffType tiffType;
    if (bitsPerSample == 16) {
        if (planarConfiguration < 2) {
            tiffType = tiff16bit;
            im = new QImage(w, h, QImage::Format_RGBX64);
        }
        else {
            m.err += "Failed m.planarConfiguration != 1 " + m.fPath + ". ";
            qDebug() << __FUNCTION__ << m.err;
//            qDebug() << __FUNCTION__ << "Failed m.planarConfiguration != 1" << p.file.fileName();
            p.file.close();
            return false;
        }
    }
    else if (bitsPerSample == 8) {
        tiffType = tiff8bit;
        im = new QImage(w, h, QImage::Format_RGB888);
    }
    else {
        m.err += "bitsPerSample =" + QString::number(bitsPerSample) + m.fPath + ". ";
        qDebug() << __FUNCTION__ << m.err;
//        qDebug() << __FUNCTION__ << "bitsPerSample =" << bitsPerSample << p.file.fileName();
        p.file.close();
        return false;
    }

    // read every nth pixel
    if (newSize) {
        int newBytesPerLine = w * bytesPerPixel;
        QByteArray ba;
        quint32 fOffset = 0;
        // read every nth pixel to create new smaller image (thumbnail)
        int y = 0;
        int line = 0;
        for (int strip = 0; strip < stripOffsets.count();++strip) {
            for (int row = 0; row < rowsPerStrip; row++) {
                if (y % nth == 0) {
                    fOffset = stripOffsets.at(strip) + static_cast<quint32>(row * bytesPerLine);
                    ba.clear();
                    for (int x = 0; x < w; x++) {
                        p.file.seek(fOffset);
//                        ba += p.file.read(rgbBytesPerPixel);  // samplesPerPixel > 3 then only use first 3
                        if (fOffset + static_cast<size_t>(bytesPerPixel) > fSize) {
                            qDebug() << __FUNCTION__ << "Read nth pixel - ATTEMPTING TO READ PAST END OF FILE";
                            break;
                        }
                        ba += p.file.read(bytesPerPixel);
                        fOffset += static_cast<uint>(nth * bytesPerPixel);
                    }
                    if (line < h) {
                        if (fOffset + static_cast<size_t>(newBytesPerLine) > fSize) {
                            qDebug() << __FUNCTION__ << "Read scanline - ATTEMPTING TO READ PAST END OF FILE";
                            break;
                        }
                        std::memcpy(im->scanLine(line), ba, static_cast<size_t>(newBytesPerLine));
                    }
                    line++;
                }
                y++;
                if (line > h) break;
            }
            if (line > h) break;
        }
        if (tiffType == tiff16bit) {
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
    if (tiffType == tiff16bit) {
        if (m.isBigEnd) invertEndian16(im);
        toRRGGBBAA(im);
    }
    p.file.close();
    // convert to standard QImage format for display in Winnow
    im->convertTo(QImage::Format_RGB32);
    image.operator=(*im);
    return true;
}

bool Tiff::encodeThumbnail()
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
              6    273       4         1      offset   StripOffsets
              7    274       3         1           1   Orientation
              8    277       3         1           3   SamplesPerPixel
              9    278       3         1        1728   RowsPerStrip
             10    279       4         1    11943936   StripByteCounts
             11    282       5         1      offset   XResolution
             12    283       5         1      offset   YResolution
             13    284       3         1           1   PlanarConfiguration
             14    296       3         1           2   ResolutionUnit
    - subsample image down to thumbnail resolution
    - write strip pixels rgb at offset StripOffsets
*/
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
