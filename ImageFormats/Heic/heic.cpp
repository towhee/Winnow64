#include "heic.h"
#include "Main/global.h"

Heic::Heic(/*QFile &file*/)
{
    isDebug = false;
}

#ifdef Q_OS_WIN

bool Heic::parseLibHeif(MetadataParameters &p, ImageMetadata &m, IFD *ifd, Exif *exif, GPS *gps)
{
    heif_context* ctx = heif_context_alloc();
    QFileInfo info(p.file);
    fPath = info.filePath();

    QElapsedTimer t; t.restart();
    auto error = heif_context_read_from_file(ctx, fPath.toLatin1().data(), nullptr);
    if (error.code) {
        qDebug() << "Heic::parseLibHeif" << "heif_context_read_from_file" << error.message;
        return false;
    }
    /*
    qDebug() << "Heic::parseLibHeif" << "Error:"
             << "code =" << error.code
             << "subcode =" << error.subcode
             << "message =" << error.message
            ;
//    */
//    qDebug() << "Heic::parseLibHeif" << "Read heif context =" << t.nsecsElapsed() << fPath;

    // get a handle to the primary image
    heif_image_handle* handle = nullptr;
    error = heif_context_get_primary_image_handle(ctx, &handle);
    if (error.code) {
        qDebug() << "Heic::parseLibHeif" << "heif_context_get_primary_image_handle" << error.message;
        return false;
    }

    // get exif in QByteArray
    heif_item_id id = 0;
    int n = heif_image_handle_get_list_of_metadata_block_IDs(handle, "Exif", &id, 1);
    if (n == 0) return false;
    const int length = static_cast<int>(heif_image_handle_get_metadata_size(handle, id));
    QByteArray exifData(length, 0);
    error = heif_image_handle_get_metadata(handle, id, exifData.data());
    if (error.code) {
        qDebug() << "Heic::parseLibHeif" << "heif_image_handle_get_metadata" << error.message;
        return false;
    }
    p.buf.setBuffer(&exifData);
    p.buf.open(QIODevice::ReadOnly);

//    Utilities::saveByteArrayAsFile("D:/Pictures/_HEIC/test.dat", exifData);

    // get endian
    bool isBigEnd = true;
    bool foundEndian = false;
    quint32 startOffset = 0;
    int count = 0;
    while (!foundEndian) {
        quint32 order = Utilities::get16(p.buf.read(2));
        if (order == 0x4949 || order == 0x4D4D) {
            order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
            foundEndian = true;
            startOffset = static_cast<quint32>(p.buf.pos()) - 2;
        }
        // add condition to check for EOF
        count++;
        if (count > 100) {
            QString msg = "Endian order not found.";
            G::issue("Error", msg, "Heic::parseLibHeif", m.row, fPath);
            p.buf.close();
            return false;
        }
    }

    //
    /*quint32 magic42 = */Utilities::get16(p.buf.read(2), isBigEnd);
    quint32 a = Utilities::get32(p.buf.read(4), isBigEnd);
    quint32 offsetIfd0 = a + startOffset;

    // read IFD0
    p.offset = offsetIfd0;
    p.hdr = "IFD0";
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD_B(p, isBigEnd);
    if (nextIFDOffset) nextIFDOffset += startOffset;

    quint32 offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;
    quint32 offsetGPS = ifd->ifdDataHash.value(34853).tagValue + startOffset;

    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.make = Utilities::getString(p.buf, ifd->ifdDataHash.value(271).tagValue + startOffset,
                     ifd->ifdDataHash.value(271).tagCount);
    m.model = Utilities::getString(p.buf, ifd->ifdDataHash.value(272).tagValue + startOffset,
                      ifd->ifdDataHash.value(272).tagCount);
    m.creator = Utilities::getString(p.buf, ifd->ifdDataHash.value(315).tagValue + startOffset,
                        ifd->ifdDataHash.value(315).tagCount);
    m.copyright = Utilities::getString(p.buf, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                          ifd->ifdDataHash.value(33432).tagCount);

//    // read IFD1
//    qDebug() << "Heic::parseLibHeif" << "nextIFDOffset IFD1 =" << nextIFDOffset;
//    if (nextIFDOffset) {
//        p.hdr = "IFD1";
//        p.offset = nextIFDOffset + startOffset;
//        nextIFDOffset = ifd->readIFD(p, m, isBigEnd);
//    }
//    // IFD1: thumbnail offset and length
//    m.offsetThumb = ifd->ifdDataHash.value(513).tagValue + 12;
//    m.lengthThumb = ifd->ifdDataHash.value(514).tagValue;

    // read EXIF
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD_B(p, isBigEnd);

    m.width = static_cast<int>(ifd->ifdDataHash.value(40962).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(40963).tagValue);
    p.offset = 0;
//    if (!m.width || !m.height) getDimensions(p, m);
    m.widthPreview = m.width;
    m.heightPreview = m.height;

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.buf, ifd->ifdDataHash.value(36868).tagValue + startOffset,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
    // try DateTimeOriginal
    if (createdExif.length() == 0) {
        createdExif = Utilities::getString(p.buf, ifd->ifdDataHash.value(36867).tagValue + startOffset,
            ifd->ifdDataHash.value(36867).tagCount).left(19);
        if (createdExif.length() > 0) {
            m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
        }
//            if(!createdDate.isValid())
//                createdDate = QDateTime::fromString("2017:10:10 17:26:08", "yyyy:MM:dd hh:mm:ss");
    }

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.buf,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      isBigEnd);
        if (x < 1) {
            double recip = static_cast<double>(1 / x);
            if (recip >= 2) m.exposureTime = "1/" + QString::number(qRound(recip));
            else m.exposureTime = "1/" + QString::number(recip, 'g', 2);
            m.exposureTimeNum = x;
        } else {
            uint t = static_cast<uint>(x);
            m.exposureTime = QString::number(t);
            m.exposureTimeNum = t;
        }
        m.exposureTime += " sec";
    } else {
        m.exposureTime = "";
    }

    // EXIF: aperture
    if (ifd->ifdDataHash.contains(33437)) {
        double x = Utilities::getReal(p.buf,
                                      ifd->ifdDataHash.value(33437).tagValue + startOffset,
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
        double x = Utilities::getReal_s(p.buf,
                                      ifd->ifdDataHash.value(37380).tagValue + startOffset,
                                      isBigEnd);
        m.exposureCompensation = QString::number(x, 'f', 1) + " EV";
        m.exposureCompensationNum = x;
    } else {
        m.exposureCompensation = "";
        m.exposureCompensationNum = 0;
    }

    // EXIF: focal length
    if (ifd->ifdDataHash.contains(37386)) {
        double x = Utilities::getReal(p.buf,
                                      ifd->ifdDataHash.value(37386).tagValue + startOffset,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // EXIF: lens model
    m.lens = Utilities::getString(p.buf, ifd->ifdDataHash.value(42036).tagValue + startOffset,
                     ifd->ifdDataHash.value(42036).tagCount);

    /* Read embedded ICC. The default color space is sRGB. If there is an embedded icc profile
    and it is sRGB then no point in saving the byte array of the profile since we already have
    it and it will take up space in the datamodel. If iccBuf is null then sRGB is assumed. */
//    if (segmentHash.contains("ICC")) {
//        if (m.iccSegmentOffset && m.iccSegmentLength) {
//            m.iccSpace = Utilities::getString_B(p.buf, m.iccSegmentOffset + 52, 4);
//            if (m.iccSpace != "sRGB") {
//                p.buf.seek(m.iccSegmentOffset);
//                m.iccBuf = p.buf.read(m.iccSegmentLength);
//            }
//        }
//    }

    // read GPS
    if (offsetGPS) {
        p.file.seek(offsetGPS);
        p.hdr = "IFD GPS";
        p.hash = &gps->hash;
        p.offset = offsetGPS;
        ifd->readIFD(p, isBigEnd);

        if (ifd->ifdDataHash.contains(1)) {  // 1 = GPSLatitudeRef
            // process GPS info
            QString gpsCoord = gps->decode(p.file, ifd->ifdDataHash, isBigEnd, startOffset);
            m.gpsCoord = gpsCoord;
        }
    }

    // read XMP
//    bool okToReadXmp = true;
//    if (m.isXmp && okToReadXmp) {
//        Xmp xmp(p.buf, m.xmpSegmentOffset, m.xmpNextSegmentOffset, p.instance);
//        m.rating = xmp.getItem("Rating");     // case is important "Rating"
//        m.label = xmp.getItem("Label");       // case is important "Label"
//        m.title = xmp.getItem("title");       // case is important "title"
//        m.cameraSN = xmp.getItem("SerialNumber");
//        if (m.lens.isEmpty()) m.lens = xmp.getItem("Lens");
//        m.lensSN = xmp.getItem("LensSerialNumber");
//        if (m.creator.isEmpty()) m.creator = xmp.getItem("creator");
//        m.copyright = xmp.getItem("rights");
//        m.email = xmp.getItem("email");
//        m.url = xmp.getItem("url");
//        m.email = xmp.getItem("CiEmailWork");
//        m.url = xmp.getItem("CiUrlWork");

//    // save original values so can determine if edited when writing changes
//    m._rating = m.rating;
//    m._label = m.label;
//    m._title = m.title;
//    m._creator = m.creator;
//    m._copyright = m.copyright;
//    m._email  = m.email ;
//    m._url = m.url;

//        if (p.report) p.xmpString = xmp.metaAsString();
//    }

    p.buf.close();
    return true;
}

namespace {

static_assert(heif_error_Ok == 0, "heif_error_Ok assumed to be 0");

template<class T, class D>
std::unique_ptr<T, D> wrapPointer(T* ptr, D deleter)
{
    return std::unique_ptr<T, D>(ptr, deleter);
}

template<class... As>
heif_error readContext(As... as)
{
#if LIBHEIF_NUMERIC_VERSION >= 0x01030000
    return heif_context_read_from_memory_without_copy(as...);
#else
    return heif_context_read_from_memory(as...);
#endif
}

}  // namespace

bool Heic::decodePrimaryImage(QString &fPath, QImage &image)
{
    heif_context* ctx = heif_context_alloc();
    auto error = heif_context_read_from_file(ctx, fPath.toLatin1().data(), nullptr);
    if (error.code) {
        qDebug() << "Heic::decodePrimaryImage" << "heif_context_read_from_file" << error.message;
        return false;
    }

    // get a handle to the primary image
    heif_image_handle* handle = nullptr;
    error = heif_context_get_primary_image_handle(ctx, &handle);
    if (error.code) {
        qDebug() << "Heic::decodePrimaryImage" << "heif_context_get_primary_image_handle" << error.message;
        return false;
    }

    // decode the image and convert colorspace to RGB, saved as 24bit interleaved
    heif_image* img = nullptr;
    error = heif_decode_image(handle,
                      &img,
                      heif_colorspace_RGB,
                      heif_chroma_interleaved_RGB,
                      nullptr);
    if (error.code || !img) {
        qDebug() << "Heic::decodePrimaryImage" << "heif_decode_image" << error.message;
        return false;
    }

    int w = heif_image_get_width(img, heif_channel_interleaved);
    int h = heif_image_get_height(img, heif_channel_interleaved);
//    m.width = w;
//    m.height = h;
//    m.widthPreview = w;
//    m.heightPreview = h;

    int stride = 0;
    const uint8_t* data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
    if (!data) {
        QString msg = "No pixel data.";
        G::issue("Warning", msg, "Heic::heic", m->row, m->fPath);
        return false;
    }

    if (stride <= 0) {
        QString msg = "Invalid stride = " + QString::number(stride) + ".";
        G::issue("Warning", msg, "Heic::heic", m->row, m->fPath);
        return false;
    }
    /*
    qDebug() << "Heic::decodePrimaryImage" << fPath
             << "w =" << w << "h =" << h
             << "stride =" << stride;
//             */
    image = QImage(data, w, h, stride, QImage::Format_RGB888);
    return true;
    /*
    heif_image* srcImagePtr = nullptr;
    heif_decode_image(handle,
                      &srcImagePtr,
                      heif_colorspace_RGB,
                      heif_chroma_interleaved_RGB,
                      nullptr);
    auto srcImage = wrapPointer(srcImagePtr, heif_image_release);
    auto channel = heif_channel_interleaved;
    int w = heif_image_get_width(srcImage.get(), channel);
    int h = heif_image_get_height(srcImage.get(), channel);
    m.width = w;
    m.height = h;
    m.widthFull = w;
    m.heightFull = h;
    QSize imgSize(w, h);
    int stride;
    const uint8_t* data = heif_image_get_plane_readonly(srcImage.get(),
                                                        heif_channel_interleaved,
                                                        &stride);
    qDebug() << "Heic::decodePrimaryImage" << fPath
             << "imgSize =" << imgSize
             << "stride =" << stride;
    // move data ownership to QImage
    heif_image* dataImage = srcImage.release();
    image = QImage(
                data, imgSize.width(), imgSize.height(),
                stride, QImage::Format_RGBA8888,
                [](void* img) { heif_image_release(static_cast<heif_image*>(img)); },
    dataImage
    );
    return true;
    */
}

bool Heic::decodeThumbnail(QString &fPath, QImage &image)
{
    heif_context* ctx = heif_context_alloc();
    heif_context_read_from_file(ctx, fPath.toLatin1().data(), nullptr);

    // get a handle to the primary image
    heif_image_handle* handle = nullptr;
    heif_context_get_primary_image_handle(ctx, &handle);

    // get a handle to the primary image thumbnail
    heif_image_handle* thumbHandle = nullptr;
    int count = heif_image_handle_get_number_of_thumbnails(handle);
    if (count > 0) {
        heif_item_id ids[1];
        heif_image_handle_get_list_of_thumbnail_IDs(handle, ids, count);
//        qDebug() << "Heic::decodeThumbnail" << ids[0];
        heif_image_handle_get_thumbnail(handle, ids[0], &thumbHandle);
        handle = thumbHandle;
    }

    // decode the image and convert colorspace to RGB, saved as 24bit interleaved
    heif_image* img = nullptr;
    heif_decode_image(handle,
                      &img,
                      heif_colorspace_RGB,
                      heif_chroma_interleaved_RGB,
                      nullptr);

    int w = heif_image_get_width(img, heif_channel_interleaved);
    int h = heif_image_get_height(img, heif_channel_interleaved);
    int stride;
    const uint8_t* data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
    /*
    qDebug() << "Heic::decodeThumbnail" << fPath
             << "w =" << w << "h =" << h
             << "stride =" << stride;   //*/
    image = QImage(data, w, h, stride, QImage::Format_RGB888);
    return true;
}
#endif

bool Heic::parseHeic(MetadataParameters &p, ImageMetadata &m, IFD *ifd, Exif *exif, GPS *gps)
{
    if (G::isLogger) G::log("Heic::parseHeic");
    if (p.report) isDebug = true;
    else isDebug = false;
//    isDebug = true;

    this->m = &m;
    file = &p.file;

    quint32 offset = 0;
    quint32 length;
    QString type;
    eof = file->size();

    // initialize
    exifItemID = 0;
    exifOffset = 0;

    QFileInfo info(*file);
    if (isDebug) {
        qDebug() << "\nHeic::parseHeic" << info.filePath() << "\n";
    }

    // iterate box structures to find exif data offset
    while (offset < eof) {
        file->seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
    }

    if (isDebug) qDebug() << "Heic::parseHeic"
                          << "exifItemID =" << exifItemID
                          << "exifOffset =" << exifOffset;

    // EXIF data found?
    if (exifItemID == -1) return false;

    p.offset = exifOffset;
    return parseExif(p, m, ifd, exif, gps);
}

bool Heic::parseExif(MetadataParameters &p, ImageMetadata &m, IFD *ifd, Exif *exif, GPS *gps)
{
    p.file.seek(p.offset);

    // get endian
    bool isBigEnd = true;
    bool foundEndian = false;
    quint32 startOffset = 0;
    int count = 0;
    while (!foundEndian) {
        quint32 order = Utilities::get16(p.file.read(2));
        if (order == 0x4949 || order == 0x4D4D) {
            order == 0x4D4D ? isBigEnd = true : isBigEnd = false;
            foundEndian = true;
            startOffset = static_cast<quint32>(p.file.pos()) - 2;
        }
        // add condition to check for EOF
        count++;
        if (count > 100) {
            QString msg = "Endian order not found.";
            G::issue("Error", msg, "Heic::parseExif", m.row, fPath);
            p.file.close();
            return false;
        }
    }

    //
    /*quint32 magic42 = */Utilities::get16(p.file.read(2), isBigEnd);
    quint32 a = Utilities::get32(p.file.read(4), isBigEnd);
    quint32 offsetIfd0 = a + startOffset;

    // read IFD0
    p.offset = offsetIfd0;
    p.hdr = "IFD0";
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p, isBigEnd);
    if (nextIFDOffset) nextIFDOffset += startOffset;

    // IFD0: *******************************************************************

    // get the offset for EXIFIFD
    quint32 offsetEXIF = 0;
    if (ifd->ifdDataHash.contains(34665))
         offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;

    // get the offset for GPSIFD
    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue + startOffset;

    m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    m.make = Utilities::getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
                                  ifd->ifdDataHash.value(271).tagCount);
    m.model = Utilities::getString(p.file, ifd->ifdDataHash.value(272).tagValue + startOffset,
                                   ifd->ifdDataHash.value(272).tagCount);
    m.creator = Utilities::getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
                                     ifd->ifdDataHash.value(315).tagCount);
    m.copyright = Utilities::getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                                       ifd->ifdDataHash.value(33432).tagCount);


    // EXIF: *******************************************************************
    p.hdr = "IFD Exif";
    p.offset = offsetEXIF;
    ifd->readIFD(p, isBigEnd);

    m.width = static_cast<int>(ifd->ifdDataHash.value(40962).tagValue);
    m.height = static_cast<int>(ifd->ifdDataHash.value(40963).tagValue);
    p.offset = 0;
    m.widthPreview = m.width;
    m.heightPreview = m.height;

    // EXIF: created datetime
    QString createdExif;
    createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36868).tagValue + startOffset,
        ifd->ifdDataHash.value(36868).tagCount).left(19);
    if (createdExif.length() > 0) m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
    // try DateTimeOriginal
    if (createdExif.length() == 0) {
        createdExif = Utilities::getString(p.file, ifd->ifdDataHash.value(36867).tagValue + startOffset,
            ifd->ifdDataHash.value(36867).tagCount).left(19);
        if (createdExif.length() > 0) {
            m.createdDate = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
        }
    }

    // EXIF: shutter speed
    if (ifd->ifdDataHash.contains(33434)) {
        double x = Utilities::getReal(p.file,
                                      ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                      isBigEnd);
        if (x < 1) {
            double recip = static_cast<double>(1 / x);
            if (recip >= 2) m.exposureTime = "1/" + QString::number(qRound(recip));
            else m.exposureTime = "1/" + QString::number(recip, 'g', 2);
            m.exposureTimeNum = x;
        } else {
            uint t = static_cast<uint>(x);
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
                                      ifd->ifdDataHash.value(33437).tagValue + startOffset,
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
                                      ifd->ifdDataHash.value(37380).tagValue + startOffset,
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
                                      ifd->ifdDataHash.value(37386).tagValue + startOffset,
                                      isBigEnd);
        m.focalLengthNum = static_cast<int>(x);
        m.focalLength = QString::number(x, 'f', 0) + "mm";
    } else {
        m.focalLength = "";
        m.focalLengthNum = 0;
    }

    // EXIF: lens model
    m.lens = Utilities::getString(p.file, ifd->ifdDataHash.value(42036).tagValue + startOffset,
                     ifd->ifdDataHash.value(42036).tagCount);

    // GPSIFD: *******************************************************************
    if (offsetGPS) {
        p.file.seek(offsetGPS);
        p.hdr = "IFD GPS";
        p.hash = &gps->hash;
        p.offset = offsetGPS;
        ifd->readIFD(p, isBigEnd);

        if (ifd->ifdDataHash.contains(1)) {  // 1 = GPSLatitudeRef
            // process GPS info
            QString gpsCoord = gps->decode(p.file, ifd->ifdDataHash, isBigEnd, startOffset);
            m.gpsCoord = gpsCoord;
        }
    }

    // XMP
    m.isXmp = false;

    /* Read embedded ICC. The default color space is sRGB. If there is an embedded icc profile
    and it is sRGB then no point in saving the byte array of the profile since we already have
    it and it will take up space in the datamodel. If iccBuf is null then sRGB is assumed. */
//    if (segmentHash.contains("ICC")) {
//        if (m.iccSegmentOffset && m.iccSegmentLength) {
//            m.iccSpace = Utilities::getString_B(p.file, m.iccSegmentOffset + 52, 4);
//            if (m.iccSpace != "sRGB") {
//                p.file.seek(m.iccSegmentOffset);
//                m.iccBuf = p.file.read(m.iccSegmentLength);
//            }
//        }
//    }

    return true;
}

bool Heic::nextHeifBox(quint32 &length, QString &type)
{
    qint64 offset = file->pos();
    length = Utilities::get32(file->read(4));
    if (length < 2) length = static_cast<quint32>(eof - offset);
    type = file->read(4);
    if (isDebug) {
        qDebug() << " ";
        qDebug() << "Heic::nextHeifBox"
                 << "offset:" << offset
                 << "length:" << length
                 << "type:" << type;
    }
    return (length > 0);
}

bool Heic::getHeifBox(QString &type, quint32 &offset, quint32 &length)
{
    if (isDebug) {
        qDebug() << "Heic::getHeifBox"
                 << "type =" << type
                 << "offset =" << offset
                 << "length =" << length;
    }
    if (type == "ftyp") return ftypBox(offset, length);
    if (type == "meta") return metaBox(offset, length);
    if (type == "hdlr") return hdlrBox(offset, length);
    if (type == "dinf") return dinfBox(offset, length);
    if (type == "pitm") return pitmBox(offset, length);
    if (type == "iloc") return ilocBox(offset, length);
    if (type == "iinf") return iinfBox(offset, length);
    if (type == "iref") return irefBox(offset, length);
    if (type == "iprp") return iprpBox(offset, length);
    if (type == "hvcC") return hvcCBox(offset, length);
    if (type == "ispe") return ispeBox(offset, length);
    if (type == "ipma") return ipmaBox(offset, length);
    if (type == "mdat") return mdatBox(offset, length);
    if (type == "idat") return idatBox(offset, length);
    if (type == "dref") return drefBox(offset, length);
    if (type == "url ") return  urlBox(offset, length);
    if (type == "urn ") return  urnBox(offset, length);
    if (type == "colr") return colrBox(offset, length);
    if (type == "irot") return irotBox(offset, length);
    if (type == "pixi") return pixiBox(offset, length);

    QString msg = "Box type " + type + " is unknown";
    G::issue("Warning", msg, "Heic::getHeifBox", m->row, m->fPath);
    offset += length;
    return false;
}

bool Heic::ftypBox(quint32 &offset, quint32 &length)
{
    if (length == 0) {
        // err
        qDebug() << "Heic::ftypBox" << "ftyp not found";
        return false;
    }

    // item_ID_Exif used to determine which iloc points to EXIF data
    exifItemID = -1;
    bool isHeic = false;
    /* compatible brands 32bits each to end of ftype box
       ftyp box = length + type + major brand + minor version + compatible brands (each 4 bytes) */
    int compatibleBrands = static_cast<int>(length) - 16;
    file->seek(16);
    for (int i = 0; i < compatibleBrands; i++) {
        QString brand = file->read(4);
        if (brand == "heic" || brand == "HEIC") {
            isHeic = true;
            break;
        }
    }
    if (!isHeic) {
        QString msg = "Brand heic or HEIC not found.";
        G::issue("Error", msg, "Heic::ftypBox", m->row, fPath);
        return false;
    }
    offset += length;
    return true;
}

bool Heic::metaBox(quint32 &offset, quint32 &length)
{
    QString type;
    quint32 metaEnd = offset + length;
    offset += 12;

    if (isDebug)
        qDebug() << "Heic::metaBox"
                 << "offset =" << offset
                 << "length =" << length;

    while (offset < metaEnd) {
        file->seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
    }

    offset = metaEnd;

    return true;
}

bool Heic::hdlrBox(quint32 &offset, quint32 &length)
{
    file->seek(offset + 16);
    QString hdlrType = file->read(4);
    if (isDebug)
        qDebug() << "Heic::hdlrBox"
                 << "hdlrType =" << hdlrType;
    if (hdlrType != "pict") {
        // err
        return false;
    }

    offset += length;
    return true;
}

bool Heic::dinfBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        qDebug() << "Heic::dinfBox" << "offset =" << offset;
    }
    QString type;
    quint32 dinfEnd = offset + length;
    offset += 8;

    while (offset < dinfEnd) {
        file->seek(offset);
        nextHeifBox(length, type);
        getHeifBox(type, offset, length);
    }

    offset = dinfEnd;
    return true;
}

bool Heic::drefBox(quint32 &offset, quint32 &length)
{
    QString type;
    quint32 drefEnd = offset + length;
    file->seek(offset + 12);
    if (isDebug) {
        quint32 entry_count = Utilities::get32(file->read(4));
        qDebug() << "Heic::drefBox" << "entry_count =" << entry_count;

        for (quint32 i = 0; i < entry_count; i++) {
            if (offset > drefEnd) return false;
            nextHeifBox(length, type);
            qDebug() << "Heic::drefBox" << "entry#" << i;
            if (type == "url ") getHeifBox(type, offset, length);
    //        if (type == "urln") getHeifBox(type, offset, length);
        }
    }
    offset = drefEnd;
    return true;
}

bool Heic::urlBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        QString location = "";
        uint urlLength = length - 12;
        file->seek(offset + 12);
        if (urlLength > 0) {
            location = file->read(length - 12);
        }
        qDebug() << "Heic::urlBox"
                 << "    location =" << location;
        }
    offset += length;
    return true;
}

bool Heic::urnBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        QString name = "";
        QString location = "";
        uint urnLength = length - 12;
        file->seek(offset + 12);
        if (urnLength > 0) {
            name = Utilities::getCString(*file);
            if (file->pos() < offset + length) location = Utilities::getCString(*file);
        }
        qDebug() << "Heic:urnBox:"
                 << "    name =" << name
                 << "location =" << location;
        }
    offset += length;
    file->seek(offset);
    return true;
}

bool Heic::colrBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        file->seek(offset + 8);
        QString colrType = file->read(4);
        qDebug() << "Heic::colrBox" << "colrType =" << colrType;
        QByteArray colorProfile;
        if (colrType == "nclx") {
            quint16 colour_primaries = Utilities::get16(file->read(2));
            quint16 transfer_characteristics = Utilities::get16(file->read(2));
            quint16 matrix_coefficients = Utilities::get16(file->read(2));
            quint8 x = Utilities::get8(file->read(1));
            quint8 full_range_flag = (x & 0b10000000) >> 7;             // first 1 bit
            qDebug() << "Heic::colrBox"
                     << "colour_primaries =" << colour_primaries
                     << "transfer_characteristics =" << transfer_characteristics
                     << "matrix_coefficients =" << matrix_coefficients
                     << "full_range_flag =" << full_range_flag;
        }
        else if (colrType == "rICC" || colrType == "prof") {
            colorProfile = file->read(length - 8);
            qDebug() << "Heic::colrBox" << colorProfile;
        }
        else {
            // err
            qDebug() << "Heic::colrBox" << "*** color type" << colrType << "is not recognized";
            offset += length;
            return false;
        }
    }
    offset += length;
    return true;
}

bool Heic::pixiBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        file->seek(offset + 12);
        quint8 num_channels = Utilities::get8(file->read(1));

        for (quint8 i = 0; i < num_channels; i++) {
            quint8 bits_per_channel = Utilities::get8(file->read(1));
            qDebug() << "Heic::pixiBox" << "channel#" << i << "bits_per_channel =" << bits_per_channel;
        }
    }
    offset += length;
    return true;
}

bool Heic::irotBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        file->seek(offset + 8);
        quint8 x = Utilities::get8(file->read(1));
        quint8 angle = (x & 0b00000011);             // first 1 bit
        quint16 angle_degrees = angle * 90;
        qDebug() << "Heic::irotBox" << "angle =" << angle << "angle degrees =" << angle_degrees;
    }
    offset += length;
    return true;
}

bool Heic::pitmBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        file->seek(offset + 16);
        pitmId = Utilities::get16(file->read(2));
        qDebug() << "Heic::pitmBox"
                 << "pitmId =" << pitmId;
    }
    offset += length;
    return true;
}

bool Heic::ilocBox(quint32 &offset, quint32 &length)
{
/*
    Find offset and length of embedded EXIF data here if exifItemID != -1.  exifItemID
    is defined in the iinfBox calling infeBox.
*/
    file->seek(offset + 12);
    QByteArray c = file->read(1);
    ilocOffsetSize = Utilities::get4_1st(c);
    ilocLengthSize = Utilities::get4_2nd(c);
    c = file->read(1);
    ilocBaseOffsetSize = Utilities::get4_1st(c);
    ilocItemCount = Utilities::get16(file->read(2));
    if (isDebug) {
        qDebug()  << "Heic::ilocBox"
                 << "ilocOffsetSize =" << ilocOffsetSize
                 << "ilocLengthSize =" << ilocLengthSize
                 << "ilocBaseOffsetSize =" << ilocBaseOffsetSize
                 << "ilocItemCount =" << ilocItemCount;
    }
    if (ilocItemCount > 100) {
        QString msg = "Failed, ilocItemCount > 100.";
        G::issue("Warning", msg, "Heic::ilocBox", m->row, m->fPath);
        offset += length;
        return false;
    }
    for (int i = 0; i < ilocItemCount; i++) {
        quint16 item_ID = Utilities::get16(file->read(2));
        quint16 data_reference_index = Utilities::get16(file->read(2));
        quint32 base_offset = 0;
        if (ilocBaseOffsetSize == 0) base_offset = Utilities::get8(file->read(2));
        if (ilocBaseOffsetSize == 1) base_offset = Utilities::get8(file->read(1));
        if (ilocBaseOffsetSize == 2) base_offset = Utilities::get16(file->read(2));
        if (ilocBaseOffsetSize == 4) base_offset = Utilities::get32(file->read(4));
        quint16 extent_count = Utilities::get16(file->read(2));
        if (isDebug) {
            qDebug() << "Heic::ilocBox" << "Item:" << i
                     << "itemId" << item_ID
                     << "data_reference_index" << data_reference_index
                     << "base_offset" << base_offset
                     << "extent_count" << extent_count;
        }
        if (extent_count > 100) {
            QString msg = "Quitting because extent_count has reached " +
                          QString::number(extent_count);
            G::issue("Warning", msg, "Heic::ilocBox", m->row, fPath);
            offset += length;
            return false;
        }
        for (int j = 0; j < extent_count; j++) {
            quint32 extent_offset = 0;
            if (ilocOffsetSize == 0) extent_offset = Utilities::get8(file->read(2));
            if (ilocOffsetSize == 1) extent_offset = Utilities::get8(file->read(1));
            if (ilocOffsetSize == 2) extent_offset = Utilities::get16(file->read(2));
            if (ilocOffsetSize == 4) extent_offset = Utilities::get32(file->read(4));
            quint32 extent_length = 0;
            if (ilocLengthSize == 0) extent_length = Utilities::get8(file->read(2));
            if (ilocLengthSize == 1) extent_length = Utilities::get8(file->read(1));
            if (ilocLengthSize == 2) extent_length = Utilities::get16(file->read(2));
            if (ilocLengthSize == 4) extent_length = Utilities::get32(file->read(4));

            // capture the EXIF data segment
            if (item_ID == exifItemID) {
                exifOffset = extent_offset;
                exifLength = extent_length;
                if (isDebug) {
                    qDebug() << "Heic::ilocBox" << "    EXIF item_ID:" << i + 1
                             << "exifOffset" << exifOffset
                             << "exifLength" << exifLength;
                }
            }
            if (isDebug) {
                qDebug() <<"Heic::ilocBox" << "    Extent:" << i + 1
                         << "extent_offset" << extent_offset
                         << "extent_length" << extent_length;
            }
        }
    }
    if (isDebug) {
        qDebug() << "Heic::ilocBox" << "file->pos()" << file->pos();
    }
    //    if (hdlrType != "pict") {
    //        // err
    //        return false;
    //    }

    offset += length;
    return true;
}

bool Heic::infeBox(quint32 &offset, quint32 &length)
{
/*
    Find exifItemID which establishes which iloc to used to get the exifOffset
    and exifLength for the embedded EXIF data.
*/
    int endOfBox = offset + length;
    file->seek(offset + 12);
    quint16 item_ID = Utilities::get16(file->read(2));
    quint16 item_protection_index = Utilities::get16(file->read(2));
    QString item_name = Utilities::getCString(*file);
//    QString content_type = Utilities::getCString(*file);
//    QString content_encoding = Utilities::getCString(*file);

    QString content_type = "";
    QString content_encoding = "";
    if (file->pos() + 1 < endOfBox) {
//        file->seek(file->pos() + 1);
        content_type = Utilities::getCString(*file);
        if (content_type.length() && file->pos() + 1 < endOfBox) {
//            file->seek(file->pos() + 1);
            content_encoding = Utilities::getCString(*file);
        }
    }

    if (item_name == "Exif") exifItemID = item_ID;

    file->seek(endOfBox);

//    qDebug() << "Heic::infeBox"
//             << "file->pos() =" << file->pos()
//             << "endOfBox =" << endOfBox
//             << "item_name =" << item_name
//             << "content_type =" << content_type
//             << "content_encoding =" << content_encoding
//                ;

    if (isDebug)
        qDebug() << "   " << "Heic::infeBox"
                 << "item_ID =" << item_ID
                 << "length =" << length
//                 << "file->pos() =" << file->pos()
                 << "endOfBox =" << endOfBox
                 << "item_protection_index =" << item_protection_index
                 << "item_name =" << item_name
                 << "content_type =" << content_type
                 << "content_encoding =" << content_encoding
                    ;
    return true;
}

bool Heic::iinfBox(quint32 &offset, quint32 &length)
{
/*
    Call infeBox for each item to determine exifItemID which establishes which
    iloc to used to get the exifOffset and exifLength for the embedded EXIF data.
*/
    // skip version stuff
    file->seek(offset + 12);
//    file->seek(offset + 14);
    quint16 entry_count = Utilities::get16(file->read(2));
    if (isDebug) {
        qDebug() << "Heic::iinfBox" << "iint entry count =" << entry_count << file->pos();
    }
    if (entry_count == 0) {
        // err
        QString msg = "No iinf entries found.";
        G::issue("Warning", msg, "Heic::iinfBox", m->row, m->fPath);
        offset += length;
        return false;
    }

    quint32 infeOffset = static_cast<quint32>(file->pos());
    quint32 infeLength;
    for (int i = 0; i < entry_count; i++) {
        QString infeType;
        nextHeifBox(infeLength, infeType);
        if (isDebug) {
            qDebug() << "Heic::iinfBox" << "   Item Info Entry " << i + 1;
        }
        infeBox(infeOffset, infeLength);
        infeOffset += infeLength;
    }

    offset += length;
    return true;
}

// add a large version sitrBoxL
bool Heic::sitrBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        file->seek(offset + 8);
        quint16 from_item_ID = Utilities::get16(file->read(2));
        quint16 reference_count = Utilities::get16(file->read(2));

        qDebug() << "Heic::sitrBox"
                 << "from_item_ID =" << from_item_ID
                 << "reference_count =" << reference_count;

        for (int i = 0; i < reference_count; i++) {
            quint32 to_item_ID = Utilities::get32(file->read(4));
            qDebug() << "Heic::sitrBox" << i << ": to_item_ID =" << to_item_ID;
        }
    }
    offset += length;
    return true;
}

bool Heic::irefBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        irefOffset = offset;
        irefLength = length;
        quint32 irefEndOffset = offset + length;
        file->seek(offset + 8);
        uint version = Utilities::get8(file->read(1));
        quint32 sitrOffset = offset + 12;
        qDebug() << "Heic::irefBox" << "offset =" << offset
                 << "length =" << length << "sitrOffset" << sitrOffset;
        while (sitrOffset < irefEndOffset) {
            quint32 sitrLength;
            QString type;
            file->seek(sitrOffset);
            nextHeifBox(sitrLength, type);
            if (version == 0) sitrBox(sitrOffset, sitrLength);
    //        else sitrBoxL(sitrOffset, sitrLength);
            qDebug() << "Heic::irefBox" << "irefEndOffset =" << irefEndOffset << "sitrOffset" << sitrOffset;
        }
    }
    offset += length;
    return true;
}

bool Heic::hvcCBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        qDebug() << "Heic::hvcCBox" << "file position:" << file->pos();
        uint configurationVersion = Utilities::get8(file->read(1));
        quint8 x = Utilities::get8(file->read(1));
        int general_profile_space = (x & 0b11000000) >> 6;             // first 2 bits
        int general_tier_flag =     (x & 0b00100000) >> 5;             // 3rd bit
        int general_profile_idc =   (x & 0b00011111);                  // last 5 bits
        quint32 general_profile_compatibility_flags = Utilities::get32(file->read(4));
        auto general_constraint_indicator_flags = Utilities::get48(file->read(6));
        quint8 general_level_idc  = Utilities::get8(file->read(1));
        quint16 min_spatial_segmentation_idc = Utilities::get16(file->read(2));
        // 4 bits reserved, last 12 = min_spatial_segmentation_idc
        min_spatial_segmentation_idc = min_spatial_segmentation_idc & 0b0000111111111111;

        quint8 parallelismType = Utilities::get8(file->read(1));
        // 6 bits reserved, last 2 = parallelismType
        parallelismType = parallelismType & 0b00000011;

        quint8 chroma_format_idc = Utilities::get8(file->read(1));
        // 6 bits reserved, last 2 = chroma_format_idc
        chroma_format_idc = chroma_format_idc & 0b00000011;

        quint8 bit_depth_luma_minus8 = Utilities::get8(file->read(1));
        // 5 bits reserved, last 3 = bit_depth_luma_minus8
        bit_depth_luma_minus8 = bit_depth_luma_minus8 & 0b00000111;
        quint8 bit_depth_chroma_minus8 = Utilities::get8(file->read(1));
        // 5 bits reserved, last 3 = bit_depth_chroma_minus8
        bit_depth_chroma_minus8 = bit_depth_chroma_minus8 & 0b00000111;

        quint16 avgFrameRate = Utilities::get16(file->read(2));

        x = Utilities::get8(file->read(1));
        int constantFrameRate = (x & 0b11000000) >> 6;             // first 2 bits
        int numTemporalLayers =     (x & 0b00111000) >> 3;         // next 3 bits
        int temporalIdNested =     (x & 0b00000100) >> 5;          // next bit
        int lengthSizeMinusOne =   (x & 0b00000011);               // last 2 bits

        quint8 numOfArrays = Utilities::get8(file->read(1));

        qDebug() << "Heic::hvcCBox" << "configurationVersion =" << configurationVersion;
        qDebug() << "Heic::hvcCBox" << "general_profile_space =" << general_profile_space;
        qDebug() << "Heic::hvcCBox" << "general_tier_flag =" << general_tier_flag;
        qDebug() << "Heic::hvcCBox" << "general_profile_idc =" << general_profile_idc;
        qDebug() << "Heic::hvcCBox" << "general_profile_compatibility_flags =" << general_profile_compatibility_flags;
        qDebug() << "Heic::hvcCBox" << "general_constraint_indicator_flags =" << general_constraint_indicator_flags;
        qDebug() << "Heic::hvcCBox" << "general_level_idc =" << general_level_idc;
        qDebug() << "Heic::hvcCBox" << "min_spatial_segmentation_idc =" << min_spatial_segmentation_idc;
        qDebug() << "Heic::hvcCBox" << "parallelismType =" << parallelismType;
        qDebug() << "Heic::hvcCBox" << "chroma_format_idc =" << chroma_format_idc;
        qDebug() << "Heic::hvcCBox" << "bit_depth_luma_minus8 =" << bit_depth_luma_minus8;
        qDebug() << "Heic::hvcCBox" << "bit_depth_chroma_minus8 =" << bit_depth_chroma_minus8;
        qDebug() << "Heic::hvcCBox" << "avgFrameRate =" << avgFrameRate;
        qDebug() << "Heic::hvcCBox" << "constantFrameRate =" << constantFrameRate;
        qDebug() << "Heic::hvcCBox" << "numTemporalLayers =" << numTemporalLayers;
        qDebug() << "Heic::hvcCBox" << "temporalIdNested =" << temporalIdNested;
        qDebug() << "Heic::hvcCBox" << "lengthSizeMinusOne =" << lengthSizeMinusOne;
        qDebug() << "Heic::hvcCBox" << "numOfArrays =" << numOfArrays;

        for (int i = 0; i < numOfArrays; i++) {
            x = Utilities::get8(file->read(1));
            int array_completeness = (x & 0b10000000) >> 6;             // first bit
                                                                        // 2nd bit reserved
            int NAL_unit_type = (x & 0b00111111);                         // last 6 bits
            quint16 numNalus = Utilities::get16(file->read(2));
            qDebug() << "Heic::hvcCBox" << "    array # =" << i;
            qDebug() << "Heic::hvcCBox" << "    array_completeness =" << array_completeness;
            qDebug() << "Heic::hvcCBox" << "    NAL_unit_type =" << NAL_unit_type;
            qDebug() << "Heic::hvcCBox" << "    numNalus =" << numNalus;

            for (quint16 j = 0; j < numNalus; j++) {
                quint16 nalUnitLength = Utilities::get16(file->read(2));
                QByteArray nalUnit = file->read(nalUnitLength);
                qDebug() << "Heic::hvcCBox" << "        nalUnitLength =" << nalUnitLength;
            }
        }

        qDebug() << "Heic::hvcCBox"
                 << "offset:" << offset
                 << "length:" << length
                 << "offset + length" << offset + length
                 << "file position:" << file->pos();
    }
    offset += length;
    return true;
}

bool Heic::ispeBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        file->seek(offset + 12);
        quint32 image_width = Utilities::get32(file->read(4));
        quint32 image_height = Utilities::get32(file->read(4));
        qDebug() << "Heic::ispeBox" << "image_width =" << image_width;
        qDebug() << "Heic::ispeBox" << "image_height =" << image_height << "\n";
    }
    offset += length;       // temp for testing
    return true;
}

bool Heic::ipmaBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        file->seek(offset + 8);
        quint16 x = Utilities::get16(file->read(2));
        auto version = (x & 0b1111000000000000) >> 12;      // first 4 bits
        auto flags =   (x & 0b0000111111111111);            // next 12 bits

        file->seek(offset + 12);
        quint32 entry_count = Utilities::get32(file->read(4));
        qDebug() << "Heic::ipmaBox" << "entry_count =" << entry_count;

        for (quint16 i = 0; i < entry_count; i++ ) {
            quint32 item_ID;
            if (version < 1) {
                item_ID = Utilities::get16(file->read(2));
            }
            else {
                item_ID = Utilities::get32(file->read(4));
            }
            quint8 association_count = Utilities::get8(file->read(1));
            qDebug() << "    entry#" << i
                     << "item_ID =" << item_ID
                     << "association_count =" << association_count;

            for (int j = 0; j < association_count; j++) {
                int x = Utilities::get8(file->peek(1));
                int essential =  (x & 0b10000000) >> 7;
                quint16 property_index;
                if (flags & 1) {
                    property_index = Utilities::get16(file->read(2));
                    property_index  = property_index & 0b0111111111111111;
                }
                else {
                    property_index = Utilities::get8(file->read(1));
                    property_index  = property_index & 0b01111111;
                }
                qDebug() << "        association#" << j
                         << "essential =" << essential
                         << "property_index =" << property_index;
            }
        }

        qDebug() << "Heic::ipmaBox"
                 << "offset:" << offset
                 << "length:" << length
                 << "offset + length" << offset + length
                 << "file position:" << file->pos();
    }
    offset += length;
    return true;
}

bool Heic::iprpBox(quint32 &offset, quint32 &length)
{
    quint32 iprpEnd = offset + length;
    if (isDebug) {
        qDebug() << "Heic::iprpBox" << "offset =" << offset;
        QString type;
        file->seek(offset + 8);

        // should be an ipco box next
        quint32 ipcoLength;
        QString ipcoType;
        nextHeifBox(ipcoLength, ipcoType);

        if (ipcoType != "ipco") {
            // err
            QString msg = "Type ipco not found in iprp box.";
            G::issue("Error", msg, "Heic::iprpBox", m->row, fPath);
            return false;
        }

        offset += 16;

        while (offset < iprpEnd) {
            file->seek(offset);
            nextHeifBox(length, type);
            getHeifBox(type, offset, length);
        }
    }
    offset = iprpEnd;

    return true;
}

bool Heic::mdatBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        qDebug() << "Heic::mdatBox" << "offset =" << offset;
    }
    offset += length;
    return true;
}

bool Heic::idatBox(quint32 &offset, quint32 &length)
{
    if (isDebug) {
        qDebug() << "Heic::idatBox" << "offset =" << offset;
    }
    offset += length;       // temp for testing
    return true;
}
