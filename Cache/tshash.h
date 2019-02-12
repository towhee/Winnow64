#ifndef TSHASH_H
#define TSHASH_H

#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMultiHash>
#include <QImage>
#include "Metadata/imagemetadata.h"
#include "Main/global.h"

// ASync

template<typename Key, typename Value>
class TSHash
{
public:
    explicit TSHash() {}

    bool contains(const Key &key) const
    {
        QReadLocker locker(&lock);
        return hash.contains(key);
    }


    int count() const
    {
        QReadLocker locker(&lock);
        return hash.count();
    }


    int count(const Key &key) const
    {
        QReadLocker locker(&lock);
        return hash.count(key);
    }


    bool isEmpty() const
    {
        QReadLocker locker(&lock);
        return hash.isEmpty();
    }


    void clear()
    {
        QWriteLocker locker(&lock);
        hash.clear();
    }


    void insert(const Key &key, const Value &value)
    {
        QWriteLocker locker(&lock);
        qDebug() << "MetaHash - Inserting key" << key
                    << "hash items" << hash.size();
        hash.insert(key, value);
    }

    int remove(const Key &key)
    {
        QWriteLocker locker(&lock);
        return hash.remove(key);
    }


    int remove(const Key &key, const Value &value)
    {
        QWriteLocker locker(&lock);
        return hash.remove(key, value);
    }


    QList<Value> value(const Key &key) const
    {
        QReadLocker locker(&lock);
        return hash.value(key);
    }

    void takeOne(ImageMetadata &m, bool &more, bool &gotOne)
    {
        if (hash.size() == 0) {
            more = false;
            qDebug() << "No items in the MetaHash   more = false";
            return;
        }
        QWriteLocker locker(&lock);
        QHashIterator<int, ImageMetadata> i(hash);
        if (i.hasNext()) {
            i.next();
            m = i.value();
            gotOne = true;
            qDebug() << "MetaHash - Removing hash row" << i.key()
                     << "DM row" << m.row
                     << "hash items remaining" << hash.size();
            hash.remove(i.key());
            return;
        }
        more = false;
     }

private:
    mutable QReadWriteLock lock;
    QHash<Key, Value> hash;
};

class ImageHash
{
public:
    explicit ImageHash() {}

    void clear()
    {
        QWriteLocker locker(&lock);
        hash.clear();
    }

    void insert(const int &key, const QImage &value)
    {
        qDebug() << "IconHash - Inserting key" << key
                 << "hash items remaining" << hash.size();
        hash.insert(key, value);
    }

    void takeOne(int &row, QImage &image, bool &more)
    {
        if (hash.size() == 0) {
            more = false;
            row = -1;
            qDebug() << "No items in the IconHash   more = false, row = -1";
            return;
        }
        QWriteLocker locker(&lock);
        QHashIterator<int, QImage> i(hash);
        if (i.hasNext()) {
            i.next();
            row = i.key();
            image = i.value();
            hash.remove(i.key());
            qDebug() << "IconHash - Removing row" << i.key()
                     << "hash items remaining" << hash.size();
            return;
        }
        more = false;
        row = -1;
    }

private:
    mutable QReadWriteLock lock;
    QHash<int, QImage> hash;
};
#endif // TSHASH_H
