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
