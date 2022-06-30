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
        - mediaPlayer signals frameDecoder->frameChanged
        - frameDecoder->frameChanged signals dm->setIconFromFrame
        - dm->setIconFromFrame deletes frameDecoder
*/

FrameDecoder::FrameDecoder(QModelIndex dmIdx, int dmInstance)
{
    thisFrameDecoder = this;
    this->dmIdx = dmIdx;
    this->dmInstance = dmInstance;
    mediaPlayer = new QMediaPlayer();
    videoSink = new QVideoSink;
    mediaPlayer->setVideoOutput(videoSink);
    connect(videoSink, &QVideoSink::videoFrameChanged, this, &FrameDecoder::frameChanged);
}

void FrameDecoder::getFrame(QString path)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    fPath = path;
    QFile f(fPath);
    mediaPlayer->setSource(fPath);
    mediaPlayer->play();
}

void FrameDecoder::frameChanged(const QVideoFrame frame)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    if (thumbnailAcquired) return;
    QImage im = frame.toImage();
    if (im.isNull()) return;
    thumbnailAcquired = true;
    mediaPlayer->stop();
    qint64 duration = mediaPlayer->duration();
    QPixmap pm = QPixmap::fromImage(im.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    emit setFrameIcon(dmIdx, pm, dmInstance, duration, thisFrameDecoder);
}
