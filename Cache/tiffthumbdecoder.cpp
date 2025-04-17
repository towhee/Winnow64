#include "tiffthumbdecoder.h"

TiffThumbDecoder::TiffThumbDecoder() {}

void TiffThumbDecoder::addToQueue(QString fPath, QString source,
                             QModelIndex dmIdx, int dmInstance)
{
    if (G::isLogger) G::log("TiffThumbDecoder::addToQueue");

    if (abort) return;
    Item item;
    item.fPath = fPath;
    item.source = source;
    item.dmIdx = dmIdx;
    item.dmInstance = dmInstance;
    queue.append(item);

    if (status == Status::Idle) {
        status = Status::Busy;
        getNextThumbNail("addToQueue");
    }
}

void TiffThumbDecoder::getNextThumbNail(QString src = "")
{
    if (G::isLogger) G::log("TiffThumbDecoder::getNextThumbNail");

    if (abort) {
        abort = false;
    }

    if (queue.isEmpty()) {
        if (isDebugging)
        {
            qDebug() << "TiffThumbDecoder::getNextThumbNail           "
                     << "queue.isEmpty() =" << queue.isEmpty()
                     << "queue size =" << queue.size()
                     << "  abort =" << abort
                ;
        }
        status = Status::Idle;
        return;
    }

    // process thumbnail

}
