#include "metareader.h"

MetaReader::MetaReader(QObject *parent,
                       int id,
                       DataModel *dm/*,
                       Metadata *metadata*/)
    : QThread(parent)
{
    if (G::isLogger) G::log(__PRETTY_FUNCTION__);
    this->dm = dm;
    metadata = new Metadata;
    threadId = id;
    row = -1;
    abort = false;
}

MetaReader::~MetaReader()
{
    delete metadata;
}

void MetaReader::stop()
{
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }
    abort = false;
    row = -1;
}

bool MetaReader::isBusy()
{
    return isRunning();
}

void MetaReader::read(int row)
{
//    qDebug() << __PRETTY_FUNCTION__ << row;
    this->row = row;
    if (isRunning()) wait();
    start();
}

void MetaReader::run()
{
    if (abort) stop();
    row = threadId;
    while (row < dm->rowCount()) {
        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __PRETTY_FUNCTION__)) {
            metadata->m.row = row;
            if (abort) stop();
            dm->addMetadataForItem(metadata->m);
        }
        row += 8;
    }
    if (abort) stop();
//    qDebug() << __PRETTY_FUNCTION__ << "reader emit done" << threadId;
    emit done(threadId);
}
