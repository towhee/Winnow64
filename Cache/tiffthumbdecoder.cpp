#include "tiffthumbdecoder.h"

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
