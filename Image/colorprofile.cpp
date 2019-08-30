/*
   This code was created by Qt and then abandoned
*/

#include "image/colorprofile.h"
#include "image/colorprofile_p.h"
#include "qshareddata.h"
#include "qiodevice.h"
#include "lcms2.h"
#include "qfile.h"

QT_BEGIN_NAMESPACE

#define _SUPPORTED_QIMAGE_FORMATS (QImage::Format_RGB32 | QImage::Format_ARGB32 |\
                                   QImage::Format_ARGB32_Premultiplied | QImage::Format_RGB888)


cmsUInt32Number qt_format_to_littleCMS(QImage::Format f)
{
    switch (f) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        return TYPE_BGRA_8;
    case QImage::Format_RGB888:
        return TYPE_RGB_8;
    default:
        return 0xDEAD;
    }
}

bool QColorProfilePrivate::applyTo(QImage *img) const
{
    QColorProfile prof = img->colorProfile();
    const QColorProfilePrivate *colorProfilePrivate = QColorProfilePrivate::get(&prof);
    cmsHTRANSFORM transformHandler = 0;
    cmsHPROFILE srcProfile = colorProfilePrivate->profileHandler;

    if (!_SUPPORTED_QIMAGE_FORMATS)
        *img = img->convertToFormat(QImage::Format_ARGB32_Premultiplied);

    transformHandler = cmsCreateTransform( srcProfile,
                                          qt_format_to_littleCMS(img->format()),
                                          profileHandler,
                                          qt_format_to_littleCMS(img->format()),
                                          INTENT_PERCEPTUAL, 0);

    if (!transformHandler)
        return false;

    cmsDoTransform(transformHandler, img->constBits(),
                   img->bits(), img->width()*img->height());
    cmsDeleteTransform(transformHandler);

    return true;
}

bool QColorProfilePrivate::applyTo(QColorProfile &src, QColor *c) const
{

    const QColorProfilePrivate *colorProfilePrivate = QColorProfilePrivate::get(&src);
    cmsHTRANSFORM transformHandler = 0;
    cmsHPROFILE srcProfile = colorProfilePrivate->profileHandler;

    transformHandler = cmsCreateTransform( srcProfile,
                                          TYPE_BGRA_8,
                                          profileHandler,
                                          TYPE_BGRA_8,
                                          INTENT_PERCEPTUAL, 0);

    if (!transformHandler)
        return false;

    QRgb rgb = c->rgba();
    cmsDoTransform(transformHandler, &rgb, &rgb, 1);
    cmsDeleteTransform(transformHandler);

    *c = QColor::fromRgba(rgb);

    return true;
}

void QColorProfilePrivate::makesRGB()
{
    profileHandler = cmsCreate_sRGBProfile();
}

void QColorProfilePrivate::init()
{
    profileHandler = 0;
}

QColorProfilePrivate::QColorProfilePrivate()
{
    init();
    makesRGB();
    cmsMD5computeID(profileHandler);
}

QColorProfilePrivate::QColorProfilePrivate(QColorProfile::ColorProfileName profileName)
{
    init();

    if (profileName == QColorProfile::sRGB)
        makesRGB();

    cmsMD5computeID(profileHandler);
}

QColorProfilePrivate::QColorProfilePrivate(const QColorProfilePrivate &other)
    : QSharedData(other)
{
}

QColorProfilePrivate::QColorProfilePrivate(const QString &fileName)
{
    init();
    if (fileName.isEmpty()) {
        makesRGB();
    }
    else {
        QFile file(fileName);
        load(&file);
    }
}

QColorProfilePrivate::QColorProfilePrivate(const QByteArray &b)
{
    init();
    if (b.isEmpty()) {
        makesRGB();
    }
    else {
     load(b);
    }
}

bool QColorProfilePrivate::load(QIODevice *device)
{
    device->open(QIODevice::ReadOnly);
    bool res = load(device->readAll());
    device->close();

    return res;
}

bool QColorProfilePrivate::load(const QByteArray &bin)
{
    cleanup();
    profileHandler = cmsOpenProfileFromMem(bin.data(), bin.size());

    if (!profileHandler)
        return false;

    cmsMD5computeID(profileHandler);

    return true;
}

void QColorProfilePrivate::cleanup()
{
    if (profileHandler) {
        cmsCloseProfile(profileHandler);
        profileHandler = 0;
    }
}

QColorProfilePrivate::~QColorProfilePrivate()
{
    cleanup();
}


/*!
    \class QColorProfile
    \since 5.1.0
    \brief The QColorProfile class provides an abstraction of the ICC (International
    Color Consirtum) color profile standard.

    \ingroup painting
    \ingroup shared


    This class allows importing ICC color profiles to Qt. These color profiles
    can be provided as file paths or raw memory addresses holding the profile's
    binary data. This class is used by \a QImage, which automatically grabs the
    embedded color profiles of JPEG and PNG images.
*/

/*!
    \enum QColorProfile::ColorSpace
    \since 5.1.0

    This enum type describes the color space of a pixmap.

    \value RGB
         The most common color space using Red Green Blue alignment
    \value CMYK
         The printing color space using Cyan, Magenta, Yellow and Black
    \value Unsupported
         This should not happen but gets returned if the color format of
         the color profile is neither RGB nor CMYK
*/

/*!
    \enum QColorProfile::ColorProfileName
    \since 5.1.0

    This enum type is used for pre-defined color profiles such as sRGB.

    \value sRGB
         The most common color profile used on devices and image formats
*/

/*!
    Constructs a QColorProfile with the sRGB default color profile
    \since 5.1.0
*/
QColorProfile::QColorProfile()
    : d(new QColorProfilePrivate)
{
}

/*!
    Constructs a QColorProfile from the ColorProfileName enum specified

    \since 5.1.0
*/
QColorProfile::QColorProfile(ColorProfileName profileName)
    : d(new QColorProfilePrivate(profileName))
{

}

/*!
    Constructs a color profile from the file path specified
    \since 5.1.0

    \note If the file is not found, an sRGB profile is created
*/
QColorProfile::QColorProfile(const QString &fileName)
    : d(new QColorProfilePrivate(fileName))
{
}

/*!
    Constructs a color profile from the file path specified
    \since 5.1.0

    \note If the file is not found, an sRGB profile is created
*/
QColorProfile::QColorProfile(const char *fileName)
    : d(new QColorProfilePrivate(QString(fileName)))
{
}

/*!
    Constructs a deep copy of the given \a colorProfile.
    \since 5.1.0
*/
QColorProfile::QColorProfile(const QColorProfile &colorProfile)
    : d(colorProfile.d)
{

}

/*!
    Constructs a color profile from the byte array \a b.
    \since 5.1.0

    \note If the array is empty or invalid, an sRGB profile is created
*/
QColorProfile::QColorProfile(const QByteArray &b)
    : d(new QColorProfilePrivate(b))
{
}

/*!
    Constructs a shallow copy of the given \a other.
    \since 5.1.0
*/
QColorProfile &QColorProfile::operator=(const QColorProfile &other)
{
    if (this != &other)
    {
        d = other.d;
    }

    return *this;
}

/*!
    Compares for equality the properties of two color profiles.
    \since 5.1.0
*/
bool QColorProfile::operator==(const QColorProfile &other) const
{

    if (!d->profileHandler || !other.d->profileHandler)
        return false;

    return (d == other.d) || (d->id() == other.d->id());
}

/*!
    Compares for inequality the properties of two color profiles.
    \since 5.1.0
*/
bool QColorProfile::operator!=(const QColorProfile &other) const
{

    if (!d->profileHandler || !other.d->profileHandler)
        return true;

    return (d != other.d) || (d->id() != other.d->id());
}

/*!
    Destroys the color profile and cleans up.
    \since 5.1.0
*/
QColorProfile::~QColorProfile()
{
}

/*!
    Returns the version of the profile
    \since 5.1.0
*/
qreal QColorProfile::version() const
{
    if (!d->profileHandler)
        return -1;

    return cmsGetProfileVersion(d->profileHandler);
}

/*!
    Returns the description (usually the name) of a color profile
    \since 5.1.0
*/
QString QColorProfile::description() const
{
    if (!d->profileHandler)
        return QLatin1String("");

    char tmp_str[255];
    cmsGetProfileInfoASCII(d->profileHandler, cmsInfoDescription, "en", "en", tmp_str, 255);

    return QLatin1String(tmp_str);
}


/*!
    Returns the Colorspace of the profile \a QColorProfile::ColorSpace
    \since 5.1.0
*/
QColorProfile::ColorSpace QColorProfile::colorSpace() const
{
    if (!d->profileHandler)
        return Unsupported;

    cmsColorSpaceSignature cs = cmsGetColorSpace(d->profileHandler);

    if (cs == cmsSigCmykData) {
        return CMYK;
    }
    else if (cs == cmsSigRgbData) {
        return RGB;
    }

    return Unsupported;
}

/*!
    Returns a copy of the binary data of the color profile.
    \since 5.1.0
*/
QByteArray QColorProfile::data() const
{
    return d->data();
}

/*!
    Transforms the image's data to this color profile.
    \warning This method will modify the image's internal buffer

    Returns true if the conversion succeeded
    \since 5.1.0
*/
bool QColorProfile::convertToColorProfile(QImage *img) const
{
    if (!d->applyTo(img))
        return false;

    // Since the colorspace representing the pixel buffer has been converted
    // we need to explicitly set it
    img->setColorProfile(*this);

    return true;
}

/*!
    Converts \a color from \a srcProfile to \a dstProfile and returns a new instance of color
    \since 5.1.0
*/
QColor QColorProfile::convertToColorProfile(QColorProfile &srcProfile, QColorProfile &dstProfile, const QColor &color)
{
    QColor result = color;

    dstProfile.d->applyTo(srcProfile, &result);

    return result;
}

QT_END_NAMESPACE
