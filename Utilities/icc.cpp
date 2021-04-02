#include "icc.h"

namespace ICC
{

    cmsHPROFILE hInProfile;
    cmsHPROFILE hOutProfile;
    cmsHTRANSFORM hTransform;

    void setInProfile(const QByteArray &buf)
    {
        if (G::isLogger) G::log(__FUNCTION__);
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
        if (G::isLogger) G::log(__FUNCTION__);
        hOutProfile = cmsOpenProfileFromFile(QFile::encodeName(G::winOutProfilePath).constData(), "r") ;
    }

    void transform(QImage &image)
    {
        if (G::isLogger) G::log(__FUNCTION__);
        hTransform = cmsCreateTransform(hInProfile,
                                        TYPE_BGRA_8,
                                        hOutProfile,
                                        TYPE_BGRA_8,
                                        INTENT_PERCEPTUAL, 0);
        if (hTransform) {
            cmsDoTransform(hTransform, image.constBits(), image.bits(),
                 static_cast<uint>(image.width()*image.height()));
        }
        else {
            qDebug() << __FUNCTION__ << "ICC hTransform failed.";
//            Utilities::log
        }
    }
}
