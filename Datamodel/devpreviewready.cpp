#include "Datamodel/devpreviewready.h"
#include "Datamodel/datamodel.h"
#include "Cache/devpreviewcache.h"
#include "Cache/imagedecoder.h"
#include "Utilities/versionkey.h"
#include <QFileInfo>
#include <QThreadPool>
#include <QTimer>
#include <QVector>

bool DevPreviewReady::isReady(const QString &rowKey, const QString &baseKey)
{
    if (rowKey.isEmpty()) return false;
    const int instance = dm->instance.load();
    if (instance != memoInstance) {
        memo.clear();
        pending.clear();
        inFlight.clear();
        memoInstance = instance;
    }

    const quint64 gen = DevPreviewCache::instance().generation();
    auto it = memo.constFind(rowKey);
    if (it == memo.cend()) {
        queue(rowKey, baseKey);
        return false;
    }
    // stale: keep showing the last answer while it is re-probed
    if (it->baseKey != baseKey || it->gen != gen) queue(rowKey, baseKey);
    return it->ready;
}

void DevPreviewReady::queue(const QString &rowKey, const QString &baseKey)
{
    if (inFlight.contains(rowKey)) return;
    pending.insert(rowKey, baseKey);
    if (flushQueued) return;
    flushQueued = true;
    /* Next event-loop pass, so one paint of the grid becomes one probe. */
    QTimer::singleShot(0, dm, [this]{ flush(); });
}

void DevPreviewReady::flush()
{
    flushQueued = false;
    if (pending.isEmpty()) return;

    struct Probe { QString rowKey, baseKey; bool ready = false; };
    QVector<Probe> batch;
    batch.reserve(pending.size());
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        batch.append({it.key(), it.value(), false});
        inFlight.insert(it.key());
    }
    pending.clear();

    const int instance = memoInstance;
    /* Taken BEFORE the probe: a put landing while it runs leaves these entries stale,
       so the next paint asks again rather than trusting an answer that predates it. */
    const quint64 gen = DevPreviewCache::instance().generation();

    QThreadPool::globalInstance()->start([this, batch, instance, gen]() mutable {
        for (Probe &p : batch) {
            /* The same key ImageDecoder::loadDevPreview asks the cache for, and the
               same answer, a reduced preview included: it is what the loupe shows. */
            const QString fPath = VersionKey::sourceOf(p.rowKey);
            const QString ext = QFileInfo(fPath).suffix().toLower();
            const QString key =
                ImageDecoder::devPreviewKeyFor(p.baseKey, p.rowKey, fPath, ext);
            p.ready = !key.isEmpty() &&
                      DevPreviewCache::instance().contains(p.rowKey, key.toLatin1(),
                                                           /*acceptReduced*/ true);
        }
        QMetaObject::invokeMethod(dm, [this, batch, instance, gen]() {
            if (instance != memoInstance || instance != dm->instance.load()) return;
            for (const Probe &p : batch) {
                inFlight.remove(p.rowKey);
                auto it = memo.find(p.rowKey);
                /* Repaint when the answer moved, or when it is a first true: an
                   unknown row was painted as false. */
                const bool changed = it == memo.end() ? p.ready : it->ready != p.ready;
                memo.insert(p.rowKey, {p.baseKey, gen, p.ready});
                if (changed) dm->scheduleVisibleEmit(dm->rowFromKey(p.rowKey));
            }
        }, Qt::QueuedConnection);
    });
}
