#ifndef TSHASH_H
#define TSHASH_H

#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutex>
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
//        QWriteLocker locker(&lock);
        mutex.lock();
        hash.clear();
        mutex.unlock();
    }


    void insert(const Key &key, const Value &value)
    {
//        QWriteLocker locker(&lock);
#ifdef ISTEST
        qDebug() << "MetaHash - Inserting key" << key
                 << "hash items" << hash.size();
#endif
        mutex.lock();
        hash.insert(key, value);
        mutex.unlock();
    }

    int remove(const Key &key)
    {
        QWriteLocker locker(&lock);
        return hash.remove(key);
    }


    int remove(const Key &key, const Value &value)
    {
//        QWriteLocker locker(&lock);
        mutex.lock();
        return hash.remove(key, value);
        mutex.unlock();
    }


    QList<Value> value(const Key &key) const
    {
        QReadLocker locker(&lock);
        return hash.value(key);
    }

    void takeOne(ImageMetadata *m, bool *more, bool *gotOne)
    {        
        if (hash.size() == 0) {
            mutex.lock();
            *more = false;
            mutex.unlock();
#ifdef ISTEST
            qDebug() << "No items in the MetaHash   more = false";
#endif
            return;
        }
//        QWriteLocker locker(&lock);
        QHashIterator<int, ImageMetadata> i(hash);
        if (i.hasNext()) {
            mutex.lock();
            i.next();
            *m = i.value();
            *gotOne = true;
#ifdef ISTEST
            qDebug() << "MetaHash - Removing hash row" << i.key()
                     << "DM row" << m.row
                     << "hash items remaining" << hash.size();
#endif
            hash.remove(i.key());
            mutex.unlock();
            return;
        }
//        *more = false;
     }

private:
    QMutex mutex;
    mutable QReadWriteLock lock;
    QHash<Key, Value> hash;
};

class ImageHash
{
public:
    explicit ImageHash() {}

    void clear()
    {
//        QWriteLocker locker(&lock);
        mutex.lock();
        hash.clear();
        mutex.unlock();
    }

    void insert(const int &key, const QImage &value)
    {
#ifdef ISTEST
        qDebug() << "IconHash - Inserting key" << key
                 << "hash items remaining" << hash.size();
#endif
        mutex.lock();
        hash.insert(key, value);
        mutex.unlock();
    }

    void takeOne(int *row, QImage *image, bool *more)
    {
        if (hash.size() == 0) {
            mutex.lock();
            *more = false;
            *row = -1;
            mutex.unlock();
#ifdef ISTEST
            qDebug() << "No items in the IconHash   more = false, row = -1";
#endif
            return;
        }
//        QWriteLocker locker(&lock);
        QHashIterator<int, QImage> i(hash);
        if (i.hasNext()) {
            mutex.lock();
            i.next();
            *row = i.key();
            *image = i.value();
            hash.remove(i.key());
            mutex.unlock();
#ifdef ISTEST
            qDebug() << "IconHash - Removing row" << i.key()
                     << "hash items remaining" << hash.size();
#endif
            return;
        }
//        *more = false;
//        *row = -1;
    }

private:
    QMutex mutex;
    mutable QReadWriteLock lock;
    QHash<int, QImage> hash;
};
#endif // TSHASH_H
