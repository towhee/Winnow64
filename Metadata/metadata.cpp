#include "Metadata/metadata.h"
#include <QDebug>
#include "Main/global.h"

Metadata::Metadata(QObject *parent) : QObject(parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // some initialization
    initOrientationHash();
    initSupportedFiles();    
    report = false;
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

void Metadata::initSupportedFiles()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    // add raw file types here as they are supported
    rawFormats << "arw" << "cr2" << "dng" << "nef" << "orf" << "raf" << "sr2" << "rw2";
    getMetadataFormats << "arw" << "cr2" << "dng" << "nef" << "orf" << "raf" << "sr2" << "rw2"
                       << "jpg" << "jpeg" << "tif";
    embeddedICCFormats << "jpg" << "jpeg";
    sidecarFormats << "arw" << "cr2" << "nef" << "orf" << "raf" << "sr2" << "rw2"
                   << "jpg" << "jpeg";
    internalXmpFormats << "notyetjpg";
    xmpWriteFormats << "jpg" << "jpeg" << "arw" << "cr2" << "nef" << "orf" << "raf" << "sr2"
                    << "rw2";

    supportedFormats << "arw" << "bmp" << "cr2" << "cur" << "dds" << "dng" << "gif" << "heic"
    << "icns" << "ico" << "jpeg" << "jpg" << "jp2" << "jpe" << "mng" << "nef" << "orf" <<
    "pbm" << "pgm" << "png" << "ppm" << "raf" << "rw2" << "sr2" << "svg" << "svgz" << "tga" <<
    "tif" << "wbmp" << "webp" << "xbm" << "xpm";
}

void Metadata::initOrientationHash()
{
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    QString createdExif = imageMetadata.createdDate.toString("yyyy-MM-dd hh:mm:ss");
    rpt << "\n";
    rpt.reset();
    rpt.setFieldAlignment(QTextStream::AlignLeft);

    rpt.setFieldWidth(25); rpt << "offsetFullJPG"       << imageMetadata.offsetFullJPG;       rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lengthFullJPG"       << imageMetadata.lengthFullJPG;       rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetThumbJPG"      << imageMetadata.offsetThumbJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lengthThumbJPG"      << imageMetadata.lengthThumbJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetSmallJPG"      << imageMetadata.offsetSmallJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lengthSmallJPG"      << imageMetadata.lengthSmallJPG;      rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetXMPSeg"        << imageMetadata.xmpSegmentOffset;    rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "offsetNextXMPSegment"<< imageMetadata.xmpNextSegmentOffset;rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "orientation"         << imageMetadata.orientation;         rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "width"               << imageMetadata.width;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "height"              << imageMetadata.height;              rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "created"             << createdExif;                       rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "make"                << imageMetadata.make;                rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "model"               << imageMetadata.model;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "exposureTime"        << imageMetadata.exposureTime;        rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "aperture"            << imageMetadata.aperture;            rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "ISO"                 << imageMetadata.ISO;                 rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "focalLength"         << imageMetadata.focalLength;         rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "title"               << imageMetadata.title;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lens"                << imageMetadata.lens;                rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "creator"             << imageMetadata.creator;             rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "copyright"           << imageMetadata.copyright;           rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "email"               << imageMetadata.email;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "url"                 << imageMetadata.url;                 rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "rating"              << imageMetadata.rating;              rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "label"               << imageMetadata.label;               rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "cameraSN"            << imageMetadata.cameraSN;            rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "lensSN"              << imageMetadata.lensSN;              rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "shutterCount"        << imageMetadata.shutterCount;        rpt.setFieldWidth(0); rpt << "\n";
    rpt.setFieldWidth(25); rpt << "nikonLensCode"       << nikonLensCode;       rpt.setFieldWidth(0); rpt << "\n";

    if (isXmp && xmpString.length() > 0) {
        rpt << "\nXMP Extract:\n\n";
        QXmlQuery query;
        query.setQuery(xmpString);

        // Set up the output device
        QByteArray outArray;
        QBuffer buffer(&outArray);
        buffer.open(QIODevice::ReadWrite);

        // format xmp
        QXmlFormatter formatter(query, &buffer);
        query.evaluateTo(&formatter);

        QString xmpStr = QTextCodec::codecForMib(106)->toUnicode(outArray);
        rpt << xmpStr;
    }

    // moved to diagnostics()
//    QDialog *dlg = new QDialog;
//    Ui::metadataReporttDlg md;
//    md.setupUi(dlg);
//    md.textBrowser->setText(reportString);
//    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
//    dlg->show();
//    std::cout << reportString.toStdString() << std::flush;
}

QString Metadata::diagnostics(QString fPath)
{
    readMetadata(true, fPath);
    return reportString;
}

void Metadata::reportMetadataHeader(QString title)
{
    int hdrWidth = 90;
    int titleWidth = title.size() + 4;
    int padWidth = (hdrWidth - titleWidth) / 2;
    rpt.reset();
    rpt << "\n";
    rpt.setPadChar('-');
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << "-";
    rpt.reset();
    rpt.setFieldWidth(titleWidth);
    rpt.setFieldAlignment(QTextStream::AlignCenter);
    rpt << title;
    rpt.setPadChar('-');
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << "-";
    rpt.reset();
    rpt << "\n";
}

void Metadata::track(QString fPath, QString msg)
{
    if (G::isThreadTrackingOn) qDebug() << G::t.restart() << "\t" << "• Metadata Caching" << fPath << msg;
}

int Metadata::getNewOrientation(int orientation, int rotation)
{
    int degrees = orientationToDegrees[orientation];
    degrees += rotation;
    if (degrees > 270) degrees -= 360;
    return orientationFromDegrees[degrees];
}

bool Metadata::writeMetadata(const QString &fPath, ImageMetadata m, QByteArray &buffer)
{
/*
Called from ingest (Ingestdlg).  If it is a supported image type a copy of the
image file is made and any metadata changes are updated in buffer.  If it is a
raw file in the sidecarFormats hash then the xmp data for existing and changed
metadata is written to buffer and the original image file is copied unchanged.
*/
    // is xmp supported for this file
    QFileInfo info(fPath);
    QString suffix = info.suffix().toLower();
    if (!xmpWriteFormats.contains(suffix)) return false;

//    qDebug() << "Metadata::writeMetadata fPath =" << fPath;
    // set locals to image data  ie title = metaCache[fPath].title
//    setMetadata(fPath);  // replaced by dm->getMetadata which supplies ImageMetadata m
//    reportMetadataCache(fPath);

    bool useSidecar = sidecarFormats.contains(suffix);

    // new orientation
    int newOrientation = getNewOrientation(m.orientation, m.rotationDegrees);

    // has metadata been edited? ( _ is original data)
    bool ratingChanged = m.rating != ""; //_rating;
    bool labelChanged = m.label != "";  //_label;
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
        && !orientationChanged ) return false;

    // data edited, open image file
    file.setFileName(fPath);
    // rgh error trap file operation
    file.open(QIODevice::ReadOnly);

    // update xmp data
    Xmp xmp(file, xmpSegmentOffset, xmpNextSegmentOffset, useSidecar);

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

    // get the buffer to write to a new file
    if (suffix == "jpg") {
        file.seek(0);
        buffer = file.readAll();
        /* Update orientation first, as orientation is written to EXIF, not
        XMP, for known formats. Writing subsequent xmp could change file length
        and make the orientationOffset incorrect.
        */
        if (orientationChanged && orientationOffset > 0) {
            QChar c = newOrientation & 0xFF;
            QByteArray ba;
            ba.append(c);
            buffer.replace(static_cast<int>(m.orientationOffset), 1, ba);
        }
        xmp.writeJPG(buffer);
    }
    if (useSidecar)
        xmp.writeSidecar(buffer);

    return true;
}

quint32 Metadata::findInFile(QString s, quint32 offset, quint32 range)
{
/*
Returns the file offset to the start of the search string. If not
found returns -1.

QFile file must be assigned and open.
*/
    uint firstCharCode = static_cast<unsigned int>(s[0].unicode());
    file.seek(offset);
    for (quint32 i = offset; i < offset + range; i++) {
        if (Utilities::get8(file.read(1)) == firstCharCode) {
            bool rejected = false;
            for (int j = 1; j < s.length(); j++) {
                uint nextCharCode = static_cast<unsigned int>(s[j].unicode());
                uint byte = Utilities::get8(file.read(1));
                if (byte != nextCharCode) rejected = true;
                if (rejected) break;
            }
            if (!rejected) return static_cast<quint32>(file.pos() - s.length());
        }
    }
    return 0;
}

bool Metadata::readIRB(quint32 offset)
{
/*
Read a Image Resource Block looking for embedded thumb
    - see https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
This is a recursive function, iterating through consecutive resource blocks until
the embedded jpg preview is found (irbID == 1036)
*/
    {
//    #ifdef ISDEBUG
//    G::track(__FUNCTION__);
//    #endif
    }

    // Photoshop IRBs use big endian
    quint32 oldOrder = order;
    order = 0x4D4D;

    // check signature to make sure this is the start of an IRB
    file.seek(offset);
    QByteArray irbSignature("8BIM");
    QByteArray signature = file.read(4);
    if (signature != irbSignature) {
        order = oldOrder;
        return foundTifThumb;
    }

    // Get the IRB ID (we're looking for 1036 = thumb)
    uint irbID = static_cast<uint>(Utilities::get16(file.read(2)));
    if (irbID == 1036) foundTifThumb = true;
//    qDebug() << __FUNCTION__ << fPath << "irbID =" << irbID;

    // read the pascal string which we don't care about
    uint pascalStringLength = static_cast<uint>(Utilities::get16(file.read(2)));
    if (pascalStringLength > 0) file.read(pascalStringLength);

    // get the length of the IRB data block
    quint32 dataBlockLength = Utilities::get32(file.read(4));
    // round to even 2 bytes
    dataBlockLength % 2 == 0 ? dataBlockLength : dataBlockLength++;

    // reset order as going to return or recurse next
    order = oldOrder;

    // found the thumb, collect offsets and return
    if (foundTifThumb) {
        offsetThumbJPG = static_cast<quint32>(file.pos()) + 28;
        lengthThumbJPG = dataBlockLength - 28;
        return foundTifThumb;
    }

    // did not find the thumb, try again
    file.read(dataBlockLength);
    offset = static_cast<quint32>(file.pos());
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    file.seek(offset);
    if (Utilities::get16(file.peek(2)) == 0xFFD8) {
        file.seek(offset + length - 2);
        if (Utilities::get16(file.peek(2)) == 0xFFD9) {
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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (nikon == nullptr) nikon = new Nikon;
    if (ifd == nullptr) ifd = new IFD;
    nikon->parse(file, imageMetadata, ifd, exif, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseCanon()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (canon == nullptr) canon = new Canon;
    canon->parse(file, imageMetadata, ifd, exif, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseOlympus()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (olympus == nullptr) olympus = new Olympus;
    if (ifd == nullptr) ifd = new IFD;
    if (exif == nullptr) exif = new Exif;
    if (jpeg == nullptr) jpeg = new Jpeg;
    olympus->parse(file, imageMetadata, ifd, exif, jpeg, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseSony()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (sony == nullptr) sony = new Sony;
    if (ifd == nullptr) ifd = new IFD;
    if (exif == nullptr) exif = new Exif;
    if (jpeg == nullptr) jpeg = new Jpeg;
    sony->parse(file, imageMetadata, ifd, exif, jpeg, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseFuji()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (fuji == nullptr) fuji = new Fuji;
    if (jpeg == nullptr) jpeg = new Jpeg;
    if (iptc == nullptr) iptc = new IPTC;
    if (ifd == nullptr) ifd = new IFD;
    fuji->parse(file, imageMetadata, ifd, exif, jpeg, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseDNG()
{
    if (dng == nullptr) dng = new DNG;
    if (jpeg == nullptr) jpeg = new Jpeg;
    if (iptc == nullptr) iptc = new IPTC;
    if (ifd == nullptr) ifd = new IFD;
    dng->parse(file, imageMetadata, ifd, iptc, exif, jpeg, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseTIF()
{
    if (tiff == nullptr) tiff = new Tiff;
    if (ifd == nullptr) ifd = new IFD;
    if (exif == nullptr) exif = new Exif;
    if (jpeg == nullptr) jpeg = new Jpeg;
    tiff->parse(file, imageMetadata, ifd, iptc, exif, jpeg, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parsePanasonic()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (panasonic == nullptr) panasonic = new Panasonic;
    if (ifd == nullptr) ifd = new IFD;
    if (exif == nullptr) exif = new Exif;
    if (jpeg == nullptr) jpeg = new Jpeg;
    panasonic->parse(file, imageMetadata, ifd, exif, jpeg, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseJPG(quint32 startOffset)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    if (jpeg == nullptr) jpeg = new Jpeg;
    if (ifd == nullptr) ifd = new IFD;
    if (exif == nullptr) exif = new Exif;
    jpeg->parse(file, startOffset, imageMetadata, ifd, iptc, exif, report, rpt, xmpString);
    if (report) reportMetadata();
    return true;
}

bool Metadata::parseHEIF()
{
    file.setFileName("D:/Pictures/_HEIC/iphone.HEIC");
//    file.setFileName("D:/Pictures/_HEIC/example.HEIC");
    file.open(QIODevice::ReadOnly);
    Heic *heic = new Heic(file);
    // do some stuff
    delete heic;
    return true;
}

void Metadata::clearMetadata()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #ifdef ISPROFILE
    G::track(__FUNCTION__);
    #endif
    #endif
    }
    imageMetadata.offsetFullJPG = 0;
    imageMetadata.lengthFullJPG = 0;
    imageMetadata.offsetThumbJPG = 0;
    imageMetadata.lengthThumbJPG = 0;
    imageMetadata.offsetSmallJPG = 0;
    imageMetadata.lengthSmallJPG = 0;
    imageMetadata.xmpSegmentOffset = 0;
    imageMetadata.orientationOffset = 0;
    imageMetadata.iccSegmentOffset = 0;
    imageMetadata.iccSegmentLength = 0;
    imageMetadata.iccBuf.clear();
    imageMetadata.width = 0;
    imageMetadata.height = 0;
    imageMetadata.orientation = 1;
//    created = "";
    imageMetadata.make = "";
    imageMetadata.model = "";
    imageMetadata.exposureTime = "";
    imageMetadata.exposureTimeNum = 0;
    imageMetadata.aperture = "";
    imageMetadata.apertureNum = 0;
    imageMetadata.ISO = "";
    imageMetadata.ISONum = 0;
    imageMetadata.focalLength = "";
    imageMetadata.focalLengthNum = 0;
    imageMetadata.title = "";
    imageMetadata.lens = "";
    imageMetadata.creator = "";
    imageMetadata.copyright = "";
    imageMetadata.email = "";
    imageMetadata.url = "";
    imageMetadata.err = "";
    imageMetadata.shutterCount = 0;
    imageMetadata.cameraSN = "";
    imageMetadata.lensSN = "";
    imageMetadata.rating = "";
    imageMetadata.label = "";

//    ifd->ifdDataHash.clear();
    nikonLensCode = "";     // rgh how to handle?
}

void Metadata::testNewFileFormat(const QString &path)
{
    report = true;

    if (report) {
        rpt.flush();
        reportString = "";
        rpt.setString(&reportString);
        rpt << "\nFile name = " << path << "\n";
    }
    clearMetadata();
    file.setFileName(path);
    file.open(QIODevice::ReadOnly);

    // edit test format to use:
    parseDNG();
//    reportMetadata();
}

bool Metadata::readMetadata(bool isReport, const QString &path)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, path);
    #endif
    }
//    qDebug() << __FUNCTION__ << "path =" << path;

    report = isReport;

    if (report) {
        rpt.flush();
        reportString = "";
        rpt.setString(&reportString);
        rpt << Utilities::centeredRptHdr('=', "Metadata Diagnostics for Current Image");
        rpt << "\n";
    }
    clearMetadata();

    file.setFileName(path);
    if (file.isOpen()) {
        err = "File is already open";
        qDebug() << __FUNCTION__ << err;
        return false;
    }
    if (report) {
        rpt << "\nFile name = " << path << "\n";
    }
    QFileInfo fileInfo(path);
    QString ext = fileInfo.suffix().toLower();
    bool success = false;
//    int totDelay = 50;
    int msDelay = 0;
    uint msInc = 10;
    bool fileOpened = false;
//    do {
        if (file.open(QIODevice::ReadOnly)) {
            if (ext == "cr2") fileOpened = parseCanon();
            if (ext == "dng") fileOpened = parseDNG();
            if (ext == "raf") fileOpened = parseFuji();
            if (ext == "jpg") fileOpened = parseJPG(0);
            if (ext == "nef") fileOpened = parseNikon();
            if (ext == "orf") fileOpened = parseOlympus();
            if (ext == "rw2") fileOpened = parsePanasonic();
            if (ext == "arw") fileOpened = parseSony();
            if (ext == "tif") fileOpened = parseTIF();
            file.close();
//            qDebug() << __FUNCTION__ << "fileOpened = " << fileOpened << path;
            if (fileOpened) success = true;
            else {
                err = "Unable to read format for " + path;
                qDebug() << __FUNCTION__ << err;
            }
        }
        else {
            qDebug() << __FUNCTION__ << "Could not open " << path;
            err = "Could not open file to read metadata";    // try again
            QThread::msleep(msInc);
        }
        msDelay += msInc;
//    }
//    while ((msDelay < totDelay) && !success);

    if (!success) {
        return false;
    }

    // not all files have thumb or small jpg embedded
    if (offsetFullJPG == 0 && ext != "jpg" && fileOpened) {
        err = "No embedded JPG found";
    }

    if (lengthFullJPG == 0) {
        offsetFullJPG = offsetSmallJPG;
        lengthFullJPG = lengthSmallJPG;
    }
    if (lengthSmallJPG == 0) {
        offsetSmallJPG = offsetFullJPG;
        lengthSmallJPG = lengthFullJPG;
    }
    if (lengthThumbJPG == 0) {
        offsetThumbJPG = offsetSmallJPG;
        lengthThumbJPG = lengthSmallJPG;
    }

    // error flags
    thumbUnavailable = imageUnavailable = false;
    if (lengthFullJPG == 0) {
        imageUnavailable = true;
        err = "No embedded preview found";
    }
    if (lengthThumbJPG == 0) {
        thumbUnavailable = true;
        err = "No embedded thumbnail or preview found";
    }

    // initialize edited rotation
    rotationDegrees = 0;

    return success;
}

void Metadata::removeImage(QString &imageFileName)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metaCache.remove(imageFileName);
}

void Metadata::setPick(const QString &imageFileName, bool choice)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metaCache[imageFileName].isPicked = choice;
}

void Metadata::clear()
{
    /*  Clears the QMap that holds the ImageMetadata for each image in the
       datamodel.

       rgh - Look at eliminating this structure and just use datamodel.
    */
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    metaCache.clear();
}

void Metadata::loadFromThread(QFileInfo &fileInfo)
{
    loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__);
}

bool Metadata::loadImageMetadata(const QFileInfo &fileInfo,
                                 bool essential, bool nonEssential,
                                 bool isReport, bool isLoadXmp, QString source)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__, fileInfo.filePath());
    #endif
    }
    // check if already loaded
    fPath = fileInfo.filePath();
    if (fPath == "") {
        qDebug() << __FUNCTION__ << "NULL FILE REQUESTED FROM "
                 << source;
        return false;
    }

    // For JPG, readNonEssentialMetadata adds 10-15% time to load
    readEssentialMetadata = essential;
    readNonEssentialMetadata = nonEssential;
    okToReadXmp = isLoadXmp;
    okToReadXmp = true;

    // read metadata
    bool result = readMetadata(isReport, fPath);
    if (!result) {
        qDebug() << __FUNCTION__ << err << source;
        imageMetadata.metadataLoaded = result;
        imageMetadata.err = err;
        return false;
    }

    imageMetadata.isPicked = false;

    imageMetadata.copyFileNamePrefix = createdDate.toString("yyyy-MM-dd");

    QString s = imageMetadata.model;
    s += "  " + imageMetadata.focalLength;
    s += "  " + imageMetadata.exposureTime;
    s += (imageMetadata.aperture == "") ? "" : " at " + imageMetadata.aperture;
    s += (imageMetadata.ISO == "") ? "" : ", ISO " + imageMetadata.ISO;
    imageMetadata.shootingInfo = s;

    imageMetadata.thumbUnavailable = thumbUnavailable;
    imageMetadata.imageUnavailable = imageUnavailable;
    imageMetadata.err = err;

    imageMetadata.metadataLoaded = result;
    return result;
}

// End Metadata
