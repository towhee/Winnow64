#ifndef COLORPROFILE_P_H
#define COLORPROFILE_P_H

/*
   This code was created by Qt and then abandoned
*/

#include "qstring.h"
#include "qiodevice.h"
#include "qbytearray.h"
#include "lcms2.h"
#include "image/colorprofile.h"
#include "qhash.h"
#include <QSharedData>
#include "qdebug.h"

class QColorProfilePrivate : public QSharedData
{
public:
    QColorProfilePrivate();
    QColorProfilePrivate(QColorProfile::ColorProfileName profileName);
    QColorProfilePrivate(const QString &fileName);
    QColorProfilePrivate(const QByteArray &b);
    QColorProfilePrivate(const QColorProfilePrivate &other);

    void init();
    void makesRGB();
    void cleanup();

    bool load(QIODevice *device);
    bool load(const QByteArray &bin);

    bool applyTo(QImage *img) const;
    bool applyTo(QColorProfile &src, QColor *c) const;

    inline QByteArray data() const;
    inline QByteArray id() const;

    virtual ~QColorProfilePrivate();

    static const QColorProfilePrivate *get(const QColorProfile *p) {
        return p->d;
    }

    mutable cmsHPROFILE profileHandler;
};

QByteArray QColorProfilePrivate::data() const
{
    QByteArray profileData;

    cmsUInt32Number size;
    cmsSaveProfileToMem(profileHandler, 0, &size);
    profileData.resize(size);
    cmsSaveProfileToMem(profileHandler, profileData.data(), &size);

    return profileData;
}

QByteArray QColorProfilePrivate::id() const
{
    QByteArray id(16,0);

    cmsMD5computeID(profileHandler);
    cmsGetHeaderProfileID(profileHandler,(uchar*)id.data());

    return id;
}
#endif // COLORPROFILE_P_H
