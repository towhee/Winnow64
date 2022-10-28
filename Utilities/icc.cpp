/*
    WHEN COPY NEW C CODE FROM LCMS2 AT https://github.com/mm2/Little-CMS

    1. Copy .c .h files from src and include to Lcms2 in Winnow64

    2. In lcms2.h uncomment as per:

        // Uncomment this to remove the "register" storage class
        // #define CMS_NO_REGISTER_KEYWORD 1

*/

#include "icc.h"

namespace ICC
{

    cmsHPROFILE hOutProfile;

    void err(cmsContext contextID, cmsUInt32Number errorCode, const char *text)
    {
        qWarning() << "WARNING" << "ICC::err" << errorCode << text << G::winOutProfilePath;
    }

    bool setOutProfile()
    {
        if (G::isLogger) G::log(__PRETTY_FUNCTION__);
        if (G::winOutProfilePath == "") {
            qWarning() << "WARNING" << "The outProfilePath has not been assigned";
            cmsSetLogErrorHandler(err);
            return false;
        }
        hOutProfile = cmsOpenProfileFromFile(QFile::encodeName(G::winOutProfilePath).constData(), "r") ;
        return true;
    }

    void transform(const QByteArray &buf, QImage &image)
    {
        if (G::isLogger) G::log(__PRETTY_FUNCTION__);

        cmsHPROFILE hInProfile;
        if (buf == nullptr) {
            hInProfile = cmsCreate_sRGBProfile();
        }
        else {
            hInProfile = cmsOpenProfileFromMem(buf.data(), static_cast<uint32_t>(buf.length()));
            if (!hInProfile) {
                qWarning() << __PRETTY_FUNCTION__ << "ICC hInProfile failed.";
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
            quint32 size = static_cast<quint32>(image.height()*image.bytesPerLine()/4);
            cmsDoTransform(hTransform, image.constBits(), image.bits(), size);
            if (!cmsCloseProfile(hInProfile)) {
                qWarning() << __PRETTY_FUNCTION__ << "ICC cmsCloseProfile failed.";
                cmsSetLogErrorHandler(err);
            }
            cmsDeleteTransform(hTransform);
        }
        else {
            qWarning() << __PRETTY_FUNCTION__
                       << "ICC cmsCreateTransform failed."
                          ;
            cmsSetLogErrorHandler(err);
        }
    }
}
