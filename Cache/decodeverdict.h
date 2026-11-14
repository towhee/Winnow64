#ifndef DECODEVERDICT_H
#define DECODEVERDICT_H

/*
    WHAT IMAGECACHE DOES WITH A FINISHED DECODE (ImageCache::okToCache; Documentation
    4.4.13).

    A decode can finish long after the request that started it. By then the model may
    be a new instance (folder, filter, sort, version), the cache may have been reloaded
    for a new mode (Preview / Develop, Decode Raw, preview source, colour management),
    or the file may have been invalidated (edited outside Winnow, a new Develop recipe,
    rewritten in place). Each of those makes the pixels wrong, and each used to be
    checked -- or not -- in a different place: a reload superseded nothing, so the old
    mode's decode landed after it and the correct decode was then thrown away as
    "already cached" (C26).

    The rules, in order:

      Stale       the model is a new instance, or the row's key is gone. The queue
                  that asked for it was rebuilt; drop it. Not an attempt at the image.
      Superseded  the cache was reloaded (generation) or the key invalidated since the
                  decode started. The image is still wanted, as it is NOW: drop the
                  pixels, give the attempt back, queue the row again.
      Accept      a good image.
      Defer       no embedded-JPG geometry yet (NoGeometry): wait for the backstop,
                  attempt given back (4.4.10).
      Retry       a transient failure (Undefined, Abort, InstanceClash): stays queued,
                  the attempt counts (the attempts cap bounds it).
      Fail        a terminal failure: off the queue.

    Pure, so tests/unit/tst_decodeverdict.cpp pins the order.
*/
namespace DecodeVerdict {

enum class StatusClass { Success, NoGeometry, Transient, Terminal };
enum class Verdict { Stale, Superseded, Accept, Defer, Retry, Fail };

struct Result {
    bool instanceCurrent = true;        // decoder's instance == the model's
    bool keyInModel = true;             // the decoded key is still in the snapshot
    bool generationCurrent = true;      // no cache reload since the decode started
    bool superseded = false;            // the key was invalidated since it started
    StatusClass status = StatusClass::Success;
};

inline Verdict judge(const Result &r)
{
    if (!r.instanceCurrent || !r.keyInModel) return Verdict::Stale;
    if (!r.generationCurrent || r.superseded) return Verdict::Superseded;
    switch (r.status) {
    case StatusClass::Success:    return Verdict::Accept;
    case StatusClass::NoGeometry: return Verdict::Defer;
    case StatusClass::Transient:  return Verdict::Retry;
    case StatusClass::Terminal:   return Verdict::Fail;
    }
    return Verdict::Fail;
}

// not an attempt at the image: the attempt is given back
inline bool givesAttemptBack(Verdict v)
{
    return v == Verdict::Stale || v == Verdict::Superseded || v == Verdict::Defer;
}

inline const char *name(Verdict v)
{
    switch (v) {
    case Verdict::Stale:      return "Stale";
    case Verdict::Superseded: return "Superseded";
    case Verdict::Accept:     return "Accept";
    case Verdict::Defer:      return "Defer";
    case Verdict::Retry:      return "Retry";
    case Verdict::Fail:       return "Fail";
    }
    return "?";
}

} // namespace DecodeVerdict

#endif // DECODEVERDICT_H
