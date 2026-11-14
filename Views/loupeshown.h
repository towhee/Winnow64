#ifndef LOUPESHOWN_H
#define LOUPESHOWN_H

#include <QString>

/*
    WHAT THE LOUPE IS SHOWING (Documentation 4.4.11).

    ImageView::currentImagePath names the last image whose pixels were painted, and a
    cache miss leaves it alone, so it cannot answer "is K on screen?". After A shown ->
    B missed (black) -> back to A, the "already displayed" shortcut fired for A over the
    black pixmap and nothing repainted it, though A was in the cache. The ingest probe
    and the self-test read the same field and counted that as shown.

    LoupeShown is updated on EVERY load attempt, hit or miss:

      Full         the cached image, or the Develop render of it
      Interim      the Develop stand-in (embedded JPG or devPreview) while the decode runs
      Placeholder  the thumbnail scaled up, on any other miss (Documentation 4.4.14)
      None         nothing: a miss with no thumbnail yet, or cleared

    Rules:
      - The shortcut (skip a load because the image is already up) applies only to a
        Full image of the same path, and only when the caller did not ask to replace.
      - A miss on the image already on screen keeps what is shown: the stand-in, or the
        Full image when a forced reload finds it gone from the cache. Black would stay up
        until the decode lands, which repaints it anyway.
      - A Develop render landing on the image whose stand-in is up makes it Full.
      - Only Full is "shown" for the current-image watchdog (MW::loupeWatchdog): a
        stand-in means the real pixels are still owed.

    GUI thread. Pure, so tests/unit/tst_loupeshown.cpp pins it.
*/
class LoupeShown
{
public:
    enum Kind { None, Full, Interim, Placeholder };

    bool isFull(const QString &path) const { return kind == Full && shown == path; }
    bool isShowing(const QString &path) const
        { return kind != None && !path.isEmpty() && shown == path; }

    // true: skip the load, the image is already on screen
    bool canSkipLoad(const QString &path, bool replace) const
        { return !replace && isFull(path); }

    void noteHit(const QString &path) { shown = path; kind = Full; }
    void noteInterim(const QString &path) { shown = path; kind = Interim; }
    void notePlaceholder(const QString &path) { shown = path; kind = Placeholder; }

    /*  A cache miss for path. Returns true if the caller should keep the pixmap it has
        (the same image is already on screen); false if it should blank it, in which
        case the state now says None. */
    bool noteMiss(const QString &path)
    {
        if (isShowing(path)) return true;
        shown = path;
        kind = None;
        return false;
    }

    // a Develop render of renderPath was painted
    void noteRender(const QString &renderPath)
        { if (kind != None && shown == renderPath) kind = Full; }

    void clear() { shown.clear(); kind = None; }

    const QString &path() const { return shown; }
    Kind state() const { return kind; }
    const char *stateName() const
    {
        switch (kind) {
        case Full:        return "Full";
        case Interim:     return "Interim";
        case Placeholder: return "Placeholder";
        default:          return "None";
        }
    }

private:
    QString shown;
    Kind kind = None;
};

#endif // LOUPESHOWN_H
