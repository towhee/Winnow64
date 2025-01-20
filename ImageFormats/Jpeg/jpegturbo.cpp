#include "jpegturbo.h"
#include "Main/global.h"

JpegTurbo::JpegTurbo() {}

QImage JpegTurbo::decode(QString &fPath)
{
/*
    libjpeg-turbo library
*/
    // Read JPEG file into memory
    QFile file(fPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QString msg = "Failed to open JPEG file";
        G::issue("Warning", msg, "JpegTurbo::decode(fPath)", -1, fPath);
        return QImage();
    }

    QByteArray ba = file.readAll();
    file.close();

    return decode(ba);
}

QImage JpegTurbo::decode(QByteArray &ba)
{
    /*
    libjpeg-turbo library
*/
    // Initialize TurboJPEG decompressor
    tjhandle tjInstance = tjInitDecompress();
    if (!tjInstance) {
        QString msg = "Failed to initialize TurboJPEG: " + QString(tjGetErrorStr());
        G::issue("Warning", msg, "JpegTurbo::decode(fPath)", -1);
        return QImage();
    }

    // Get image dimensions and colorspace
    int width, height, jpegSubsamp, jpegColorspace;
    if (tjDecompressHeader3(tjInstance, (unsigned char*)ba.data(), ba.size(),
                            &width, &height, &jpegSubsamp, &jpegColorspace) != 0) {
        QString msg = "Failed to read JPEG header: " + QString(tjGetErrorStr());
        G::issue("Warning", msg, "JpegTurbo::decode(fPath)", -1);
        tjDestroy(tjInstance);
        return QImage();
    }

    // Allocate memory for the decompressed image
    int pixelFormat = TJPF_RGB;
    int pitch = width * tjPixelSize[pixelFormat];
    unsigned char* imgBuffer = (unsigned char*)tjAlloc(pitch * height);
    if (!imgBuffer) {
        QString msg = "Failed to allocate memory for decompressed image.";
        G::issue("Warning", msg, "JpegTurbo::decode(fPath)", -1);
        tjDestroy(tjInstance);
        return QImage();
    }

    /*
    qDebug() << "JpegTurbo::decode1(QString)"
             << "width = " << width
             << "height =" << height
             << "pitch =" << pitch
             << "jpegSubsamp = " << jpegSubsamp
             << "jpegColorspace = " << jpegColorspace
        ; //*/

    // Decompress the JPEG image
    if (tjDecompress2(tjInstance, (unsigned char*)ba.data(), ba.size(), imgBuffer,
                      width, pitch, height, pixelFormat, 0) != 0) {
        QString msg = "Failed to decompress JPEG image: " + QString(tjGetErrorStr());
        G::issue("Warning", msg, "JpegTurbo::decode(fPath)", -1);
        tjFree(imgBuffer);
        tjDestroy(tjInstance);
        return QImage();
    }

    // Create QImage from the decompressed data
    QImage img(imgBuffer, width, height, pitch, QImage::Format_RGB888);
    QImage finalImage = img.copy(); // Make a deep copy to take ownership of the buffer
    finalImage.convertTo(QImage::Format_ARGB32);

    // Cleanup
    tjFree(imgBuffer);
    tjDestroy(tjInstance);

    return finalImage;
}
