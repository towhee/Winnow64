#include "icc.h"

namespace ICC
{

    cmsHPROFILE hInProfile;
    cmsHPROFILE hOutProfile;
    cmsHTRANSFORM hTransform;

    void setInProfile(const QByteArray &buf)
    {
        {
        #ifdef ISDEBUG
        G::track(__FUNCTION__);
        #endif
        }
        if (buf == nullptr) {
//            qDebug() << __FUNCTION__ << "null buffer = sRGB";
            hInProfile = cmsCreate_sRGBProfile();
        }
        else {
            hInProfile = cmsOpenProfileFromMem (buf.data(), static_cast<uint32_t>(buf.length()));
        }
    }

    void setOutProfile()
    {
        {
        #ifdef ISDEBUG
        G::track(__FUNCTION__);
        #endif
        }
        hOutProfile = cmsOpenProfileFromFile(QFile::encodeName(G::winOutProfilePath).constData(), "r") ;
    }

    void transform(QImage &image)
    {
        {
        #ifdef ISDEBUG
        G::track(__FUNCTION__);
        #endif
        }
        hTransform = cmsCreateTransform(hInProfile,
                                        TYPE_BGRA_8,
                                        hOutProfile,
                                        TYPE_BGRA_8,
                                        INTENT_PERCEPTUAL, 0);
        if (hTransform) {
            cmsDoTransform(hTransform, image.constBits(), image.bits(),
                 static_cast<uint>(image.width()*image.height()));
        }
    }
}
