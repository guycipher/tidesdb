/*
 * Copyright 2024 TidesDB
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */

#ifndef TIDESDB_LIBRARY_H
#define TIDESDB_LIBRARY_H

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <string>
#include <vector>

// We include the protobuf headers here because we need to use the
// generated files for our KeyValue and Operation structs
#include "proto/kv.pb.h"
#include "proto/operation.pb.h"

// The TidesDB namespace
namespace TidesDB {

class LSMT;  // Forward declaration of the LSMT class

// Constants
constexpr int PAGE_SIZE =
    (1024 * 4);  // Page size, as discussed on Reddit 1024*4 is a better page size.
constexpr int PAGE_HEADER_SIZE = sizeof(int64_t);             // 8 bytes for overflow page pointer
constexpr int PAGE_BODY_SIZE = PAGE_SIZE - PAGE_HEADER_SIZE;  // Page body size
const std::string SSTABLE_EXTENSION = ".sst";                 // SSTable file extension
constexpr const char *TOMBSTONE_VALUE = "$tombstone";         // Tombstone value
const std::string WAL_EXTENSION = ".wal";                     // Write-ahead log file extension

// ConvertToUint8Vector converts a vector of characters to a vector of unsigned
// 8-bit integers
std::vector<uint8_t> ConvertToUint8Vector(const std::vector<char> &input);

// ConvertToCharVector converts a vector of unsigned 8-bit integers to a vector
// of characters
std::vector<char> ConvertToCharVector(const std::vector<uint8_t> &input);

// TidesDBException is an exception class for TidesDB
class TidesDBException : public std::exception {
   private:
    std::string message;  // Exception message
   public:
    explicit TidesDBException(const std::string &msg) : message(msg) {}
    virtual const char *what() const noexcept override { return message.c_str(); }
};

// Operation types (for transactions)
enum class OperationType {
    OpPut,    // Put operation
    OpDelete  // Delete operation
};

// Transaction operation
struct TransactionOperation {
    Operation op;               // the operation
    struct Rollback *rollback;  // Rollback information
};

// Rollback information for a transaction operation
struct Rollback {
    OperationType type;          // Type of the operation (OpPut or OpDelete)
    std::vector<uint8_t> key;    // Key of the operation
    std::vector<uint8_t> value;  // Value of the operation (for OpPut)
};

// Transaction struct
struct Transaction {
    std::vector<TransactionOperation> operations;  // List of operations
    bool aborted = false;                          // Whether the transaction was aborted
    std::mutex operationsMutex;                    // Mutex for operations
};

// deserialize serializes the KeyValue struct to a byte vector
std::vector<uint8_t> serialize(const KeyValue &kv);

// deserialize deserializes a byte vector to a KeyValue struct
KeyValue deserialize(const std::vector<uint8_t> &buffer);

// serializeOperation serializes the Operation struct to a byte vector
std::vector<uint8_t> serializeOperation(const Operation &op);

// deserializeOperation deserializes a byte vector to an Operation struct
Operation deserializeOperation(const std::vector<uint8_t> &buffer);

// getPathSeparator Gets os specific path separator
std::string getPathSeparator();

// SkipListNode is a node in a skip list
class SkipListNode {
   public:
    // Each node contains a key, value, and an array of forward pointers
    std::vector<uint8_t> key;                          // Node key
    std::vector<uint8_t> value;                        // Node value
    std::vector<std::atomic<SkipListNode *>> forward;  // Forward pointers

    // SkipListNode Constructor
    SkipListNode(const std::vector<uint8_t> &k, const std::vector<uint8_t> &v, int level)
        : key(k), value(v), forward(level + 1) {
        for (int i = 0; i <= level; ++i) {                         // Initialize forward pointers
            forward[i].store(nullptr, std::memory_order_relaxed);  // Initialize to nullptr
        }
    }
};

// SkipList is a lock-free skip list class
class SkipList {
   private:
    int maxLevel;                          // Maximum level of the skip list
    float probability;                     // Probability of a node having a higher level
    std::atomic<int> level;                // Current level of the skip list
    std::shared_ptr<SkipListNode> head;    // Head node of the skip list
    std::mt19937 gen;                      // Random number generator
    std::uniform_real_distribution<> dis;  // Uniform distribution
    std::atomic<int> cachedSize;           // Size of the skip list

    // randomLevel generates a random level for a new node
    int randomLevel();

   public:
    // Constructor
    SkipList(int maxLevel, float probability)
        : maxLevel(maxLevel),
          probability(probability),
          level(0),
          gen(std::random_device{}()),
          dis(0, RAND_MAX),
          cachedSize(0) {
        head = std::make_unique<SkipListNode>(std::vector<uint8_t>(), std::vector<uint8_t>(),
                                              maxLevel);
    }

    // insert inserts a key-value pair into the skip list
    bool insert(const std::vector<uint8_t> &key, const std::vector<uint8_t> &value);

    // deleteKV deletes a key from the skip list
    void deleteKV(const std::vector<uint8_t> &key);

    // clearCache clears the cached size of the skip list
    void clearCache();

    // get gets the value for a given key
    std::vector<uint8_t> get(const std::vector<uint8_t> &key) const;

    // inOrderTraversal traverses the skip list in in-order traversal and calls a function on each
    // node
    void inOrderTraversal(
        std::function<void(const std::vector<uint8_t> &, const std::vector<uint8_t> &)> func) const;

    // getSize returns the number of nodes in the skip list
    int getSize() const;

    // clear clears the skip list
    void clear();
};

// AVL Node class
// @deprecated
class AVLNode {
   public:
    std::vector<uint8_t> key;    // Key
    std::vector<uint8_t> value;  // Value
    AVLNode *left;               // Left child
    AVLNode *right;              // Right child
    int height;                  // Height of the node

    // Constructor
    AVLNode(const std::vector<uint8_t> &k, const std::vector<uint8_t> &v)
        : key(k), value(v), left(nullptr), right(nullptr), height(1) {}
};

// AVL Tree class
// @deprecated
class AVLTree {
   private:
    AVLNode *root;             // Root node
    std::shared_mutex rwlock;  // Read-write lock

    // rightRotate rotates the AVL tree to the right
    // @deprecated
    static AVLNode *rightRotate(AVLNode *y);

    // leftRotate rotates the AVL tree to the left
    // @deprecated
    static AVLNode *leftRotate(AVLNode *x);

    // getBalance gets the balance factor of a node
    // @deprecated
    static int getBalance(AVLNode *node);

    // insert inserts a key-value pair into the AVL tree
    // @deprecated
    AVLNode *insert(AVLNode *node, const std::vector<uint8_t> &key,
                    const std::vector<uint8_t> &value);

    // printHex prints a vector of unsigned 8-bit integers in hexadecimal format
    // @deprecated
    static void printHex(const std::vector<uint8_t> &data);

    // deleteNode deletes a node from the AVL tree
    // @deprecated
    AVLNode *deleteNode(AVLNode *root, const std::vector<uint8_t> &key);

    // deleteKey deletes a key from the AVL tree
    // @deprecated
    void deleteKey(const std::vector<uint8_t> &key);

    // inOrder prints the key-value pairs in the AVL tree in in-order traversal
    // @deprecated
    void inOrder(AVLNode *node);

    // minValueNode finds the node with the minimum value in the AVL tree
    // @deprecated
    static AVLNode *minValueNode(AVLNode *node);

    // height gets the height of a node
    // @deprecated
    static int height(AVLNode *node);

    // inOrderTraversal traverses the AVL tree in in-order traversal and calls a
    // function on each node
    // @deprecated
    void inOrderTraversal(
        AVLNode *node,
        std::function<void(const std::vector<uint8_t> &, const std::vector<uint8_t> &)> func);

   public:
    // insert inserts a key-value pair into the AVL tree
    // @deprecated
    void insert(const std::vector<uint8_t> &key, const std::vector<uint8_t> &value);

    // deleteKV deletes a key from the AVL tree
    // @deprecated
    void deleteKV(const std::vector<uint8_t> &key);

    // inOrder prints the key-value pairs in the AVL tree in in-order traversal
    // @deprecated
    void inOrder();

    // inOrderTraversal traverses the AVL tree in in-order traversal and calls a
    // function on each node
    // @deprecated
    void inOrderTraversal(
        std::function<void(const std::vector<uint8_t> &, const std::vector<uint8_t> &)> func);

    // clear clears the AVL tree
    // @deprecated
    void clear();

    // Get
    // Returns the value for a given key
    // @deprecated
    std::vector<uint8_t> Get(const std::vector<uint8_t> &key);

    // GetSize returns the number of nodes in the AVL tree
    // @deprecated
    int GetSize(AVLNode *root);

    // GetRoot returns the root node of the AVL tree
    // @deprecated
    int GetSize();
};

// Pager class
// Manages reading and writing pages to a file
class Pager {
   private:
    std::string fileName;                                       // File name
    std::fstream file;                                          // File stream
    std::vector<std::shared_ptr<std::shared_mutex>> pageLocks;  // Lock for each page
    std::shared_mutex fileMutex;                                // Mutex for writing to the file
   public:
    // Constructor
    Pager(const std::string &filename, std::ios::openmode mode);

    // Destructor
    // ** Use Close() to close the pager
    ~Pager();

    // Close
    // gracefully closes the pager
    bool Close() {
        try {
            // Check if we require to release any locks
            if (!pageLocks.empty()) {
                pageLocks.clear();
            }
            // Close the file
            if (file.is_open()) {
                file.close();
                return true;
            }
        } catch (const std::system_error &e) {
            std::cerr << "System error during close: " << e.what() << std::endl;
            return false;
        }
        return false;
    }

    // Write
    // Writes a new page to the file
    // takes a vector of characters as input
    // If the page is full, it writes to a new page and updates the overflow page
    // number in the header returns page number
    int64_t Write(const std::vector<uint8_t> &data);

    // GetFileName
    std::string GetFileName() const;

    // Read
    // Reads a page from the file
    // takes a page number as input
    // returns a vector of characters
    // takes into account overflow pages
    std::vector<uint8_t> Read(int64_t page_number);

    // GetFile
    // Returns the file stream
    std::fstream &GetFile();

    // PagesCount
    // Returns the number of pages in the file
    int64_t PagesCount();

};  // Pager class

// Wal (write ahead log) class
class Wal {
   public:
    // Constructor
    // This constructor takes a pager and is used for normal pager operations
    explicit Wal(Pager *pager) : pager(pager) {
        // Start the background thread
        backgroundThread = std::thread(&Wal::backgroundThreadFunc, this);
    }

    // Constructor
    // This constructor takes a path for the write-ahead log and is used for recovery
    explicit Wal(const std::string &path) : walPath(path) {
        // Open the write-ahead log
        pager = new Pager(path, std::ios::in | std::ios::out | std::ios::binary);
    }

    std::shared_mutex lock;  // Mutex for write-ahead log

    // AppendOperation appends an operation to the write-ahead log queue
    void AppendOperation(
        const Operation &op);  // AppendOperation writes an operation to the write-ahead log

    // Recover recovers operations from the write-ahead log
    bool Recover(LSMT &lsmt) const;

    mutable std::mutex queueMutex;         // Mutex for operation queue
    std::condition_variable queueCondVar;  // Condition variable for operation queue
    std::queue<Operation> operationQueue;  // Operation queue
    bool stopBackgroundThread = false;     // Stop background thread
    std::thread backgroundThread;          // Background thread

    // backgroundThreadFunc is the function that runs in the background thread
    // instead of appending on every write, we append to a queue and write in the background to not
    // block the main thread
    void backgroundThreadFunc();

    // Close closes the write-ahead log
    void Close();

   private:
    Pager *pager;                       // Pager instance
    mutable std::shared_mutex walLock;  // Mutex for write-ahead log
    std::string walPath;                // Path to the write-ahead log, for when recovering
};                                      // Wal class

// SSTable class
class SSTable {
   public:
    // Constructor
    // Accepts a shared pointer to a Pager instance
    SSTable(std::shared_ptr<Pager> pager) : pager(pager) {}

    std::shared_ptr<Pager> pager;  // Pager instance
    std::vector<uint8_t> minKey;   // Minimum key
    std::vector<uint8_t> maxKey;   // Maximum key
    std::shared_mutex
        lock;  // Mutex for SSTable (used mainly for compaction, otherwise page locks are used)
    std::string GetFilePath() const;  // Get file path of SSTable
};                                    // SSTable class

// SSTableIterator class
// Used to iterate over the key-value pairs in an SSTable
class SSTableIterator {
   public:
    // Constructor
    // Accepts a shared pointer to a Pager instance
    SSTableIterator(std::shared_ptr<Pager> pager)
        : pager(pager), maxPages(pager ? pager->PagesCount() : 0), currentPage(0) {
        if (!pager) {
            throw std::invalid_argument("Pager cannot be null");
        }

        if (maxPages == 0) {
            throw std::invalid_argument("Pager has no pages");
        }
    }

    // Ok
    // checks if the iterator is valid
    bool Ok() const { return currentPage < maxPages; }

    // Next
    // returns the next key-value pair in the SSTable
    std::optional<KeyValue> Next() {
        while (Ok()) {
            std::vector<uint8_t> data = pager->Read(currentPage++);
            if (data.empty()) {
                std::cerr << "Error: Failed to read data from pager.\n";
                continue;  // Skip empty data
            }

            try {
                return deserialize(data);  // Deserialize the data
            } catch (const std::exception &e) {
                std::cerr << "Warning: Failed to deserialize data: " << e.what() << "\n";
                continue;  // Skip invalid data
            }
        }
        return std::nullopt;  // No more valid data
    }

   private:
    std::shared_ptr<Pager> pager;  // Use shared_ptr to manage Pager instance
    int64_t maxPages;              // Maximum number of pages
    int64_t currentPage;           // Current page
};

// LSMT class
// Log-structured merge-tree
// This class is the main data storage class for TidesDB
class LSMT {
   public:
    // Constructor
    // Accepts a directory, memtable flush size, compaction interval, pager, and maximum number of
    // compaction threads, you can also specify the maximum level and probability for the memtable
    LSMT(const std::string &directory, int memtable_flush_size, int compaction_interval,
         const std::shared_ptr<Pager> &pager, int max_compaction_threads, int maxLevel = 12,
         float probability = 0.25)
        : directory(directory),
          memtableFlushSize(memtable_flush_size),
          compactionInterval(compaction_interval),
          maxCompactionThreads(max_compaction_threads),
          maxLevel(maxLevel),
          probability(probability) {
        wal = new Wal(new Pager(directory + getPathSeparator() + WAL_EXTENSION,
                                std::ios::in | std::ios::out | std::ios::binary));
        isFlushing.store(0);    // Initialize isFlushing to 0 which means not flushing
        isCompacting.store(0);  // Initialize isCompacting to 0 which means not compacting

        // Set the stop background threads flag to false
        stopBackgroundThreads.store(false);

        // Initialize the sstables vector
        this->sstables = std::vector<std::shared_ptr<SSTable>>();

        // Load SSTables
        for (const auto &entry : std::filesystem::directory_iterator(directory)) {
            if (entry.path().extension() == SSTABLE_EXTENSION) {
                auto pager = std::make_shared<Pager>(
                    entry.path().string(), std::ios::in | std::ios::out | std::ios::binary);
                auto sstable = std::make_shared<SSTable>(pager);  // Pass shared_ptr

                // Populate minKey and maxKey
                SSTableIterator it(pager);
                if (it.Ok()) {
                    auto kv = it.Next();
                    if (kv) {
                        sstable->minKey = std::vector<uint8_t>(kv->key().begin(), kv->key().end());
                    }
                }

                while (it.Ok()) {
                    auto kv = it.Next();
                    if (kv) {
                        sstable->maxKey = std::vector<uint8_t>(kv->key().begin(), kv->key().end());
                    }
                }

                this->sstables.push_back(sstable);
            }
        }

        // Sort SSTables by last modified time
        std::sort(this->sstables.begin(), this->sstables.end(),
                  [](const std::shared_ptr<SSTable> &a, const std::shared_ptr<SSTable> &b) {
                      auto a_time = std::filesystem::last_write_time(a->pager->GetFileName());
                      auto b_time = std::filesystem::last_write_time(b->pager->GetFileName());
                      return a_time < b_time;
                  });

        // Create a new memtable
        memtable = new SkipList(maxLevel, probability);

        // Start background thread for flushing
        flushThread = std::thread(&LSMT::flushThreadFunc, this);
    }

    // New creates a new LSMT instance
    // Opens all SSTables in the directory and loads them into memory (just the pager, min and max)
    static std::unique_ptr<LSMT> New(const std::string &directory,
                                     std::filesystem::perms directoryPerm, int memtableFlushSize,
                                     int compactionInterval,
                                     std::optional<int> maxCompactionThreads = std::nullopt) {
        if (directory.empty()) {
            throw TidesDBException("directory cannot be empty");
        }

        if (!std::filesystem::exists(directory)) {
            std::filesystem::create_directory(directory);
            std::filesystem::permissions(directory, directoryPerm);
        }

        auto walPager = std::make_shared<Pager>(
            directory + getPathSeparator() + WAL_EXTENSION,
            std::ios::in | std::ios::out | std::ios::binary);  // Open the write-ahead log

        int availableThreads =
            std::thread::hardware_concurrency();  // Get the number of available threads

        if (availableThreads == 0) {
            throw TidesDBException("could not determine number of available threads");
        }
        int maxThreads = maxCompactionThreads.value_or(std::max(1, availableThreads - 3));

        if (maxThreads > availableThreads) {
            throw TidesDBException(
                "max compaction threads cannot be greater than available threads");
        }

        return std::make_unique<LSMT>(directory, memtableFlushSize, compactionInterval, walPager,
                                      maxThreads);
    }

    // Destructor
    ~LSMT() {}

    // Put inserts a key-value pair into the LSMT
    bool Put(const std::vector<uint8_t> &key, const std::vector<uint8_t> &value);

    // Delete deletes a key from the LSMT
    bool Delete(const std::vector<uint8_t> &key);

    // Compact compacts the SSTables
    bool Compact();

    // IsFlushing returns whether the memtable is being flushed
    bool IsFlushing() const { return isFlushing.load(); }

    // IsCompacting returns whether the SSTables are being compacted
    bool IsCompacting() const { return isCompacting.load(); }

    // GetMemtable returns the memtable
    SkipList *GetMemtable() const { return memtable; }

    // GetSSTableCount returns the number of SSTables
    int GetSSTableCount() {
        // lock the SSTables
        std::shared_lock<std::shared_mutex> sstablesLockGuard(sstablesLock);
        return sstables.size();
    }

    // NGet returns all key-value pairs not equal to a given key
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> NGet(
        const std::vector<uint8_t> &key) const;

    // LessThan returns all key-value pairs less than a given key
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> LessThan(
        const std::vector<uint8_t> &key) const;

    // GreaterThan returns all key-value pairs greater than a given key
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> GreaterThan(
        const std::vector<uint8_t> &key) const;

    // Range returns all key-value pairs between a start and end key
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> Range(
        const std::vector<uint8_t> &start, const std::vector<uint8_t> &end) const;

    // NRange returns all key-value pairs not between a start and end key
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> NRange(
        const std::vector<uint8_t> &start, const std::vector<uint8_t> &end) const;

    // LessThanEq returns all key-value pairs less than or equal to a given key
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> LessThanEq(
        const std::vector<uint8_t> &key) const;

    // GreaterThanEq returns all key-value pairs greater than or equal to a given
    // key
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> GreaterThanEq(
        const std::vector<uint8_t> &key) const;

    // BeginTransaction begins a new transaction
    Transaction *BeginTransaction();

    // CommitTransaction commits a transaction
    bool CommitTransaction(Transaction *tx);

    // RollbackTransaction rolls back a transaction
    void RollbackTransaction(Transaction *tx);

    // AddDelete adds a delete operation to a transaction
    static void AddDelete(Transaction *tx, const std::vector<uint8_t> &key,
                          const std::vector<uint8_t> &value);

    // AddPut adds a put operation to a transaction
    static void AddPut(Transaction *tx, const std::vector<uint8_t> &key,
                       const std::vector<uint8_t> &value);

    // Get returns the value for a given key
    std::vector<uint8_t> Get(const std::vector<uint8_t> &key);

    // Close closes the LSMT gracefully
    void Close() {
        try {
            // Flush the memtable to disk
            if (!flushMemtable()) {
                throw TidesDBException("failed to flush memtable to disk");
            }

            // Notify the condition variables to wake up the threads
            stopBackgroundThreads.store(true);
            flushQueueCondVar.notify_all();
            cond.notify_all();

            // Wait for the flush queue to be empty
            {
                std::unique_lock<std::mutex> lock(flushQueueMutex);
                flushQueueCondVar.wait(lock, [this] { return flushQueue.empty(); });
            }

            // Wait for the flush thread to finish
            if (flushThread.joinable()) {
                flushThread.join();
            }

            // Wait for the compaction thread to finish
            if (compactionThread.joinable()) {
                compactionThread.join();
            }

            // Close the write-ahead log
            wal->Close();

            // Clear the memtable
            memtable->clear();

            {
                std::unique_lock<std::shared_mutex> sstablesLockGuard(
                    sstablesLock);  // Lock the SSTables

                // Iterate over the SSTables and close them
                for (const auto &sstable : sstables) {
                    sstable->pager->Close();
                }

                // Clear the list of SSTables
                sstables.clear();
            }

        } catch (const std::system_error &e) {
            throw TidesDBException("system error during close: " + std::string(e.what()));
        } catch (const std::exception &e) {
            throw TidesDBException("error during close: " + std::string(e.what()));
        }
    }

   private:
    // Active transactions
    // Transactions that are actively being written to and awaiting commit
    std::vector<Transaction *> activeTransactions;   // List of active transactions
    std::shared_mutex activeTransactionsLock;        // Mutex for active transactions
    std::vector<std::shared_ptr<SSTable>> sstables;  // List of SSTables
    std::shared_mutex sstablesLock;                  // Mutex for SSTables
    std::shared_mutex walLock;                       // Mutex for write-ahead log
    Wal *wal;                                        // Write-ahead log
    std::string directory;                           // Directory for storing data
    int compactionInterval;  // Compaction interval (amount of SSTables to wait before compacting)
                             // we should have at least this many SSTables, if there
                             // are less after compaction, we will not further compact
    std::condition_variable_any cond;        // Condition variable for flushing and compacting
    SkipList *memtable;                      // Skip list memtable
    std::atomic<int32_t> isFlushing;         // Whether the memtable is being flushed
    std::atomic<int32_t> isCompacting;       // Whether the SSTables are being compacted
    int memtableFlushSize;                   // Memtable flush size
    std::vector<std::future<void>> futures;  // List of futures, used for flushing and compacting
    std::thread flushThread;                 // Thread for flushing
    std::queue<std::unique_ptr<SkipList>> flushQueue;  // Queue for flushing
    std::mutex flushQueueMutex;                        // Mutex for flush queue
    std::atomic_bool stopBackgroundThreads = false;    // Stop background thread
    std::condition_variable flushQueueCondVar;         // Condition variable for flush queue
    std::thread compactionThread;                      // Thread for compaction
    int maxCompactionThreads;  // Maximum number of threads for paired compaction
    int maxLevel;              // Maximum level of the skip list
    float probability;         // Probability for skiplist

    // flushMemtable
    // responsible for flushing the current memtable to disk. It creates a new memtable,
    // transfers the data from the current memtable to the new one, and then adds the new memtable
    // to the flush queue. Finally, it notifies the flush thread to process the queue
    bool flushMemtable();

    // flushThreadFunc is the function that runs in the flush thread
    // This function waits notification and pops latest memtable from the queue and flushes it to
    // disk
    void flushThreadFunc();
};

}  // namespace TidesDB

#endif  // TIDESDB_LIBRARY_H
