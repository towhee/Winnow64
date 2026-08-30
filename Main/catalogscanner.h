#ifndef CATALOGSCANNER_H
#define CATALOGSCANNER_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <atomic>

#include "Cache/catalog.h"

class Metadata;

/*
    Walks the folders the user has nominated for cataloguing and indexes what it finds,
    so search covers a library BEFORE the user has browsed it. See
    notes/Documentation.txt "Cataloguing Designated Roots".

    WHY IT IS SEPARATE FROM THE OPPORTUNISTIC CAPTURE. Opening a folder catalogues it
    (MW::folderChangeCompleted), which is free -- the metadata has just been read anyway.
    That only ever covers folders the user has actually visited, so a search on a fresh
    install finds nothing and there is no way to say "index my library". This is that way.

    IT READS METADATA AND NOTHING ELSE. No icon, no decode, no preview: a catalog row is
    text, and rendering anything would turn a background index into hours of GPU work.

    IT OWNS ITS OWN Metadata INSTANCE, created on the scanner thread -- the same rule
    Cache/reader.cpp follows, because Metadata holds one reusable ImageMetadata and one
    parser set and is not safe to share across threads.

    IT YIELDS TO THE USER, and that is the whole reason it is one thread rather than a
    pool. Browsing must not get slower because an index is being built: the scan pauses
    whenever the datamodel is being modified (a folder load is in progress) and abandons
    itself on G::stop. The same courtesy the devPreview builder shows by declining to run
    inside Develop.

    IT SKIPS WORK ALREADY DONE. Every folder is stat'd first and handed to
    Catalog::staleOf, so a rescan of an unchanged root costs one stat per file and no
    parsing at all. That is what makes "Scan now" cheap enough to offer as a button.

    NOTHING HERE IS AUTHORITATIVE. Like the rest of the catalog it only builds an index;
    it never writes to an image or a sidecar.
*/
class CatalogScanner : public QObject
{
    Q_OBJECT

public:
    explicit CatalogScanner(QObject *parent = nullptr);
    ~CatalogScanner() override;

    bool isRunning() const { return running.load(std::memory_order_relaxed); }

    /* This object lives here, so scan() never runs on the GUI thread. Owned rather than
       managed by MW, following Cache/metaread.h. */
    QThread scannerThread;

public slots:
    /* Scan these roots. Runs on whatever thread this object lives on, which MW makes a
       dedicated one -- never call it directly from the GUI thread. */
    void scan(const QStringList &roots, bool recurse);
    /* Ask the running scan to stop. Safe from any thread; the scan notices between
       files, so it ends promptly but not instantly. */
    void stop();

signals:
    /* done/total are FILES, updated per folder rather than per file: at a hundred
       thousand images a signal each would cost more than the indexing. */
    void progress(int done, int total);
    /* indexed = rows actually written (unchanged files are skipped, so this is usually
       far smaller than the number scanned). */
    void finished(int scanned, int indexed, bool aborted);
    void status(const QString &msg);

private:
    /* True when the scan should give way -- a folder load is running, or the app is
       shutting down. */
    bool shouldPause() const;
    /* Block while shouldPause(), returning false if we were asked to stop instead. */
    bool waitWhilePaused();

    /* Fill a CatalogRow from what is on disk WITHOUT parsing the image: path, folder,
       size, mtimes. That is everything staleOf needs to decide whether parsing is
       worth doing. */
    static CatalogRow stampOnly(const QString &fPath);
    /* Parse fPath and fill the rest of row. Returns false if the file could not be
       read, in which case it is not catalogued. */
    bool parseInto(CatalogRow &row);

    Metadata *metadata = nullptr;      // created lazily, on the scanner thread
    std::atomic<bool> abort{false};
    std::atomic<bool> running{false};
};

#endif // CATALOGSCANNER_H
