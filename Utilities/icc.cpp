#include "icc.h"

namespace ICC
{

    cmsHPROFILE hOutProfile;

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
            if (!hInProfile) {
                qWarning() << __FUNCTION__ << "ICC hInProfile failed.";
                return;
            }
        }

        cmsHTRANSFORM hTransform;
        hTransform = cmsCreateTransform(hInProfile,
                                        TYPE_BGRA_8,
                                        hOutProfile,
                                        TYPE_BGRA_8,
                                        INTENT_PERCEPTUAL, 0);
        if (hTransform) {
            /* another way to get size
            uchar bitsPerPixel = image.pixelFormat().bitsPerPixel();
            quint32 size = static_cast<quint32>(image.width()*image.height()*bitsPerPixel/32);
            //*/
            quint32 size = static_cast<quint32>(image.height()*image.bytesPerLine()/4);
            cmsDoTransform(hTransform, image.constBits(), image.bits(), size);
            if (!cmsCloseProfile(hInProfile)) {
                qWarning() << __FUNCTION__ << "ICC cmsCloseProfile failed.";
            }
            cmsDeleteTransform(hTransform);
        }
        else {
            qWarning() << __FUNCTION__ << "ICC cmsCreateTransform failed.";
    //            Utilities::log
        }
    }
}
