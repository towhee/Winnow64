#ifndef JPEGTURBO_H
#define JPEGTURBO_H

#include <QtWidgets>
#include <QObject>
#include "Main/global.h"
// #include <turbojpeg.h>
#include "/opt/homebrew/Cellar/jpeg-turbo/3.0.4/include/turbojpeg.h"

/*
    libjpeg-turbo is the fastest jpeg library I have found, frequently 30-50%
    faster than the jpeg library used by Qt.  It has excellent documentation.

    https://github.com/libjpeg-turbo/libjpeg-turbo
    https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/libjpeg.txt
    https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/structure.txt
    https://libjpeg-turbo.org/
*/

class JpegTurbo : public QObject
{
    Q_OBJECT
public:
    JpegTurbo();
    QImage decode(QString &fPath);
    QImage decode(QByteArray &ba);
};

#endif // JPEGTURBO_H
