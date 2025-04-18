#ifndef TIFFTHUMBDECODER_H
#define TIFFTHUMBDECODER_H

#include <QtWidgets>
#include <QObject>
#include "ImageFormats/Tiff/tiff.h"

class TiffThumbDecoder : public QObject
{
    Q_OBJECT

public:
    TiffThumbDecoder();

signals:
    void setIcon(QModelIndex dmIdx, QPixmap pm, int instance, QString src);

public slots:
    void addToQueue(QString fPath, QModelIndex dmIdx, int dmInstance, int offset);

private:
    void processQueue();
    enum Status {Idle, Busy} status;
    struct Item {
        QString fPath;
        QModelIndex dmIdx;
        int dmInstance;
        int offset;
    };
    QList<Item> queue;
    bool abort = false;
    bool isDebugging;
};

#endif // TIFFTHUMBDECODER_H
