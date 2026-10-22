#include "linux.h"
#include "Main/global.h"

#include <QFile>
#include <unistd.h>

namespace {

quint64 meminfoMB(const QByteArray &key)
{
/*
    Returns the /proc/meminfo field named key ("MemTotal", "MemAvailable", ...) in MB,
    or 0 if it cannot be read. The file reports kB.
*/
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    const QByteArray prefix = key + ':';
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        if (!line.startsWith(prefix)) continue;
        const QList<QByteArray> parts = line.mid(prefix.size()).simplified().split(' ');
        bool ok = false;
        const quint64 kB = parts.isEmpty() ? 0 : parts.first().toULongLong(&ok);
        return ok ? kB / 1024 : 0;
    }
    return 0;
}

} // namespace

void Linux::availableMemory()
{
/*
    MemAvailable is the kernel's estimate of memory available to start new work
    without swapping (free + reclaimable cache), the analog of Mac::availableMemory's
    free + inactive pages.
*/
    if (G::isLogger) G::log("Linux::availableMemory");
    const quint64 mb = meminfoMB("MemAvailable");
    if (mb > 0) G::availableMemoryMB = mb;
}

quint64 Linux::totalMemoryMB()
{
    return meminfoMB("MemTotal");
}

quint64 Linux::processFootprintMB()
{
    // /proc/self/statm: size resident shared ... (in pages)
    QFile f("/proc/self/statm");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    const QList<QByteArray> parts = f.readAll().simplified().split(' ');
    if (parts.size() < 2) return 0;
    bool ok = false;
    const quint64 pages = parts.at(1).toULongLong(&ok);
    if (!ok) return 0;
    const quint64 pageSize = static_cast<quint64>(sysconf(_SC_PAGESIZE));
    return pages * pageSize / (1024ull * 1024ull);
}
