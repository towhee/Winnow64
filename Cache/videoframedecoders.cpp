#include "videoframedecoders.h"

VideoFrameDecoders::VideoFrameDecoders(QObject *parent, DataModel *dm)
    : QObject{parent}
{
    this->dm = dm;

    // create n frame decoder threads
    decoderThreadCount = QThread::idealThreadCount();
    for (int i = 0; i < decoderThreadCount; ++i) {
        QThread *thread = new QThread;
        decoderThread.append(thread);
        FrameDecoder *frameDecoder = new FrameDecoder;
        decoder[i] = frameDecoder;
        decoder[i]->moveToThread(thread);
//        connect(decoder[id], &ImageDecoder::done, this, &ImageCache::fillCache);
    }
}

void VideoFrameDecoders::getVideoFrame(QString fPath, QModelIndex dmIdx, int dmInstance)
{
    Frame frame;
    frame.decoderId = -1;
    frame.fPath = fPath;
    frame.dmIdx = dmIdx;
    frame.dmInstance = dmInstance;
    frame.status = "decode";
    mutex.lock();
    ftd.insert(fPath, frame);
    mutex.unlock();
}

void VideoFrameDecoders::dispatch(int decoderId, QString fPath, QModelIndex dmIdx, int dmInstance)
{
/*

*/
    //
    if (decoderId == -1) {

    }

    // returning decoder
    ftd.remove(fPath);
    decodeNextFrame(decoderId);
}

void VideoFrameDecoders::decodeNextFrame(int id)
{
    for (Frame frame : ftd) {
        if (frame.status == "decode") {
            decoder[id]->getFrame(frame.fPath, frame.dmIdx, frame.dmInstance);
        }
    }
}


