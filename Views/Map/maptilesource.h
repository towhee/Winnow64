#ifndef MAPTILESOURCE_H
#define MAPTILESOURCE_H

#include <QCache>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/*
    MAP TILE SOURCE: the raster tiles behind MapView, fetched from the tile provider the
    user sets in Preferences > Map.

    THE DEFAULT IS OPENSTREETMAP'S OWN TILES (Provider::openStreetMap): no key, no
    account, nothing to set up. Their tile usage policy allows an application like this
    on conditions Winnow meets: a User-Agent that names the app, the attribution drawn on
    the map, tiles cached locally, at most 2 connections (maxConnections), and no bulk
    or ahead-of-view downloading -- only the tiles a paint asks for are fetched. OSM can
    block an app that abuses the service, so do not relax any of those. Google is not an
    option at all (its terms forbid Google tiles in a map that is not Google's; see "Map
    Module" in notes/Documentation.txt).

    A DIFFERENT PROVIDER is optional: Preferences > Map takes a URL template, a key and
    the attribution text for one the user has an account with (MapTiler, Stadia ...).

    CACHING. Two levels. The QNetworkDiskCache (QStandardPaths::CacheLocation/maptiles)
    keeps what the provider sent, honouring its cache headers, so a place looked at
    before draws without the network. The QCache holds decoded pixmaps for painting.

    REQUESTS. MapView asks for every tile it wants each paint (tile()); a tile not in
    memory is QUEUED, not fetched at once. pump() starts at most maxConnections requests,
    nearest the view centre first, and drops queued tiles the last frame did not ask for
    -- a fast zoom would otherwise download every level it passed through.

    FAILURES. A tile that failed is not asked for again for kRetryMs, and the last error
    is kept (lastError) so the map can say WHY it is blank, ie a rejected key (HTTP 401
    or 403), instead of just staying grey.
*/

class MapTileSource : public QObject
{
    Q_OBJECT
public:
    struct Provider {
        QString urlTemplate;        // {z} {x} {y}, optionally {key} and {s} (a/b/c)
        QString apiKey;
        QString attribution;
        int maxZoom = 19;
        int maxConnections = 6;     // OpenStreetMap's policy allows 2
        bool isValid() const;
        /* The built-in styles, all keyless and all on volunteer-run servers that
           allow apps on OpenStreetMap's fair-use terms, hence 2 connections each. */
        static Provider openStreetMap();
        static Provider cyclOsm();
        static Provider openTopoMap();
    };

    explicit MapTileSource(QObject *parent = nullptr);
    ~MapTileSource() override;

    void setProvider(const Provider &p);
    const Provider &provider() const { return mProvider; }

    /* The frame protocol: beginFrame, then tile() for every tile the paint needs, then
       endFrame with the centre tile, which queues and starts requests. */
    void beginFrame();
    QPixmap tile(int z, int x, int y);                  // null if not in memory yet
    QPixmap cachedTile(int z, int x, int y) const;      // memory only, never queues
    void endFrame(int z, double centreTileX, double centreTileY);

    QString lastError() const { return mLastError; }

signals:
    void tileReady();
    void errorChanged(const QString &error);

private:
    static quint64 key(int z, int x, int y);
    static void unkey(quint64 k, int &z, int &x, int &y);
    QString url(int z, int x, int y) const;
    void pump();
    void finished(QNetworkReply *reply);
    void setError(const QString &error);

    static constexpr qint64 kRetryMs = 60000;

    Provider mProvider;
    QNetworkAccessManager *nam = nullptr;
    QCache<quint64, QPixmap> mem;
    QList<quint64> queue;                   // wanted and not requested, nearest first
    QSet<quint64> frameWanted;              // tiles the current frame asked for
    QHash<quint64, QNetworkReply *> inFlight;
    QHash<quint64, qint64> failedAt;        // msecs since epoch
    int mGeneration = 0;                    // setProvider bumps it: stale replies dropped
    QString mLastError;
};

#endif // MAPTILESOURCE_H
