#include "Metadata/metadata.h"
#include <QDebug>
#include "ImageFormats/Heic/heic.h"
#include "Main/global.h"

Metadata::Metadata(QObject *parent) : QObject(parent)
{
    if (G::isLogger) G::log("Metadata::Metadata");
    // some initialization
    initOrientationHash();
    initSupportedFiles();
    initSupportedLabelsRatings();
    p.report = false;

    canon = new Canon;
    canonCR3 = new CanonCR3;
    dng = new DNG;
    fuji = new Fuji;
    nikon = new Nikon;
    olympus = new Olympus;
    panasonic = new Panasonic;
    sony = new Sony;
    tiff = new Tiff;

    #ifdef Q_OS_WIN
    exifToolPath = qApp->applicationDirPath() + "/et.exe";
    #endif
    #ifdef Q_OS_MAC
    exifToolPath = qApp->applicationDirPath() + "/ExifTool/exiftool";
    #endif
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
    if (G::isLogger) G::log("Metadata::initSupportedLabelsRatings");
    ratings << "" << "1" << "2" << "3" << "4" << "5";
    labels << "Red" << "Yellow" << "Green" << "Blue" << "Purple";
}

void Metadata::initSupportedFiles()
{
    if (G::isLogger) G::log("Metadata::initSupportedFiles");
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

    hasMetadataFormats  << "arw"
                        << "cr2"
                        << "cr3"
                        << "dng"
                        << "heic"
                        << "hif"
                        << "nef"
                        << "orf"
                        //<< "png"
                        << "raf"
                        << "sr2"
                        << "rw2"
                        << "jpg"
                        << "jpeg"
                        << "tif"
                           ;

    canEmbedThumb       << "jpg"
                        << "jpeg"
                        << "tif"
                           ;

    // rotate based on metadata for orientation (heif/hif rotated by library)
    rotateFormats       << "arw"
                        << "cr2"
                        << "cr3"
                        << "dng"
                        << "heic"
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
                        << "xmp"
                        << "tif"
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
    if (G::isLogger) G::log("Metadata::initOrientationHash");
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

    if (G::isLogger) G::log("Metadata::reportMetadata");
    QString createdExif = m.createdDate.toString("yyyy-MM-dd hh:mm:ss");
    MetaReport::header("Winnow datamodel", p.rpt);
//    p.rpt << "\nDATAMODEL:\n\n";
    p.rpt.reset();
    p.rpt.setFieldAlignment(QTextStream::AlignLeft);
    int n = 25;

    p.rpt << G::sj("fPath", n) << G::s(m.fPath) << "\n";
    p.rpt << G::sj("fName", n) << G::s(m.fName) << "\n";
    p.rpt << G::sj("type", n) << G::s(m.type) << "\n";
    p.rpt << G::sj("video", n) << G::s(m.video) << "\n";
    p.rpt << G::sj("size", n) << G::s(m.size) << "\n";
    p.rpt << G::sj("createdDate", n) << G::s(m.createdDate) << "\n";
    p.rpt << G::sj("modifiedDate", n) << G::s(m.modifiedDate) << "\n";
    p.rpt << G::sj("year", n) << G::s(m.year) << "\n";
    p.rpt << G::sj("day", n) << G::s(m.day) << "\n";
    p.rpt << "\n";
    p.rpt << G::sj("pick", n) << G::s(m.pick) << "\n";
    p.rpt << G::sj("ingested", n) << G::s(m.ingested) << "\n";
    p.rpt << G::sj("metadataLoaded", n) << G::s(m.metadataLoaded) << "\n";
    p.rpt << G::sj("isSearch", n) << G::s(m.isSearch) << "\n";
    p.rpt << "\n";
    p.rpt << G::sj("width", n) << G::s(m.width) << "\n";
    p.rpt << G::sj("height", n) << G::s(m.height) << "\n";
    p.rpt << G::sj("widthPreview", n) << G::s(m.widthPreview) << "\n";
    p.rpt << G::sj("heightPreview", n) << G::s(m.heightPreview) << "\n";
    p.rpt << G::sj("dimensions", n) << G::s(m.dimensions) << "\n";
    p.rpt << G::sj("megapixels", n) << G::s(m.megapixels) << "\n";
    p.rpt << G::sj("loadMsecPerMp", n) << G::s(m.loadMsecPerMp) << "\n";
    p.rpt << G::sj("aspectRatio", n) << G::s(m.aspectRatio) << "\n";
    p.rpt << G::sj("orientation", n) << G::s(m.orientation) << "\n";
    p.rpt << G::sj("orientationOffset", n) << G::s(m.orientationOffset) << "\n";
    p.rpt << G::sj("rotationDegrees", n) << G::s(m.rotationDegrees) << "\n";
    p.rpt << "\n";
    p.rpt << G::sj("make", n) << G::s(m.make) << "\n";
    p.rpt << G::sj("model", n) << G::s(m.model) << "\n";
    p.rpt << G::sj("exposureTime", n) << G::s(m.exposureTime) << "\n";
    p.rpt << G::sj("aperture", n) << G::s(m.aperture) << "\n";
    p.rpt << G::sj("ISO", n) << G::s(m.ISO) << "\n";
    p.rpt << G::sj("exposureCompensation", n) << G::s(m.exposureCompensation) << "\n";
    p.rpt << G::sj("focalLength", n) << G::s(m.focalLength) << "\n";
    p.rpt << G::sj("lens", n) << G::s(m.lens) << "\n";
    p.rpt << G::sj("shootingInfo", n) << G::s(m.shootingInfo) << "\n";
    p.rpt << G::sj("gpsCoord", n) << G::s(m.gpsCoord) << "\n";
    p.rpt << G::sj("keywords", n) << G::s(Utilities::stringListToString(m.keywords)) << "\n";
    p.rpt << G::sj("duration", n) << G::s(m.duration) << "\n";
    p.rpt << G::sj("cameraSN", n) << G::s(m.cameraSN) << "\n";
    p.rpt << G::sj("lensSN", n) << G::s(m.lensSN) << "\n";
    p.rpt << G::sj("shutterCount", n) << G::s(m.shutterCount) << "\n";
    p.rpt << G::sj("nikonLensCode", n) << G::s(m.nikonLensCode) << "\n";
    p.rpt << "\n";
    p.rpt << G::sj("title", n) << G::s(m.title) << "\n";
    p.rpt << G::sj("_title", n) << G::s(m._title) << "\n";
    p.rpt << G::sj("creator", n) << G::s(m.creator) << "\n";
    p.rpt << G::sj("_creator", n) << G::s(m._creator) << "\n";
    p.rpt << G::sj("copyright", n) << G::s(m.copyright) << "\n";
    p.rpt << G::sj("_copyright", n) << G::s(m._copyright) << "\n";
    p.rpt << G::sj("email", n) << G::s(m.email) << "\n";
    p.rpt << G::sj("_email", n) << G::s(m._email) << "\n";
    p.rpt << G::sj("url", n) << G::s(m.url) << "\n";
    p.rpt << G::sj("_url", n) << G::s(m._url) << "\n";
    p.rpt << G::sj("label", n) << G::s(m.label) << "\n";
    p.rpt << G::sj("_label", n) << G::s(m._label) << "\n";
    p.rpt << G::sj("rating", n) << G::s(m.rating) << "\n";
    p.rpt << G::sj("_rating", n) << G::s(m._rating) << "\n";
    p.rpt << "\n";
    p.rpt << G::sj("offsetFull", n) << G::s(m.offsetFull) << "\n";
    p.rpt << G::sj("lengthFull", n) << G::s(m.lengthFull) << "\n";
    p.rpt << G::sj("offsetThumb", n) << G::s(m.offsetThumb) << "\n";
    p.rpt << G::sj("lengthThumb", n) << G::s(m.lengthThumb) << "\n";
    p.rpt << G::sj("samplesPerPixel", n) << G::s(m.samplesPerPixel) << "\n";
    p.rpt << G::sj("isBigEndian", n) << G::s(m.isBigEnd) << "\n";
    p.rpt << G::sj("ifd0Offset", n) << G::s(m.ifd0Offset) << "\n";
    p.rpt << G::sj("isXmp", n) << G::s(m.isXmp) << "\n";
    p.rpt << G::sj("xmpSegmentOffset", n) << G::s(m.xmpSegmentOffset) << "\n";
    p.rpt << G::sj("xmpSegmentLength", n) << G::s(m.xmpSegmentLength) << "\n";
    p.rpt << G::sj("iccSegmentOffset", n) << G::s(m.iccSegmentOffset) << "\n";
    p.rpt << G::sj("iccSegmentLength", n) << G::s(m.iccSegmentLength) << "\n";
    p.rpt << G::sj("iccSpace", n) << G::s(m.iccSpace) << "\n";
    p.rpt << "\n";
    p.rpt << G::sj("searchStr", n) << G::s(m.searchStr) << "\n";

    if (m.isXmp) {
        // sidecar xmp
        MetaReport::header("Embedded XMP Extract", p.rpt);
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength, p.instance);
        if (xmp.isValid) p.rpt << xmp.docToQString();
        else {
            p.rpt << "ERROR: " << xmp.errMsg[xmp.err] << "\n";
            p.rpt << xmp.docToQString();
        }
    }
}

QString Metadata::diagnostics(QString fPath)
{
    readMetadata(true, fPath, "Metadata::diagnostics");
    return reportString;
}

void Metadata::missingThumbnailWarning()
{
    return;
    QString msg =
            "JPG and/or TIFF files were found without embedded thumbnails.<p>"
            "Add missing thumbnails is turned off in preferences.<p>"
            "Turning on add missing thumbnails will dramatically improve<br>"
            "folder loading performance.<p>"
            "WARNING: this will change your TIFF/JPG file.  Please make<br>"
            "sure you have backups until you are sure this does not corrupt<br>"
            "your images.<p>"
            "Press ESC to continue.";
    G::popUp->showPopup(msg, 0, true, 0.75, Qt::AlignLeft);
}

int Metadata::getNewOrientation(int orientation, int rotation)
{
    if (G::isLogger) G::log("Metadata::getNewOrientation");
    int degrees = orientationToDegrees[orientation];
    degrees += rotation;
    if (degrees > 270) degrees -= 360;
    return orientationFromDegrees[degrees];
}

void Metadata::writeOrientation(QString fPath, QString orientationNumber)
{
    if (G::isLogger) G::log("Metadata::writeOrientation");
    qDebug() << "Metadata::writeOrientation" << fPath;
    if (G::modifySourceFiles) {
        ExifTool et;
        et.setOverWrite(true);
        et.writeOrientation(fPath, orientationNumber);
        et.close();
    }
}

bool Metadata::writeXMP(const QString &fPath, QString src)
{
/*
    Called by MW::setRating, MW::setColorClass, InfoView::dataChanged

    fPath: the absolute path of the file to receive the xmp metadata
    buffer: byte array containing the xmp data

    m: the current state of the metadata for fPath MUST BE UPDATED BEFORE CALLING with
       dm->imMetadata(fPath);
    because dm is not available from Metadata.

    If it is a supported image type a copy of the image file is made and any metadata changes
    are updated in buffer. If it is a raw file in the sidecarFormats hash then the xmp data
    for existing and changed metadata is written to buffer and the original image file is
    copied unchanged.
*/
    if (G::isLogger) G::log("Metadata::writeXMP");

    // is xmp supported for this file
    QFileInfo info(fPath);
    QString suffix = info.suffix().toLower();

    // TEMP PREVENT WRITING TO ANYTHING BUT .XMP
    if (suffix != "xmp") return false;

//    if (!sidecarFormats.contains(suffix)) {
////        qDebug() << "Metadata::writeXMP" << "Unable to write xmp buffer."  << suffix << "not in xmpWriteFormats";
//        return false;
//    }

    // write to a sidecar file for all formats for now.  May write inside source image in the future
//    bool useSidecar = true;
//    useSidecar = sidecarFormats.contains(suffix);

    // new orientation
    int newOrientation = getNewOrientation(m.orientation, m.rotationDegrees);

    /* debug
    qDebug() << "Metadata::writeXMP" << "m.rating =" << m.rating << "m._rating = " << m._rating;
    qDebug() << "Metadata::writeXMP" << "m.label =" << m.label << "m._label = " << m._label;
    qDebug() << "Metadata::writeXMP" << "m.title =" << m.title << "m._title = " << m._title;
    qDebug() << "Metadata::writeXMP" << "m.creator =" << m.creator << "m._creator = " << m._creator;
    qDebug() << "Metadata::writeXMP" << "m.copyright =" << m.copyright << "m._copyright = " << m._copyright;
    qDebug() << "Metadata::writeXMP" << "m.email =" << m.email << "m._email = " << m._email;
    qDebug() << "Metadata::writeXMP" << "m.url =" << m.url << "m._url = " << m._url;

//    qDebug() << "Metadata::writeXMP" << "m.orientation =" << m.orientation << "m._orientation = " << m._orientation;
//    qDebug() << "Metadata::writeXMP" << "m.rotationDegrees =" << m.rotationDegrees << "m._rotationDegrees = " << m._rotationDegrees;
    qDebug();
    //*/

    // has metadata been edited? ( _ is original data)
    bool ratingChanged = m.rating != m._rating;
    bool labelChanged = m.label != m._label;
    bool titleChanged = m.title != m._title;
    bool creatorChanged = m.creator != m._creator;
    bool copyrightChanged = m.copyright != m._copyright;
    bool emailChanged = m.email != m._email;
    bool urlChanged = m.url != m._url;
//    bool orientationChanged = m.orientation != m._orientation;
//    bool rotationChanged = m.rotationDegrees != m._rotationDegrees;
    if (   !ratingChanged
        && !labelChanged
        && !titleChanged
        && !creatorChanged
        && !copyrightChanged
        && !emailChanged
        && !urlChanged
//        && !orientationChanged
//        && !rotationChanged
       ) {
        qDebug() << "Metadata::writeXMP" << "Unable to write xmp buffer. No metadata has been edited."
                 << "src =" << src;
        return false;
    }

    // make sure file is available ie usb drive may have been ejected
//    QFileInfo fileInfo(fPath);
//    if (!fileInfo.exists()) return false;

    // data edited, open image file
    p.file.setFileName(fPath);
    if (p.file.isOpen()) return false;
    // rgh error trap file operation
    if (p.file.isOpen()) {
        p.file.close();
    }
    if (!p.file.open(QIODevice::ReadWrite)) return false;

    // if current xmp is invalid then fix
    Xmp xmp(p.file, p.instance);
    if (!xmp.isValid) xmp.fix();

    /*
    // orientation is written to xmp sidecars only
    if (orientationChanged && G::useSidecar) {
        QString s = QString::number(newOrientation);
        xmp.setItem("Orientation", s.toLatin1());
    }
    //*/

    // update xmp data
    if (urlChanged) xmp.setItem("url", m.url.toLatin1());
    if (emailChanged) xmp.setItem("email", m.email.toLatin1());
    if (copyrightChanged) xmp.setItem("rights", m.copyright.toLatin1());
    if (creatorChanged) xmp.setItem("creator", m.creator.toLatin1());
    if (titleChanged) xmp.setItem("title", m.title.toLatin1());
    if (labelChanged) xmp.setItem("Label", m.label.toLatin1());
    if (ratingChanged) xmp.setItem("Rating", m.rating.toLatin1());

    QString modifyDate = QDateTime::currentDateTime().toOffsetFromUtc
        (QDateTime::currentDateTime().offsetFromUtc()).toString(Qt::ISODate);
    xmp.setItem("ModifyDate", modifyDate.toLatin1());


    // get the buffer to write to a new p.file
    /* write orientation directly into jpg
    if (suffix == "jpg") {
        QByteArray buffer;
        p.file.seek(0);
        buffer = p.file.readAll();
//        Update orientation first, as orientation is written to EXIF, not
//        XMP, for known formats. Writing subsequent xmp could change file length
//        and make the orientationOffset incorrect.

        if (orientationChanged && m.orientationOffset > 0) {
            QChar c = newOrientation & 0xFF;
            QByteArray ba;
            ba.append(c);
            buffer.replace(static_cast<int>(m.orientationOffset), 1, ba);
        }
        xmp.writeJPG(buffer);
    }
    //*/

//    if (G::useSidecar) xmp.writeSidecar();
    xmp.writeSidecar(p.file);

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
    if (G::isLogger) G::log("Metadata::findInFile");
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

void Metadata::verifyEmbeddedJpg(quint32 &offset, quint32 &length)
{
/*
JPEGs start with FFD8 and end with FFD9.  This function confirms the embedded
JPEG is correct.  If it is not then the function sets the offset and length
to zero.  At the end of the readMetadata function the offsets and lengths are
checked to make sure there is valid data.

** Not being used **
*/
    if (G::isLogger) G::log("Metadata::verifyEmbeddedJpg");
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
    if (G::isLogger) G::log("Metadata::parseNikon");
    if (nikon == nullptr) nikon = new Nikon;
    nikon->parse(p, m, ifd, exif, jpeg, gps);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseCanon()
{
    if (G::isLogger) G::log("Metadata::parseCanon");
    if (canon == nullptr) canon = new Canon;
    canon->parse(p, m, ifd, exif, jpeg, gps);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseCanonCR3()
{
    if (G::isLogger) G::log("Metadata::parseCanonCR3");
//    CanonCR3 canonCR3(p, m, ifd, exif, jpeg);
//    canonCR3.parse();
    if (canonCR3 == nullptr) canonCR3 = new CanonCR3;
    canonCR3->parse(&p, &m, ifd, exif, jpeg, gps);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseOlympus()
{
    if (G::isLogger) G::log("Metadata::parseOlympus");
    if (olympus == nullptr) olympus = new Olympus;
    olympus->parse(p, m, ifd, exif, jpeg, gps);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseSony()
{
    if (G::isLogger) G::log("Metadata::parseSony");
    if (sony == nullptr) sony = new Sony;
    sony->parse(p, m, ifd, exif, jpeg, gps);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseFuji()
{
    if (G::isLogger) G::log("Metadata::parseFuji");
    if (fuji == nullptr) fuji = new Fuji;
    fuji->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseDNG()
{
    if (G::isLogger) G::log("Metadata::parseDNG");
    if (dng == nullptr) dng = new DNG;
    if (iptc == nullptr) iptc = new IPTC;
    dng->parse(p, m, ifd, iptc, exif, jpeg, gps);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseTIF()
{
    if (G::isLogger) G::log("Metadata::parseTIF");
    if (tiff == nullptr) tiff = new Tiff;
    if (irb == nullptr) irb = new IRB;
    tiff->parse(p, m, ifd, irb, iptc, exif, gps);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parsePanasonic()
{
    if (G::isLogger) G::log("Metadata::parsePanasonic");
    if (panasonic == nullptr) panasonic = new Panasonic;
    panasonic->parse(p, m, ifd, exif, jpeg);
    if (p.report) reportMetadata();
    return true;
}

bool Metadata::parseJPG(quint32 startOffset)
{
//    qDebug() << "Metadata::parseJPG" << p.file.fileName();
    if (G::isLogger) G::log("Metadata::parseJPG");
    if (!p.file.isOpen()) {
        qWarning() << "WARNING" << "Metadata::parseJPG" << p.file.fileName() << "is not open";
        return false;
    }

    // might be a HEIC
    if (p.file.read(14).contains("ftypheic")) {
        return parseHEIF();
    }
    p.file.seek(0);

    p.offset = startOffset;
    if (p.file.fileName() == "") {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "Metadata::parseJPG" << "Blank file name";
        return false;
    }
    bool ok = jpeg->parse(p, m, ifd, iptc, exif, gps);
    if (ok && p.report) reportMetadata();

    // check if HEIC file with JPG extension
    if (!ok) {

    }

    // fix missing thumbnail (use external program ExifTool)
    if (ok && m.isEmbeddedThumbMissing && m.isReadWrite &&
        G::modifySourceFiles && G::autoAddMissingThumbnails)
    {
        p.file.close();
        jpeg->embedThumbnail(m);
    }

    return ok;
}

bool Metadata::parseHEIF()
{
    if (G::isLogger) G::log("Metadata::parseHEIF");
    // might be a JPG
    if (Utilities::get16(p.file.read(2)) == 0xFFD8) {
        if (G::isWarningLogger)
        qDebug() << "Metadata::parseHEIF  is a jpg";
        parseJPG(0);
    }
#ifdef Q_OS_WIN
    // rgh remove heic
    if (heic == nullptr) heic = new Heic;
    bool ok = heic->parseLibHeif(p, m, ifd, exif, gps);
    if (ok && p.report) reportMetadata();
    return ok;
#endif
#ifdef Q_OS_MAC
    heic = new Heic;
    bool ok = heic->parseHeic(p, m, ifd, exif, gps);
    delete heic;
    if (ok && p.report) reportMetadata();
    return ok;
#endif
}

bool Metadata::parseSidecar()
{
    if (G::isLogger) G::log("Metadata::parseSidecar");
    if (G::stop) return false;

    QFileInfo info(p.file);
    QString sidecarPath = info.absoluteDir().path() + "/" + info.baseName() + ".xmp";
    QFile sidecarFile(sidecarPath);
    /* debug
    qDebug() << "Metadata::parseSidecar"
             << "sidecarPath" << sidecarPath
             << "sidecarFile.exists()" << sidecarFile.exists()
                ;
                //*/

    // no sidecar file
    if (!sidecarFile.exists()) {
        return false;
    }

    if (G::instanceClash(p.instance, "Metadata::parseSidecar")) {
        return false;
    }

    if (!sidecarFile.open(QIODevice::ReadOnly)) {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "Metadata::parseSidecar" << "Failed to open sidecar file" << sidecarPath;
        return false;
    }

    // parse sidecar
    Xmp xmp(sidecarFile, p.instance);

    // report
    if (p.report) {
        MetaReport::header("Sidecar XMP Extract", p.rpt);
        if (!xmp.err) p.rpt << xmp.docToQString();
        else {
            p.rpt << "ERROR: " << xmp.errMsg[xmp.err] << "\n";
            p.rpt << xmp.docToQString();
        }
    }

    if (!xmp.isValid) {
        sidecarFile.close();
        return false;
    }

    // extract metadata from sidecar xmp
    if (p.instance != G::dmInstance)
        if (G::isWarningLogger)
        qWarning() << "WARNING"
                   << "Metadata::parseSidecar  Instance conflict"
                   << "p.instance =" << p.instance
                   << "G::dmInstance =" << G::dmInstance
                   << "sidecarPath" << sidecarPath
                   << "sidecarFile.exists()" << sidecarFile.exists()
                      ;
    QString s;
    s = xmp.getItem("rating"); if (!s.isEmpty()) {m.rating = s; m._rating = s;}
    s = xmp.getItem("label"); if (!s.isEmpty()) {m.label = s; m._label = s;}
    s = xmp.getItem("title"); if (!s.isEmpty()) {m.title = s; m._title = s;}
    s = xmp.getItem("creator"); if (!s.isEmpty()) {m.creator = s; m._creator = s;}
    s = xmp.getItem("rights"); if (!s.isEmpty()) {m.copyright = s; m._copyright = s;}
    s = xmp.getItem("email"); if (!s.isEmpty()) {m.email = s; m._email = s;}
    s = xmp.getItem("url"); if (!s.isEmpty()) {m.url = s; m._url = s;}
    /*
    qDebug() << "Metadata::parseSidecar" << s << sidecarPath;
    //*/

    sidecarFile.close();
    return true;

}

QString Metadata::sidecarPath(QString fPath)
/*
    The sidecar file has the same name as the image file, but uses the extension "xmp".
*/
{
    if (G::isLogger) G::log("Metadata::sidecarPath");
    QFileInfo info(fPath);
    return info.absoluteDir().path() + "/" + info.baseName() + ".xmp";
}

void Metadata::clearMetadata()
{
    if (G::isLogger) G::log("Metadata::clearMetadata");
    p.fPath = "";
    m.offsetFull = 0;
    m.lengthFull = 0;
    m.widthPreview = 0;
    m.heightPreview = 0;
    m.offsetThumb = 0;
    m.lengthThumb = 0;
    m.isBigEnd = false;
    m.ifd0Offset = 0;
    m.offsetFull = 0;
    m.lengthFull = 0;
    m.widthPreview = 0;
    m.heightPreview = 0;
    m.offsetThumb = 0;
    m.lengthThumb = 0;
    m.xmpSegmentOffset = 0;
    m.orientationOffset = 0;
    m.iccSegmentOffset = 0;
    m.iccSegmentLength = 0;
    m.iccBuf.clear();
    m.isEmbeddedThumbMissing = false;
    m.compare = false;
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
    m.exposureCompensationNum = 0;  // crazy value so cane check if not found
    m.focalLength = "";
    m.focalLengthNum = 0;
    m.focusX = 0;
    m.focusY = 0;
    m.gpsCoord = "";
    m.keywords.clear();
    m.title = "";
    m.lens = "";
    m.creator = "";
    m.copyright = "";
    m.email = "";
    m.url = "";
    m.duration = "";
    m.shutterCount = 0;
    m.cameraSN = "";
    m.lensSN = "";
    m.rating = "";
    m.label = "";
    m._title = "";                    // original value
    m._creator = "";                  // original value
    m._copyright = "";                // original value
    m._email = "";                    // original value
    m._url = "";                      // original value
    m._rating = "";                   // original value
    m._label = "";                    // original value
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
    if (p.file.isOpen()) {
        return;
//        qDebug() << "Metadata::testNewFileFormat" << "Close" << p.file.fileName();
    }
//    qDebug() << "Metadata::testNewFileFormat" << "Open " << p.file.fileName();
    p.file.open(QIODevice::ReadOnly);

    // edit test format to use:
    parseDNG();
    p.file.close();
//    qDebug() << "Metadata::testNewFileFormat" << "Close" << p.file.fileName();
//    reportMetadata();
}

QString Metadata::readExifToolTag(QString fPath, QString tag)
{
    QStringList args;
    args += "-T";
    args += "-" + tag;            // tag
    args += fPath;                    // src file
    QProcess process;
    process.start(exifToolPath, args);
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    if (output.endsWith("\n")) {
        output.chop(1);
    }
    qDebug() << "Metadata::readExifToolTag" << output << fPath;
    return output;
}

bool Metadata::readMetadata(bool isReport, const QString &path, QString source)
{
    if (G::isLogger) G::log("Metadata::readMetadata", "Source: " + source);
//    qDebug() << "Metadata::readMetadata" << source;

    // make sure file is available ie usb drive might have been ejected
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) return false;

    p.report = isReport;
    if (p.report) {
        p.rpt.flush();
        reportString = "";
        p.rpt.setString(&reportString);
        p.rpt << Utilities::centeredRptHdr('=', "Metadata Diagnostics for Current Image");
        p.rpt << "\n";
    }
//    clearMetadata();  // moved to loadMetadata
    m.fPath = path;
    p.fPath = path;
    p.file.setFileName(path);

    if (p.file.isOpen()) {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "Metadata::readMetadata" << "File already open" << path;
        return false;
    }

    if (p.report) {
        p.rpt << "\nFile name = " << path << "\n";
    }

    // check file permissions
    QFileDevice::Permissions oldPermissions = fileInfo.permissions();
    m.permissions = oldPermissions;
    m.isReadWrite = (oldPermissions & QFileDevice::ReadUser) && (oldPermissions & QFileDevice::WriteUser);
    if (!(oldPermissions & QFileDevice::ReadUser)) {
        QFileDevice::Permissions newPermissions = fileInfo.permissions() | QFileDevice::ReadUser;
        QFile(path).setPermissions(newPermissions);
    }

    QString ext = fileInfo.suffix().toLower();
    bool parsed = false;
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
        //QFile(path).setPermissions(oldPermissions);
        if (p.file.isOpen()) {
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "Metadata::readMetadata" << "Could not close" << path << "after format was read";
        }

        if (!parsed) {
            p.file.close();
            QString msg =  "Unable to parse metadata for " + path + ". ";
            m.err += msg;
            if (G::isWarningLogger)
            qWarning() << "WARNING" << "Metadata::readMetadata" << msg;
            return false;
        }

        if (G::useSidecar) {
            parseSidecar();
        }
    }
    else {  // not open file
        QString msg =  "Unable to open file " + path + ".";
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "Metadata::readMetadata" << msg;
        G::error("Could not open p.file to read metadata.", "Metadata::readMetadata", path);
        return false;
    }

    return true;

    /* not all files have thumb or small jpg embedded
    if (m.offsetFull == 0 && ext != "jpg" && parsed) {
        G::error("Metadata::readMetadata", path, "No embedded JPG found.");
    }

    if (m.lengthFull == 0 && m.lengthThumb > 0) {
        m.offsetFull = m.offsetThumb;
        m.lengthFull = m.lengthThumb;
    }
    //*/
}

bool Metadata::loadImageMetadata(const QFileInfo &fileInfo, int instance,
                                 bool essential, bool nonEssential,
                                 bool isReport, bool isLoadXmp, QString source,
                                 bool isRemote)
{
    if (G::isLogger) G::log("Metadata::loadImageMetadata", fileInfo.filePath() + "  Source: " + source);

    // check abort
    if (G::dmEmpty && !isRemote) return false;

    // check instance up-to-date
    if (instance != G::dmInstance && !isRemote) {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "Metadata::loadImageMetadata"
                   << "Instance clash"
                   << "this =" << instance
                   << "DM =" << G::dmInstance
                      ;
        QString msg = "this instance: " + QString::number(instance) +
                "DM instance: " + QString::number(G::dmInstance);
        if (G::isFileLogger) Utilities::log("Metadata::loadImageMetadata Instance clash", msg);
        return false;
    }

    // check if already loaded
    QString fPath = fileInfo.filePath();
    if (fPath == "") {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "Metadata::loadImageMetadata"
                   << "NULL FILE REQUESTED FROM "
                   << source;
        if (G::isFileLogger) Utilities::log("Metadata::loadImageMetadata File not exist", fPath);
        return false;
    }

    clearMetadata();
    p.instance = instance;
    m.fPath = fPath;
    m.currRootFolder = fileInfo.absoluteDir().absolutePath();
    m.size = fileInfo.size();

    // check if format with metadata
    QString ext = fileInfo.suffix().toLower();
    m.type = ext;
    m.ext = ext;

    m.video = videoFormats.contains(ext);

    if (!hasMetadataFormats.contains(ext)) {
        bool parsedSidcar = false;
        // get exif image created for image formats not included in Winnow
        QString createdDate = readExifToolTag(m.fPath, "createdate");
        m.createdDate = QDateTime::fromString(createdDate, "yyyy:MM:dd hh:mm:ss");
        m.metadataLoaded = true;
        if (G::useSidecar) {
            p.file.setFileName(fPath);
            if (p.file.open(QIODevice::ReadOnly)) {
                if (parseSidecar()) {
                    parsedSidcar = true;
                }
                p.file.close();
            }
        }
        //qDebug() << "Metadata::loadImageMetadata" << t.elapsed() << fPath;
        return true;
    }

    // For JPG, readNonEssentialMetadata adds 10-15% time to load
    readEssentialMetadata = essential;
    readNonEssentialMetadata = nonEssential;
    okToReadXmp = isLoadXmp;
    okToReadXmp = true;

    // read metadata
    m.metadataLoaded = readMetadata(isReport, fPath, source);
    if (!m.metadataLoaded) {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "Metadata::loadImageMetadata  Metadata not loaded for" << fPath;
        if (G::isFileLogger) Utilities::log("Metadata::loadImageMetadata  Metadata not loaded for ", fPath);
        //qDebug() << "Metadata::loadImageMetadata" << t.elapsed() << fPath;
        return false;
    }

    m.currRootFolder = fileInfo.absoluteDir().absolutePath();

    QString s = m.model;
    s += "  " + m.focalLength;
    s += "  " + m.exposureTime;
    s += (m.aperture == "") ? "" : " at " + m.aperture;
    s += (m.ISO == "") ? "" : ", ISO " + m.ISO;
    m.shootingInfo = s;
    m.loadMsecPerMp = 0;

//    m.thumbUnavailable = thumbUnavailable;
//    m.imageUnavailable = imageUnavailable;
    //qDebug() << "Metadata::loadImageMetadata" << t.elapsed() << fPath;
    return m.metadataLoaded;
}

// End Metadata
