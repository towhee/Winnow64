#ifndef METAREAD3_H
#define METAREAD3_H

#include <QtWidgets>
#include <QObject>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Cache/reader.h"
#include "Cache/imagecache.h"

class MetaRead3 : public QObject
{
    Q_OBJECT
public:
    MetaRead3(QObject *parent,
              DataModel *dm,
              Metadata *metadata,
              FrameDecoder *frameDecoder,
              ImageCache *imageCache);
};

#endif // METAREAD3_H
