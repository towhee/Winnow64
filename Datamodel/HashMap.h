#ifndef HASHMAP_H
#define HASHMAP_H


#include <cstdint>
#include <iostream>
#include <QVector>
#include <functional>
#include <mutex>
#include "HashNode.h"
#include <QDebug>

// Source: https://github.com/kshk123/hashMap

// A prime number as hash size gives a better distribution of values in buckets
constexpr size_t HASH_SIZE_DEFAULT = 1031;

// Concurrent Thread Safe Library
namespace CTSL
{
/*
The class representing the hash map. It is expected for user defined types, the hash
function will be provided. By default, the std::hash function will be used. If the hash
size is not provided, then a default size of 1031 will be used.  The hash is converted into
an integer between 0 and hashSize (hashValue % hashSize).

The hash table itself consists of an array of hash buckets. Each hash bucket is
implemented as singly linked list with the head as a dummy node created during the
creation of the bucket. All the hash buckets are created during the construction of the
map. Locks are taken per bucket, hence multiple threads can write simultaneously in
different buckets in the hash map.

The hash values are inserted into the hashTable: an array of HashBuckets.  One bucket
could end up with multiple hashValues, that are stored in the linked list.  Each item in
the linked list is a hash node key/value pair.
*/

template <typename K, typename V, typename F = std::hash<K> >
class HashMap
{
    public:
        HashMap(size_t hashSize_ = HASH_SIZE_DEFAULT) : hashSize(hashSize_)
        {
            // create the hash table as an array of hash buckets
            hashTable = new HashBucket<K, V>[hashSize];
        }

        ~HashMap()
        {
            delete [] hashTable;
        }
        //Copy and Move of the HashMap are not supported at this moment
        HashMap(const HashMap&) = delete;
        HashMap(HashMap&&) = delete;
        HashMap& operator=(const HashMap&) = delete;
        HashMap& operator=(HashMap&&) = delete;

        bool contains(const K &key) const
        /*
        Function to find an entry in the bucket matching the key. If key is found the
        function returns true. If key is not found, function returns false
        */
        {
            size_t hashValue = hashFn(key) % hashSize ;
            return hashTable[hashValue].contains(key);

        }

        bool find(const K &key, V &value) const
        /*
        Function to find an entry in the hash map matching the key. If key is found, the
        corresponding value is referenced the parameter "value" and function returns true.
        If key is not found, function returns false.
        */
        {
            size_t hashValue = hashFn(key) % hashSize;
            return hashTable[hashValue].find(key, value);
        }

        void insert(const K &key, const V &value)
        /*
        Function to insert into the hash map. If key already exists, update the value,
        else insert a new node in the bucket with the <key, value> pair.
        */
        {
            size_t hashValue = hashFn(key) % hashSize;
            hashTable[hashValue].insert(key, value);
        }

        void remove(const K &key)
        /*
        Remove a key/value node from its bucket.
        */
        {
            size_t hashValue = hashFn(key) % hashSize;
            hashTable[hashValue].remove(key);
        }

        void clear()
        /*
        Remove all entries from the hash map.
        */
        {
            for (size_t i = 0; i < hashSize; i++) {
                (hashTable[i]).clear();
            }
        }

        void getKeys(QVector<K> &keys)
        /*
        Populates the vector keys with all the keys in the hash map.  This is useful to
        iterate through the hash map.
        */
        {
            for (size_t i = 0; i < hashSize; ++i) {
                hashTable[i].getKeys(keys);
            }
        }

    private:
        HashBucket<K, V> * hashTable;
        F hashFn;
        const size_t hashSize;

}; // end class HashMap
} // end namespace CTSL

#endif // HASHMAP_H
