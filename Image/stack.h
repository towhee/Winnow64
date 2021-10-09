#ifndef STACK_H
#define STACK_H

#include <QObject>
#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Effects/effects.h"
#include "Image/pixmap.h"
#include "Cache/cachedata.h"

class Stack : public QObject
{
    Q_OBJECT
public:
    enum Method {average, mean};
    Stack(Method method, DataModel *dm, Metadata *metadata, ImageCacheData *icd);

private:
    DataModel *dm;
    Metadata *metadata;
    ImageCacheData *icd;
    QList<QString> pickList;
    struct PX {
        double r;
        double g;
        double b;
    };

    void getPicks();
    void doMean();

};

#endif // STACK_H
