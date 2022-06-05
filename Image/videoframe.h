#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <QObject>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include "Datamodel/datamodel.h"
#include "Main/global.h"

class VideoFrame : public QObject
{
    Q_OBJECT
public:
    VideoFrame(DataModel *dm, QObject *parent = nullptr);
//    ~VideoFrame() override;
    void getFrame(QString fPath);

private:
    void thumbnail(const QVideoFrame frame);
    DataModel *dm;
    QVideoSink *videoSink;
    QMediaPlayer *mediaPlayer;
    QString fPath;
    QModelIndex dmIdx;

signals:
    void setIcon(QModelIndex dmIdx, QPixmap &pm, int instance);
};

#endif // VIDEOFRAME_H
