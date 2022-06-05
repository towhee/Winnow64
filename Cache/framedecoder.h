#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
//#include "Datamodel/datamodel.h"

class FrameDecoder : public QThread
{
    Q_OBJECT
public:
    FrameDecoder(QModelIndex dmIdx, int dmInstance, QObject *parent = nullptr);
    void stop();
    void getFrame(QString fPath);

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setFrameIcon(QModelIndex dmIdx, QPixmap &pm, int instance,
                      FrameDecoder *thisFrameDecoder);

private:
    FrameDecoder *thisFrameDecoder;
    QMutex mutex;
    QWaitCondition condition;
    void thumbnail(const QVideoFrame frame);
    QVideoSink *videoSink;
    QMediaPlayer *mediaPlayer;
    QString fPath;
    int dmInstance;
    QModelIndex dmIdx;
    bool abort;

};
#endif // FRAMEDECODER_H
