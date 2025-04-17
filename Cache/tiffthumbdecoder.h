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

public slots:
    void addToQueue(QString fPath, QString source = "",
                    QModelIndex dmIdx = QModelIndex(), int dmInstance = 0);

private:
    void getNextThumbNail(QString src = "");
    enum Status {Idle, Busy} status;
    struct Item {
        QString fPath;
        QModelIndex dmIdx;
        int dmInstance;
        QString source;
    };
    QList<Item> queue;
    bool abort = false;
    bool isDebugging;
};

#endif // TIFFTHUMBDECODER_H
