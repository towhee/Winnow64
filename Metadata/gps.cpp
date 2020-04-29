#include "gps.h"

GPS::GPS()
{
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
