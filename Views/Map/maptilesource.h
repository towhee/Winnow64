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

    WHY A PROVIDER SETTING AND NOT A BUILT-IN URL. OpenStreetMap's own tile servers
    forbid an application's users pulling tiles from them, and Google's terms forbid
    Google tiles in a map that is not Google's (see "Map Module" in
    notes/Documentation.txt). So Winnow ships no provider: the user enters a URL
    template from a provider they have an account with (MapTiler, Stadia, Thunderforest,
    Mapbox ...), its key, and its attribution text, which MapView must draw.

    CACHING. Two levels. The QNetworkDiskCache (QStandardPaths::CacheLocation/maptiles)
    keeps what the provider sent, honouring its cache headers, so a place looked at
    before draws without the network. The QCache holds decoded pixmaps for painting.

    REQUESTS. MapView asks for every tile it wants each paint (tile()); a tile not in
    memory is QUEUED, not fetched at once. pump() starts at most kMaxInFlight requests,
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
        QString urlTemplate;        // {z} {x} {y} and optionally {key}
        QString apiKey;
        QString attribution;
        int maxZoom = 19;
        bool isValid() const;
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

    static constexpr int kMaxInFlight = 6;
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
