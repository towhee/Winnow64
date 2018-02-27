#ifndef PIXMAP_H
#define PIXMAP_H

#include <QObject>
#include <QtWidgets>
#include <QHash>
#include "metadata.h"
#include "datamodel.h"

class Pixmap : public QObject
{
    Q_OBJECT
public:
    explicit Pixmap(QObject *parent, Metadata *metadata);
    bool load(QString &fPath, QPixmap &pm);
    bool load(QString &fPath, QImage &image);

private:
    Metadata *metadata;
};

#endif // PIXMAP_H
