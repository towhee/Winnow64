#include "performance.h"

#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRandomGenerator>
#include <algorithm>

namespace {
    /* Stop gathering candidates once we have plenty, or the scan runs long.   */
    const int    kMaxCandidates  = 2000;
    const qint64 kEnumBudgetMs   = 1500;   // soft cap once some candidates found
    const qint64 kEnumHardCapMs  = 4000;   // hard cap even if none found yet
    /*
        Only sample files in this size band: below kMinFileBytes the read is
        dominated by open/seek latency rather than bandwidth (this is why tiny
        system icons made the average collapse to tens of MB/sec); above
        kMaxFileBytes a single file would dominate the timing.
    */
    const qint64 kMinFileBytes   = 4LL * 1024 * 1024;
    const qint64 kMaxFileBytes   = 256LL * 1024 * 1024;
    /* Stop reading once enough data is sampled for a stable measurement.      */
    const qint64 kReadCapBytes   = 512LL * 1024 * 1024;
}

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
    qDebug() << "Performance::mediaReadSpeed"
             << "bytes =" << bytes
             << "nSec =" << nSec
             << "sec =" << sec
             << "Gb/sec =" << gbs
                ;
    return gbs;
}

Performance::ReadSpeedResult Performance::sampleReadSpeed(const QString &rootPath,
                                                          const QStringList &nameFilters,
                                                          int sampleCount,
                                                          double cacheRejectFactor)
{
    ReadSpeedResult r;

    /*
        Gather sizeable candidate image files under rootPath. Bounded by a count
        and a time budget so scanning a large (or system) volume stays responsive.
        NoSymLinks avoids descending the /Volumes boot symlink loop on macOS.
    */
    QStringList candidates;
    QElapsedTimer budget;
    budget.start();
    QDirIterator it(rootPath, nameFilters, QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        qint64 sz = it.fileInfo().size();
        if (sz < kMinFileBytes || sz > kMaxFileBytes) continue;
        candidates << it.filePath();
        if (candidates.size() >= kMaxCandidates) break;
        qint64 el = budget.elapsed();
        if (el > kEnumBudgetMs && !candidates.isEmpty()) break;
        if (el > kEnumHardCapMs) break;
    }
    if (candidates.isEmpty()) {
        r.error = "No files ≥ 4 MB";
        return r;
    }

    /* Pick up to sampleCount distinct files at random, spread across the drive. */
    int n = candidates.size();
    int want = qMin(sampleCount, n);
    QSet<int> used;
    QStringList sample;
    while (sample.size() < want) {
        int idx = QRandomGenerator::global()->bounded(n);
        if (used.contains(idx)) continue;
        used.insert(idx);
        sample << candidates.at(idx);
    }

    /* Read each sampled file, recording bytes, seconds and per-file MB/sec. */
    struct Read { double bytes; double sec; double mbps; };
    QList<Read> reads;
    qint64 readSoFar = 0;
    for (const QString &fPath : std::as_const(sample)) {
        QFile file(fPath);
        if (!file.open(QIODevice::ReadOnly)) continue;
        QElapsedTimer t;
        t.start();
        QByteArray data = file.readAll();
        double sec = t.nsecsElapsed() / 1000000000.0;
        file.close();
        double bytes = data.size();
        if (bytes <= 0 || sec <= 0) continue;
        reads << Read{bytes, sec, (bytes / (1024 * 1024)) / sec};
        readSoFar += static_cast<qint64>(bytes);
        if (readSoFar >= kReadCapBytes) break;
    }
    if (reads.isEmpty()) {
        r.error = "Could not read files";
        return r;
    }

    /* Median per-file rate, used only to identify cache hits. */
    QList<double> rates;
    for (const Read &rd : std::as_const(reads)) rates << rd.mbps;
    std::sort(rates.begin(), rates.end());
    int mid = rates.size() / 2;
    double median = (rates.size() % 2)
                        ? rates.at(mid)
                        : (rates.at(mid - 1) + rates.at(mid)) / 2.0;

    /*
        Reject reads faster than cacheRejectFactor x median as OS-cache hits
        (RAM-speed reads sit far above the media's median), then report the
        accepted reads as byte-weighted throughput. Only reject when there are
        enough samples for the median to be meaningful.
    */
    double threshold = median * cacheRejectFactor;
    double totalBytes = 0;
    double totalSec = 0;
    for (const Read &rd : std::as_const(reads)) {
        if (reads.size() > 2 && rd.mbps > threshold) continue;
        totalBytes += rd.bytes;
        totalSec += rd.sec;
    }
    r.mbPerSec = (totalSec > 0) ? (totalBytes / (1024 * 1024)) / totalSec : 0;
    return r;
}
