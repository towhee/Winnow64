#ifndef LIBTIFF_H
#define LIBTIFF_H

#include <QtWidgets>
#include <QObject>
#include "Metadata/imagemetadata.h"
#include "zlib.h"
#include <tiffio.h>    // libtiff
// used on macbookair
// #include "/opt/homebrew/Cellar/libtiff/4.7.0/include/tiffio.h"    // libtiff

class LibTiff : public QObject
{
    Q_OBJECT
public:
    explicit LibTiff(QObject *parent = nullptr);
    QImage readTiffToQImage(const QString &filePath);
    QImage testLibtiff(QString fPath, int row);
    void listDirectories(ImageMetadata &m);

    // from QTiffHandler, adapted for Winnow and using Winnow libtiff, which reads jpg encoding
    bool read(QString fPath, QImage *image, quint32 ifdOffset = 0);

private:
    struct TiffFields {
        int directory;
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
    };
    void rptFields(TiffFields &f);
    int add_jpeg_thumbnail(TIFF* tif, uint32 width, uint32 height, uint8* thumb_data, uint32 thumb_size);
};

#endif // LIBTIFF_H
