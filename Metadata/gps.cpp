#include "gps.h"

GPS::GPS()
{
    hash[0] = "GPSVersionID ";
    hash[1] = "GPSLatitudeRef ";
    hash[2] = "GPSLatitude ";
    hash[3] = "GPSLongitudeRef";
    hash[4] = "GPSLongitude";
    hash[5] = "GPSAltitudeRef";
    hash[6] = "GPSAltitude";
    hash[12] = "GPSSpeedRef";
    hash[13] = "GPSSpeed";
    hash[16] = "GPSImgDirectionRef";
    hash[17] = "GPSImgDirection";
    hash[23] = "GPSDestBearingRef";
    hash[24] = "GPSDestBearing";
    hash[29] = "GPSDateStamp";
    hash[31] = "GPSHPositioningError";
}

QString GPS::decode(QFile &file,
                    QHash<uint, IFDData> &ifdDataHash,
                    bool isBigEnd,
                    int jpgOffset)
{
    // process GPS info
    QString gpsCoord = "";
    QString degreeSymbol = "°";
    QString minuteSymbol = "'";
    QString secondSymbol = QChar('\"');

    // GPS lat ref
    int gpsLatOffset = ifdDataHash.value(2).tagValue + jpgOffset;
    double gpsLatHr = u.getReal(file, gpsLatOffset, isBigEnd);
    gpsCoord += (QString::number(static_cast<int>(gpsLatHr)) + degreeSymbol);
    gpsLatOffset += 8;
    double gpsLatMin = u.getReal(file, gpsLatOffset, isBigEnd);
    if (gpsLatMin == 0) gpsLatMin = (gpsLatHr - static_cast<int>(gpsLatHr)) * 60;
    gpsCoord += (QString::number(static_cast<int>(gpsLatMin)) + minuteSymbol);
    gpsLatOffset += 8;
    double gpsLatSec = u.getReal(file, gpsLatOffset, isBigEnd);
    if (gpsLatSec == 0) gpsLatSec = (gpsLatMin - static_cast<int>(gpsLatMin)) * 60;
    gpsCoord += (QString::number(gpsLatSec, 'f', 3) + secondSymbol);
    QString gpsLatRef = QChar(ifdDataHash.value(1).tagValue);
    gpsCoord += " " + gpsLatRef + " ";

    // GPS long ref
    int gpsLongOffset = ifdDataHash.value(4).tagValue + jpgOffset;
    double gpsLongHr = u.getReal(file, gpsLongOffset, isBigEnd);
    gpsCoord += (QString::number(static_cast<int>(gpsLongHr)) + degreeSymbol);
    gpsLongOffset += 8;
    double gpsLongMin = u.getReal(file, gpsLongOffset, isBigEnd);
    if (gpsLongMin == 0) gpsLongMin = (gpsLongHr - static_cast<int>(gpsLongHr)) * 60;
    gpsCoord += (QString::number(static_cast<int>(gpsLongMin)) + minuteSymbol);
    gpsLongOffset += 8;
    double gpsLongSec = u.getReal(file, gpsLongOffset, isBigEnd);
    if (gpsLongSec == 0) gpsLongSec = (gpsLongMin- static_cast<int>(gpsLongMin)) * 60;
    gpsCoord += (QString::number(gpsLongSec, 'f', 3) + secondSymbol);
    QString gpsLongRef = QChar(ifdDataHash.value(3).tagValue);
    gpsCoord += " " + gpsLongRef;

    qDebug() << "GPS::decode" << gpsCoord;
    return gpsCoord;
}
