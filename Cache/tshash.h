#ifndef TSHASH_H
#define TSHASH_H

#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMultiHash>

//#include <QtCore>

#include "Metadata/imagemetadata.h"
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
//    QList<Value> values(const Key &key) const
//    {
//        QReadLocker locker(&lock);
//        return hash.values(key);
//    }


    ImageMetadata takeOne(bool *more)
    {
        Q_ASSERT(more);
        QWriteLocker locker(&lock);
        typename QHash<Key, Value>::const_iterator i =
                hash.constBegin();
        if (i == hash.constEnd()) {
            *more = false;
            ImageMetadata nullValue;
            return nullValue;
        }
        *more = true;
        const ImageMetadata value = hash.value(i.key());
        hash.remove(i.key());
        return value;
    }

//    const QList<Value> takeOne(bool *more)
//    {
//        Q_ASSERT(more);
//        QWriteLocker locker(&lock);
//        typename QMultiHash<Key, Value>::const_iterator i =
//                hash.constBegin();
//        if (i == hash.constEnd()) {
//            *more = false;
//            return QList<Value>();
//        }
//        *more = true;
//        const QList<Value> values = hash.values(i.key());
//        hash.remove(i.key());
//        return values;
//    }

private:
    mutable QReadWriteLock lock;
    QHash<Key, Value> hash;
//    QMultiHash<Key, Value> hash;
};

#endif // TSHASH_H
