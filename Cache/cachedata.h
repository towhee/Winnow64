#ifndef CACHEDATA_H
#define CACHEDATA_H

#include <QtWidgets>

class ImageCacheData : public QObject
{
    Q_OBJECT
public:
    explicit ImageCacheData(QObject *);

    // image cache hash
    QHash<QString, QImage> imCache;

};
#endif // CACHEDATA_H
