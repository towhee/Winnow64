#ifndef HASHNODE_H
#define HASHNODE_H


#include <shared_mutex>
#include <QVector>
#include <QDebug>

// Source: https://github.com/kshk123/hashMap

namespace CTSL  // Concurrent Thread Safe Library
{

template <typename K, typename V>
class HashNode
/*
Class representing a templatized hash node
*/
{
    public:
        HashNode() : next(nullptr)
        {}
        HashNode(K key_, V value_) : next(nullptr), key(key_), value(value_)
        {}
        ~HashNode()
        {
            next = nullptr;
        }

        void setKey(K key_) {key = key_;}
        const K& getKey() const {return key;}
        void setValue(V value_) {value = value_;}
        const V& getValue() const {return value;}

        HashNode *next;             // Pointer to the next node in the same bucket
    private:
        K key;                      // the hash key
        V value;                    // the value corresponding to the key

}; // end class HashNode

template <typename K, typename V>
class HashBucket
/*
Class representing a hash bucket. The bucket is implemented as a singly linked list. A
bucket is always constructed with a dummy head node
*/
{
    public:
        HashBucket() : head(nullptr)
        {}

        ~HashBucket() //delete the bucket
        {
            clear();
        }

        int count()
        /*
        Return number of keys in the bucket
        */
        {
            // A shared mutex is used to enable mutiple concurrent reads
            std::shared_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> * node = head;
            int n = 0;
            while (node != nullptr) {
                n++;
                node = node->next;
            }
            return n;
        }

        void getKeys(QVector<K> &keys)
        /*
        Return all the keys in the bucket
        */
        {
            // A shared mutex is used to enable mutiple concurrent reads
            std::shared_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> * node = head;

            while (node != nullptr) {
                keys.append(node->getKey());
                node = node->next;
            }
        }

        bool contains(const K &key) const
        /*
        Function to find an entry in the bucket matching the key. If key is found the
        function returns true. If key is not found, function returns false
        */
        {
            // A shared mutex is used to enable mutiple concurrent reads
            std::shared_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> * node = head;

            while (node != nullptr) {
                if (node->getKey() == key) {
                    return true;
                }
                node = node->next;
            }
            return false;
        }

        bool find(const K &key, V &value) const
        /*
        Function to find an entry in the bucket matching the key. If key is found, the
        corresponding value is copied into the parameter "value" and function returns
        true. If key is not found, function returns false.
        */
        {
            // A shared mutex is used to enable mutiple concurrent reads
            std::shared_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> * node = head;

            while (node != nullptr) {
                if (node->getKey() == key) {
                    value = node->getValue();
                    return true;
                }
                node = node->next;
            }
            return false;
        }

        bool rename(const K &key, const K &newKey)
        /*

        */
        {
            // A shared mutex is used to enable mutiple concurrent reads
            std::shared_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> * node = head;

            while (node != nullptr) {
                if (node->getKey() == key) {
                    node->setKey(newKey);
                    return true;
                }
                node = node->next;
            }
            return false;
        }

        void insert(const K &key, const V &value)
        /*
        Function to insert into the bucket. If key already exists, update the value, else
        insert a new node in the bucket with the <key, value> pair.
        */
        {
            //Exclusive lock to enable single write in the bucket
            std::unique_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> * prev = nullptr;
            HashNode<K, V> * node = head;

            while (node != nullptr && node->getKey() != key) {
                prev = node;
                node = node->next;
            }

            // New entry, create a node and add to bucket
            if (nullptr == node)
            {
                if(nullptr == head) {
                    head = new HashNode<K, V>(key, value);
                }
                else {
                    prev->next = new HashNode<K, V>(key, value);
                }
            }
            else {
                node->setValue(value); //Key found in bucket, update the value
            }

        }

        void remove(const K &key)
        /*
        Function to remove an entry from the bucket, if found.
        */
        {
            //Exclusive lock to enable single write in the bucket
            std::unique_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> *prev  = nullptr;
            HashNode<K, V> * node = head;

            while (node != nullptr && node->getKey() != key) {
                prev = node;
                node = node->next;
            }

            // Key not found, nothing to be done
            if (nullptr == node) {
                return;
            }
            // Remove the node from the bucket
            else {
                if(head == node) {
                    head = node->next;
                }
                else {
                    prev->next = node->next;
                }
                delete node; //Free up the memory
            }
        }

        void clear()
        /*
        clear the bucket
        */
        {
            //Exclusive lock to enable single write in the bucket
            std::unique_lock<std::shared_timed_mutex> lock(mutex_);
            HashNode<K, V> * prev = nullptr;
            HashNode<K, V> * node = head;
            while(node != nullptr) {
                prev = node;
                node = node->next;
                delete prev;
            }
            head = nullptr;
        }

    private:
        // The head node of the bucket
        HashNode<K, V> * head;
        // The mutex for this bucket
        mutable std::shared_timed_mutex mutex_;

}; // end class HashBucket
} // end namespace CTSL

#endif // HASHNODE_H
