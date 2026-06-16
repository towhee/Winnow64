#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <QtWidgets>

class Performance
{
public:
    Performance();
    static double mediaReadSpeed(QFile &file);

    /* Result of benchmarking a single drive (see sampleReadSpeed). */
    struct ReadSpeedResult {
        double  mbPerSec = 0;       // byte-weighted throughput of accepted reads
        QString error;              // non-empty => nothing measurable
    };

    /*
        Benchmark a drive by reading a random sample of sizeable image files
        found under rootPath (matched by nameFilters, e.g. {"*.jpg","*.nef",...}).
        Reads faster than cacheRejectFactor x the median are discarded as OS-cache
        hits, and the remainder is reported as byte-weighted throughput (total
        bytes / total seconds) so the result reflects bandwidth, not per-file
        latency.
    */
    static ReadSpeedResult sampleReadSpeed(const QString &rootPath,
                                           const QStringList &nameFilters,
                                           int sampleCount = 25,
                                           double cacheRejectFactor = 2.0);
};

#endif // PERFORMANCE_H
