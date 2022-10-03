#include "videoframedispatcher.h"

/*
    VideoFrameDispatcher is created in MW::createVideoFrameDispatcher during the app
    initialization.  A number of threads are established, each with their own
    FrameDecoder.

    Frames to decode are added to the queue in getVideoFrame.  Each Frame comprises:
        QString fPath
        QModelIndex dmIdx   // required by dm->setIconFromFrame
        int dmInstance      // required by dm->setIconFromFrame
        QString status      // "decode", "decoding"

    Decoders are invoked in decodeNextFrame, which searches the queue for the next frame
    to decode. The decoder sets its status to "busy", gets the first video frame and
    emits the QImage to the DataModel. It then signals dispatch to look for another frame
    to decode.  If there are no more frames to decode then the decoder status is set
    to "idle".

    After adding a frame to the queue, if an "idle" decoder is available it is invoked.
    Otherwise it will be decoded later when the dispatch cycle gets to it.

*/

VideoFrameDispatcher::VideoFrameDispatcher(QObject *parent, DataModel *dm)
//    : QObject{parent}
{
    this->dm = dm;

    // create n frame decoder threads
    decoderThreadCount = QThread::idealThreadCount();
    for (int i = 0; i < decoderThreadCount; ++i) {
        QThread *thread = new QThread;
//        decoderThread.append(thread);
        FrameDecoder *frameDecoder = new FrameDecoder(this, i);
        decoder.append(frameDecoder);
        decoder[i]->moveToThread(thread);
        connect(decoder[i], &FrameDecoder::dispatch,
                this, &VideoFrameDispatcher::dispatch);
        connect(decoder[i], &FrameDecoder::setFrameIcon,
                dm, &DataModel::setIconFromVideoFrame);
        thread->start();
    }

    isDebugging = true;
    if (isDebugging) qDebug() << "VideoFrameDispatcher::VideoFrameDispatcher"
                              << "decoderThreadCount =" << decoderThreadCount
                                 ;
}

void VideoFrameDispatcher::clear()
{
    queue.clear();
}

void VideoFrameDispatcher::getVideoFrame(QString fPath, QModelIndex dmIdx, int dmInstance)
{
    Frame frame;
//    frame.decoderId = -1;
    frame.fPath = fPath;
    frame.dmIdx = dmIdx;
    frame.dmInstance = dmInstance;
    frame.status = "decode";
    mutex.lock();
    queue.append(frame);
    mutex.unlock();

    /* Dispatch the first idle decoder.  It will decode the first frame with status "decode"
       in the QMap ftd (frames to do).  If no idle decoders, then a remaining decoder will
       invoke dispath.
       */
    for (int id = 0; id < decoderThreadCount; id++) {
        if (decoder[id]->status == "idle") {
            if (isDebugging) qDebug() << "VideoFrameDispatcher::getVideoFrame   "
                                      << "row =" << dmIdx.row()
                                      << "  id =" << id
                                      << "  Decoder status =" << decoder[id]->status
                                      << "  Queue status =" << frame.status
                                         ;
            dispatch(id);     // status: idle, busy
            return;
        }
    }
    if (isDebugging) qDebug() << "VideoFrameDispatcher::getVideoFrame   "
                              << "All decoders busy, added to queue"
                                 ;
}

void VideoFrameDispatcher::dispatch(int id)
{
/*

*/
    if (isDebugging) qDebug() << "VideoFrameDispatcher::dispatch        "
                              << "          id =" << id
                              << "  Decoder status =" << decoder[id]->status
//                              << "  Queue status =" << frame.status
                                 ;
    // remove prior frame from the queue
    if (id != -1) {
        for (int i = 0; i < queue.size(); i++) {
            if (queue.at(i).dmIdx == decoder[id]->dmIdx) {
                queue.remove(i);
                break;
            }
        }
    }
    decodeNextFrame(id);
}

void VideoFrameDispatcher::decodeNextFrame(int id)
{
    bool dispatched = false;
    for (int i = 0; i < queue.size(); i++) {
        if (queue.at(i).status == "decode") {
            // frame found to decode
            queue[i].status = "decoding";
            if (isDebugging) qDebug() << "VideoFrameDispatcher::decodeNextFrame "
                                      << "row =" << queue.at(i).dmIdx.row()
                                      << "  id =" << id
                                      << "  Decoder status =" << decoder[id]->status
                                      << "  Queue status =" << queue.at(i).status
                                         ;
//            dispatched = true;
            decoder[id]->getFrame(queue.at(i).fPath, queue.at(i).dmIdx, queue.at(i).dmInstance);
        }
    }
//    if (!dispatched) decoder[id]->setStatus("idle");
}


