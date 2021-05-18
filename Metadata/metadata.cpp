#include "Metadata/metadata.h"
#include <QDebug>
#include "Main/global.h"

Metadata::Metadata(QObject *parent) : QObject(parent)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    // some initialization
    initOrientationHash();
    initSupportedFiles();
    initSupportedLabelsRatings();
    p.report = false;
}

/* METADATA NOTES

Endian:

    Big endian = 4D4D and reads bytes from left to right
                 ie 3F2A = 16170
                 ie 00000001 = 1

    Litte endian = 4949 and reads bytes from right to left
                 ie 3f2A gets read as 2A3F = 10815
                 ie 00000001 gets read as 01000000 = 16777216

TIF data types:
    Value	Type
    1       unsigned byte
    2       ascii strings
    3       unsigned short
    4       unsigned quint32
    5       unsigned rational
    6       signed byte
    7       undefined
    8       signed short
    9       signed quint32
    10      signed rational
    11      single float
    12      double float
    13      quint32 (used for tiff IFD offsets)


*/

void Metadata::initSupportedLabelsRatings()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    ratings << "" << "1" << "2" << "3" << "4" << "5";
    labels << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
}

void Metadata::initSupportedFiles()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    // add raw file types here as they are supported
    hasJpg              << "arw"
                        << "cr2"
                        << "cr3"
                        << "dng"
                        << "nef"
                        << "orf"
                        << "raf"
                        << "sr2"
                        << "rw2"
                           ;

    hasHeic
//                        << "cr3"
                        << "heic"
                        << "hif"
                           ;

    getMetadataFormats  << "arw"
                        << "cr2"
                        << "cr3"
                        << "dng"
                        << "heic"
                        << "hif"
                        << "nef"
                        << "orf"
                        << "raf"
                        << "sr2"
                        << "rw2"
                        << "jpg"
                        << "jpeg"
                        << "tif"
                           ;

    // rotate based on metadata for orientation (heif/hif rotated by library)
    rotateFormats       << "arw"
                        << "cr2"
                        << "cr3"
                        << "dng"
                        << "nef"
                        << "orf"
                        << "raf"
                        << "sr2"
                        << "rw2"
                        << "jpg"
                        << "jpeg"
                        << "tif"
                           ;

    embeddedICCFormats  << "jpg"
                        << "jpeg"
                           ;

    sidecarFormats      << "arw"
                        << "cr2"
                        << "cr3"
                        << "nef"
                        << "orf"
                        << "raf"
                        << "rw2"
                        << "sr2"
                        << "jpg"
                        << "jpeg"
                           ;

    internalXmpFormats  << "notyetjpg";

    iccFormats          << "jpg"
                        << "jpeg"
                        << "arw"
                        << "cr2"
                        << "cr3"
                        << "nef"
                        << "orf"
                        << "raf"
                        << "rw2"
                        << "sr2"
                        << "tif"
                           ;

    noMetadataFormats   << "bmp"
                        << "cur"
                        << "dds"
                        << "gif"
                        << "hif"
                        << "icns"
                        << "ico"
                        << "jp2"
                        << "jpe"
                        << "mng"
                        << "pbm"
                        << "pgm"
                        << "png"
                        << "ppm"
                        << "svg"
                        << "svgz"
                        << "tga"
                        << "wbmp"
                        << "webp"
                        << "xbm"
                        << "xpm"
                        // video formats
                        << "asf"
                        << "amv"
                        << "avi"
                        << "flv"
                        << "gifv"
                        << "mng"
                        << "mts"
                        << "m2ts"
                        << "ogg"
                        << "ogv"
                        << "m2v"
                        << "m4v"
                        << "mkv"
                        << "mov"
                        << "mp2"
                        << "mp4"
                        << "mpg"
                        << "m4p"
                        << "mpe"
                        << "mpeg"
                        << "mpv"
                        << "qt"
                        << "svi"
                        << "vob"
                        << "webm"
                        << "wmv"
                        << "yuv"
                           ;

    supportedFormats    << "arw"
                        << "bmp"
                        << "cr2"
                        << "cr3"
                        << "cur"
                        << "dds"
                        << "dng"
                        << "gif"
                        << "hif"
                        << "heic"
                        << "icns"
                        << "ico"
                        << "jpeg"
                        << "jpg"
                        << "jp2"
                        << "jpe"
                        << "mng"
                        << "nef"
                        << "orf"
                        << "pbm"
                        << "pgm"
                        << "png"
                        << "ppm"
                        << "raf"
                        << "rw2"
                        << "sr2"
                        << "svg"
                        << "svgz"
                        << "tga"
                        << "tif"
                        << "wbmp"
                        << "webp"
                        << "xbm"
                        << "xpm"
                        // video formats (just show thumbnail "Video")
                        << "asf"
                        << "amv"
                        << "avi"
                        << "flv"
                        << "gifv"
                        << "mng"
                        << "mts"
                        << "m2ts"
                        << "ogg"
                        << "ogv"
                        << "m2v"
                        << "m4v"
                        << "mkv"
                        << "mov"
                        << "mp2"
                        << "mp4"
                        << "mpg"
                        << "m4p"
                        << "mpe"
                        << "mpeg"
                        << "mpv"
                        << "qt"
                        << "svi"
                        << "vob"
                        << "webm"
                        << "wmv"
                        << "yuv"
                           ;

        videoFormats    << "asf"
                        << "amv"
                        << "avi"
                        << "flv"
                        << "gifv"
                        << "mng"
                        << "mts"
                        << "m2ts"
                        << "ogg"
                        << "ogv"
                        << "m2v"
                        << "m4v"
                        << "mkv"
                        << "mp2"
                        << "mp4"
                        << "mpg"
                        << "mov"
                        << "m4p"
                        << "mpe"
                        << "mpeg"
                        << "mpv"
                        << "qt"
                        << "svi"
                        << "vob"
                        << "webm"
                        << "wmv"
                        << "yuv"
                           ;
}

void Metadata::initOrientationHash()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    orientationDescription[1] = "Horizontal";
    orientationDescription[2] = "Mirrow horizontal";
    orientationDescription[3] = "Rotate 180";
    orientationDescription[4] = "Mirror vertical";
    orientationDescription[5] = "Mirror horizontal and rotate 270";
    orientationDescription[6] = "Rotate 90";
    orientationDescription[7] = "Mirror horizontal and rotate 90";
    orientationDescription[8] = "Rotate 270";

    orientationToDegrees[1] = 0;
    orientationToDegrees[3] = 180;
    orientationToDegrees[6] = 90;
    orientationToDegrees[8] = 270;

    orientationFromDegrees[0] = 1;
    orientationFromDegrees[90] = 6;
    orientationFromDegrees[180] = 3;
    orientationFromDegrees[270] = 8;
}

void Metadata::reportMetadata()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    QString createdExif = m.createdDate.toString("yyyy-MM-dd hh:mm:ss");
    p.rpt << "\n";
    p.rpt.reset();
    p.rpt.setFieldAlignment(QTextStream::AlignLeft);

    p.rpt.setFieldWidth(25); p.rpt << "offsetFull"          << m.offsetFull;          p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "lengthFull"          << m.lengthFull;          p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "offsetThumb"         << m.offsetThumb;         p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "lengthThumb"         << m.lengthThumb;         p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "isBigEndian"         << m.isBigEnd;            p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "offsetifd0Seg"       << m.ifd0Offset;          p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "offsetXMPSeg"        << m.xmpSegmentOffset;    p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "offsetNextXMPSegment"<< m.xmpNextSegmentOffset;p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "orientation"         << m.orientation;         p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "width"               << m.width;               p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "height"              << m.height;              p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "created"             << createdExif;                       p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "make"                << m.make;                p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "model"               << m.model;               p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "exposureTime"        << m.exposureTime;        p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "aperture"            << m.aperture;            p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "ISO"                 << m.ISO;                 p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "exposureCompensation"<< m.exposureCompensation;p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "focalLength"         << m.focalLength;         p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "title"               << m.title;               p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "lens"                << m.lens;                p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "creator"             << m.creator;             p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "copyright"           << m.copyright;           p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "email"               << m.email;               p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "url"                 << m.url;                 p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "rating"              << m.rating;              p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "label"               << m.label;               p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "cameraSN"            << m.cameraSN;            p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "lensSN"              << m.lensSN;              p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "shutterCount"        << m.shutterCount;        p.rpt.setFieldWidth(0); p.rpt << "\n";
    p.rpt.setFieldWidth(25); p.rpt << "nikonLensCode"       << m.nikonLensCode;       p.rpt.setFieldWidth(0); p.rpt << "\n";

    if (m.isXmp && p.xmpString.length() > 0) {
        p.rpt << "\nXMP Extract:\n\n";
        QXmlQuery query;
        query.setQuery(p.xmpString);

        // Set up the output device
        QByteArray outArray;
        QBuffer buffer(&outArray);
        if (buffer.isOpen()) buffer.close();
        buffer.open(QIODevice::ReadWrite);

        // format xmp
        QXmlFormatter formatter(query, &buffer);
        query.evaluateTo(&formatter);
        buffer.close();

        QString xmpStr = QTextCodec::codecForMib(106)->toUnicode(outArray);
        p.rpt << xmpStr;
    }
}

QString Metadata::diagnostics(QString fPath)
{
    readMetadata(true, fPath, __FUNCTION__);
    return reportString;
}

int Metadata::getNewOrientation(int orientation, int rotation)
{
    if (G::isLogger) G::log(__FUNCTION__); 
    int degrees = orientationToDegrees[orientation];
    degrees += rotation;
    if (degrees > 270) degrees -= 360;
    return orientationFromDegrees[degrees];
}

bool Metadata::writeMetadata(const QString &fPath, ImageMetadata m, QByteArray &buffer)
{
/*
    Called from ingest (Ingestdlg). If it is a supported image type a copy of the image file
    is made and any metadata changes are updated in buffer. If it is a raw file in the
    sidecarFormats hash then the xmp data for existing and changed metadata is written to
    buffer and the original image file is copied unchanged.
*/
    if (G::isLogger) G::log(__FUNCTION__);
    // is xmp supported for this file
    QFileInfo info(fPath);
    QString suffix = info.suffix().toLower();
    if (!sidecarFormats.contains(suffix)) {
//        qDebug() << __FUNCTION__ << "Unable to write xmp buffer."  << suffix << "not in xmpWriteFormats";
        return false;
    }

    bool useSidecar = sidecarFormats.contains(suffix);

    // new orientation
    int newOrientation = getNewOrientation(m.orientation, m.rotationDegrees);

    // has metadata been edited? ( _ is original data)
    bool ratingChanged = m.rating != m._rating;
    bool labelChanged = m.label != m._label;
    bool titleChanged = m.title != m._title;
    bool creatorChanged = m.creator != m._creator;
    bool copyrightChanged = m.copyright != m._copyright;
    bool emailChanged = m.email != m._email;
    bool urlChanged = m.url != m._url;
    bool orientationChanged = m.orientation != newOrientation;
    if (   !ratingChanged
        && !labelChanged
        && !titleChanged
        && !creatorChanged
        && !copyrightChanged
        && !emailChanged
        && !urlChanged
        && !orientationChanged ) {
//        qDebug() << __FUNCTION__ << "Unable to write xmp buffer. No metadata has been edited.";
        return false;
    }

    // data edited, open image file
    p.file.setFileName(fPath);
    // rgh error trap file operation
    if (p.file.isOpen()) p.file.close();
    p.file.open(QIODevice::ReadOnly);

    // update xmp data
    Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpNextSegmentOffset, useSidecar);

    // orientation is written to xmp sidecars only
    if (orientationChanged && useSidecar) {
        QString s = QString::number(newOrientation);
        xmp.setItem("Orientation", s.toLatin1());
    }
    if (urlChanged) xmp.setItem("CiUrlWork", m.url.toLatin1());
    if (emailChanged) xmp.setItem("CiEmailWork", m.email.toLatin1());
    if (copyrightChanged) xmp.setItem("rights", m.copyright.toLatin1());
    if (creatorChanged) xmp.setItem("creator", m.creator.toLatin1());
    if (titleChanged) xmp.setItem("title", m.title.toLatin1());
    if (labelChanged) xmp.setItem("Label", m.label.toLatin1());
    if (ratingChanged) xmp.setItem("Rating", m.rating.toLatin1());

    QString modifyDate = QDateTime::currentDateTime().toOffsetFromUtc
        (QDateTime::currentDateTime().offsetFromUtc()).toString(Qt::ISODate);
    xmp.setItem("ModifyDate", modifyDate.toLatin1());

    // get the buffer to write to a new p.file
    if (suffix == "jpg") {
        p.file.seek(0);
        buffer = p.file.readAll();
        /* Update orientation first, as orientation is written to EXIF, not
        XMP, for known formats. Writing subsequent xmp could change file length
        and make the orientationOffset incorrect.
        */
        if (orientationChanged && m.orientationOffset > 0) {
            QChar c = newOrientation & 0xFF;
            QByteArray ba;
            ba.append(c);
            buffer.replace(static_cast<int>(m.orientationOffset), 1, ba);
        }
        xmp.writeJPG(buffer);
    }
    if (useSidecar)
        xmp.writeSidecar(buffer);

    p.file.close();
    return true;
}

quint32 Metadata::findInFile(QString s, quint32 offset, quint32 range)
{
/*
Returns the file offset to the start of the search string. If not
found returns 0.

QFile p.file must be assigned and open.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    uint firstCharCode = static_cast<unsigned int>(s[0].unicode());
    p.file.seek(offset);
    for (quint32 i = offset; i < offset + range; i++) {
        if (Utilities::get8(p.file.read(1)) == firstCharCode) {
            bool rejected = false;
            for (int j = 1; j < s.length(); j++) {
                uint nextCharCode = static_cast<unsigned int>(s[j].unicode());
                uint byte = Utilities::get8(p.file.read(1));
                if (byte != nextCharCode) rejected = true;
                if (rejected) break;
            }
            if (!rejected) return static_cast<quint32>(p.file.pos() - s.length());
        }
    }
    return 0;
}

// rgh where is this used?  Should it be a separate class like ifd and iptc?
bool Metadata::readIRB(quint32 offset)
{
/*
Read a Image Resource Block looking for embedded thumb
    - see https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
This is a recursive function, iterating through consecutive resource blocks until
the embedded jpg preview is found (irbID == 1036)
*/
    if (G::isLogger) G::log(__FUNCTION__); 

    // Photoshop IRBs use big endian
    quint32 oldOrder = order;
    order = 0x4D4D;

    // check signature to make sure this is the start of an IRB
    p.file.seek(offset);
    QByteArray irbSignature("8BIM");
    QByteArray signature = p.file.read(4);
    if (signature != irbSignature) {
        order = oldOrder;
        return foundTifThumb;
    }

    // Get the IRB ID (we're looking for 1036 = thumb)
    uint irbID = static_cast<uint>(Utilities::get16(p.file.read(2)));
    if (irbID == 1036) foundTifThumb = true;
//    qDebug() << __FUNCTION__ << fPath << "irbID =" << irbID;

    // read the pascal string which we don't care about
    uint pascalStringLength = static_cast<uint>(Utilities::get16(p.file.read(2)));
    if (pascalStringLength > 0) p.file.read(pascalStringLength);

    // get the length of the IRB data block
    quint32 dataBlockLength = Utilities::get32(p.file.read(4));
    // round to even 2 bytes
    dataBlockLength % 2 == 0 ? dataBlockLength : dataBlockLength++;

    // reset order as going to return or recurse next
    order = oldOrder;

    // found the thumb, collect offsets and return
    if (foundTifThumb) {
        m.offsetThumb = static_cast<quint32>(p.file.pos()) + 28;
        m.lengthThumb = dataBlockLength - 28;
        return foundTifThumb;
    }

    // did not find the thumb, try again
    p.file.read(dataBlockLength);
    offset = static_cast<quint32>(p.file.pos());
    readIRB(offset);

    // make the compiler happy
    return false;
}

void Metadata::verifyEmbeddedJpg(quint32 &offset, quint32 &length)
{
/*
JPEGs start with FFD8 and end with FFD9.  This function confirms the embedded
JPEG is correct.  If it is not then the function sets the offset and length
to zero.  At the end of the readMetadata function the offsets and lengths are
checked to make sure there is valid data.

** Not being used **
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    p.file.seek(offset);
    if (Utilities::get16(p.file.peek(2)) == 0xFFD8) {
        p.file.seek(offset + length - 2);
        if (Utilities::get16(p.file.peek(2)) == 0xFFD9) {
            // all is well
            return;
        }
    }
    // problem - set to zero
    offset = 0;
    length = 0;
}

bool Metadata::parseNikon()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (nikon == nullptr) nikon = new Nikon;
    nikon->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseCanon()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (canon == nullptr) canon = new Canon;
    canon->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseCanonCR3()
{
    if (G::isLogger) G::log(__FUNCTION__);
    CanonCR3 canonCR3(p, m, ifd, exif, jpeg);
    canonCR3.parse();
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseOlympus()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (olympus == nullptr) olympus = new Olympus;
    olympus->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseSony()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (sony == nullptr) sony = new Sony;
    sony->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseFuji()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (fuji == nullptr) fuji = new Fuji;
    fuji->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseDNG()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (dng == nullptr) dng = new DNG;
    if (iptc == nullptr) iptc = new IPTC;
    dng->parse(p, m, ifd, iptc, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseTIF()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (tiff == nullptr) tiff = new Tiff;
    tiff->parse(p, m, ifd, iptc, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parsePanasonic()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    if (panasonic == nullptr) panasonic = new Panasonic;
    panasonic->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseJPG(quint32 startOffset)
{
//    qDebug() << __FUNCTION__ << p.file.fileName();
    if (G::isLogger) G::log(__FUNCTION__); 
    if (!p.file.isOpen()) {
        qDebug() << __FUNCTION__ << p.file.fileName() << "is not open";
        return false;
    }
    if (jpeg == nullptr) jpeg = new Jpeg;
    if (ifd == nullptr) ifd = new IFD;
    if (exif == nullptr) exif = new Exif;
    if (gps == nullptr) gps = new GPS;
    p.offset = startOffset;
    if (p.file.fileName() == "") {
        qDebug() << __FUNCTION__ << "Blank file name";
        return false;
    }
    bool ok = jpeg->parse(p, m, ifd, iptc, exif, gps);
    if (ok && p.report) reportMetadata();
    return ok;
}

bool Metadata::parseHEIF()
{
    if (G::isLogger) G::log(__FUNCTION__); 
#ifdef Q_OS_WIN
    // rgh remove heic
    if (heic == nullptr) heic = new Heic;
    bool ok = heic->parseLibHeif(p, m, ifd, exif, gps);
    if (ok && p.report) reportMetadata();
    return ok;
#endif
}

void Metadata::clearMetadata()
{
    if (G::isLogger) G::log(__FUNCTION__); 
    m.offsetFull = 0;
    m.lengthFull = 0;
    m.widthFull = 0;
    m.heightFull = 0;
    m.offsetThumb = 0;
    m.lengthThumb = 0;
    m.isBigEnd = false;
    m.ifd0Offset = 0;
    m.offsetFull = 0;
    m.lengthFull = 0;
    m.widthFull = 0;
    m.heightFull = 0;
    m.offsetThumb = 0;
    m.lengthThumb = 0;
    m.xmpSegmentOffset = 0;
    m.orientationOffset = 0;
    m.iccSegmentOffset = 0;
    m.iccSegmentLength = 0;
    m.iccBuf.clear();
    m.width = 0;
    m.height = 0;
    m.orientation = 1;
    m.rotationDegrees = 0;
    m.make = "";
    m.model = "";
    m.exposureTime = "";
    m.exposureTimeNum = 0;
    m.aperture = "";
    m.apertureNum = 0;
    m.ISO = "";
    m.ISONum = 0;
    m.exposureCompensation = "";
    m.exposureCompensationNum = 0;
    m.focalLength = "";
    m.focalLengthNum = 0;
    m.title = "";
    m.lens = "";
    m.creator = "";
    m.copyright = "";
    m.email = "";
    m.url = "";
    m.err.clear();
    m.shutterCount = 0;
    m.cameraSN = "";
    m.lensSN = "";
    m.rating = "";
    m.label = "";
    m.loadMsecPerMp = 0;
    nikonLensCode = "";
}

void Metadata::testNewFileFormat(const QString &path)
{
    p.report = true;

    if (p.report) {
        p.rpt.flush();
        reportString = "";
        p.rpt.setString(&reportString);
        p.rpt << "\nFile name = " << path << "\n";
    }
    clearMetadata();
    p.file.setFileName(path);
    if (p.file.isOpen()) p.file.close();
    p.file.open(QIODevice::ReadOnly);

    // edit test format to use:
    parseDNG();
    p.file.close();
//    reportMetadata();
}

bool Metadata::readMetadata(bool isReport, const QString &path, QString source)
{
    if (G::isLogger) G::log(__FUNCTION__, "Source: " + source);
//    G::log(__FUNCTION__, "Source =" + source + "  " + path);
//    qDebug() << __FUNCTION__ << "called by" << source << path;
//    isReport = true;
    p.report = isReport;

    if (p.report) {
        p.rpt.flush();
        reportString = "";
        p.rpt.setString(&reportString);
        p.rpt << Utilities::centeredRptHdr('=', "Metadata Diagnostics for Current Image");
        p.rpt << "\n";
    }
    clearMetadata();

    if (p.file.isOpen()) p.file.close();
    if (p.file.isOpen()) {
        qDebug() << __FUNCTION__ << "Could not close" << path;
        return false;
    }
    p.file.setFileName(path);

    if (p.report) {
        p.rpt << "\nFile name = " << path << "\n";
    }
    QFileInfo fileInfo(path);
    QString ext = fileInfo.suffix().toLower();
    bool parsed = false;
    // rgh next triggers crash sometimes when skip to end of thumbnails
    if (p.file.open(QIODevice::ReadOnly)) {
        if (jpeg == nullptr) jpeg = new Jpeg;
        if (ifd == nullptr) ifd = new IFD;
        if (exif == nullptr) exif = new Exif;
        if (gps == nullptr) gps = new GPS;
        if (ext == "arw")  parsed = parseSony();
        if (ext == "cr2")  parsed = parseCanon();
        if (ext == "cr3")  parsed = parseCanonCR3();
        if (ext == "dng")  parsed = parseDNG();
        if (ext == "heic") parsed = parseHEIF();   // rgh remove heic ??
        if (ext == "hif")  parsed = parseHEIF();
        if (ext == "jpg")  parsed = parseJPG(0);
        if (ext == "jpeg") parsed = parseJPG(0);
        if (ext == "nef")  parsed = parseNikon();
        if (ext == "orf")  parsed = parseOlympus();
        if (ext == "raf")  parsed = parseFuji();
        if (ext == "rw2")  parsed = parsePanasonic();
        if (ext == "tif")  parsed = parseTIF();
        p.file.close();
        if (p.file.isOpen()) {
            qDebug() << __FUNCTION__ << "Could not close" << path << "after format was read";
        }
        if (!parsed) {
            p.file.close();
            m.err += "Unable to read format for " + path + ". ";
            qDebug() << __FUNCTION__ << m.err;
            return false;
        }
    }
    else {
        if (p.file.isOpen()) p.file.close();
        qDebug() << __FUNCTION__ << "Could not open " << path;
        m.err += "Could not open p.file to read metadata. ";
        return false;
    }

    // not all files have thumb or small jpg embedded
    if (m.offsetFull == 0 && ext != "jpg" && parsed) {
        m.err += "No embedded JPG found. ";
    }

    if (m.lengthFull == 0 && m.lengthThumb > 0) {
        m.offsetFull = m.offsetThumb;
        m.lengthFull = m.lengthThumb;
    }

    if (m.lengthThumb == 0 && m.lengthFull > 0) {
        m.offsetThumb = m.offsetFull;
        m.lengthThumb = m.lengthFull;
    }

    // error flags
    thumbUnavailable = imageUnavailable = false;
    if (m.lengthFull == 0) {
        imageUnavailable = true;
        m.err += "No embedded preview found. ";
    }
    if (m.lengthThumb == 0) {
        thumbUnavailable = true;
        m.err += "No embedded thumbnail or preview found. ";
    }

    return true;
}

bool Metadata::loadImageMetadata(const QFileInfo &fileInfo,
                                 bool essential, bool nonEssential,
                                 bool isReport, bool isLoadXmp, QString source)
{
//    mutex.lock();
    if (G::isLogger) G::log(__FUNCTION__, fileInfo.filePath() + "  Source: " + source);
//    qDebug() << __FUNCTION__ << "called by" << source;
    // check if already loaded
    QString fPath = fileInfo.filePath();
    if (fPath == "") {
        qDebug() << __FUNCTION__ << "NULL FILE REQUESTED FROM "
                 << source;
//        mutex.unlock();
        return false;
    }

    // check if format with metadata
    QString ext = fileInfo.suffix().toLower();
    if (!getMetadataFormats.contains(ext)) {
        clearMetadata();
//        mutex.unlock();
        return false;
    }

    // For JPG, readNonEssentialMetadata adds 10-15% time to load
    readEssentialMetadata = essential;
    readNonEssentialMetadata = nonEssential;
    okToReadXmp = isLoadXmp;
    okToReadXmp = true;

    // read metadata
    m.metadataLoaded = readMetadata(isReport, fPath, source);
    if (!m.metadataLoaded) {
        qDebug() << __FUNCTION__ << m.err << source;
//        mutex.unlock();
        return false;
    }

    m.isPicked = false;

    m.copyFileNamePrefix = m.createdDate.toString("yyyy-MM-dd");

    QString s = m.model;
    s += "  " + m.focalLength;
    s += "  " + m.exposureTime;
    s += (m.aperture == "") ? "" : " at " + m.aperture;
    s += (m.ISO == "") ? "" : ", ISO " + m.ISO;
    m.shootingInfo = s;
    m.loadMsecPerMp = 0;

    m.thumbUnavailable = thumbUnavailable;
    m.imageUnavailable = imageUnavailable;
//    mutex.unlock();

    return m.metadataLoaded;
}

bool Metadata::writeMetadata(QStringList &paths, const QString tag, const QString tagValue)
{
    if (G::isLogger) G::log(__FUNCTION__, tag);
    QString tagName;
    if (tag == "Title") tagName = "XMP-dc:Title";
    if (tag == "Creator") tagName = "XMP-dc:creator";
    if (tag == "Copyright") tagName = "";
    if (tag == "Email") tagName = "";
    if (tag == "Url") tagName = "";
    ExifTool et;
    et.overwrite();
    for (int i = 0; i < paths.length(); ++i) {

    }
    return true;
}

// End Metadata
