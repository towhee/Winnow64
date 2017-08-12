#ifndef PIXMAP_H
#define PIXMAP_H

#include <QObject>
#include <QtWidgets>
#include <QHash>
#include "metadata.h"

class Pixmap : public QObject
{
    Q_OBJECT
public:
    explicit Pixmap(QObject *parent, Metadata *metadata);
//    explicit Pixmap(QObject *parent = nullptr);
    bool load(QString &fPath, QPixmap &pm);

private:
    Metadata *metadata;
};

#endif // PIXMAP_H
