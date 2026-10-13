#ifndef PMRIDCACHE_H
#define PMRIDCACHE_H

#include <QString>
#include <QAtomicInt>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <list>
#include <set>
#include "Develop/workingimage.h"

/*
    Process-wide cache of the FULL-STRENGTH PMRID base: the camera-native WorkingImage a
    raw decodes to with the denoiser run on its mosaic (RawFormat::Decode). It is
    amount-independent -- the Lum/Color amounts only scale Develop::BlendRawDenoise
    against the clean base -- so one entry serves every amount, every version of the
    file and every consumer.

    WHY IT EXISTS. Three paths each ran the ~7 s model on their own: the interactive
    denoise (MW::ensureRawDenoise), the ImageCache decode of an image carrying a saved
    "Denoise raw" recipe, and the export / devPreview render (MW::developPixelSource).
    Selecting a denoised image in Develop started the first two AT ONCE, so the same
    image was denoised twice concurrently, each run slowing the other (measured
    2026-10-13: two ~9 s decodes finishing 0.5 s apart).

    IN-FLIGHT DEDUPE. A producer claims with tryAcquire() (non-blocking) and, if the
    answer is InFlight, does its other work and then waits with acquire():
        Hit      already cached -- use it, run nothing;
        InFlight (tryAcquire only) another thread is making it -- call acquire() later;
        Claimed  nobody has it or is making it -- the caller runs PMRID and MUST end with
                 publish() or abandon() (Claim does that for it);
        Waited   another thread was making it -- acquire blocked until it was published
                 and returns it;
        Aborted  the caller's abort flag went up while waiting.
    A producer that abandons (aborted decode, PMRID could not run) wakes the waiters and
    one of them claims in its place, so an aborted ImageCache decode cannot strand the
    interactive denoise.

    KEY: the SOURCE file (a version key maps to its source, like WorkingImageCache) plus
    ISO, which conditions the noise model. Winnow engine only -- the callers consult it
    only on that engine, since the Apple engine has no mosaic to denoise.

    MEMORY: an entry is W*H*12 bytes (~600 MB at 50 MP). Byte-budgeted LRU; the most
    recent entry is never evicted, so the image being edited always hits. Entries are
    shared_ptr<const>: a holder keeps its pixels alive after eviction or clear().
*/
class PmridCache
{
public:
    static PmridCache &instance();

    enum class Result { Hit, Claimed, Waited, Aborted, InFlight };

    /* Non-blocking lookup; nullptr on a miss (or while it is still being made). */
    std::shared_ptr<const WorkingImage> get(const QString &fPath, int iso);

    /* Non-blocking claim: Hit (out set), Claimed, or InFlight -- another decode is making
       it; do other work, then acquire() to wait for it. RawFormat::Decode claims with
       this up front so a second decode overlaps its own clean demosaic with the producer
       instead of waiting before it unpacks. */
    Result tryAcquire(const QString &fPath, int iso, std::shared_ptr<const WorkingImage> &out);

    /* Blocking: Hit / Claimed as above, else wait until it is published (Waited, out
       set) or abandoned (then Claimed). abort (may be null) is polled while waiting. */
    Result acquire(const QString &fPath, int iso, const QAtomicInt *abort,
                   std::shared_ptr<const WorkingImage> &out);

    void publish(const QString &fPath, int iso, const std::shared_ptr<const WorkingImage> &img);
    void abandon(const QString &fPath, int iso);

    /* RAII for a Claimed acquire: abandons on destruction unless published, so every
       early return of the decode releases the claim. */
    class Claim {
    public:
        Claim() = default;
        Claim(const QString &fPath, int iso) : path(fPath), iso(iso), held(true) {}
        Claim(const Claim &) = delete;
        Claim &operator=(const Claim &) = delete;
        ~Claim() { if (held) PmridCache::instance().abandon(path, iso); }
        void publish(const std::shared_ptr<const WorkingImage> &img)
        {
            if (!held) return;
            PmridCache::instance().publish(path, iso, img);
            held = false;
        }
        bool isHeld() const { return held; }
    private:
        QString path;
        int iso = 0;
        bool held = false;
    };

    void remove(const QString &fPath);   // every ISO of one file (file changed on disk)
    void clear();                        // new folder; in-flight claims are unaffected
    int count() const;
    qint64 currentBytes() const;

private:
    PmridCache() = default;
    static QString keyOf(const QString &fPath, int iso);
    void evictLocked();

    struct Entry {
        QString key;
        std::shared_ptr<const WorkingImage> img;
        qint64 bytes = 0;
    };
    static constexpr qint64 kMaxBytes = 768LL * 1024 * 1024;   // ~1 entry at 50-60 MP

    mutable std::mutex mtx;
    std::condition_variable cv;
    std::list<Entry> lru;                // front = most recently used
    std::set<QString> inFlight;
    qint64 bytes = 0;
};

#endif // PMRIDCACHE_H
