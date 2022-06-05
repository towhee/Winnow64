#include "framedecoder.h"

FrameDecoder::FrameDecoder(QModelIndex dmIdx, int dmInstance, QObject *parent)
{
    thisFrameDecoder = this;
    this->dmIdx = dmIdx;
    this->dmInstance = dmInstance;
    mediaPlayer = new QMediaPlayer(/*this*/);
    videoSink = new QVideoSink;
    mediaPlayer->setVideoOutput(videoSink);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &FrameDecoder::thumbnail);
//    connect(this, &FrameDecoder::setFrameIcon, dm, &DataModel::setIcon);
}

void FrameDecoder::stop()
{
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
    quit();
}

void FrameDecoder::getFrame(QString path)
{
    qDebug() << "FrameDecoder::thumbnail" << fPath << this;
    fPath = path;
    start();
}

void FrameDecoder::thumbnail(const QVideoFrame frame)
{
    qDebug() << "FrameDecoder::thumbnail this =" << this;
    QPixmap pm;
    QImage im;
    im = frame.toImage();
    qDebug() << "FrameDecoder::thumbnail isNull =" << im.isNull();
    if (im.isNull()) return;
    pm = QPixmap::fromImage(im.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    emit setFrameIcon(dmIdx, pm, dmInstance, thisFrameDecoder);
    mediaPlayer->stop();
    qDebug() << "FrameDecoder::thumbnail"
             << fPath
             << "width =" << pm.width();
}

void FrameDecoder::run()
{
    mediaPlayer->setSource(fPath);
    mediaPlayer->play();
}
