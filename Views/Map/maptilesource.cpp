#include "Views/Map/maptilesource.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

bool MapTileSource::Provider::isValid() const
{
    return urlTemplate.contains("{z}") && urlTemplate.contains("{x}")
           && urlTemplate.contains("{y}")
           && (urlTemplate.startsWith("https://") || urlTemplate.startsWith("http://"));
}

MapTileSource::MapTileSource(QObject *parent) : QObject(parent)
{
    nam = new QNetworkAccessManager(this);
    auto *disk = new QNetworkDiskCache(this);
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/maptiles";
    QDir().mkpath(dir);
    disk->setCacheDirectory(dir);
    disk->setMaximumCacheSize(500LL * 1024 * 1024);
    nam->setCache(disk);
    connect(nam, &QNetworkAccessManager::finished, this, &MapTileSource::finished);

    mem.setMaxCost(64 * 1024);              // KB: ~256 decoded 256px tiles
}

MapTileSource::~MapTileSource()
{
    /* Abort before the manager goes: a reply finishing into a half-destroyed source
       would call finished() on it. */
    disconnect(nam, nullptr, this, nullptr);
    for (QNetworkReply *r : std::as_const(inFlight)) r->abort();
}

void MapTileSource::setProvider(const Provider &p)
{
    if (p.urlTemplate == mProvider.urlTemplate && p.apiKey == mProvider.apiKey
        && p.maxZoom == mProvider.maxZoom) {
        mProvider.attribution = p.attribution;
        return;
    }
    mProvider = p;
    ++mGeneration;
    for (QNetworkReply *r : std::as_const(inFlight)) r->abort();
    inFlight.clear();
    queue.clear();
    failedAt.clear();
    mem.clear();
    setError(QString());
    emit tileReady();
}

quint64 MapTileSource::key(int z, int x, int y)
{
    return (static_cast<quint64>(z) << 56) | (static_cast<quint64>(x) << 28)
           | static_cast<quint64>(y);
}

void MapTileSource::unkey(quint64 k, int &z, int &x, int &y)
{
    z = static_cast<int>(k >> 56);
    x = static_cast<int>((k >> 28) & 0xFFFFFFF);
    y = static_cast<int>(k & 0xFFFFFFF);
}

QString MapTileSource::url(int z, int x, int y) const
{
    QString u = mProvider.urlTemplate;
    u.replace("{z}", QString::number(z));
    u.replace("{x}", QString::number(x));
    u.replace("{y}", QString::number(y));
    u.replace("{key}", mProvider.apiKey);
    return u;
}

void MapTileSource::beginFrame()
{
    frameWanted.clear();
}

QPixmap MapTileSource::cachedTile(int z, int x, int y) const
{
    if (const QPixmap *p = mem.object(key(z, x, y))) return *p;
    return QPixmap();
}

QPixmap MapTileSource::tile(int z, int x, int y)
{
    const quint64 k = key(z, x, y);
    if (const QPixmap *p = mem.object(k)) return *p;
    if (mProvider.isValid()) frameWanted.insert(k);
    return QPixmap();
}

void MapTileSource::endFrame(int z, double centreTileX, double centreTileY)
{
    /* Rebuild the queue from this frame's wants, nearest the centre first. Tiles already
       in flight are left to finish -- they land in the caches either way. */
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    queue.clear();
    for (quint64 k : std::as_const(frameWanted)) {
        if (inFlight.contains(k)) continue;
        auto f = failedAt.constFind(k);
        if (f != failedAt.constEnd() && now - *f < kRetryMs) continue;
        queue.append(k);
    }
    std::sort(queue.begin(), queue.end(), [&](quint64 a, quint64 b) {
        int za, xa, ya, zb, xb, yb;
        unkey(a, za, xa, ya);
        unkey(b, zb, xb, yb);
        if ((za == z) != (zb == z)) return za == z;
        const double da = std::hypot(xa + 0.5 - centreTileX, ya + 0.5 - centreTileY);
        const double db = std::hypot(xb + 0.5 - centreTileX, yb + 0.5 - centreTileY);
        return da < db;
    });
    pump();
}

void MapTileSource::pump()
{
    static const QByteArray userAgent =
        ("Winnow/" + QCoreApplication::applicationVersion()).toUtf8();
    while (inFlight.size() < kMaxInFlight && !queue.isEmpty()) {
        const quint64 k = queue.takeFirst();
        int z, x, y;
        unkey(k, z, x, y);
        QNetworkRequest req{QUrl(url(z, x, y))};
        req.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(20000);
        QNetworkReply *r = nam->get(req);
        r->setProperty("tileKey", k);
        r->setProperty("generation", mGeneration);
        inFlight.insert(k, r);
    }
}

void MapTileSource::finished(QNetworkReply *reply)
{
    reply->deleteLater();
    const quint64 k = reply->property("tileKey").toULongLong();
    if (reply->property("generation").toInt() != mGeneration) return;
    inFlight.remove(k);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QPixmap pm;
    if (reply->error() == QNetworkReply::NoError) {
        pm.loadFromData(reply->readAll());
    }
    if (pm.isNull()) {
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            failedAt.insert(k, QDateTime::currentMSecsSinceEpoch());
            if (status == 401 || status == 403)
                setError(tr("The map tile provider refused the request (HTTP %1). "
                            "Check the API key in Preferences > Map.").arg(status));
            else if (status == 429)
                setError(tr("The map tile provider is limiting requests (HTTP 429)."));
            else if (reply->error() != QNetworkReply::NoError)
                setError(tr("Map tiles could not be loaded: %1").arg(reply->errorString()));
            else
                setError(tr("The map tile provider sent something that is not an image. "
                            "Check the URL template in Preferences > Map."));
        }
    }
    else {
        failedAt.remove(k);
        mem.insert(k, new QPixmap(pm),
                   std::max(1LL, pm.width() * pm.height() * 4LL / 1024));
        setError(QString());
        emit tileReady();
    }
    pump();
}

void MapTileSource::setError(const QString &error)
{
    if (error == mLastError) return;
    mLastError = error;
    emit errorChanged(error);
}
