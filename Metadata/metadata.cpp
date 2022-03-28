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
//                        << "jpg"
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
    MetaReport::header("Winnow datamodel", p.rpt);
//    p.rpt << "\nDATAMODEL:\n\n";
    p.rpt.reset();
    p.rpt.setFieldAlignment(QTextStream::AlignLeft);
    int n = 25;

    p.rpt << G::sj("fPath", n) << G::s(m.fPath) << "\n";
    p.rpt << G::sj("fName", n) << G::s(m.fName) << "\n";
    p.rpt << G::sj("type", n) << G::s(m.type) << "\n";
    p.rpt << G::sj("size", n) << G::s(m.size) << "\n";
    p.rpt << G::sj("createdDate", n) << G::s(m.createdDate) << "\n";
    p.rpt << G::sj("modifiedDate", n) << G::s(m.modifiedDate) << "\n";
    p.rpt << G::sj("year", n) << G::s(m.year) << "\n";
    p.rpt << G::sj("day", n) << G::s(m.day) << "\n";
    p.rpt << "\n";
    p.rpt << G::sj("refine", n) << G::s(m.refine) << "\n";
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
    /*
//    p.rpt << "fName"               << m.fName;                  p.rpt << "\n";
//    p.rpt << "type"                << m.type;                   p.rpt << "\n";
//    p.rpt << "size"                << m.size;                   p.rpt << "\n";
//    p.rpt << "createdDate"         << d(m.createdDate);         p.rpt << "\n";
//    p.rpt << "modifiedDate"        << d(m.modifiedDate);        p.rpt << "\n";
//    p.rpt << "year"                << m.year;                   p.rpt << "\n";
//    p.rpt << "day"                 << m.day;                    p.rpt << "\n";
//    p.rpt << "\n";
//    p.rpt << "refine"              << b(m.refine);              p.rpt << "\n";
//    p.rpt << "pick"                << b(m.pick);                p.rpt << "\n";
//    p.rpt << "ingested"            << b(m.ingested);            p.rpt << "\n";
//    p.rpt << "metadataLoaded"      << b(m.metadataLoaded);      p.rpt << "\n";
//    p.rpt << "isSearch"            << b(m.isSearch);            p.rpt << "\n";
//    p.rpt << "\n";
//    p.rpt << "width"               << m.width;                  p.rpt << "\n";
//    p.rpt << "height"              << m.height;                 p.rpt << "\n";
//    p.rpt << "widthPreview"        << m.widthPreview;           p.rpt << "\n";
//    p.rpt << "heightPreview"       << m.heightPreview;          p.rpt << "\n";
//    p.rpt << "dimensions"          << m.dimensions;             p.rpt << "\n";
//    p.rpt << "megapixels"          << m.megapixels;             p.rpt << "\n";
//    p.rpt << "loadMsecPerMp"       << m.loadMsecPerMp;          p.rpt << "\n";
//    p.rpt << "aspectRatio"         << m.aspectRatio;            p.rpt << "\n";
//    p.rpt << "orientation"         << m.orientation;            p.rpt << "\n";
//    //     p.rpt << "_orientation"        << m._orientation;           p.rpt << "\n";
//    p.rpt << "orientationOffset"   << m.orientationOffset;      p.rpt << "\n";
//    p.rpt << "rotationDegrees"     << m.rotationDegrees;        p.rpt << "\n";
//    //     p.rpt << "_rotationDegrees"    << m._rotationDegrees;       p.rpt << "\n";
//    p.rpt << "\n";
//    p.rpt << "make"                << m.make;                   p.rpt << "\n";
//    p.rpt << "model"               << m.model;                  p.rpt << "\n";
//    p.rpt << "exposureTime"        << m.exposureTime;           p.rpt << "\n";
//    p.rpt << "aperture"            << m.aperture;               p.rpt << "\n";
//    p.rpt << "ISO"                 << m.ISO;                    p.rpt << "\n";
//    p.rpt << "exposureCompensation"<< m.exposureCompensation;   p.rpt << "\n";
//    p.rpt << "focalLength"         << m.focalLength;            p.rpt << "\n";
//    p.rpt << "lens"                << m.lens;                   p.rpt << "\n";
//    p.rpt << "shootingInfo"        << m.shootingInfo;           p.rpt << "\n";
//    p.rpt << "cameraSN"            << m.cameraSN;               p.rpt << "\n";
//    p.rpt << "lensSN"              << m.lensSN;                 p.rpt << "\n";
//    p.rpt << "shutterCount"        << m.shutterCount;           p.rpt << "\n";
//    p.rpt << "nikonLensCode"       << m.nikonLensCode;          p.rpt << "\n";
//    p.rpt << "\n";
//    p.rpt << "title"               << m.title;                  p.rpt << "\n";
//    p.rpt << "_title"              << m._title;                 p.rpt << "\n";
//    p.rpt << "creator"             << m.creator;                p.rpt << "\n";
//    p.rpt << "_creator"            << m._creator;               p.rpt << "\n";
//    p.rpt << "copyright"           << m.copyright;              p.rpt << "\n";
//    p.rpt << "_copyright"          << m._copyright;             p.rpt << "\n";
//    p.rpt << "email"               << m.email;                  p.rpt << "\n";
//    p.rpt << "_email"              << m._email;                 p.rpt << "\n";
//    p.rpt << "url"                 << m.url;                    p.rpt << "\n";
//    p.rpt << "_url"                << m._url;                   p.rpt << "\n";
//    p.rpt << "label"               << m.label;                  p.rpt << "\n";
//    p.rpt << "_label"              << m._label;                 p.rpt << "\n";
//    p.rpt << "rating"              << m.rating;                 p.rpt << "\n";
//    p.rpt << "_rating"             << m._rating;                p.rpt << "\n";
//    p.rpt << "\n";
//    p.rpt << "offsetFull"          << m.offsetFull;             p.rpt << "\n";
//    p.rpt << "lengthFull"          << m.lengthFull;             p.rpt << "\n";
//    p.rpt << "offsetThumb"         << m.offsetThumb;            p.rpt << "\n";
//    p.rpt << "lengthThumb"         << m.lengthThumb;            p.rpt << "\n";
//    p.rpt << "samplesPerPixel"     << m.samplesPerPixel;        p.rpt << "\n";
//    p.rpt << "isBigEndian"         << b(m.isBigEnd);            p.rpt << "\n";
//    p.rpt << "offsetifd0Seg"       << m.ifd0Offset;             p.rpt << "\n";
//    p.rpt << "isXmp"               << b(m.isXmp);               p.rpt << "\n";
//    p.rpt << "offsetXMPSeg"        << m.xmpSegmentOffset;       p.rpt << "\n";
//    p.rpt << "lengthXMPSeg"        << m.xmpSegmentLength;       p.rpt << "\n";
//    p.rpt << "offsetICCSeg"        << m.iccSegmentOffset;       p.rpt << "\n";
//    p.rpt << "lengthICCSeg"        << m.iccSegmentLength;       p.rpt << "\n";
//    p.rpt << "iccSpace"            << m.iccSpace;               p.rpt << "\n";
//    p.rpt << "\n";
//    p.rpt << "searchStr"           << m.searchStr;              p.rpt << "\n";
*/

    if (m.isXmp) {
        // sidecar xmp
        MetaReport::header("Embedded XMP Extract", p.rpt);
        Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength);
        if (xmp.isValid) p.rpt << xmp.docToQString();
        else {
            p.rpt << "ERROR: " << xmp.errMsg[xmp.err] << "\n";
            p.rpt << xmp.docToQString();
        }
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

void Metadata::writeOrientation(QString fPath, QString orientationNumber)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << __FUNCTION__ << fPath;
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
    Called from ingest (Ingestdlg).  // not anymore rgh update this

    fPath: the absolute path of the file to receive the xmp metadata
    buffer: byte array containing the xmp data

    m: the current state of the metadata for fPath MUST BE UPDATED BEFORE CALLING with
       dm->imMetadata(fPath); because dm is not available from Metadata.

    If it is a supported image type a copy of the image file is made and any metadata changes
    are updated in buffer. If it is a raw file in the sidecarFormats hash then the xmp data
    for existing and changed metadata is written to buffer and the original image file is
    copied unchanged.
*/
    if (G::isLogger) G::log(__FUNCTION__);

    // is xmp supported for this file
    QFileInfo info(fPath);
    QString suffix = info.suffix().toLower();

    // TEMP PREVENT WRITING TO ANYTHING BUT .XMP
    if (suffix != "xmp") return false;

//    if (!sidecarFormats.contains(suffix)) {
////        qDebug() << __FUNCTION__ << "Unable to write xmp buffer."  << suffix << "not in xmpWriteFormats";
//        return false;
//    }

    // write to a sidecar file for all formats for now.  May write inside source image in the future
//    bool useSidecar = true;
//    useSidecar = sidecarFormats.contains(suffix);

    // new orientation
    int newOrientation = getNewOrientation(m.orientation, m.rotationDegrees);

    /* debug
    qDebug() << __FUNCTION__ << "m.rating =" << m.rating << "m._rating = " << m._rating;
    qDebug() << __FUNCTION__ << "m.label =" << m.label << "m._label = " << m._label;
    qDebug() << __FUNCTION__ << "m.title =" << m.title << "m._title = " << m._title;
    qDebug() << __FUNCTION__ << "m.creator =" << m.creator << "m._creator = " << m._creator;
    qDebug() << __FUNCTION__ << "m.copyright =" << m.copyright << "m._copyright = " << m._copyright;
    qDebug() << __FUNCTION__ << "m.email =" << m.email << "m._email = " << m._email;
    qDebug() << __FUNCTION__ << "m.url =" << m.url << "m._url = " << m._url;

//    qDebug() << __FUNCTION__ << "m.orientation =" << m.orientation << "m._orientation = " << m._orientation;
//    qDebug() << __FUNCTION__ << "m.rotationDegrees =" << m.rotationDegrees << "m._rotationDegrees = " << m._rotationDegrees;
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
        qDebug() << __FUNCTION__ << "Unable to write xmp buffer. No metadata has been edited.";
        return false;
    }

    // make sure file is available ie usb drive may have been ejected
//    QFileInfo fileInfo(fPath);
//    if (!fileInfo.exists()) return false;

    // data edited, open image file
    p.file.setFileName(fPath);
    // rgh error trap file operation
    if (p.file.isOpen()) p.file.close();
    p.file.open(QIODevice::ReadWrite);

    // if current xmp is invalid then fix
    Xmp xmp(p.file);
    if (!xmp.isValid) xmp.fix();
//    {
//        p.file.close();
//        return false;
//    }

    // orientation is written to xmp sidecars only
//    if (orientationChanged && G::useSidecar) {
//        QString s = QString::number(newOrientation);
//        xmp.setItem("Orientation", s.toLatin1());
//    }

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

    m._rating = m.rating;
    m._label = m.label;
    m._title = m.title;
    m._creator = m.creator;
    m._copyright = m.copyright;
    m._email = m.email;
    m._url = m.url;

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
    if (irb == nullptr) irb = new IRB;
    tiff->parse(p, m, ifd, irb, iptc, exif, gps);
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

bool Metadata::parseSidecar()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::stop) return false;

    QFileInfo info(p.file);
    QString sidecarPath = info.absoluteDir().path() + "/" + info.baseName() + ".xmp";
    QFile sidecarFile(sidecarPath);

    // no sidecar file
    if (!sidecarFile.exists()) {
        return false;
    }

    if (!sidecarFile.open(QIODevice::ReadOnly)) {
        qWarning() << __FUNCTION__ << "Failed to open sidecar file" << sidecarPath;
        return false;
    }

    // parse sidecar
    Xmp xmp(sidecarFile);

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
    else {
        QString s;
        s = xmp.getItem("rating"); if (!s.isEmpty()) {m.rating = s; m._rating = s;}
        s = xmp.getItem("label"); if (!s.isEmpty()) {m.label = s; m._label = s;}
        s = xmp.getItem("title"); if (!s.isEmpty()) {m.title = s; m._title = s;}
        s = xmp.getItem("creator"); if (!s.isEmpty()) {m.creator = s; m._creator = s;}
        s = xmp.getItem("rights"); if (!s.isEmpty()) {m.copyright = s; m._copyright = s;}
        s = xmp.getItem("email"); if (!s.isEmpty()) {m.email = s; m._email = s;}
        s = xmp.getItem("url"); if (!s.isEmpty()) {m.url = s; m._url = s;}
    }

    sidecarFile.close();
    return true;

}

QString Metadata::sidecarPath(QString fPath)
/*
    The sidecar file has the same name as the image file, but uses the extension "xmp".
*/
{
    if (G::isLogger) G::log(__FUNCTION__);
    QFileInfo info(fPath);
    return info.absoluteDir().path() + "/" + info.baseName() + ".xmp";
}

void Metadata::clearMetadata()
{
    if (G::isLogger) G::log(__FUNCTION__); 
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

    // make sure file is available ie usb drive has been ejected
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
    clearMetadata();
    m.fPath = path;
    p.fPath = path;

    if (p.file.isOpen()) {
        qDebug() << __FUNCTION__ << "File already open" << path;
        return false;
    }
    p.file.setFileName(path);

    if (p.report) {
        p.rpt << "\nFile name = " << path << "\n";
    }
    QString ext = fileInfo.suffix().toLower();
    bool parsed = false;
    // rgh next triggers crash sometimes when skip to end of thumbnails
    if (p.file.open(QIODevice::ReadWrite)) {
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
            QString msg =  "Unable to parse metadata for " + path + ". ";
            m.err += msg;
            qWarning() << __FUNCTION__ << msg;
            return false;
        }
        if (G::useSidecar) {
            parseSidecar();
        }
    }
    else {
        if (p.file.isOpen()) p.file.close();
        G::error(__FUNCTION__, path, "Could not open p.file to read metadata.");
        return false;
    }

    return true;

    // not all files have thumb or small jpg embedded
    if (m.offsetFull == 0 && ext != "jpg" && parsed) {
        G::error(__FUNCTION__, path, "No embedded JPG found.");
    }

    if (m.lengthFull == 0 && m.lengthThumb > 0) {
        m.offsetFull = m.offsetThumb;
        m.lengthFull = m.lengthThumb;
    }

//    if (m.lengthThumb == 0 && m.lengthFull > 0) {
//        m.offsetThumb = m.offsetFull;
//        m.lengthThumb = m.lengthFull;
//    }

    // error flags
    thumbUnavailable = imageUnavailable = false;
    if (m.lengthFull == 0) {
        imageUnavailable = true;
        G::error(__FUNCTION__, path, "No embedded preview found.");
    }
    if (m.lengthThumb == 0) {
        thumbUnavailable = true;
        G::error(__FUNCTION__, path, "No embedded thumbnail or preview found.");
    }

    return true;
}

bool Metadata::loadImageMetadata(const QFileInfo &fileInfo,
                                 bool essential, bool nonEssential,
                                 bool isReport, bool isLoadXmp, QString source)
{
    if (G::isLogger) G::log(__FUNCTION__, fileInfo.filePath() + "  Source: " + source);

    // check if already loaded
    QString fPath = fileInfo.filePath();
    if (fPath == "") {
        qWarning() << __FUNCTION__ << "NULL FILE REQUESTED FROM "
                   << source;
        return false;
    }

    // check if format with metadata
    QString ext = fileInfo.suffix().toLower();
    if (!hasMetadataFormats.contains(ext)) {
        clearMetadata();
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
//        G::error(__FUNCTION__, fPath, "Failed to read metadata.");
        return false;
    }

//    m.isPicked = false;

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

// duplicate function - do we need this??
//bool Metadata::writeXMP(QStringList &paths, const QString tag, const QString tagValue)
//{
//    if (G::isLogger) G::log(__FUNCTION__, tag);
//    QString tagName;
//    if (tag == "Title") tagName = "XMP-dc:Title";
//    if (tag == "Creator") tagName = "XMP-dc:creator";
//    if (tag == "Copyright") tagName = "";
//    if (tag == "Email") tagName = "";
//    if (tag == "Url") tagName = "";
//    ExifTool et;
//    et.setOverWrite(true);
//    for (int i = 0; i < paths.length(); ++i) {

//    }
//    return true;
//}

// End Metadata
