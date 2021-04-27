#include "icc.h"

namespace ICC
{

//    cmsHPROFILE hInProfile;
    cmsHPROFILE hOutProfile;
//    cmsHTRANSFORM hTransform;

    void setOutProfile()
    {
        if (G::isLogger) G::log(__FUNCTION__);
        hOutProfile = cmsOpenProfileFromFile(QFile::encodeName(G::winOutProfilePath).constData(), "r") ;
    }

    void transform(const QByteArray &buf, QImage &image)
    {
        if (G::isLogger) G::log(__FUNCTION__);

        cmsHPROFILE hInProfile;
        if (buf == nullptr) {
            hInProfile = cmsCreate_sRGBProfile();
        }
        else {
            hInProfile = cmsOpenProfileFromMem(buf.data(), static_cast<uint32_t>(buf.length()));
        }

        cmsHTRANSFORM hTransform;
        hTransform = cmsCreateTransform(hInProfile,
                                        TYPE_BGRA_8,
                                        hOutProfile,
                                        TYPE_BGRA_8,
                                        INTENT_PERCEPTUAL, 0);
        if (hTransform) {
            cmsDoTransform(hTransform, image.constBits(), image.bits(),
                 static_cast<uint>(image.width()*image.height()));
            cmsCloseProfile(hInProfile);
            cmsDeleteTransform(hTransform);
        }
        else {
            qWarning() << __FUNCTION__ << "ICC hTransform failed.";
//            Utilities::log
        }
    }
}
