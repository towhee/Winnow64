#include "videoframe.h"

VideoFrame::VideoFrame(DataModel *dm, QObject *parent)
{
    this->dm = dm;
    mediaPlayer = new QMediaPlayer(/*this*/);
    videoSink = new QVideoSink;
    mediaPlayer->setVideoOutput(videoSink);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &VideoFrame::thumbnail);
    connect(this, &VideoFrame::setIcon, dm, &DataModel::setIcon);
}

//VideoFrame::~VideoFrame()
//{
//    qDebug() << __FUNCTION__ << "Destructor";
//}

void VideoFrame::getFrame(QString fPath)
{
    qDebug() << "VideoFrame::thumbnail" << fPath << this;
    this->fPath = fPath;
    mediaPlayer->setSource(fPath);
    int dmRow = dm->rowFromPath(fPath);
    dmIdx = dm->index(dmRow, 0);
    mediaPlayer->play();
}

void VideoFrame::thumbnail(const QVideoFrame frame)
{
    qDebug() << "VideoFrame::thumbnail this =" << this;
    QPixmap pm;
    QImage im;
    im = frame.toImage();
    qDebug() << "VideoFrame::thumbnail isNull =" << im.isNull();
    if (im.isNull()) return;
    pm = QPixmap::fromImage(im.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    emit setIcon(dmIdx, pm, dm->instance);
    mediaPlayer->stop();
    qDebug() << "VideoFrame::thumbnail"
             << fPath
             << "width =" << pm.width();
}

