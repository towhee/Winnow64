#ifndef CACHEDATA_H
#define CACHEDATA_H

#include <QtWidgets>

class ImageCacheData : public QObject
{
    Q_OBJECT
public:
    explicit ImageCacheData(QObject *);

    bool contains(const QString &key);
    void insert(const QString &key, const QImage &image);
    void remove(const QString &key);
    void clear();

    // image cache hash
    QHash<QString, QImage> imCache;

    QReadWriteLock rwLock;
};
#endif // CACHEDATA_H
