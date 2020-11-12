#include "performance.h"

Performance::Performance()
{
}

double Performance::mediaReadSpeed(QFile &file)
{
/*  return Gb/sec */
    if (!file.open(QIODevice::ReadOnly)) return -1;
    QElapsedTimer t;
    t.start();
    file.readAll();
    double nSec = t.nsecsElapsed();
    double sec = nSec / 1000000000;
    file.close();
    double bytes = static_cast<double>(file.size());
    // bytes to gigibits = bytes * 8 / (1024*1024*1024) = bytes / 134217728
    double gbs = bytes / 134217728 / sec;
    qDebug() << __FUNCTION__
             << "bytes =" << bytes
             << "nSec =" << nSec
             << "sec =" << sec
             << "Gb/sec =" << gbs
                ;
    return gbs;
}
