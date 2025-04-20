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

void TiffThumbDecoder::addToQueue(QString fPath, QModelIndex dmIdx, int dmInstance,
                                  int offset)
{
    if (G::isLogger) G::log("TiffThumbDecoder::addToQueue");

    if (abort) return;
    Item item;
    item.fPath = fPath;
    item.dmIdx = dmIdx;
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

    if (abort) {
        abort = false;
        queue.clear();
        return;
    }

    // process thumbnail
    QImage image;
    QString fPath = queue.at(0).fPath;
    QModelIndex dmIdx = queue.at(0).dmIdx;
    int offset = queue.at(0).offset;
    int instance = queue.at(0).dmInstance;
    QString src = "TiffThumbDecoder::processQueue";
    Tiff tiff(src);
    if (tiff.read(fPath, &image, offset)) {
        QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        emit setIcon(dmIdx, pm, instance, src);
    }

    // recurse
    queue.removeFirst();
    if (!queue.isEmpty()) {
        processQueue();
    }

    // done
    status = Status::Idle;
}
