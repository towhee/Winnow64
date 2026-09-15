#ifndef LUTSTORE_H
#define LUTSTORE_H

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "Develop/lut3d.h"

/*
    Which film-look LUTs are installed, and reading the one that was chosen -- the
    discovery half of the look feature, and the twin of Develop/cameraprofilestore.
    Develop/lutparse reads a file; Develop/lut3d applies one; this finds them.

    A SESSION-LIFETIME SINGLETON for the same reason the profile store is: the index and
    every parsed table are worth exactly one build, and neither changes while the app
    runs.

    LOOKS ARE NOT SHIPPED WITH WINNOW. There is one root -- a "Looks" folder beside the
    user's camera profiles, created on first scan so it can be found at all -- and the
    user fills it with .cube or HaldCLUT files of their own. That is the whole install
    story, and it is deliberately not a preferences page.

    THE INDEX KEY IS THE FILE'S PATH UNDER ITS ROOT, minus the extension and with forward
    slashes: "Fuji/Classic Chrome". NOT the .cube TITLE, which is missing or duplicated
    across a pack often enough to collide, and not the bare basename, which collides the
    moment two packs ship a "Portra 400". Subfolders therefore group the list for free.

    AND IT IS RESOLVED BY LOOKUP, NEVER BY JOINING IT ONTO A ROOT. The key comes back
    from a sidecar, which is user-editable text; a store that built a path from it would
    hand "../../../etc/passwd" straight to QFile. lut() only ever returns a path the
    scan itself found.
*/
class LutStore : public QObject
{
    Q_OBJECT

public:
    /* One row of the Profile combo's Looks group. */
    struct Entry {
        QString key;        // "Fuji/Classic Chrome" -- what a sidecar stores
        QString label;      // what the combo shows (the key's last segment)
        QString path;       // the file it was found at
        QString title;      // the .cube TITLE, when it carries one; tooltip only
    };

    static LutStore &instance();

    /* Start the index sweep if it has not been started. Cheap and idempotent; call it
       whenever a Develop panel is about to want looks. */
    void ensureIndex();

    bool indexReady() const;

    /* Every installed look, sorted by key. Empty while the sweep is still running, and
       empty when nothing is installed -- the honest answer, not a placeholder. */
    QList<Entry> entries();

    /*
        The parsed table a key selects, or nullptr when the key no longer resolves -- a
        look the user deleted, or a sidecar carried from another machine. A caller that
        gets nullptr must render WITHOUT a look and say so, never silently substitute.

        BUILDS THE INDEX SYNCHRONOUSLY IF IT HAS NOT BEEN BUILT, which is the one place
        this store deliberately differs from CameraProfileStore. The raw decode path
        (ImageFormats/Raw/rawformat.cpp) runs on a worker with no panel to have called
        ensureIndex(), and answering "not ready" there would export the image without the
        look that the sidecar plainly asks for. Affordable here and not for profiles: a
        Looks folder holds tens of files and needs no per-file peek, where the profile
        sweep is ~4400.
    */
    std::shared_ptr<const Lut3d::Table> lut(const QString &key);

    /* The folders swept, in order. Public so a diagnostic and the panel can tell the
       user where to put files. */
    static QStringList roots();

signals:
    /* The index finished building. Emitted on the GUI thread. */
    void indexChanged();

private:
    explicit LutStore(QObject *parent = nullptr);

    void scan();                            // may run on a worker OR inline; see lut()

    mutable QMutex mutex;                   // guards index / parsed / ready / started
    QMutex scanMutex;                       // serialises scan() so it is never done twice
    QList<Entry> index;
    QHash<QString, std::shared_ptr<const Lut3d::Table>> parsed;   // key -> table
    bool started = false;
    bool ready = false;
};

#endif // LUTSTORE_H
