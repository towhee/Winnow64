#ifndef ICC_H
#define ICC_H

#include <QtWidgets>
#include "Main/global.h"
#include "lcms2.h"                      // little color management system
#include "lcms2_fast_float.h"
#include "lcms2_internal.h"
#include "lcms2_plugin.h"
//#include "fast_float_internal.h"

namespace ICC
{
    extern void setOutProfile();
    extern void transform(const QByteArray &buf, QImage &image);

//    extern cmsHPROFILE hInProfile;
    extern cmsHPROFILE hOutProfile;
//    extern cmsHTRANSFORM hTransform;
}

#endif // ICC_H
