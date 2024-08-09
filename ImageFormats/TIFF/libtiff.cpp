#include "libtiff.h"

LibTiff::LibTiff(QObject *parent)
    : QObject{parent}
{}

QImage LibTiff::readTiffToQImage(const QString &filePath) {

    TIFF *tif = TIFFOpen(filePath.toStdString().c_str(), "r");
    if (!tif) {
        qDebug()
            << "LibTiff::readTiffToQImage"
            << "Failed to open TIFF file."
            << filePath
            ;
        return QImage();
    }

    quint32 width, height;
    quint16 samplesPerPixel, bitsPerSample;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);

    // if (bitsPerSample != 8) {
    //     qDebug()
    //         << "LibTiff::readTiffToQImage"
    //         << "Unsupported TIFF format. Only 8 bits per channel are supported."
    //         << filePath
    //         ;
    //     TIFFClose(tif);
    //     return QImage();
    // }

    size_t npixels = width * height;
    quint32 *raster = (quint32 *)_TIFFmalloc(npixels * sizeof(quint32));
    if (!raster) {
        qDebug()
            << "LibTiff::readTiffToQImage"
            << "Failed to allocate memory for raster."
            << filePath
            ;
        TIFFClose(tif);
        return QImage();
    }

    QImage::Format imageFormat = QImage::Format_Invalid;
    if (samplesPerPixel == 3) {
        imageFormat = QImage::Format_RGB888;
    } else if (samplesPerPixel == 4) {
        imageFormat = QImage::Format_ARGB32;
    } else {
        qDebug()
            << "LibTiff::readTiffToQImage"
            << "Unsupported TIFF format. Only 3-channel (RGB) or 4-channel (RGBA) images are supported."
            << filePath
            ;
        _TIFFfree(raster);
        TIFFClose(tif);
        return QImage();
    }

    if (!TIFFReadRGBAImage(tif, width, height, raster, 0)) {
        qDebug()
            << "LibTiff::readTiffToQImage"
            << "Failed to read TIFF image."
            << filePath
            ;
        _TIFFfree(raster);
        TIFFClose(tif);
        return QImage();
    }

    QImage image(width, height, imageFormat);
    for (quint32 y = 0; y < height; ++y) {
        for (quint32 x = 0; x < width; ++x) {
            quint32 pixel = raster[y * width + x];
            uint8_t r = TIFFGetR(pixel);
            uint8_t g = TIFFGetG(pixel);
            uint8_t b = TIFFGetB(pixel);
            uint8_t a = TIFFGetA(pixel);
            if (samplesPerPixel == 3) {
                image.setPixel(x, height - y - 1, qRgb(r, g, b)); // Flip image vertically
            } else if (samplesPerPixel == 4) {
                image.setPixel(x, height - y - 1, qRgba(r, g, b, a)); // Flip image vertically
            }
        }
    }

    _TIFFfree(raster);
    TIFFClose(tif);

    // Convert to AARRBBGG format if necessary
    if (imageFormat == QImage::Format_ARGB32) {
        QImage convertedImage = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < convertedImage.height(); ++y) {
            QRgb *line = (QRgb *)convertedImage.scanLine(y);
            for (int x = 0; x < convertedImage.width(); ++x) {
                QRgb pixel = line[x];
                uint8_t a = qAlpha(pixel);
                uint8_t r = qRed(pixel);
                uint8_t g = qGreen(pixel);
                uint8_t b = qBlue(pixel);
                line[x] = (a << 24) | (a << 16) | (r << 8) | g;
            }
        }
        return convertedImage;
    }

    return image;
    //*/
}

void LibTiff::rptFields(TiffFields &f)
{
    qDebug().noquote() << "LibTiff::testLibtiff"
                       << "directory =" << QString::number(f.directory).rightJustified(2)
                       << "width =" << QString::number(f.width).rightJustified(4)
                       << "height =" << QString::number(f.height).rightJustified(4)
                       << "depth =" << QString::number(f.depth).rightJustified(4)
                       << "samplesPerPixel =" << QString::number(f.samplesPerPixel).rightJustified(2)
                       << "bitsPerSample =" << QString::number(f.bitsPerSample).rightJustified(2)
                       << "planarConfig =" << QString::number(f.planarConfig).rightJustified(1)
                       << "predictor =" << QString::number(f.predictor).rightJustified(1)
                       << "compression =" << QString::number(f.compression).rightJustified(2)
                       << "orientation =" << QString::number(f.orientation).rightJustified(2)
                       << "photometric =" << QString::number(f.photometric).rightJustified(2)
        ;
}

void LibTiff::listDirectories(ImageMetadata &m)
{
    TIFF *tif = TIFFOpen(m.fPath.toStdString().c_str(), "r");  // sets directory 0
    if (!tif) {
        qDebug().noquote()
            << "LibTiff::listDirectories"
            << "Failed to open TIFF file."
            << m.fPath
            ;
        return;
    }

    // write/read to ensure internal structures are updated
    // TIFFWriteDirectory(tif);
    // TIFFSetDirectory(tif, TIFFCurrentDirectory(tif));

    TiffFields f;
    int dirCounter = 0;
    uint32_t directories = m.ifdOffsets.count();
    // uint32_t directories = TIFFNumberOfDirectories(tif);
    qDebug() << "LibTiff::listDirectories  directory count =" << directories << m.fPath;
    do {
        f.directory = dirCounter;
        TIFFSetSubDirectory(tif, m.ifdOffsets.at(dirCounter).toUInt());
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &f.width);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &f.height);
        TIFFGetField(tif, TIFFTAG_IMAGEDEPTH, &f.depth);
        TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &f.samplesPerPixel);
        TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &f.bitsPerSample);
        TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &f.planarConfig);
        TIFFGetField(tif, TIFFTAG_PREDICTOR, &f.predictor);
        TIFFGetField(tif, TIFFTAG_COMPRESSION, &f.compression);
        TIFFGetField(tif, TIFFTAG_ORIENTATION, &f.orientation);
        TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &f.photometric);
        rptFields(f);
        dirCounter++;
        // } while (TIFFReadDirectory(tif));
    } while (directories > dirCounter);

}

QImage LibTiff::testLibtiff(QString fPath, int row)
{
    TIFF *tif = TIFFOpen(fPath.toStdString().c_str(), "r");
    if (!tif) {
        qDebug().noquote()
            << "LibTiff::testLibtiff"
            << "Failed to open TIFF file."
            << fPath
            ;
        return QImage();
    }

    // TIFF directories
    int dircount = 0;
    do {
        dircount++;
    } while (TIFFReadDirectory(tif));

    // TIFF fields
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint16_t samplesPerPixel;
    uint16_t bitsPerSample;
    uint16_t compression;
    uint16_t planarConfig;
    uint16_t predictor;
    uint16_t orientation;
    uint16_t photometric;
    uint16_t datatype;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_IMAGEDEPTH, &depth);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarConfig);
    TIFFGetField(tif, TIFFTAG_PREDICTOR, &predictor);
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
    TIFFGetField(tif, TIFFTAG_ORIENTATION, &orientation);
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetField(tif, TIFFTAG_DATATYPE, &datatype);
    qDebug().noquote() << "LibTiff::testLibtiff"
                       << "directories =" << QString::number(dircount).rightJustified(2)
                       << "width =" << QString::number(width).rightJustified(4)
                       << "height =" << QString::number(height).rightJustified(4)
                       << "depth =" << QString::number(depth).rightJustified(4)
                       << "samplesPerPixel =" << QString::number(samplesPerPixel).rightJustified(2)
                       << "bitsPerSample =" << QString::number(bitsPerSample).rightJustified(2)
                       << "planarConfig =" << QString::number(planarConfig).rightJustified(1)
                       << "predictor =" << QString::number(predictor).rightJustified(1)
                       << "compression =" << QString::number(compression).rightJustified(2)
                       << "orientation =" << QString::number(orientation).rightJustified(2)
                       << "photometric =" << QString::number(photometric).rightJustified(2)
                       // << "datatype =" << QString::number(datatype).rightJustified(2)
                       << "row =" << QString::number(row).rightJustified(3)
                       // << "byte width =" << TIFFDataWidth(TIFF_BYTE)
                       << fPath
        ;

    // read tiff Winnow QImage format in ImageCache = c
    size_t nPixels;
    uint32_t* raster;
    nPixels = width * height;
    raster = (uint32_t*) _TIFFmalloc(nPixels * sizeof (uint32_t));
    if (raster == NULL) {
        // error
        qDebug().noquote() << "LibTiff::testLibtiff null raster.";
        return QImage();
    }
    if (!TIFFReadRGBAImageOriented(tif, width, height, raster, ORIENTATION_TOPLEFT, 0)) {
        // error
        qDebug().noquote() << "LibTiff::testLibtiff TIFFReadRGBAImageOriented failed,";
        _TIFFfree(raster);
        return QImage();
    }

    QElapsedTimer t;
    t.start();
    // Convert raster to QByteArray
    QByteArray ba(reinterpret_cast<const char*>(raster), width * height * sizeof(uint32_t));
    // const uchar *ba = (reinterpret_cast<const char*>(raster), w * h * sizeof(uint32_t));

    // Create a QImage from raw data
    QImage image(reinterpret_cast<const uchar*>(ba.data()), width, height, QImage::Format_RGBA8888);

    // Clean up
    _TIFFfree(raster);
    TIFFClose(tif);

    qDebug() << "LibTiff::testLibtiff  ms =" << t.elapsed();
    return image;
}

// QImageIOHandler::Transformations LibTiff::exif2Qt(int exifOrientation)
// {
//     switch (exifOrientation) {
//     case 1: // normal
//         return QImageIOHandler::TransformationNone;
//     case 2: // mirror horizontal
//         return QImageIOHandler::TransformationMirror;
//     case 3: // rotate 180
//         return QImageIOHandler::TransformationRotate180;
//     case 4: // mirror vertical
//         return QImageIOHandler::TransformationFlip;
//     case 5: // mirror horizontal and rotate 270 CW
//         return QImageIOHandler::TransformationFlipAndRotate90;
//     case 6: // rotate 90 CW
//         return QImageIOHandler::TransformationRotate90;
//     case 7: // mirror horizontal and rotate 90 CW
//         return QImageIOHandler::TransformationMirrorAndRotate90;
//     case 8: // rotate 270 CW
//         return QImageIOHandler::TransformationRotate270;
//     }
//     qWarning("Invalid EXIF orientation");
//     return QImageIOHandler::TransformationNone;
// }

// int LibTiff::qt2Exif(QImageIOHandler::Transformations transformation)
// {
//     switch (transformation) {
//     case QImageIOHandler::TransformationNone:
//         return 1;
//     case QImageIOHandler::TransformationMirror:
//         return 2;
//     case QImageIOHandler::TransformationRotate180:
//         return 3;
//     case QImageIOHandler::TransformationFlip:
//         return 4;
//     case QImageIOHandler::TransformationFlipAndRotate90:
//         return 5;
//     case QImageIOHandler::TransformationRotate90:
//         return 6;
//     case QImageIOHandler::TransformationMirrorAndRotate90:
//         return 7;
//     case QImageIOHandler::TransformationRotate270:
//         return 8;
//     }
//     qWarning("Invalid Qt image transformation");
//     return 1;
// }

// void LibTiff::convert32BitOrder(void *buffer, int width)
// {
//     uint32_t *target = reinterpret_cast<uint32_t *>(buffer);
//     for (int32_t x=0; x<width; ++x) {
//         uint32_t p = target[x];
//         // convert between ARGB and ABGR
//         target[x] = (p & 0xff000000)
//                     | ((p & 0x00ff0000) >> 16)
//                     | (p & 0x0000ff00)
//                     | ((p & 0x000000ff) << 16);
//     }
// }

// void LibTiff::rgb48fixup(QImage *image, bool floatingPoint)
// {
//     Q_ASSERT(image->depth() == 64);
//     const int h = image->height();
//     const int w = image->width();
//     uchar *scanline = image->bits();
//     const qsizetype bpl = image->bytesPerLine();
//     quint16 mask = 0xffff;
//     const qfloat16 fp_mask = qfloat16(1.0f);
//     if (floatingPoint)
//         memcpy(&mask, &fp_mask, 2);
//     for (int y = 0; y < h; ++y) {
//         quint16 *dst = reinterpret_cast<uint16_t *>(scanline);
//         for (int x = w - 1; x >= 0; --x) {
//             dst[x * 4 + 3] = mask;
//             dst[x * 4 + 2] = dst[x * 3 + 2];
//             dst[x * 4 + 1] = dst[x * 3 + 1];
//             dst[x * 4 + 0] = dst[x * 3 + 0];
//         }
//         scanline += bpl;
//     }
// }

// void LibTiff::rgb96fixup(QImage *image)
// {
//     Q_ASSERT(image->depth() == 128);
//     const int h = image->height();
//     const int w = image->width();
//     uchar *scanline = image->bits();
//     const qsizetype bpl = image->bytesPerLine();
//     for (int y = 0; y < h; ++y) {
//         float *dst = reinterpret_cast<float *>(scanline);
//         for (int x = w - 1; x >= 0; --x) {
//             dst[x * 4 + 3] = 1.0f;
//             dst[x * 4 + 2] = dst[x * 3 + 2];
//             dst[x * 4 + 1] = dst[x * 3 + 1];
//             dst[x * 4 + 0] = dst[x * 3 + 0];
//         }
//         scanline += bpl;
//     }
// }

// void LibTiff::rgbFixup(QImage *image)
// {
//     // Q_ASSERT(floatingPoint);
//     if (image->depth() == 64) {
//         const int h = image->height();
//         const int w = image->width();
//         uchar *scanline = image->bits();
//         const qsizetype bpl = image->bytesPerLine();
//         for (int y = 0; y < h; ++y) {
//             qfloat16 *dst = reinterpret_cast<qfloat16 *>(scanline);
//             for (int x = w - 1; x >= 0; --x) {
//                 dst[x * 4 + 3] = qfloat16(1.0f);
//                 dst[x * 4 + 2] = dst[x];
//                 dst[x * 4 + 1] = dst[x];
//                 dst[x * 4 + 0] = dst[x];
//             }
//             scanline += bpl;
//         }
//     } else {
//         const int h = image->height();
//         const int w = image->width();
//         uchar *scanline = image->bits();
//         const qsizetype bpl = image->bytesPerLine();
//         for (int y = 0; y < h; ++y) {
//             float *dst = reinterpret_cast<float *>(scanline);
//             for (int x = w - 1; x >= 0; --x) {
//                 dst[x * 4 + 3] = 1.0f;
//                 dst[x * 4 + 2] = dst[x];
//                 dst[x * 4 + 1] = dst[x];
//                 dst[x * 4 + 0] = dst[x];
//             }
//             scanline += bpl;
//         }
//     }
// }

// bool LibTiff::readHeaders(TIFF *tiff, QSize &size, QImage::Format &format,
//                        uint16_t &photometric, bool &grayscale, bool &floatingPoint,
//                        QImageIOHandler::Transformations transformation)
// {
//     /*
//     From QTiffHandler.cpp to assign key tiff fields and assign the right QImage format
// */
//     uint32_t width;
//     uint32_t height;
//     if (!TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width)
//         || !TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height)
//         || !TIFFGetField(tiff, TIFFTAG_PHOTOMETRIC, &photometric)) {
//         return false;
//     }
//     size = QSize(width, height);

//     uint16_t orientationTag;
//     if (TIFFGetField(tiff, TIFFTAG_ORIENTATION, &orientationTag))
//         transformation = exif2Qt(orientationTag);

//     // BitsPerSample defaults to 1 according to the TIFF spec.
//     uint16_t bitPerSample;
//     if (!TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bitPerSample))
//         bitPerSample = 1;
//     uint16_t samplesPerPixel; // they may be e.g. grayscale with 2 samples per pixel
//     if (!TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel))
//         samplesPerPixel = 1;
//     uint16_t sampleFormat;
//     if (!TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat))
//         sampleFormat = SAMPLEFORMAT_VOID;
//     floatingPoint = (sampleFormat == SAMPLEFORMAT_IEEEFP);

//     grayscale = photometric == PHOTOMETRIC_MINISBLACK || photometric == PHOTOMETRIC_MINISWHITE;

//     if (grayscale && bitPerSample == 1 && samplesPerPixel == 1)
//         format = QImage::Format_Mono;
//     else if (photometric == PHOTOMETRIC_MINISBLACK && bitPerSample == 8 && samplesPerPixel == 1)
//         format = QImage::Format_Grayscale8;
//     else if (photometric == PHOTOMETRIC_MINISBLACK && bitPerSample == 16 && samplesPerPixel == 1 && !floatingPoint)
//         format = QImage::Format_Grayscale16;
//     else if ((grayscale || photometric == PHOTOMETRIC_PALETTE) && bitPerSample == 8 && samplesPerPixel == 1)
//         format = QImage::Format_Indexed8;
//     else if (samplesPerPixel < 4)
//         if (bitPerSample == 16 && (photometric == PHOTOMETRIC_RGB || photometric == PHOTOMETRIC_MINISBLACK))
//             format = floatingPoint ? QImage::Format_RGBX16FPx4 : QImage::Format_RGBX64;
//         else if (bitPerSample == 32 && floatingPoint && (photometric == PHOTOMETRIC_RGB || photometric == PHOTOMETRIC_MINISBLACK))
//             format = QImage::Format_RGBX32FPx4;
//         else
//             format = QImage::Format_RGB32;
//     else {
//         uint16_t count;
//         uint16_t *extrasamples;
//         // If there is any definition of the alpha-channel, libtiff will return premultiplied
//         // data to us. If there is none, libtiff will not touch it and  we assume it to be
//         // non-premultiplied, matching behavior of tested image editors, and how older Qt
//         // versions used to save it.
//         bool premultiplied = true;
//         bool gotField = TIFFGetField(tiff, TIFFTAG_EXTRASAMPLES, &count, &extrasamples);
//         if (!gotField || !count || extrasamples[0] == EXTRASAMPLE_UNSPECIFIED)
//             premultiplied = false;

//         if (bitPerSample == 16 && photometric == PHOTOMETRIC_RGB) {
//             // We read 64-bit raw, so unassoc remains unpremultiplied.
//             if (gotField && count && extrasamples[0] == EXTRASAMPLE_UNASSALPHA)
//                 premultiplied = false;
//             if (premultiplied)
//                 format = floatingPoint ? QImage::Format_RGBA16FPx4_Premultiplied : QImage::Format_RGBA64_Premultiplied;
//             else
//                 format = floatingPoint ? QImage::Format_RGBA16FPx4 : QImage::Format_RGBA64;
//         } else if (bitPerSample == 32 && floatingPoint && photometric == PHOTOMETRIC_RGB) {
//             if (gotField && count && extrasamples[0] == EXTRASAMPLE_UNASSALPHA)
//                 premultiplied = false;
//             if (premultiplied)
//                 format = QImage::Format_RGBA32FPx4_Premultiplied;
//             else
//                 format = QImage::Format_RGBA32FPx4;
//         } else {
//             if (premultiplied)
//                 format = QImage::Format_ARGB32_Premultiplied;
//             else
//                 format = QImage::Format_ARGB32;
//         }
//     }

//     return true;
// }

// bool LibTiff::read(QString fPath, QImage *image, quint32 ifdOffset)
// {
//     TIFF *tiff = TIFFOpen(fPath.toStdString().c_str(), "r");
//     if (!tiff) {
//         qDebug() << "LibTiff::read Failed to open TIFF file." << fPath;
//         return false;
//     }

//     // check if another directory offset
//     if (ifdOffset) {
//         TIFFSetSubDirectory(tiff, ifdOffset);
//     }

//     QSize size;
//     QImage::Format format;
//     QImageIOHandler::Transformations transformation;
//     uint16_t photometric;
//     bool grayscale;
//     bool floatingPoint;

//     if (!readHeaders(tiff, size, format, photometric, grayscale, floatingPoint, transformation)) {
//         qDebug() << "LibTiff::read Failed to read headers." << fPath;
//         return false;
//     }

//     if (!QImageIOHandler::allocateImage(size, format, image)) {
//         TIFFClose(tiff); tiff = nullptr;
//         qDebug() << "LibTiff::read Failed to QImageIOHandler::allocateImage." << fPath;
//         return false;
//     }

//     if (TIFFIsTiled(tiff) && TIFFTileSize64(tiff) > uint64_t(image->sizeInBytes())) {// Corrupt image
//         TIFFClose(tiff); tiff = nullptr;
//         qDebug() << "LibTiff::read Corrupt image." << fPath;
//         return false;
//     }

//     const quint32 width = size.width();
//     const quint32 height = size.height();

//     // Setup color tables
//     if (format == QImage::Format_Mono || format == QImage::Format_Indexed8) {
//         if (format == QImage::Format_Mono) {
//             QList<QRgb> colortable(2);
//             if (photometric == PHOTOMETRIC_MINISBLACK) {
//                 colortable[0] = 0xff000000;
//                 colortable[1] = 0xffffffff;
//             } else {
//                 colortable[0] = 0xffffffff;
//                 colortable[1] = 0xff000000;
//             }
//             image->setColorTable(colortable);
//         } else if (format == QImage::Format_Indexed8) {
//             const uint16_t tableSize = 256;
//             QList<QRgb> qtColorTable(tableSize);
//             if (grayscale) {
//                 for (int i = 0; i<tableSize; ++i) {
//                     const int c = (photometric == PHOTOMETRIC_MINISBLACK) ? i : (255 - i);
//                     qtColorTable[i] = qRgb(c, c, c);
//                 }
//             } else {
//                 // create the color table
//                 uint16_t *redTable = 0;
//                 uint16_t *greenTable = 0;
//                 uint16_t *blueTable = 0;
//                 if (!TIFFGetField(tiff, TIFFTAG_COLORMAP, &redTable, &greenTable, &blueTable)) {
//                     TIFFClose(tiff); tiff = nullptr;
//                     qDebug() << "LibTiff::read Failed to get field TIFFTAG_COLORMAP." << fPath;
//                     return false;
//                 }
//                 if (!redTable || !greenTable || !blueTable) {
//                     TIFFClose(tiff); tiff = nullptr;
//                     return false;
//                 }

//                 for (int i = 0; i<tableSize ;++i) {
//                     // emulate libtiff behavior for 16->8 bit color map conversion: just ignore the lower 8 bits
//                     const int red = redTable[i] >> 8;
//                     const int green = greenTable[i] >> 8;
//                     const int blue = blueTable[i] >> 8;
//                     qtColorTable[i] = qRgb(red, green, blue);
//                 }
//             }
//             image->setColorTable(qtColorTable);
//             // free redTable, greenTable and greenTable done by libtiff
//         }
//     }
//     bool format8bit = (format == QImage::Format_Mono || format == QImage::Format_Indexed8 || format == QImage::Format_Grayscale8);
//     bool format16bit = (format == QImage::Format_Grayscale16);
//     bool format64bit = (format == QImage::Format_RGBX64 || format == QImage::Format_RGBA64 || format == QImage::Format_RGBA64_Premultiplied);
//     bool format64fp = (format == QImage::Format_RGBX16FPx4 || format == QImage::Format_RGBA16FPx4 || format == QImage::Format_RGBA16FPx4_Premultiplied);
//     bool format128fp = (format == QImage::Format_RGBX32FPx4 || format == QImage::Format_RGBA32FPx4 || format == QImage::Format_RGBA32FPx4_Premultiplied);

//     // Formats we read directly, instead of over RGBA32:
//     if (format8bit || format16bit || format64bit || format64fp || format128fp) {
//         int bytesPerPixel = image->depth() / 8;
//         if (format == QImage::Format_RGBX64 || format == QImage::Format_RGBX16FPx4)
//             bytesPerPixel = photometric == PHOTOMETRIC_RGB ? 6 : 2;
//         else if (format == QImage::Format_RGBX32FPx4)
//             bytesPerPixel = photometric == PHOTOMETRIC_RGB ? 12 : 4;
//         if (TIFFIsTiled(tiff)) {
//             quint32 tileWidth, tileLength;
//             TIFFGetField(tiff, TIFFTAG_TILEWIDTH, &tileWidth);
//             TIFFGetField(tiff, TIFFTAG_TILELENGTH, &tileLength);
//             if (!tileWidth || !tileLength || tileWidth % 16 || tileLength % 16) {
//                 TIFFClose(tiff); tiff = nullptr;
//                 return false;
//             }
//             quint32 byteWidth = (format == QImage::Format_Mono) ? (width + 7)/8 : (width * bytesPerPixel);
//             quint32 byteTileWidth = (format == QImage::Format_Mono) ? tileWidth/8 : (tileWidth * bytesPerPixel);
//             tmsize_t byteTileSize = TIFFTileSize(tiff);
//             if (byteTileSize > image->sizeInBytes() || byteTileSize / tileLength < byteTileWidth) {
//                 TIFFClose(tiff); tiff = nullptr;
//                 return false;
//             }
//             uchar *buf = (uchar *)_TIFFmalloc(byteTileSize);
//             if (!buf) {
//                 TIFFClose(tiff); tiff = nullptr;
//                 return false;
//             }
//             for (quint32 y = 0; y < height; y += tileLength) {
//                 for (quint32 x = 0; x < width; x += tileWidth) {
//                     if (TIFFReadTile(tiff, buf, x, y, 0, 0) < 0) {
//                         _TIFFfree(buf);
//                         TIFFClose(tiff); tiff = nullptr;
//                         return false;
//                     }
//                     quint32 linesToCopy = qMin(tileLength, height - y);
//                     quint32 byteOffset = (format == QImage::Format_Mono) ? x/8 : (x * bytesPerPixel);
//                     quint32 widthToCopy = qMin(byteTileWidth, byteWidth - byteOffset);
//                     for (quint32 i = 0; i < linesToCopy; i++) {
//                         ::memcpy(image->scanLine(y + i) + byteOffset, buf + (i * byteTileWidth), widthToCopy);
//                     }
//                 }
//             }
//             _TIFFfree(buf);
//         } else {
//             if (image->bytesPerLine() < TIFFScanlineSize(tiff)) {
//                 TIFFClose(tiff); tiff = nullptr;
//                 return false;
//             }
//             for (uint32_t y=0; y<height; ++y) {
//                 if (TIFFReadScanline(tiff, image->scanLine(y), y, 0) < 0) {
//                     TIFFClose(tiff); tiff = nullptr;
//                     return false;
//                 }
//             }
//         }
//         if (format == QImage::Format_RGBX64 || format == QImage::Format_RGBX16FPx4) {
//             if (photometric == PHOTOMETRIC_RGB)
//                 rgb48fixup(image, floatingPoint);
//             else if (floatingPoint)
//                 rgbFixup(image);
//         } else if (format == QImage::Format_RGBX32FPx4) {
//             if (photometric == PHOTOMETRIC_RGB)
//                 rgb96fixup(image);
//             else if (floatingPoint)
//                 rgbFixup(image);
//         }
//     } else {
//         const int stopOnError = 1;
//         if (TIFFReadRGBAImageOriented(tiff, width, height, reinterpret_cast<uint32_t *>(image->bits()), qt2Exif(transformation), stopOnError)) {
//             for (uint32_t y=0; y<height; ++y)
//                 convert32BitOrder(image->scanLine(y), width);
//             // qDebug() << "LibTiff::read Succeeded: TIFFReadRGBAImageOriented." << filePath;
//         } else {
//             TIFFClose(tiff); tiff = nullptr;
//             // qDebug() << "LibTiff::read Failed to TIFFReadRGBAImageOriented." << filePath;
//             return false;
//         }
//     }


//     float resX = 0;
//     float resY = 0;
//     uint16_t resUnit;
//     if (!TIFFGetField(tiff, TIFFTAG_RESOLUTIONUNIT, &resUnit))
//         resUnit = RESUNIT_INCH;

//     if (TIFFGetField(tiff, TIFFTAG_XRESOLUTION, &resX)
//         && TIFFGetField(tiff, TIFFTAG_YRESOLUTION, &resY)) {

//         switch(resUnit) {
//         case RESUNIT_CENTIMETER:
//             image->setDotsPerMeterX(qRound(resX * 100));
//             image->setDotsPerMeterY(qRound(resY * 100));
//             break;
//         case RESUNIT_INCH:
//             image->setDotsPerMeterX(qRound(resX * (100 / 2.54)));
//             image->setDotsPerMeterY(qRound(resY * (100 / 2.54)));
//             break;
//         default:
//             // do nothing as defaults have already
//             // been set within the QImage class
//             break;
//         }
//     }

//     uint32_t count;
//     void *profile;
//     if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &count, &profile)) {
//         QByteArray iccProfile(reinterpret_cast<const char *>(profile), count);
//         image->setColorSpace(QColorSpace::fromIccProfile(iccProfile));
//     }
//     // We do not handle colorimetric metadat not on ICC profile form, it seems to be a lot
//     // less common, and would need additional API in QColorSpace.

//     return true;
// }
