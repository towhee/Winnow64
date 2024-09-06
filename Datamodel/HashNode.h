// #ifndef HASHNODE_H
// #define HASHNODE_H


// #include <shared_mutex>
// #include <QVector>
// #include <QDebug>

// // Source: https://github.com/kshk123/hashMap

// namespace CTSL  // Concurrent Thread Safe Library
// {

// template <typename K, typename V>
// class HashNode
// /*
// Class representing a templatized hash node
// */
// {
//     public:
//         HashNode() : next(nullptr)
//         {}
//         HashNode(K key_, V value_) : next(nullptr), key(key_), value(value_)
//         {}
//         ~HashNode()
//         {
//             next = nullptr;
//         }

//         void setKey(K key_) {key = key_;}
//         const K& getKey() const {return key;}
//         void setValue(V value_) {value = value_;}
//         const V& getValue() const {return value;}

//         HashNode *next;             // Pointer to the next node in the same bucket

//     private:
//         K key;                      // the hash key
//         V value;                    // the value corresponding to the key

// }; // end class HashNode

// template <typename K, typename V>
// class HashBucket
// /*
// Class representing a hash bucket. The bucket is implemented as a singly linked list. A
// bucket is always constructed with a dummy head node
// */
// {
//     public:
//         HashBucket() : head(nullptr)
//         {}

//         ~HashBucket() //delete the bucket
//         {
//             clear();
//         }

//         int count()
//         /*
//         Return number of keys in the bucket
//         */
//         {
//             // A shared mutex is used to enable multiple concurrent reads
//             std::shared_lock<std::shared_timed_mutex> lock(mutex_);
//             HashNode<K, V> * node = head;
//             int n = 0;
//             while (node != nullptr) {
//                 n++;
//                 node = node->next;
//             }
//             return n;
//         }

//         void getKeys(QVector<K> &keys)
//         /*
//         Return all the keys in the bucket
//         */
//         {
//             // A shared mutex is used to enable mutiple concurrent reads
//             std::shared_lock<std::shared_timed_mutex> lock(mutex_);
//             HashNode<K, V> * node = head;

//             while (node != nullptr) {
//                 keys.append(node->getKey());
//                 node = node->next;
//             }
//         }

//         bool contains(const K &key) const
//         /*
//         Function to find an entry in the bucket matching the key. If key is found the
//         function returns true. If key is not found, function returns false
//         */
//         {
//             // A shared mutex is used to enable multiple concurrent reads
//             std::shared_lock<std::shared_timed_mutex> lock(mutex_);
//             HashNode<K, V> * node = head;

//             while (node != nullptr) {
//                 if (node->getKey() == key) {
//                     return true;
//                 }
//                 node = node->next;
//             }
//             return false;
//         }

//         bool find(const K &key, V &value) const
//         /*
//         Function to find an entry in the bucket matching the key. If key is found, the
//         corresponding value is copied into the parameter "value" and function returns
//         true. If key is not found, function returns false.
//         */
//         {
//             // A shared mutex is used to enable mutiple concurrent reads
//             std::shared_lock<std::shared_timed_mutex> lock(mutex_);
//             HashNode<K, V> * node = head;

//             while (node != nullptr) {
//                 if (node->getKey() == key) {
//                     value = node->getValue();
//                     return true;
//                 }
//                 node = node->next;
//             }
//             return false;
//         }

//         void insert(const K &key, const V &value)
//         /*
//         Function to insert into the bucket. If key already exists, update the value, else
//         insert a new node in the bucket with the <key, value> pair.
//         */
//         {
//             //Exclusive lock to enable single write in the bucket
//             std::unique_lock<std::shared_timed_mutex> lock(mutex_);
//             HashNode<K, V> * prev = nullptr;
//             HashNode<K, V> * node = head;

//             while (node != nullptr && node->getKey() != key) {
//                 prev = node;
//                 node = node->next;
//             }

//             // New entry, create a node and add to bucket
//             if (nullptr == node)
//             {
//                 if (nullptr == head) {
//                     head = new HashNode<K, V>(key, value);
//                 }
//                 else {
//                     prev->next = new HashNode<K, V>(key, value);
//                 }
//             }
//             else {
//                 node->setValue(value); // Key found in bucket, update the value
//             }

//         }

//         void remove(const K &key)
//         /*
//         Function to remove an entry from the bucket, if found.
//         */
//         {
//             //Exclusive lock to enable single write in the bucket
//             std::unique_lock<std::shared_timed_mutex> lock(mutex_);
//             HashNode<K, V> *prev  = nullptr;
//             HashNode<K, V> * node = head;

//             while (node != nullptr && node->getKey() != key) {
//                 prev = node;
//                 node = node->next;
//             }

//             // Key not found, nothing to be done
//             if (nullptr == node) {
//                 return;
//             }

//             // Remove the node from the bucket
//             else {
//                 if (head == node) {
//                     head = node->next;
//                 }
//                 else {
//                     prev->next = node->next;
//                 }
//                 delete node; //Free up the memory
//             }
//         }

//         void clear()
//         /*
//         clear the bucket
//         */
//         {
//             //Exclusive lock to enable single write in the bucket
//             std::unique_lock<std::shared_timed_mutex> lock(mutex_);
//             HashNode<K, V> * prev = nullptr;
//             HashNode<K, V> * node = head;
//             while (node != nullptr) {
//                 prev = node;
//                 node = node->next;
//                 delete prev;
//             }
//             head = nullptr;
//         }

//     private:
//         // The head node of the bucket
//         HashNode<K, V> * head;
//         // The mutex for this bucket
//         mutable std::shared_timed_mutex mutex_;

// }; // end class HashBucket
// } // end namespace CTSL

// #endif // HASHNODE_H

// CHATGPT VERSION *************************************************************************

// #ifndef HASHNODE_H
// #define HASHNODE_H

// #include <shared_mutex>
// #include <QVector>
// #include <QDebug>
// #include <memory>

// // Source: https://github.com/kshk123/hashMap

// namespace CTSL  // Concurrent Thread Safe Library
// {

// template <typename K, typename V>
// class HashNode
// /*
// Class representing a templatized hash node
// */
// {
// public:
//     HashNode() : next(nullptr)
//     {}
//     HashNode(K key_, V value_) : next(nullptr), key(key_), value(value_)
//     {}
//     ~HashNode()
//     {
//         next = nullptr;
//     }

//     void setKey(K key_) {key = key_;}
//     const K& getKey() const {return key;}
//     void setValue(V value_) {value = value_;}
//     const V& getValue() const {return value;}

//     std::shared_ptr<HashNode<K, V>> next; // Pointer to the next node in the same bucket

// private:
//     K key;                      // the hash key
//     V value;                    // the value corresponding to the key

// }; // end class HashNode

// template <typename K, typename V>
// class HashBucket
// /*
// Class representing a hash bucket. The bucket is implemented as a singly linked list.
// */
// {
// public:
//     HashBucket() : head(nullptr)
//     {}

//     ~HashBucket() //delete the bucket
//     {
//         clear();
//     }

//     int count() const
//     /*
//         Return number of keys in the bucket
//         */
//     {
//         std::shared_lock<std::shared_mutex> lock(mutex_);
//         auto node = head;
//         int n = 0;
//         while (node != nullptr) {
//             n++;
//             node = node->next;
//         }
//         return n;
//     }

//     void getKeys(QVector<K> &keys) const
//     /*
//         Return all the keys in the bucket
//         */
//     {
//         std::shared_lock<std::shared_mutex> lock(mutex_);
//         auto node = head;

//         while (node != nullptr) {
//             keys.append(node->getKey());
//             node = node->next;
//         }
//     }

//     bool contains(const K &key) const
//     /*
//         Function to find an entry in the bucket matching the key. If key is found the
//         function returns true. If key is not found, function returns false
//         */
//     {
//         std::shared_lock<std::shared_mutex> lock(mutex_);
//         auto node = head;

//         while (node != nullptr) {
//             if (node->getKey() == key) {
//                 return true;
//             }
//             node = node->next;
//         }
//         return false;
//     }

//     bool find(const K &key, V &value) const
//     /*
//         Function to find an entry in the bucket matching the key. If key is found, the
//         corresponding value is copied into the parameter "value" and function returns
//         true. If key is not found, function returns false.
//         */
//     {
//         std::shared_lock<std::shared_mutex> lock(mutex_);
//         auto node = head;

//         while (node != nullptr) {
//             if (node->getKey() == key) {
//                 value = node->getValue();
//                 return true;
//             }
//             node = node->next;
//         }
//         return false;
//     }

//     void insert(const K &key, const V &value)
//     /*
//         Function to insert into the bucket. If key already exists, update the value, else
//         insert a new node in the bucket with the <key, value> pair.
//     */
//     {
//         std::unique_lock<std::shared_mutex> lock(mutex_);
//         auto prev = head;
//         auto node = head;

//         while (node != nullptr && node->getKey() != key) {
//             prev = node;
//             node = node->next;
//         }

//         if (nullptr == node)
//         {
//             auto newNode = std::make_shared<HashNode<K, V>>(key, value);
//             if (nullptr == head) {
//                 head = newNode;
//             }
//             else {
//                 prev->next = newNode;
//             }
//         }
//         else {
//             node->setValue(value);
//         }
//     }

//     void remove(const K &key)
//     /*
//         Function to remove an entry from the bucket, if found.
//     */
//     {
//         std::unique_lock<std::shared_mutex> lock(mutex_);
//         auto prev = head;
//         auto node = head;

//         while (node != nullptr && node->getKey() != key) {
//             prev = node;
//             node = node->next;
//         }

//         if (nullptr == node) {
//             return;
//         }
//         else {
//             if (head == node) {
//                 head = node->next;
//             }
//             else {
//                 prev->next = node->next;
//             }
//         }
//     }

//     void clear()
//     /*
//         Clear the bucket
//     */
//     {
//         std::unique_lock<std::shared_mutex> lock(mutex_);
//         head = nullptr;
//     }

// private:
//     std::shared_ptr<HashNode<K, V>> head;
//     mutable std::shared_mutex mutex_;

// }; // end class HashBucket
// } // end namespace CTSL

// #endif // HASHNO

// CHATGPT VERSION 2 ***********************************************************************
// No changes

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

#ifndef HASHNODE_H
#define HASHNODE_H

#include <memory>
#include <shared_mutex>
#include <QVector>
#include <QDebug>

namespace CTSL  // Concurrent Thread Safe Library
{

template <typename K, typename V>
class HashNode
{
public:
    HashNode(K key_, V value_) : key(key_), value(value_)
    {}

    const K& getKey() const { return key; }
    const V& getValue() const { return value; }
    void setValue(V value_) { value = value_; }

    std::unique_ptr<HashNode> next;  // Pointer to the next node in the same bucket

private:
    K key;  // the hash key
    V value;  // the value corresponding to the key
};

template <typename K, typename V>
class HashBucket
{
public:
    HashBucket() = default;
    ~HashBucket() = default;

    int count() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        int n = 0;
        auto node = head.get();
        while (node != nullptr)
        {
            n++;
            node = node->next.get();
        }
        return n;
    }

    void getKeys(QVector<K>& keys) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto node = head.get();
        while (node != nullptr)
        {
            keys.append(node->getKey());
            node = node->next.get();
        }
    }

    bool contains(const K& key) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto node = head.get();
        while (node != nullptr)
        {
            if (node->getKey() == key)
            {
                return true;
            }
            node = node->next.get();
        }
        return false;
    }

    bool find(const K& key, V& value) const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto node = head.get();
        while (node != nullptr)
        {
            if (node->getKey() == key)
            {
                value = node->getValue();
                return true;
            }
            node = node->next.get();
        }
        return false;
    }

    void insert(const K& key, const V& value)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto node = head.get();
        HashNode<K, V>* prev = nullptr;
        while (node != nullptr && node->getKey() != key)
        {
            prev = node;
            node = node->next.get();
        }

        if (node == nullptr)
        {
            auto newNode = std::make_unique<HashNode<K, V>>(key, value);
            if (prev == nullptr)
            {
                head = std::move(newNode);
            }
            else
            {
                prev->next = std::move(newNode);
            }
        }
        else
        {
            node->setValue(value);
        }
    }

    void remove(const K& key)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        HashNode<K, V>* prev = nullptr;
        auto node = head.get();

        while (node != nullptr && node->getKey() != key)
        {
            prev = node;
            node = node->next.get();
        }

        if (node == nullptr)
        {
            return;
        }

        if (prev == nullptr)
        {
            head = std::move(node->next);
        }
        else
        {
            prev->next = std::move(node->next);
        }
    }

    void clear()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        head.reset();
    }

private:
    std::unique_ptr<HashNode<K, V>> head;
    mutable std::shared_mutex mutex_;
};

} // end namespace CTSL

#endif // HASHNODE_H
