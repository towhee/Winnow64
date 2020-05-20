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

    // IFD0: Length of main image byte stream
    (ifd->ifdDataHash.contains(279))
        ? m.lengthFull = static_cast<uint>(ifd->ifdDataHash.value(279).tagValue)
        : m.lengthFull = 0;

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

    // IFD0: bitsPerSampleFull
    m.bitsPerSampleFull = 0;
    if (ifd->ifdDataHash.contains(258)) {
        p.file.seek(static_cast<int>(ifd->ifdDataHash.value(258).tagValue));
        m.bitsPerSampleFull = Utilities::get16(p.file.read(2), isBigEnd);
    }

    // IFD0: photoInterpFull
    (ifd->ifdDataHash.contains(262))
            ? m.photoInterpFull = static_cast<int>(ifd->ifdDataHash.value(262).tagValue)
            : m.photoInterpFull = 0;

    // IFD0: samplesPerPixelFull
    (ifd->ifdDataHash.contains(277))
            ? m.samplesPerPixelFull = static_cast<int>(ifd->ifdDataHash.value(277).tagValue)
            : m.samplesPerPixelFull = 0;

    // IFD0: compressionFull
    (ifd->ifdDataHash.contains(259))
            ? m.compressionFull = static_cast<int>(ifd->ifdDataHash.value(259).tagValue)
            : m.compressionFull = 0;

    // IFD0: stripByteCountsFull
    (ifd->ifdDataHash.contains(279))
            ? m.stripByteCountsFull = ifd->ifdDataHash.value(279).tagValue
            : m.stripByteCountsFull = 0;

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

    // IFD0: XMP offset
    quint32 ifdXMPOffset = 0;
    if (ifd->ifdDataHash.contains(700)) {
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

    // Photoshop: **************************************************************
    // Get embedded JPG if available

//    foundTifThumb = false;
//    if (ifdPhotoshopOffset) readIRB(ifdPhotoshopOffset);    // rgh need to add IRB for this and DNG

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

bool Tiff::decode(ImageMetadata &m, QFile &file, QImage &image, bool mainImage)
{
    if (mainImage || !m.bitsPerSampleThumb) {
        w = m.width;
        h = m.height;
        bitsPerSample = m.bitsPerSampleFull;
        photoInterp = m.photoInterpFull;
        samplesPerPixel = m.samplesPerPixelFull;
        compression = m.compressionFull;
        stripByteCounts = m.stripByteCountsFull;
        offset = m.offsetFull;
        length = m.lengthFull;
    }
    else {
        w = m.widthThumb;
        h = m.heightThumb;
        bitsPerSample = m.bitsPerSampleThumb;
        photoInterp = m.photoInterpThumb;
        samplesPerPixel = m.samplesPerPixelThumb;
        compression = m.compressionThumb;
        stripByteCounts = m.stripByteCountsThumb;
        offset = m.offsetThumb;
        length = m.lengthThumb;
    }

    QImage im(w, h, QImage::Format_RGBX64);

    file.open(QIODevice::ReadOnly);
    // first two bytes is the endian order
    quint16 order = Utilities::get16(file.read(2));
    if (order != 0x4D4D && order != 0x4949) return false;
    bool isBigEnd;
    order == 0x4D4D ? isBigEnd = true : isBigEnd = false;

    // define TIFF type
    TiffType tiffType;
    if (bitsPerSample == 16) {
        if (isBigEnd) tiffType = tiff16bit_MM;
        else tiffType = tiff16bit_II;
    }
    else {
        tiffType = unknown;
//        return false;
    }

    qDebug() << __FUNCTION__ << file.fileName()
             << "bitsPerSample =" << bitsPerSample
             << "w =" << w
             << "h =" << h
             << "photoInterp =" << photoInterp
             << "samplesPerPixel =" << samplesPerPixel
             << "compression =" << compression
             << "stripByteCounts =" << stripByteCounts
             << "offset =" << offset
             << "length =" << length
             << "isBigEnd =" << isBigEnd
             << "tiffType =" << tiffType;

    file.seek(offset);

    QByteArray ba, a, r, g, b;
    a = QByteArray::fromHex("FFFF");

    int bytesPerLine = im.bytesPerLine();

    switch(tiffType) {
        case tiff16bit_II:
        for (int line = 0; line < h; ++line) {
            ba.clear();
            for (int x = 0; x < w; ++x) {
                ba += file.read(6) + a;
            }
    //        qDebug() << __FUNCTION__ << line << ba.length() << bytesPerLine;
            std::memcpy(im.scanLine(line), ba, bytesPerLine);
        }
        break;
    case tiff16bit_MM:
        QByteArray b;
        for (int line = 0; line < h; ++line) {
            ba.clear();
            for (int x = 0; x < w; ++x) {
                b = file.read(2);
                ba += b[1];
                ba += b[0];
                b = file.read(2);
                ba += b[1];
                ba += b[0];
                b = file.read(2);
                ba += b[1];
                ba += b[0];
                ba +=  a;
                if (line == 0 && x == 0)
                  qDebug() << __FUNCTION__ << ba;
            }
            std::memcpy(im.scanLine(line), ba, bytesPerLine);
        }
        break;
    }
    im.convertTo(QImage::Format_RGB32);
    image.operator=(im);

    return true;
}
