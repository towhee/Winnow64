#ifndef CACHEDATA_H
#define CACHEDATA_H

#include <QtWidgets>
#include <atomic>

class ImageCacheData : public QObject
{
    Q_OBJECT
public:
    explicit ImageCacheData(QObject *);

    bool contains(const QString &key);
    void insert(const QString &key, const QImage &image);
    void remove(const QString &key);
    void clear();

    // O(1) read, no lock needed
    quint64 sizeBytes() const noexcept {
        return bytes.load(std::memory_order_relaxed);
    }
    // quint64 sizeMB() const noexcept {
    //     return static_cast<quint64>(sizeBytes() / (1024 * 1024));
    // }
    double  sizeMB() const noexcept {
        return double(sizeBytes()) / (1024.0 * 1024.0);
    }

    // image cache hash
    QHash<QString, QImage> imCache;

    QReadWriteLock rwLock;

private:
    std::atomic<quint64> bytes{0}; // total pixel bytes of images
};
#endif // CACHEDATA_H
