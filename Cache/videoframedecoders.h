#ifndef VIDEOFRAMEDECODERS_H
#define VIDEOFRAMEDECODERS_H

#include <QObject>
#include <QMutex>
#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"

class VideoFrameDecoders : public QObject
{
    Q_OBJECT
public:
    explicit VideoFrameDecoders(QObject *parent, DataModel *dm);
    void getVideoFrame(QString fPath, QModelIndex dmIdx, int dmInstance);

signals:

private:
    QMutex mutex;

    DataModel *dm;
    QVector<QThread*> decoderThread;       // all the frame decoder threads
    QVector<FrameDecoder*> decoder;
    int decoderThreadCount;                 // number of decoder threads

    struct Frame {
        int decoderId;
        QString fPath;
        QModelIndex dmIdx;
        int dmInstance;
        QString status;
    };
    QMap<QString, Frame> ftd;                // frames to do list

    void dispatch(int decoderId, QString fPath, QModelIndex dmIdx, int dmInstance);
    void decodeNextFrame(int id);
};

#endif // VIDEOFRAMEDECODERS_H
