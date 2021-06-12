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
