#include "rowtrail.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <vector>

namespace {

struct Entry {
    qint64 ms = -1;             // since the first event; -1 = unused slot
    int dmRow = -1;
    bool gui = false;
    const char *event = nullptr;
    const char *tag = nullptr;
    QString fPath;
    QString detail;
};

constexpr int kCapacity = 1 << 18;

QMutex mutex;
std::vector<Entry> ring;        // allocated on the first note
int head = 0;                   // next slot to write
QElapsedTimer trailClock;
thread_local const char *currentTag = nullptr;

QString format(const Entry &e)
{
    // Concatenated, not chained arg(): a path or detail may itself contain "%1".
    QString s = QString::number(e.ms / 1000.0, 'f', 3).rightJustified(10)
                + (e.gui ? "  GUI " : "  work")
                + "  dmRow " + QString::number(e.dmRow).rightJustified(6)
                + "  " + QString::fromLatin1(e.event).leftJustified(24);
    if (!e.detail.isEmpty()) s += "  " + e.detail;
    if (e.tag) s += "  [" + QString::fromLatin1(e.tag) + "]";
    return s;
}

} // namespace

void RowTrail::note(const QString &fPath, int dmRow, const char *event,
                    const QString &detail)
{
    if (fPath.isEmpty()) return;
    const bool gui = qApp && QThread::currentThread() == qApp->thread();
    QMutexLocker lock(&mutex);
    if (ring.empty()) {
        ring.resize(kCapacity);
        trailClock.start();
    }
    Entry &e = ring[head];
    e.ms = trailClock.elapsed();
    e.dmRow = dmRow;
    e.gui = gui;
    e.event = event;
    e.tag = currentTag;
    e.fPath = fPath;
    e.detail = detail;
    head = (head + 1) % kCapacity;
}

QStringList RowTrail::history(const QString &fPath)
{
    QStringList out;
    QMutexLocker lock(&mutex);
    if (ring.empty()) return out;
    for (int i = 0; i < kCapacity; ++i) {
        const Entry &e = ring[(head + i) % kCapacity];
        if (e.ms >= 0 && e.fPath == fPath) out << format(e);
    }
    return out;
}

RowTrail::Tag::Tag(const char *src) : prev(currentTag)
{
    currentTag = src;
}

RowTrail::Tag::~Tag()
{
    currentTag = prev;
}
