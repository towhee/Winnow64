#ifndef PIXMAP_H
#define PIXMAP_H

#include <QObject>
#include <QtWidgets>
#include <QHash>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"

class Pixmap : public QObject
{
    Q_OBJECT
public:
    explicit Pixmap(QObject *parent, DataModel *dm, Metadata *metadata);
    bool load(QString &fPath, QPixmap &pm);
    bool load(QString &fPath, QImage &image);

private:
    DataModel *dm;
    Metadata *metadata;
};

#endif // PIXMAP_H
