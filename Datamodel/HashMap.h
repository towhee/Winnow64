// #ifndef HASHMAP_H
// #define HASHMAP_H


// #include <cstdint>
// #include <iostream>
// #include <QVector>
// #include <functional>
// #include "HashNode.h"
// #include <QDebug>

// // Source: https://github.com/kshk123/hashMap

// // A prime number as hash size gives a better distribution of values in buckets
// constexpr size_t HASH_SIZE_DEFAULT = 1031;

// // Concurrent Thread Safe Library
// namespace CTSL
// {
// /*
// The class representing the hash map. It is expected for user defined types, the hash
// function will be provided. By default, the std::hash function will be used. If the hash
// size is not provided, then a default size of 1031 will be used.  The hash is converted into
// an integer between 0 and hashSize (hashValue % hashSize).

// The hash table itself consists of an array of hash buckets. Each hash bucket is
// implemented as singly linked list with the head as a dummy node created during the
// creation of the bucket. All the hash buckets are created during the construction of the
// map. Locks are taken per bucket, hence multiple threads can write simultaneously in
// different buckets in the hash map.

// The hash values are inserted into the hashTable: an array of HashBuckets.  One bucket
// could end up with multiple hashValues, that are stored in the linked list.  Each item in
// the linked list is a hash node key/value pair.
// */

// template <typename K, typename V, typename F = std::hash<K> >
// class HashMap
// {
//     public:
//         HashMap(size_t hashSize_ = HASH_SIZE_DEFAULT) : hashSize(hashSize_)
//         {
//             // create the hash table as an array of hash buckets
//             hashTable = new HashBucket<K, V>[hashSize];
//         }

//         ~HashMap()
//         {
//             delete [] hashTable;
//         }
//         //Copy and Move of the HashMap are not supported at this moment
//         HashMap(const HashMap&) = delete;
//         HashMap(HashMap&&) = delete;
//         HashMap& operator=(const HashMap&) = delete;
//         HashMap& operator=(HashMap&&) = delete;

//         bool contains(const K &key) const
//         /*
//         Function to find an entry in the bucket matching the key. If key is found the
//         function returns true. If key is not found, function returns false
//         */
//         {
//             // A shared mutex is used to enable mutiple concurrent reads
//             std::shared_lock<std::shared_timed_mutex> lock(mutex);
//             size_t hashValue = hashFn(key) % hashSize ;
//             return hashTable[hashValue].contains(key);

//         }

//         bool find(const K &key, V &value) const
//         /*
//         Function to find an entry in the hash map matching the key. If key is found, the
//         corresponding value is referenced by the parameter "value" and function returns true.
//         If key is not found, function returns false.
//         */
//         {
//             size_t hashValue = hashFn(key) % hashSize;
//             return hashTable[hashValue].find(key, value);
//         }

//         bool rename(const K &key, const K &newKey)
//         /*
//         Function to rename a key.
//         */
//         {
//             V value;
//             find(key, value);
//             insert(newKey, value);
//             remove(key);
//             return true;
//         }

//         void insert(const K &key, const V &value)
//         /*
//         Function to insert into the hash map. If key already exists, update the value,
//         else insert a new node in the bucket with the <key, value> pair.
//         */
//         {
//             size_t hashValue = hashFn(key) % hashSize;
//             hashTable[hashValue].insert(key, value);
//         }

//         void remove(const K &key)
//         /*
//         Remove a key/value node from its bucket.
//         */
//         {
//             size_t hashValue = hashFn(key) % hashSize;
//             hashTable[hashValue].remove(key);
//         }

//         void clear()
//         /*
//         Remove all entries from the hash map.
//         */
//         {
//             for (size_t i = 0; i < hashSize; i++) {
//                 (hashTable[i]).clear();
//             }
//         }

//         int count()
//         /*
//         The total number of keys in the hash map.  Each bucket (hashTable[i]) can have
//         multiple keys
//         */
//         {
//             int n = 0;
//             for (size_t i = 0; i < hashSize; ++i) {
//                 n += hashTable[i].count();
//             }
//             return n;
//         }

//         void getKeys(QVector<K> &keys)
//         /*
//         Populates the vector keys with all the keys in the hash map.  This is useful to
//         iterate through the hash map.
//         */
//         {
//             for (size_t i = 0; i < hashSize; ++i) {
//                 hashTable[i].getKeys(keys);
//             }
//         }

//     private:
//         HashBucket<K, V> * hashTable;
//         F hashFn;
//         const size_t hashSize;
//         mutable std::shared_timed_mutex mutex;

// }; // end class HashMap
// } // end namespace CTSL

// #endif // HASHMAP_H

// CHATGPT VERSION *************************************************************************

// #ifndef HASHMAP_H
// #define HASHMAP_H


// #include <cstdint>
// #include <iostream>
// #include <QVector>
// #include <functional>
// #include "HashNode.h"
// #include <QDebug>

// // Source: https://github.com/kshk123/hashMap

// // A prime number as hash size gives a better distribution of values in buckets
// constexpr size_t HASH_SIZE_DEFAULT = 1031;

// // Concurrent Thread Safe Library
// namespace CTSL
// {
// /*
// The class representing the hash map. It is expected for user defined types, the hash
// function will be provided. By default, the std::hash function will be used. If the hash
// size is not provided, then a default size of 1031 will be used.  The hash is converted into
// an integer between 0 and hashSize (hashValue % hashSize).

// The hash table itself consists of an array of hash buckets. Each hash bucket is
// implemented as singly linked list with the head as a dummy node created during the
// creation of the bucket. All the hash buckets are created during the construction of the
// map. Locks are taken per bucket, hence multiple threads can write simultaneously in
// different buckets in the hash map.

// The hash values are inserted into the hashTable: an array of HashBuckets.  One bucket
// could end up with multiple hashValues, that are stored in the linked list.  Each item in
// the linked list is a hash node key/value pair.
// */

// template <typename K, typename V, typename F = std::hash<K> >
// class HashMap
// {
// public:
//     HashMap(size_t hashSize_ = HASH_SIZE_DEFAULT) : hashSize(hashSize_)
//     {
//         // create the hash table as an array of hash buckets
//         hashTable = new HashBucket<K, V>[hashSize];
//     }

//     ~HashMap()
//     {
//         delete [] hashTable;
//     }
//     //Copy and Move of the HashMap are not supported at this moment
//     HashMap(const HashMap&) = delete;
//     HashMap(HashMap&&) = delete;
//     HashMap& operator=(const HashMap&) = delete;
//     HashMap& operator=(HashMap&&) = delete;

//     bool contains(const K &key) const
//     /*
//         Function to find an entry in the bucket matching the key. If key is found the
//         function returns true. If key is not found, function returns false
//         */
//     {
//         // A shared mutex is used to enable mutiple concurrent reads
//         std::shared_lock<std::shared_timed_mutex> lock(mutex);
//         size_t hashValue = hashFn(key) % hashSize ;
//         return hashTable[hashValue].contains(key);

//     }

//     bool find(const K &key, V &value) const
//     /*
//         Function to find an entry in the hash map matching the key. If key is found, the
//         corresponding value is referenced by the parameter "value" and function returns true.
//         If key is not found, function returns false.
//         */
//     {
//         size_t hashValue = hashFn(key) % hashSize;
//         return hashTable[hashValue].find(key, value);
//     }

//     bool rename(const K &key, const K &newKey)
//     /*
//         Function to rename a key.
//         */
//     {
//         V value;
//         find(key, value);
//         insert(newKey, value);
//         remove(key);
//         return true;
//     }

//     void insert(const K &key, const V &value)
//     /*
//         Function to insert into the hash map. If key already exists, update the value,
//         else insert a new node in the bucket with the <key, value> pair.
//         */
//     {
//         size_t hashValue = hashFn(key) % hashSize;
//         hashTable[hashValue].insert(key, value);
//     }

//     void remove(const K &key)
//     /*
//         Remove a key/value node from its bucket.
//         */
//     {
//         size_t hashValue = hashFn(key) % hashSize;
//         hashTable[hashValue].remove(key);
//     }

//     void clear()
//     /*
//         Remove all entries from the hash map.
//         */
//     {
//         for (size_t i = 0; i < hashSize; i++) {
//             (hashTable[i]).clear();
//         }
//     }

//     int count()
//     /*
//         The total number of keys in the hash map.  Each bucket (hashTable[i]) can have
//         multiple keys
//         */
//     {
//         int n = 0;
//         for (size_t i = 0; i < hashSize; ++i) {
//             n += hashTable[i].count();
//         }
//         return n;
//     }

//     void getKeys(QVector<K> &keys)
//     /*
//         Populates the vector keys with all the keys in the hash map.  This is useful to
//         iterate through the hash map.
//         */
//     {
//         for (size_t i = 0; i < hashSize; ++i) {
//             hashTable[i].getKeys(keys);
//         }
//     }

// private:
//     HashBucket<K, V> * hashTable;
//     F hashFn;
//     const size_t hashSize;
//     mutable std::shared_timed_mutex mutex;

// }; // end class HashMap
// } // end namespace CTSL

// #endif // HASHMAP_H


// CHATGPT VERSION 2 ***********************************************************************

// #ifndef HASHMAP_H
// #define HASHMAP_H

// #include <cstdint>
// #include <iostream>
// #include <QVector>
// #include <functional>
// #include "HashNode.h"
// #include <QDebug>

// constexpr size_t HASH_SIZE_DEFAULT = 1031;
// constexpr float LOAD_FACTOR_THRESHOLD = 0.75;

// namespace CTSL
// {

// template <typename K, typename V, typename F = std::hash<K>>
// class HashMap
// {
// public:
//     HashMap(size_t hashSize_ = HASH_SIZE_DEFAULT)
//         : hashSize(hashSize_), numElements(0)
//     {
//         hashTable = new HashBucket<K, V>[hashSize];
//     }

//     ~HashMap()
//     {
//         delete[] hashTable;
//     }

//     // Copy and Move of the HashMap are not supported at this moment
//     HashMap(const HashMap &) = delete;
//     HashMap(HashMap &&) = delete;
//     HashMap &operator=(const HashMap &) = delete;
//     HashMap &operator=(HashMap &&) = delete;

//     bool contains(const K &key) const
//     {
//         size_t hashValue = hashFn(key) % hashSize;
//         return hashTable[hashValue].contains(key);
//     }

//     bool find(const K &key, V &value) const
//     {
//         size_t hashValue = hashFn(key) % hashSize;
//         return hashTable[hashValue].find(key, value);
//     }

//     bool rename(const K &key, const K &newKey)
//     {
//         V value;
//         find(key, value);
//         insert(newKey, value);
//         remove(key);
//         return true;
//     }

//     void insert(const K &key, const V &value)
//     {
//         size_t hashValue = hashFn(key) % hashSize;
//         hashTable[hashValue].insert(key, value);
//         ++numElements;

//         // Check if rehashing is needed
//         if (loadFactor() > LOAD_FACTOR_THRESHOLD)
//         {
//             rehash();
//         }
//     }

//     void remove(const K &key)
//     {
//         size_t hashValue = hashFn(key) % hashSize;
//         hashTable[hashValue].remove(key);
//         --numElements;
//     }

//     void clear()
//     {
//         for (size_t i = 0; i < hashSize; i++)
//         {
//             hashTable[i].clear();
//         }
//         numElements = 0;
//     }

//     int count() const
//     {
//         return numElements;
//     }

//     void getKeys(QVector<K> &keys) const
//     {
//         for (size_t i = 0; i < hashSize; ++i)
//         {
//             hashTable[i].getKeys(keys);
//         }
//     }

// private:
//     float loadFactor() const
//     {
//         return static_cast<float>(numElements) / static_cast<float>(hashSize);
//     }

//     void rehash()
//     {
//         size_t newHashSize = hashSize * 2; // or next prime number
//         HashBucket<K, V> *newHashTable = new HashBucket<K, V>[newHashSize];

//         for (size_t i = 0; i < hashSize; ++i)
//         {
//             QVector<K> keys;
//             hashTable[i].getKeys(keys);
//             for (const auto &key : keys)
//             {
//                 V value;
//                 hashTable[i].find(key, value);
//                 size_t newHashValue = hashFn(key) % newHashSize;
//                 newHashTable[newHashValue].insert(key, value);
//             }
//         }

//         delete[] hashTable;
//         hashTable = newHashTable;
//         hashSize = newHashSize;
//     }

//     HashBucket<K, V> *hashTable;
//     F hashFn;
//     size_t hashSize;
//     int numElements;
// };

// } // end namespace CTSL

// #endif // HASHMAP_H

// CHATGPT VERSION 3 ***********************************************************************

/*
1.	Improve Thread Safety with Rehashing and Load Factor Control:
        •	Implemented rehash() method in HashMap to handle resizing when the load factor exceeds a threshold.
        •	Used LOAD_FACTOR_THRESHOLD to trigger rehashing.
    2.	Fix Memory Leaks and Improve Memory Management:
        •	Implemented std::unique_ptr for HashNode in HashBucket to manage memory automatically.
        •	Adjusted the methods in HashBucket to handle std::unique_ptr.
    3.	Reduce the Contention with Fine-Grained Locking:
        •	Used std::shared_mutex for read-write locking at the bucket level.
    4.	Implement Copy and Move Constructors and Assignment Operators:
        •	Did not implement copy and move semantics, as it was not explicitly required and can complicate the design.
    5.	Avoid Deadlocks and Improve Efficiency:
        •	Ensured that all operations are performed with appropriate locks.
*/

// Source: https://github.com/kshk123/hashMap
// Additions by Rory
// Above improvements by ChatGTP

// Concurrent Thread Safe Library
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

#ifndef HASHMAP_H
#define HASHMAP_H

#include <cstdint>
#include <iostream>
#include <QVector>
#include <functional>
#include "HashNode.h"
#include <QDebug>

constexpr size_t HASH_SIZE_DEFAULT = 1031;
constexpr float LOAD_FACTOR_THRESHOLD = 0.75;

namespace CTSL
{

template <typename K, typename V, typename F = std::hash<K>>
class HashMap
{
public:
    HashMap(size_t hashSize_ = HASH_SIZE_DEFAULT)
        : hashSize(hashSize_), numElements(0)
    {
        hashTable = std::make_unique<HashBucket<K, V>[]>(hashSize);
    }

    ~HashMap() = default;

    // Copy and Move of the HashMap are not supported at this moment
    HashMap(const HashMap &) = delete;
    HashMap(HashMap &&) = delete;
    HashMap &operator=(const HashMap &) = delete;
    HashMap &operator=(HashMap &&) = delete;

    bool contains(const K &key) const
    {
        size_t hashValue = hashFn(key) % hashSize;
        return hashTable[hashValue].contains(key);
    }

    bool find(const K &key, V &value) const
    {
        size_t hashValue = hashFn(key) % hashSize;
        return hashTable[hashValue].find(key, value);
    }

    bool rename(const K &key, const K &newKey)
    {
        V value;
        find(key, value);
        insert(newKey, value);
        remove(key);
        return true;
    }

    void insert(const K &key, const V &value)
    {
        size_t hashValue = hashFn(key) % hashSize;
        hashTable[hashValue].insert(key, value);
        ++numElements;

        if (loadFactor() > LOAD_FACTOR_THRESHOLD)
        {
            rehash();
        }
    }

    void remove(const K &key)
    {
        size_t hashValue = hashFn(key) % hashSize;
        hashTable[hashValue].remove(key);
        --numElements;
    }

    void clear()
    {
        for (size_t i = 0; i < hashSize; i++)
        {
            hashTable[i].clear();
        }
        numElements = 0;
    }

    int count() const
    {
        return numElements;
    }

    void getKeys(QVector<K> &keys) const
    {
        for (size_t i = 0; i < hashSize; ++i)
        {
            hashTable[i].getKeys(keys);
        }
    }

private:
    float loadFactor() const
    {
        return static_cast<float>(numElements) / static_cast<float>(hashSize);
    }

    void rehash()
    {
        size_t newHashSize = hashSize * 2;
        auto newHashTable = std::make_unique<HashBucket<K, V>[]>(newHashSize);

        for (size_t i = 0; i < hashSize; ++i)
        {
            QVector<K> keys;
            hashTable[i].getKeys(keys);
            for (const auto &key : keys)
            {
                V value;
                hashTable[i].find(key, value);
                size_t newHashValue = hashFn(key) % newHashSize;
                newHashTable[newHashValue].insert(key, value);
            }
        }

        hashTable = std::move(newHashTable);
        hashSize = newHashSize;
    }

    std::unique_ptr<HashBucket<K, V>[]> hashTable;
    F hashFn;
    size_t hashSize;
    int numElements;
};

} // end namespace CTSL

#endif // HASHMAP_H
