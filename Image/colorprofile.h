#ifndef COLORPROFILE_H
#define COLORPROFILE_H

/*
   This code was created by Qt and then abandoned
*/

#include "string.h"
#include "qcolor.h"
#include <QSharedData>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

class QColorProfilePrivate;
class QIODevice;
class QImage;

class Q_GUI_EXPORT QColorProfile
{
public:
    enum ColorSpace
    {
        RGB,
        CMYK,
        Unsupported
    };

    enum ColorProfileName
    {
        sRGB
    };

    QColorProfile();
    QColorProfile(ColorProfileName profileName);
    QColorProfile(const char *fileName);
    QColorProfile(const QString &fileName);
    QColorProfile(const QColorProfile &colorProfile);
    QColorProfile(const QByteArray &b);

    QColorProfile &operator=(const QColorProfile &other);
    bool operator==(const QColorProfile &other) const;
    bool operator!=(const QColorProfile &other) const;

    qreal version() const;
    QString description() const;
    ColorSpace colorSpace() const;

    QByteArray data() const;

    bool convertToColorProfile(QImage *img) const;
    static QColor convertToColorProfile(QColorProfile &srcProfile, QColorProfile &dstProfile, const QColor &color);

    virtual ~QColorProfile();

private:
    QSharedDataPointer<QColorProfilePrivate> d;
    friend class QColorProfilePrivate;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // COLORPROFILE_H
