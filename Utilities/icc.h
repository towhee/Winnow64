#ifndef ICC_H
#define ICC_H

#include <QtWidgets>
#include "Main/global.h"
#include "lcms2.h"                      // little color management system


namespace ICC
{
    extern void setOutProfile();
    extern void transform(const QByteArray &buf, QImage &image);

    extern cmsHPROFILE hOutProfile;
}

#endif // ICC_H
