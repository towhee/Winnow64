#ifndef VIDEOFRAMEDISPATCHER_H
#define VIDEOFRAMEDISPATCHER_H

#include <QObject>
#include <QMutex>
#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"

class VideoFrameDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit VideoFrameDispatcher(QObject *parent, DataModel *dm);
    void getVideoFrame(QString fPath, QModelIndex dmIdx, int dmInstance);

signals:

private:
    QMutex mutex;

    DataModel *dm;
    QVector<QThread*> decoderThread;       // all the frame decoder threads
    QVector<FrameDecoder*> decoder;
    int decoderThreadCount;                 // number of decoder threads

    struct Frame {
//        int decoderId;
        QString fPath;
        QModelIndex dmIdx;
        int dmInstance;
        QString status;
    };
    QMap<QString, Frame> queue;                // frames to do list

    bool isDebugging;

    void dispatch(int id);
    void decodeNextFrame(int id);
};

#endif // VIDEOFRAMEDISPATCHER_H
