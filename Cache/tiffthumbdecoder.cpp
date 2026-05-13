#include "tiffthumbdecoder.h"

/*
    TiffThumbDecoder should be used to generate a thumbnail from a tiff that does not
    have an embedded thumbnail. Generating a thumbnail from the full size tiff image is
    very resource intensive and can result in GUI latency. This class should be moved to
    a separate thread.  Example:

        tiffThumbDecoder = new TiffThumbDecoder();
        connect(tiffThumbDecoder, &TiffThumbDecoder::setIcon, dm, &DataModel::setIcon);
        tiffThumbDecoderThread = new QThread;
        tiffThumbDecoder->moveToThread(tiffThumbDecoderThread);
        tiffThumbDecoderThread->start();

    Stop tiffThumbDecoderThread when finished ie when Winnow closes

        tiffThumbDecoderThread->quit();
        tiffThumbDecoderThread->wait();

    When the thumbnail has been generated the icon is updated in the DataModel.

    TiffThumbDecoder is used by Reader::readIcon().
*/

TiffThumbDecoder::TiffThumbDecoder() {}

bool TiffThumbDecoder::queueContains(int dmRow, int dmInstance)
{
    for (int i = 0; i < queue.size(); ++i) {
        if (queue[i].dmRow == dmRow && queue[i].dmInstance == dmInstance)
            return true;
    }
    return false;
}

void TiffThumbDecoder::addToQueue(QString fPath, int dmRow, int dmInstance,
                                  int offset)
{
    if (G::isLogger) G::log("TiffThumbDecoder::addToQueue");
    qDebug() << "TiffThumbDecoder::addToQueue" << dmRow << fPath;

    if (abort) return;
    /* Dedupe: Reader::readIcon can fire tiffMissingThumbDecode multiple times
       for the same row (rapid scrolling re-dispatch, MetaRead retries).
       Decoding the full TIFF twice is wasted work, and the duplicate
       setIcon emit was the trigger for the QPixmap double-replace crash in
       DataModel::setIcon1. Modeled on FrameDecoder::queueContains. */
    if (queueContains(dmRow, dmInstance)) return;
    Item item;
    item.fPath = fPath;
    item.dmRow = dmRow;
    item.dmInstance = dmInstance;
    item.offset = offset;
    queue.append(item);

    if (status == Status::Idle) {
        status = Status::Busy;
        processQueue();
    }
}

void TiffThumbDecoder::processQueue()
{
    if (G::isLogger) G::log("TiffThumbDecoder::processQueue");
    qDebug().noquote() << "TiffThumbDecoder::processQueue";

    if (abort) {
        abort = false;
        queue.clear();
        return;
    }

    // process thumbnail
    QImage image;
    QString fPath = queue.at(0).fPath;
    int dmRow = queue.at(0).dmRow;
    int offset = queue.at(0).offset;
    int instance = queue.at(0).dmInstance;
    QString src = "TiffThumbDecoder::processQueue";
    Tiff tiff(src);
    if (tiff.readSample(fPath, &image, G::maxIconSize, offset)) {
        emit setIcon(dmRow, image, instance, src);
    }

    // recurse
    queue.removeFirst();
    if (!queue.isEmpty()) {
        processQueue();
    }

    // done
    status = Status::Idle;
}
