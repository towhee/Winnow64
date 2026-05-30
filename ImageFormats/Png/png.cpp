#include "png.h"
#include <QtEndian>
#include <QBuffer>
#include "Utilities/utilities.h"
#include "Metadata/xmp.h"

PNG::PNG()
{
}

bool PNG::readChunkHeader(QFile &file, quint32 &size, char (&type)[4]) {
    if (file.read((char*)&size, 4) != 4) return false;
    size = qFromBigEndian(size);  // Convert from big endian to host byte order
    if (file.read(type, 4) != 4) return false;
    return true;
}

QByteArray PNG::readChunkData(QFile &file, quint32 size) {
    QByteArray data = file.read(size);
    file.seek(file.pos() + 4);  // Skip CRC (4 bytes)
    return data;
}

quint32 PNG::readUInt32(QFile &file) {
    quint32 value;
    file.read((char*)&value, 4);
    return qFromBigEndian(value);  // Convert to host byte order
}

QDateTime PNG::createDate(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file" << filePath;
        return QDateTime();
    }

    // PNG Signature (8 bytes)
    QByteArray pngSignature = file.read(8);
    if (pngSignature != QByteArray::fromHex("89504E470D0A1A0A")) {
        qWarning() << "Not a valid PNG file!";
        return QDateTime();
    }

    quint32 chunkSize;
    char chunkType[4];

    while (!file.atEnd()) {
        if (!readChunkHeader(file, chunkSize, chunkType)) {
            qWarning() << "Failed to read chunk header!";
            break;
        }

        QString chunkTypeStr = QString::fromLatin1(chunkType, 4);
        // qDebug() << "PNG::createDate found chunk:" << chunkTypeStr;

        // Check for tEXt or iTXt chunks
        if (chunkTypeStr == "tEXt" || chunkTypeStr == "iTXt") {
            QByteArray chunkData = readChunkData(file, chunkSize);
            QList<QByteArray> fields = chunkData.split('\0');  // Split the key-value pair
            if (fields.size() == 2 && fields[0] == "date:create") {
                QString dateStr = QString::fromLatin1(fields[1]);
                QDateTime createDate = QDateTime::fromString(dateStr, Qt::ISODate);
                if (createDate.isValid()) {
                    // qDebug() << "PNG::creationDate found creation time:" << createDate;
                    return createDate;
                }
            }
        } else {
            // Skip chunk data and CRC
            file.seek(file.pos() + chunkSize + 4);  // Skip chunk data and CRC
        }
    }

    // qWarning() << "Creation time not found in PNG file!";
    return QDateTime();
}

double PNG::rationalToDouble(const QString &s)
{
    // Parses XMP-style EXIF rationals: "4/1" → 4.0, "6000/10" → 600.0, "0" → 0.0.
    if (s.isEmpty()) return 0.0;
    int slash = s.indexOf('/');
    if (slash < 0) return s.toDouble();
    double num = s.left(slash).toDouble();
    double den = s.mid(slash + 1).toDouble();
    if (den == 0.0) return 0.0;
    return num / den;
}

QByteArray PNG::zlibInflate(const QByteArray &compressed)
{
    // PNG zTXt / compressed iTXt / iCCP all use zlib format (RFC 1950).
    // qUncompress expects a 4-byte big-endian uncompressed-size hint prefix.
    QByteArray withHeader;
    withHeader.resize(4);
    quint32 hint = qToBigEndian(static_cast<quint32>(qMin<int>(64 * 1024 * 1024,
                                                               compressed.size() * 64 + 4096)));
    memcpy(withHeader.data(), &hint, 4);
    withHeader.append(compressed);
    return qUncompress(withHeader);
}

void PNG::applyTextKey(ImageMetadata &m, const QString &key, const QString &value)
{
    // PNG text keys are case-sensitive; map the standard ones to ImageMetadata.
    if (key == "Title")                 m.title = value;
    else if (key == "Author")           m.creator = value;
    else if (key == "Creator")          m.creator = value;
    else if (key == "Copyright")        m.copyright = value;
    else if (key == "Creation Time" || key == "date:create") {
        if (!m.createdDate.isValid()) {
            QDateTime dt = QDateTime::fromString(value, Qt::ISODate);
            if (!dt.isValid()) dt = QDateTime::fromString(value, Qt::RFC2822Date);
            if (dt.isValid()) m.createdDate = dt;
        }
    }
    // Description / Software / Comment / Disclaimer / Warning / Source have no DataModel column.
}

bool PNG::parseExifChunk(MetadataParameters &p, ImageMetadata &m,
                         IFD *ifd, Exif *exif, GPS *gps,
                         quint32 chunkDataOffset, quint32 chunkDataLength)
{
    /*
        The eXIf chunk data is an EXIF TIFF block, byte-identical to a JPEG APP1
        EXIF payload (sans the "Exif\0\0" prefix). This mirrors Jpeg::parse from
        the II/MM byte-order scan onward.
    */
    if (chunkDataLength < 8) return false;

    Utilities u;
    bool isBigEnd = true;
    if (!p.file.seek(chunkDataOffset)) return false;

    quint32 order = u.get16(p.file.read(2));
    if (order != 0x4949 && order != 0x4D4D) return false;
    isBigEnd = (order == 0x4D4D);
    quint32 startOffset = chunkDataOffset;  // EXIF tag offsets are relative to byte-order marker

    quint16 magic = u.get16(p.file.read(2), isBigEnd);
    if (magic != 42) return false;
    quint32 offsetIfd0 = u.get32(p.file.read(4), isBigEnd) + startOffset;

    // Read IFD0
    p.hdr = "IFD0";
    p.offset = offsetIfd0;
    p.hash = &exif->hash;
    quint32 nextIFDOffset = ifd->readIFD(p, isBigEnd);
    if (nextIFDOffset) nextIFDOffset += startOffset;

    quint32 offsetEXIF = 0;
    if (ifd->ifdDataHash.contains(34665))
        offsetEXIF = ifd->ifdDataHash.value(34665).tagValue + startOffset;

    quint32 offsetGPS = 0;
    if (ifd->ifdDataHash.contains(34853))
        offsetGPS = ifd->ifdDataHash.value(34853).tagValue + startOffset;

    if (ifd->ifdDataHash.contains(274))
        m.orientation = static_cast<int>(ifd->ifdDataHash.value(274).tagValue);
    if (ifd->ifdDataHash.contains(271))
        m.make = u.getString(p.file, ifd->ifdDataHash.value(271).tagValue + startOffset,
                             ifd->ifdDataHash.value(271).tagCount);
    if (ifd->ifdDataHash.contains(272))
        m.model = u.getString(p.file, ifd->ifdDataHash.value(272).tagValue + startOffset,
                              ifd->ifdDataHash.value(272).tagCount);
    if (ifd->ifdDataHash.contains(315))
        m.creator = u.getString(p.file, ifd->ifdDataHash.value(315).tagValue + startOffset,
                                ifd->ifdDataHash.value(315).tagCount);
    if (ifd->ifdDataHash.contains(33432))
        m.copyright = u.getString(p.file, ifd->ifdDataHash.value(33432).tagValue + startOffset,
                                  ifd->ifdDataHash.value(33432).tagCount);

    // Read EXIF IFD
    if (offsetEXIF) {
        ifd->ifdDataHash.clear();
        p.hdr = "IFD Exif";
        p.offset = offsetEXIF;
        ifd->readIFD(p, isBigEnd);

        // EXIF width/height (40962 / 40963) — only override IHDR if present and non-zero
        if (ifd->ifdDataHash.contains(40962)) {
            int w = static_cast<int>(ifd->ifdDataHash.value(40962).tagValue);
            if (w > 0) m.width = w;
        }
        if (ifd->ifdDataHash.contains(40963)) {
            int h = static_cast<int>(ifd->ifdDataHash.value(40963).tagValue);
            if (h > 0) m.height = h;
        }

        // Created datetime: prefer DateTimeOriginal (36867), fall back to CreateDate (36868)
        QString createdExif;
        if (ifd->ifdDataHash.contains(36867)) {
            createdExif = u.getString(p.file,
                                      ifd->ifdDataHash.value(36867).tagValue + startOffset,
                                      ifd->ifdDataHash.value(36867).tagCount).left(19);
        }
        if (createdExif.isEmpty() && ifd->ifdDataHash.contains(36868)) {
            createdExif = u.getString(p.file,
                                      ifd->ifdDataHash.value(36868).tagValue + startOffset,
                                      ifd->ifdDataHash.value(36868).tagCount).left(19);
        }
        if (!createdExif.isEmpty()) {
            QDateTime dt = QDateTime::fromString(createdExif, "yyyy:MM:dd hh:mm:ss");
            if (dt.isValid()) m.createdDate = dt;
        }

        // Shutter speed (33434, ExposureTime)
        if (ifd->ifdDataHash.contains(33434)) {
            double x = u.getReal(p.file,
                                 ifd->ifdDataHash.value(33434).tagValue + startOffset,
                                 isBigEnd);
            if (x > 0 && x < 1) {
                double recip = 1.0 / x;
                if (recip >= 2) m.exposureTime = "1/" + QString::number(qRound(recip));
                else m.exposureTime = "1/" + QString::number(recip, 'g', 2);
                m.exposureTimeNum = x;
            } else if (x >= 1) {
                uint t = static_cast<uint>(x);
                m.exposureTime = QString::number(t);
                m.exposureTimeNum = t;
            }
            if (!m.exposureTime.isEmpty()) m.exposureTime += " sec";
        }

        // Aperture (33437, FNumber)
        if (ifd->ifdDataHash.contains(33437)) {
            double x = u.getReal(p.file,
                                 ifd->ifdDataHash.value(33437).tagValue + startOffset,
                                 isBigEnd);
            m.aperture = "f/" + QString::number(x, 'f', 1);
            m.apertureNum = (qRound(x * 10) / 10.0);
        }

        // ISO (34855)
        if (ifd->ifdDataHash.contains(34855)) {
            quint32 x = ifd->ifdDataHash.value(34855).tagValue;
            m.ISONum = static_cast<int>(x);
            m.ISO = QString::number(m.ISONum);
        }

        // Exposure compensation (37380)
        if (ifd->ifdDataHash.contains(37380)) {
            double x = u.getReal_s(p.file,
                                   ifd->ifdDataHash.value(37380).tagValue + startOffset,
                                   isBigEnd);
            m.exposureCompensation = QString::number(x, 'f', 1) + " EV";
            m.exposureCompensationNum = x;
        }

        // Focal length (37386)
        if (ifd->ifdDataHash.contains(37386)) {
            double x = u.getReal(p.file,
                                 ifd->ifdDataHash.value(37386).tagValue + startOffset,
                                 isBigEnd);
            m.focalLengthNum = static_cast<int>(x);
            m.focalLength = QString::number(x, 'f', 0) + "mm";
        }

        // Lens model (42036)
        if (ifd->ifdDataHash.contains(42036)) {
            m.lens = u.getString(p.file,
                                 ifd->ifdDataHash.value(42036).tagValue + startOffset,
                                 ifd->ifdDataHash.value(42036).tagCount);
        }
    }

    // Read GPS IFD
    if (offsetGPS) {
        ifd->ifdDataHash.clear();
        p.hdr = "IFD GPS";
        p.offset = offsetGPS;
        p.hash = &gps->hash;
        ifd->readIFD(p, isBigEnd);

        if (ifd->ifdDataHash.contains(1)) {  // GPSLatitudeRef
            m.gpsCoord = gps->decode(p.file, ifd->ifdDataHash, isBigEnd, 12);
        }
    }

    return true;
}

bool PNG::parse(MetadataParameters &p,
                ImageMetadata &m,
                IFD *ifd,
                Exif *exif,
                GPS *gps)
{
    m.parseSource = "PNG::parse";
    m.iccSegmentOffset = 0;
    m.iccSegmentLength = 0;

    if (!p.file.isOpen()) return false;

    p.file.seek(0);
    if (p.file.read(8) != QByteArray::fromHex("89504E470D0A1A0A")) return false;

    // PNG is the entire file — used as the "full image" reference downstream.
    m.offsetFull = 0;
    m.lengthFull = static_cast<uint>(p.file.size());
    m.offsetThumb = m.offsetFull;
    m.lengthThumb = m.lengthFull;

    quint32 exifChunkOffset = 0;
    quint32 exifChunkLength = 0;
    quint32 xmpTextOffset = 0;
    quint32 xmpTextLength = 0;

    while (!p.file.atEnd()) {
        quint32 size;
        char type[4];
        if (!readChunkHeader(p.file, size, type)) break;
        quint32 dataStart = static_cast<quint32>(p.file.pos());
        QByteArray typeStr(type, 4);

        if (typeStr == "IHDR") {
            // 13 bytes: width(4), height(4), bit-depth(1), color-type(1), compression(1), filter(1), interlace(1)
            if (size >= 8) {
                QByteArray data = p.file.read(qMin<quint32>(size, 13));
                m.width = static_cast<int>(qFromBigEndian<quint32>(
                    reinterpret_cast<const uchar*>(data.constData())));
                m.height = static_cast<int>(qFromBigEndian<quint32>(
                    reinterpret_cast<const uchar*>(data.constData() + 4)));
            }
            p.file.seek(dataStart + size + 4);
        }
        else if (typeStr == "tEXt") {
            QByteArray data = p.file.read(size);
            int nul = data.indexOf('\0');
            if (nul > 0 && nul < data.size() - 1) {
                QString key = QString::fromLatin1(data.left(nul));
                QString val = QString::fromLatin1(data.mid(nul + 1));
                applyTextKey(m, key, val);
            }
            p.file.seek(dataStart + size + 4);
        }
        else if (typeStr == "zTXt") {
            QByteArray data = p.file.read(size);
            int nul = data.indexOf('\0');
            if (nul > 0 && nul + 2 < data.size()) {
                QString key = QString::fromLatin1(data.left(nul));
                // data[nul+1] is compression method (must be 0 = deflate)
                QByteArray compressed = data.mid(nul + 2);
                QByteArray inflated = zlibInflate(compressed);
                if (!inflated.isEmpty()) {
                    applyTextKey(m, key, QString::fromLatin1(inflated));
                }
            }
            p.file.seek(dataStart + size + 4);
        }
        else if (typeStr == "iTXt") {
            // keyword\0 cFlag(1) cMethod(1) lang\0 transKey\0 text
            QByteArray data = p.file.read(size);
            int kEnd = data.indexOf('\0');
            if (kEnd > 0 && kEnd + 4 < data.size()) {
                QString key = QString::fromLatin1(data.left(kEnd));
                quint8 cFlag = static_cast<quint8>(data[kEnd + 1]);
                int langStart = kEnd + 3;
                int langEnd = data.indexOf('\0', langStart);
                if (langEnd > 0) {
                    int tEnd = data.indexOf('\0', langEnd + 1);
                    if (tEnd > 0 && tEnd + 1 < data.size()) {
                        int textStart = tEnd + 1;
                        int textLen = data.size() - textStart;

                        if (key == "XML:com.adobe.xmp" && cFlag == 0) {
                            // Uncompressed: point Xmp at the file offset directly.
                            xmpTextOffset = dataStart + textStart;
                            xmpTextLength = textLen;
                        } else {
                            QByteArray textBytes;
                            if (cFlag == 0) textBytes = data.mid(textStart, textLen);
                            else            textBytes = zlibInflate(data.mid(textStart, textLen));
                            if (!textBytes.isEmpty())
                                applyTextKey(m, key, QString::fromUtf8(textBytes));
                        }
                    }
                }
            }
            p.file.seek(dataStart + size + 4);
        }
        else if (typeStr == "eXIf") {
            exifChunkOffset = dataStart;
            exifChunkLength = size;
            p.file.seek(dataStart + size + 4);
        }
        else if (typeStr == "iCCP") {
            // profile-name\0 compression-method(1) compressed-profile
            QByteArray data = p.file.read(size);
            int nul = data.indexOf('\0');
            if (nul > 0 && nul + 2 < data.size()) {
                QByteArray compressed = data.mid(nul + 2);
                QByteArray icc = zlibInflate(compressed);
                if (!icc.isEmpty()) {
                    m.iccBuf = icc;
                    m.iccSegmentLength = static_cast<quint32>(icc.size());
                    // ICC profile header: bytes 16..19 are the color space signature.
                    if (icc.size() >= 20) m.iccSpace = QString::fromLatin1(icc.mid(16, 4)).trimmed();
                    if (m.iccSpace == "sRGB") {
                        // Match Jpeg behaviour: don't carry the buffer when it's sRGB.
                        m.iccBuf.clear();
                    }
                }
            }
            p.file.seek(dataStart + size + 4);
        }
        else if (typeStr == "tIME") {
            QByteArray data = p.file.read(size);
            if (data.size() >= 7) {
                quint16 yr = qFromBigEndian<quint16>(
                    reinterpret_cast<const uchar*>(data.constData()));
                int mo = static_cast<quint8>(data[2]);
                int dy = static_cast<quint8>(data[3]);
                int hr = static_cast<quint8>(data[4]);
                int mn = static_cast<quint8>(data[5]);
                int sc = static_cast<quint8>(data[6]);
                QDateTime dt(QDate(yr, mo, dy), QTime(hr, mn, sc), Qt::UTC);
                if (dt.isValid()) m.modifiedDate = dt;
            }
            p.file.seek(dataStart + size + 4);
        }
        else if (typeStr == "IEND") {
            break;
        }
        else {
            p.file.seek(dataStart + size + 4);
        }
    }

    if (m.width <= 0 || m.height <= 0) return false;
    m.widthPreview = m.width;
    m.heightPreview = m.height;

    // EXIF (eXIf chunk) — reuse JPEG-style IFD/Exif/GPS handoff.
    if (exifChunkOffset && exifChunkLength >= 8) {
        parseExifChunk(p, m, ifd, exif, gps, exifChunkOffset, exifChunkLength);
    }

    // XMP — let the existing Xmp class locate <x:xmpmeta> within the iTXt text region.
    if (xmpTextOffset && xmpTextLength) {
        m.xmpSegmentOffset = xmpTextOffset;
        m.xmpSegmentLength = xmpTextLength;
        m.isXmp = true;
        if (!G::stop) {
            Xmp xmp(p.file, m.xmpSegmentOffset, m.xmpSegmentLength, p.instance);
            if (xmp.isValid) {
                p.xmpModifyDate = QDateTime::fromString(xmp.getItem("modifydate"), Qt::ISODate);
                m.rating = xmp.getItem("Rating");
                m.label = xmp.getItem("Label");
                if (m.title.isEmpty())     m.title = xmp.getItem("title");
                m.cameraSN = xmp.getItem("SerialNumber");
                if (m.lens.isEmpty())      m.lens = xmp.getItem("Lens");
                if (m.lens.isEmpty())      m.lens = xmp.getItem("LensModel");
                m.lensSN = xmp.getItem("LensSerialNumber");
                if (m.creator.isEmpty())   m.creator = xmp.getItem("creator");
                if (m.copyright.isEmpty()) m.copyright = xmp.getItem("rights");
                m.email = xmp.getItem("email");
                m.url = xmp.getItem("url");
                m.keywords = xmp.getItemList("subject");

                // EXIF camera fields — Lightroom/Photoshop drop the eXIf chunk
                // and mirror these into the XMP exif:/tiff: namespaces.
                if (m.make.isEmpty())  m.make  = xmp.getItem("make");
                if (m.model.isEmpty()) m.model = xmp.getItem("model");

                if (m.apertureNum == 0.0) {
                    double f = rationalToDouble(xmp.getItem("FNumber"));
                    if (f > 0) {
                        m.aperture = "f/" + QString::number(f, 'f', 1);
                        m.apertureNum = qRound(f * 10) / 10.0;
                    }
                }
                if (m.exposureTimeNum == 0.0) {
                    double t = rationalToDouble(xmp.getItem("ExposureTime"));
                    if (t > 0 && t < 1) {
                        double recip = 1.0 / t;
                        if (recip >= 2) m.exposureTime = "1/" + QString::number(qRound(recip));
                        else            m.exposureTime = "1/" + QString::number(recip, 'g', 2);
                        m.exposureTimeNum = t;
                        m.exposureTime += " sec";
                    } else if (t >= 1) {
                        m.exposureTimeNum = t;
                        m.exposureTime = QString::number(static_cast<uint>(t)) + " sec";
                    }
                }
                if (m.ISONum == 0) {
                    QStringList isoList = xmp.getItemList("isospeedratings");
                    QString isoStr = !isoList.isEmpty() ? isoList.first()
                                                       : xmp.getItem("isospeedratings");
                    int iso = isoStr.toInt();
                    if (iso > 0) {
                        m.ISONum = iso;
                        m.ISO = QString::number(iso);
                    }
                }
                if (m.focalLengthNum == 0) {
                    double fl = rationalToDouble(xmp.getItem("FocalLength"));
                    if (fl > 0) {
                        m.focalLengthNum = static_cast<int>(fl);
                        m.focalLength = QString::number(fl, 'f', 0) + "mm";
                    }
                }
                if (m.exposureCompensationNum == 0.0
                    && !xmp.getItem("ExposureBiasValue").isEmpty()) {
                    double ev = rationalToDouble(xmp.getItem("ExposureBiasValue"));
                    m.exposureCompensationNum = ev;
                    m.exposureCompensation = QString::number(ev, 'f', 1) + " EV";
                }
                if (!m.createdDate.isValid()) {
                    QString d = xmp.getItem("DateTimeOriginal");
                    if (!d.isEmpty()) {
                        QDateTime dt = QDateTime::fromString(d, Qt::ISODateWithMs);
                        if (!dt.isValid()) dt = QDateTime::fromString(d, Qt::ISODate);
                        if (dt.isValid()) m.createdDate = dt;
                    }
                }
            }
            m._rating = m.rating;
            m._label = m.label;
            m._title = m.title;
            m._creator = m.creator;
            m._copyright = m.copyright;
            m._email = m.email;
            m._url = m.url;
            m._orientation = m.orientation;
            m._rotationDegrees = m.rotationDegrees;
            if (p.report) p.xmpString = xmp.srcToString();
        }
    }

    return true;
}
