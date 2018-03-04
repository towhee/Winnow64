#ifndef THUMB_H
#define THUMB_H

#include <QObject>
#include <QtWidgets>
#include "Metadata/metadata.h"

class Thumb : public QObject
{
    Q_OBJECT
public:
    explicit Thumb(QObject *parent, Metadata *metadata);
    bool loadThumb(QString &fPath, QImage &image);

private:
    Metadata *metadata;
    QString err;
    QSize thumbMax;         // rgh review hard coding thumb size

    bool loadFromData(QString &fPath, QImage &image);
    bool loadFromEntireFile(QString &fPath, QImage &image);
    void checkOrientation(QString &fPath, QImage &image);

    void track(QString fPath, QString msg);
};

#endif // THUMB_H
