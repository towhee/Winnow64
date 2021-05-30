#ifndef ICC_H
#define ICC_H

#include <QtWidgets>
#include "Main/global.h"
#include "Lcms2/lcms2.h"                      // little color management system
#include "Lcms2/lcms2_plugin.h"

namespace ICC
{
    extern void setOutProfile();
    extern void transform(const QByteArray &buf, QImage &image);

//    extern cmsHPROFILE hInProfile;
    extern cmsHPROFILE hOutProfile;
//    extern cmsHTRANSFORM hTransform;
}

#endif // ICC_H
