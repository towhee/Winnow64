/*
    WHEN COPY NEW C CODE FROM LCMS2 AT https://github.com/mm2/Little-CMS

    1. Copy .c .h files from src and include to Lcms2 in Winnow64

    2. In lcms2.h uncomment as per:

        // Uncomment this to remove the "register" storage class
        // #define CMS_NO_REGISTER_KEYWORD 1

*/

#ifndef ICC_H
#define ICC_H

#include <QtWidgets>
#include "Lcms2/lcms2.h"                // little color management system

namespace ICC
{
    extern bool setOutProfile();
    extern void transform(const QByteArray &buf, QImage &image);
    extern cmsHPROFILE hOutProfile;
    extern void err(cmsContext contextID, cmsUInt32Number errorCode, const char *text);
}

#endif // ICC_H
