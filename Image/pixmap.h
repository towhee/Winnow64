#ifndef PIXMAP_H
#define PIXMAP_H

#include <QObject>
#include <QtWidgets>
#include <QHash>
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
//#ifdef Q_OS_WIN
#include "Utilities/icc.h"
//#endif
#include "ImageFormats/Jpeg/jpeg.h"
#ifdef Q_OS_WIN
// rgh remove heic
#include "ImageFormats/Heic/heic.h"
#endif

class Pixmap : public QObject
{
    Q_OBJECT
public:
    explicit Pixmap(QObject *parent, DataModel *dm, Metadata *metadata);
    bool load(QString &fPath, QPixmap &pm, QString src = "");
    bool load(QString &fPath, QImage &image, QString src = "");

private:
    DataModel *dm;
    Metadata *metadata;

    bool loadFromHeic(QString &fPath, QImage &image);
};

#endif // PIXMAP_H
