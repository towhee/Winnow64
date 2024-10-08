#ifndef STACK_H
#define STACK_H

#include <QObject>
#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Metadata/ExifTool.h"
#include "Effects/effects.h"
#include "Image/pixmap.h"
#include "Cache/cachedata.h"

class Stack : public QObject
{
    Q_OBJECT
public:
    Stack(QStringList &selection,
          DataModel *dm,
          Metadata *metadata,
          ImageCacheData *icd);
    QString mean();
    void stop();

private:
    QStringList &selection;
    DataModel *dm;
    Metadata *metadata;
    ImageCacheData *icd;
    QList<QString> pickList;
    struct PX {
        double r;
        double g;
        double b;
    };
    bool abort = false;
};

#endif // STACK_H
