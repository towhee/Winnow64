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
    hash[7] = "GPSTimeStamp";
    hash[8] = "GPSSatellites";
    hash[9] = "GPSStatus";
    hash[10] = "GPSMeasureMode";
    hash[11] = "GPSDOP";
    hash[12] = "GPSSpeedRef";
    hash[13] = "GPSSpeed";
    hash[14] = "GPSTrackRef";
    hash[15] = "GPSTrack";
    hash[16] = "GPSImgDirectionRef";
    hash[17] = "GPSImgDirection";
    hash[18] = "GPSMapDatum";
    hash[19] = "GPSDestLatitudeRef";
    hash[20] = "GPSDestLatitude";
    hash[21] = "GPSDestLongitudeRef";
    hash[22] = "GPSDestLongitude";
    hash[23] = "GPSDestBearingRef";
    hash[24] = "GPSDestBearing";
    hash[25] = "GPSDestBearingRef";
    hash[26] = "GPSDestBearing";
    hash[27] = "GPSDestDistanceRef";
    hash[28] = "GPSDistance";
    hash[29] = "GPSDateStamp";
    hash[31] = "GPSHPositioningError";
}

QString GPS::decode(QFile &file,
                    QHash<uint, IFDData> &ifdDataHash,
                    bool isBigEnd,
                    int offset)
{
    // process GPS info
    QString gpsCoord = "";
    QString degreeSymbol = "°";
    QString minuteSymbol = "'";
    QString secondSymbol = QChar('\"');

    // GPS lat ref
    int gpsLatOffset = ifdDataHash.value(2).tagValue + offset;
    qDebug() << "GPS::decode" << "gpsLatOffset =" << "gpsLatOffset";
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
    quint32 v = ifdDataHash.value(1).tagValue;
    if (v >= 65536) return "Error";
    QString gpsLatRef = QChar(v);
    gpsCoord += " " + gpsLatRef + " ";

    // GPS long ref
    int gpsLongOffset = ifdDataHash.value(4).tagValue + offset;
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
    v = ifdDataHash.value(3).tagValue;
    if (v >= 65536) return "Error";
    QString gpsLongRef = QChar(v);
    gpsCoord += " " + gpsLongRef;

    return gpsCoord;
}
