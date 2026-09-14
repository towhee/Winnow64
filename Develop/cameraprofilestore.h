#ifndef CAMERAPROFILESTORE_H
#define CAMERAPROFILESTORE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <memory>

#include "ImageFormats/Dcp/dcp.h"

/*
    Which camera profiles exist, and reading the one that was chosen -- the discovery half
    of Phase 4 (see notes/Documentation.txt, "Camera Profiles (DCP) -- Phase 4 Plan").
    ImageFormats/Dcp reads a file; Develop/cameraprofile turns one into a matrix; this
    finds them and hands them out.

    A SESSION-LIFETIME SINGLETON, because both halves of what it holds are worth exactly
    one build: the INDEX (which profile files exist, and which camera each is for) costs a
    ~0.5 s sweep of ~4400 files, and a PARSED profile costs ~0.5 ms and is then shared by
    every render of every image that selected it. Profiles do not change while the app runs,
    so neither is ever invalidated.

    THE INDEX IS BUILT ON A BACKGROUND THREAD and the panel is told when it lands. Half a
    second is under the one-second bar that demands a progress indicator, but it is well
    over the bar for a dropdown that must not stall the GUI when a raw is first opened.
    Until it lands, forModel() returns nothing and the Profile row shows only the built-in
    entry -- which is the honest answer, not a placeholder.

    THE INDEX READS ONLY TWO STRINGS PER FILE (Dcp::peek): which camera the profile is for
    and what it is called. Parsing the lookup tables as well would cost ~2.4 s and megabytes
    for data a menu has no use for.

    PROFILES ARE NOT SHIPPED WITH WINNOW. Adobe's .dcp files are licensed for use with
    Adobe products (ProfileCopyright / ProfileEmbedPolicy say so), so they are READ where
    the user already has them installed and never copied into the application. The third
    root below is Winnow's own folder, where a user may drop profiles of their own -- made
    with dcamprof, with Adobe's DNG Profile Editor, or supplied by a camera maker.
*/
class CameraProfileStore : public QObject
{
    Q_OBJECT

public:
    /* One menu row: what to show, and what to open. */
    struct Entry {
        QString name;       // ProfileName -- "Adobe Standard", "Camera Vivid", ...
        QString path;       // the file; for a BASE, the file it is derived FROM
        /*
            A SYNTHESISED "Camera Base" -- the profile at `path` with its look stripped
            (no LookTable, no tone curve, no exposure offset), leaving only the sensor
            characterisation. See deriveBases() for why this is one entry and not
            a per-profile switch.
        */
        bool isBase = false;
    };

    static CameraProfileStore &instance();

    /* Start the index sweep if it has not been started. Cheap and idempotent; call it
       whenever a Develop panel is about to want profiles. */
    void ensureIndex();

    bool indexReady() const;

    /*
        Every profile that names this camera, plus the synthesised "Camera Base"
        entries, sorted by name. Empty while the index is still building, and empty for
        a camera nobody has made a profile for.

        NOT const, and not free the first time: deriving the bases needs the MATRICES,
        so it fully parses this camera's curve-bearing profiles (~9-18 of them, ~10 ms)
        rather than the two strings the index sweep reads. That cost is per camera per
        session and is cached; it deliberately does NOT ride the index sweep, which has
        to stay a ~0.5 s peek over ~4400 files.
    */
    QList<Entry> forModel(const QString &cameraModel);

    /* The parsed profile a (camera, profile name) pair selects, or nullptr when the name
       no longer matches anything -- a profile the user has since uninstalled, or a sidecar
       carried over from another machine. A caller that gets nullptr must render with the
       built-in matrix and SAY SO, not silently render as if nothing was chosen. */
    std::shared_ptr<const Dcp::Profile> profile(const QString &cameraModel,
                                                const QString &profileName);

    /* The folders swept, in order. Public so the preferences page and a diagnostic can
       show the user where profiles are looked for. */
    static QStringList roots();

signals:
    /* The index finished building. Emitted on the GUI thread. */
    void indexChanged();

private:
    explicit CameraProfileStore(QObject *parent = nullptr);

    /* Normalised camera-model key: upper case, trimmed, runs of whitespace collapsed.
       Model strings differ between a camera's EXIF and a profile's UniqueCameraModel by
       exactly that kind of noise. */
    static QString key(const QString &model);

    /* Longest-prefix lookup against the index, mirroring xyzToCamForModel: a profile for
       "Sony ILCE-9" also serves "Sony ILCE-9M2" when nothing matches the longer name
       exactly. Caller holds the mutex. matchedKey, when given, receives the index key
       that answered -- which is what the derived bases must be cached under, since a
       prefix match means it is not the key that was asked for. */
    const QList<Entry> *lookupLocked(const QString &model, QString *matchedKey = nullptr) const;

    /*
        Derive the "Camera Base" entries for one camera's profiles. Parses, so the caller
        must NOT hold the mutex.

        WHY A BASE IS ONE ENTRY AND NOT A SWITCH. Adobe ships ONE colorimetric base per
        camera generation with a different look bolted on top of each of its "Camera *"
        profiles: measured over the whole installed set, 2935 such profiles, of which only
        42 carry a HueSatMap, and every one of a generation is byte-identical once the look
        is removed. So "this profile without its look" is not a property of the profile at
        all -- it is one shared rendering that nine dropdown rows collapse onto.

        SELECTED BY CONTENT, NOT BY NAME OR FOLDER: a profile contributes a base when its
        look carries a TONE CURVE. That picks out exactly the "Camera *" family (Adobe
        Standard has a LookTable but no curve), keeps working for third-party profiles, and
        is the distinction that matters -- a curve is a whole tone mapping, so removing it
        is what makes the base a materially different rendering rather than a nuance.
    */
    static QList<Entry> deriveBases(const QList<Entry> &real);

    /* By name, so a synthesised base sits where the user expects to find it rather than
       tacked on after the real profiles. */
    static QList<Entry> sorted(QList<Entry> list);

    void scan();                            // runs on the worker thread

    mutable QMutex mutex;
    QHash<QString, QList<Entry>> index;     // normalised model -> its profiles
    QHash<QString, QList<Entry>> bases;     // index key -> its derived base entries
    QHash<QString, std::shared_ptr<const Dcp::Profile>> parsed;   // cache key -> profile
    bool started = false;
    bool ready = false;
};

#endif // CAMERAPROFILESTORE_H
