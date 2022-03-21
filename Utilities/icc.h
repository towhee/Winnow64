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
#include "Main/global.h"
#include "Lcms2/lcms2.h"                // little color management system

//#include "Lcms2/lcms2_plugin.h"       // reqd if use code, not if use library

namespace ICC
{
    extern void setOutProfile();
    extern void transform(const QByteArray &buf, QImage &image);
    extern cmsHPROFILE hOutProfile;
}

#endif // ICC_H
