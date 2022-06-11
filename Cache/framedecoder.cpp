#include "framedecoder.h"

/*
    Generate a thumbnail from the first video frame in a video file.

    This is accomplished by creating a QMediaPlayer that decodes and plays the video file
    to the virtual canvas QVideoSink. Each time the video frame changes QVideoSink emits
    a videoFrameChanged signal. FrameDecoder::thumbnail receives the signal, converts the
    frame into a QImage, emits setFrameIcon and stops the QMediaPlayer. The setFrameIcon
    signal is received by DataModel::setIconFromFrame, where the icon is added to the
    datamodel and then the instance of FrameDecoder is deallocated (deleted).

    This convoluted process is required because QVideoSink does not know which file the
    video frame came from.  When processing many files, it is not guaranteed that the
    signal/slots will be sequential.  To solve this a separate instance of FrameDecoder
    is created for each file.  When the thumbnail (icon) has been received by the datamodel
    the FrameDecoder instance is deleted.

    Summary of sequence:
        - thumb->loadFromVideo creates FrameDecoder instance frameDecoder
        - thumb->loadFromVideo calls frameDecoder->getFrame
        - frameDecoder->getFrame starts mediaPlayer
        - mediaPlayer signals frameDecoder->thumbnail
        - frameDecoder->thumbnail signals dm->setIconFromFrame
        - dm->setIconFromFrame deletes frameDecoder
*/

FrameDecoder::FrameDecoder(QModelIndex dmIdx, int dmInstance, QObject *parent)
    : QThread(parent)
{
    thisFrameDecoder = this;
    this->dmIdx = dmIdx;
    this->dmInstance = dmInstance;
    mediaPlayer = new QMediaPlayer(/*this*/);
    videoSink = new QVideoSink;
    mediaPlayer->setVideoOutput(videoSink);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &FrameDecoder::frameChenged);
}

void FrameDecoder::stop()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
}

void FrameDecoder::getFrame(QString path)
{
    if (G::isLogger) G::log(__FUNCTION__);
    fPath = path;
    qDebug() << "FrameDecoder::getFrame" << fPath;
    start();
}

void FrameDecoder::frameChenged(const QVideoFrame frame)
{
    if (G::isLogger) G::log(__FUNCTION__);
    qDebug() << "FrameDecoder::frameChenged" << fPath;
    if (thumbnailAcquired) return;
    QImage im = frame.toImage();
    qDebug() << "FrameDecoder::frameChenged 1" << "im.isNull()"<< im.isNull();
    if (im.isNull()) return;
    thumbnailAcquired = true;
    mediaPlayer->stop();
    QPixmap pm = QPixmap::fromImage(im.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    emit setFrameIcon(dmIdx, pm, dmInstance, thisFrameDecoder);
}

void FrameDecoder::run()
{
    if (G::isLogger) G::log(__FUNCTION__);
    QFile f(fPath);
    qDebug() << "FrameDecoder::run  File open:" << f.isOpen() << fPath;
    mediaPlayer->setSource(fPath);
    mediaPlayer->play();
}
