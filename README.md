<div>
    <h1 align="left"><img width="148" src="artwork/tidesdb-logo-v0.1.png"></h1>
</div>

TidesDB is a fast and efficient key value storage engine library written in C.
The underlying data structure is based on a log-structured merge-tree (LSM-tree).

It is not a full-featured database, but rather a library that can be used to build a database atop of.

> [!CAUTION]
> In beta

## Features
- [x] **Concurrent** multiple threads can read and write to the storage engine.  The skiplist uses an RW lock which means multiple readers and one true writer.  SSTables are sorted, immutable and can be read concurrently they are protected via page locks.  Transactions are also thread-safe.
- [x] **Column Families** store data in separate key-value stores.  Each column family has their own memtable and sstables.
- [x] **Atomic Transactions** commit or rollback multiple operations atomically.  Rollsback all operations if one fails.
- [x] **Cursor** iterate over key-value pairs forward and backward.
- [x] **WAL** write-ahead logging for durability.  As operations are appended they are also truncated at specific points once persisted to an sstable(s).
- [x] **Multithreaded Compaction** manual multi-threaded paired and merged compaction of sstables.  When run for example 10 sstables compacts into 5 as their paired and merged.  Each thread is responsible for one pair - you can set the number of threads to use for compaction.
- [x] **Background flush** memtable flushes are enqueued and then flushed in the background.
- [x] **Chained Bloom Filters** reduce disk reads by reading initial pages of sstables to check key existence.  Bloomfilters grow with the size of the sstable using chaining and linking.
- [x] **Zstandard Compression** compression is achieved with Zstandard.  SStable entries can be compressed as well as WAL entries.
- [x] **TTL** time-to-live for key-value pairs.
- [x] **Configurable** many options are configurable for the engine, and column families.
- [x] **Error Handling** API functions return an error code and message.
- [x] **Easy API** simple and easy to use api.

## Building
Using cmake to build the shared library.
```bash
cmake -S . -B build
cmake --build build
cmake --install build
```

## Bindings

<ul>
    <li><img src="artwork/cpp.png" width="16" style="float: left; margin-right: 10px;" /> <a href="https://github.com/tidesdb/tidesdb-cpp">tidesdb-cpp</a></li>
    <li><img src="artwork/go.png" width="16" style="float: left; margin-right: 10px;" /> <a href="https://github.com/tidesdb/tidesdb-go">tidesdb-go</a></li>
    <li><img src="artwork/java.png" width="16" style="float: left; margin-right: 10px;" /> <a href="https://github.com/tidesdb/tidesdb-java">tidesdb-java</a></li>
    <li><img src="artwork/python.png" width="16" style="float: left; margin-right: 10px;" /> <a href="https://github.com/tidesdb/tidesdb-python">tidesdb-python</a></li>
    <li><img src="artwork/rust.png" width="16" style="float: left; margin-right: 10px;" /> <a href="https://github.com/tidesdb/tidesdb-rust">tidesdb-rust</a></li>
</ul>

## Include
```c
#include <tidesdb.h>
```

## Usage
Each database method returns a `tidesdb_err*` which returns an error code and message. If no error, TidesDB returns `NULL`.

**Example of error structure**
```c
typedef struct
{
    int code;
    char* message;
} tidesdb_err_t;
```

### Opening a database
To open a new database you need to create a configuration and then open the database.
```c
tidesdb_config_t* tdb_config = (malloc(sizeof(tidesdb_config_t)));
if (tdb_config == NULL)
{
    /* handle error */
    return;
}

tdb_config->db_path = "the_dir_you_want_to_store_the_db"; /* tidesdb will create the directory if not exists */
tdb_config->compressed_wal = false; /* whether you want WAL(write ahead log) entries to be compressed */

tidesdb_t tdb = NULL;
tidesdb_err_t* e = tidesdb_open(tdb_config, &tdb);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}

/* close the database */
e = tidesdb_close(tdb);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}

/* free the configuration */
free(tdb_config);
```

### Creating a column family
In order to store data in TidesDB you need a column family.
You pass
- the database you want to create the column family in.  Must be open
- the name of the column family
- memtable flush threshold in bytes.  Example below is 128MB
- skiplist max level.  Example below is 12
- skiplist probability.  Example below is 0.24
- whether column family sstable data is compressed

```c
/* create a column family */
tidesdb_err_t *e = tidesdb_create_column_family(tdb, "your_column_family", (1024 * 1024) * 128, 12, 0.24f, false);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

### Dropping a column family

```c
/* drop a column family */
tidesdb_err_t *e = tidesdb_drop_column_family(tdb, "test_cf");
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

### Putting a key-value pair
You pass
- the database you want to put the key-value pair in.  Must be open
- the column family name you want to put the key-value pair in
- the key
- the key size
- the value
- the value size
- when the key-value pair should expire.  If -1 then it never expires.

```c
/* put a key-value pair */
uint8_t key[] = "key";
uint8_t value[] = "value";

tidesdb_err_t *e = tidesdb_put(tdb, "your_column_family", key, strlen(key), value, strlen(value), -1);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

### Putting a key-value pair with TTL
```c
/* put a key-value pair with TTL */
uint8_t key[] = "key";
uint8_t value[] = "value";

time_t ttl = time(NULL) + 10; /* 10 seconds */
tidesdb_err_t *e  = tidesdb_put(tdb, "your_column_family", key, strlen(key), value, strlen(value), ttl);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

### Getting a key-value pair
You pass
- the database you want to get the key-value pair from.  Must be open
- the column family name
- the key
- the key size
- a pointer to the value
- a pointer to the value size
```c
size_t value_len = 0;
uint8_t* value_out = NULL;
uint8_t key[] = "key";

tidesdb_err_t *e = tidesdb_get(tdb, "your_column_family", key, strlen(key), &value_out, &value_len);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

### Deleting a key-value pair
You pass
- the database you want to delete the key-value pair from.  Must be open
- the column family name
- the key
- the key size
```c
uint8_t key[] = "key";

tidesdb_err_t *e = tidesdb_delete(tdb, "your_column_family", key, strlen(key));
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

### Transactions
You can perform a series of operations atomically.  This will block other threads from reading or writing to the database until the transaction is committed or rolled back.

You begin a transaction by calling `tidesdb_txn_begin`.

You pass
- the database you want to perform the operations in.  Must be open
- the transaction pointer
- the column family name you want to perform the operations in
```c
tidesdb_txn_t* transaction;
tidesdb_err_t *e = tidesdb_txn_begin(tdb, &transaction, "your_column_family");
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

Now we can add operations to the transaction.
```c
const uint8_t key[] = "example_key";
const uint8_t value[] = "example_value";
tidesdb_err_t *e = tidesdb_txn_put(transaction, key, sizeof(key), value, sizeof(value), -1); /* you can pass a ttl, similar to put */
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}

/* you can add delete operations as well */
e = tidesdb_txn_delete(transaction, key, sizeof(key));
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}

/* now we commit */
e = tidesdb_txn_commit(transaction);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}

/* before you free, you can rollback */
tidesdb_err_t *e = tidesdb_txn_rollback(transaction);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}

/* free the transaction */
tidesdb_txn_free(transaction);
```

### Cursors
You can iterate over key-value pairs in a column family.
```c
tidesdb_cursor_t* c;
tidesdb_err_t *e = tidesdb_cursor_init(tdb, "your_column_family", &c);
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
    return;
}

key_value_pair_t kv;

/* iterate forward */
while ((e = tidesdb_cursor_next(c)) == NULL)
{
    e = tidesdb_cursor_get(c, &kv);
    if (e != NULL)
    {
        /* handle error */
        tidesdb_err_free(e);
        break;
    }

    /* use kv.key and kv.value */
    free(kv.key);
    free(kv.value);
}

if (e != NULL && e->code != 1062) /* 1062 means "At end of cursor" */
{
    /* handle error */
    tidesdb_err_free(e);
}

/* iterate backward */
while ((e = tidesdb_cursor_prev(c)) == NULL)
{
    e = tidesdb_cursor_get(c, &kv);
    if (e != NULL)
    {
        /* handle error */
        tidesdb_err_free(e);
        break;
    }

    /* use kv.key and kv.value */
    free(kv.key);
    free(kv.value);
}

if (e != NULL && e->code != 1085) /* 1085 means "At beginning of cursor" */
{
    /* handle error */
    tidesdb_err_free(e);
}

tidesdb_cursor_free(c);

```

### Compaction
You can manually compact sstables.
```c
tidesdb_err_t *e = tidesdb_compact_sstables(tdb, "your_column_family", 10); /* use 10 threads */
if (e != NULL)
{
    /* handle error */
    tidesdb_err_free(e);
}
```

## Errors

| Error Code | Error Message                                                        |
|------------|----------------------------------------------------------------------|
| 1000       | Failed to allocate memory for new db                                 |
| 1001       | Config is NULL                                                       |
| 1002       | TidesDB is NULL                                                      |
| 1003       | DB path is NULL                                                      |
| 1004       | Failed to create db directory                                        |
| 1005       | Failed to unlock flush lock                                          |
| 1006       | Failed to join flush thread                                          |
| 1007       | Failed to destroy flush lock                                         |
| 1008       | Failed to sort sstables                                              |
| 1009       | Failed to replay wal                                                 |
| 1010       | Failed to initialize flush queue                                     |
| 1011       | Failed to copy memtable                                              |
| 1012       | Failed to get wal checkpoint                                         |
| 1013       | Failed to initialize column families lock                            |
| 1014       | Failed to start flush thread                                         |
| 1015       | Column family name is NULL                                           |
| 1016       | Column family name is too short                                      |
| 1017       | Flush threshold is too low                                           |
| 1018       | Max level is too low                                                 |
| 1019       | Probability is too low                                               |
| 1020       | Failed to create new column family                                   |
| 1021       | Failed to add column family                                          |
| 1022       | Failed to lock column families lock                                  |
| 1024       | Failed to acquire sstables lock                                      |
| 1025       | Failed to unlock sstables                                            |
| 1026       | Key is NULL                                                          |
| 1027       | Value is NULL                                                        |
| 1028       | Column family not found                                              |
| 1029       | Max threads is too low                                               |
| 1030       | Failed to acquire sstables lock                                      |
| 1031       | Key not found                                                        |
| 1032       | Failed to create compaction thread                                   |
| 1033       | Failed to allocate memory for thread arguments                       |
| 1034       | Failed to deserialize bloom filter                                   |
| 1035       | Failed to initialize sstable cursor                                  |
| 1036       | Failed to read sstable                                               |
| 1037       | Failed to deserialize key value pair                                 |
| 1038       | Key value pair is NULL                                               |
| 1040       | Failed to signal flush condition                                     |
| 1041       | Failed to load column families                                       |
| 1042       | Failed to open wal                                                   |
| 1043       | Failed to destroy flush condition                                    |
| 1044       | Failed to destroy column families lock                               |
| 1045       | Failed to allocate memory for queue entry                            |
| 1046       | Failed to initialize flush lock                                      |
| 1047       | Failed to initialize flush condition variable                        |
| 1048       | Failed to reallocate memory for column families                      |
| 1049       | Failed to append to wal                                              |
| 1050       | Failed to put into memtable                                          |
| 1051       | Not enough sstables to compact                                       |
| 1052       | Failed to allocate memory for transaction                            |
| 1053       | Failed to lock flush lock                                            |
| 1054       | Transaction is NULL                                                  |
| 1055       | Failed to read bloom filter                                          |
| 1056       | Failed to reallocate memory for transaction operations               |
| 1057       | Failed to allocate memory for cursor                                 |
| 1058       | Failed to initialize memtable cursor                                 |
| 1059       | Failed to initialize sstable cursor                                  |
| 1060       | Failed to get key value pair from cursor                             |
| 1061       | Cursor is NULL                                                       |
| 1062       | At end of cursor                                                     |
| 1063       | At start of cursor                                                   |
| 1064       | Key has a tombstone value.  To be deleted on next compaction         |
| 1065       | Key has expired.  To be deleted on next compaction                   |
| 1066       | Key's value cannot be a tombstone                                    |
| 1067       | Failed to allocate memory for wal                                    |
| 1068       | Failed to lock compaction or flush lock                              |
| 1069       | Failed to allocate memory for value copy                             |
| 1070       | Failed to allocate memory for tombstone                              |
| 1071       | Transaction pointer is NULL                                          |
| 1072       | Failed to allocate memory for column family name                     |
| 1073       | Failed to initialize transaction lock                                |
| 1074       | Failed to acquire transaction lock                                   |
| 1075       | Failed to allocate memory for operation                              |
| 1076       | Failed to allocate memory for key-value pair                         |
| 1077       | Failed to allocate memory for key                                    |
| 1078       | Failed to allocate memory for value                                  |
| 1079       | Failed to allocate memory for rollback operations                    |
| 1080       | Failed to allocate memory for rollback column family name            |
| 1081       | Failed to allocate memory for rollback key-value pair                |
| 1082       | Failed to allocate memory for rollback key                           |
| 1083       | Failed to acquire memtable lock for commit                           |
| 1084       | Failed to skip initial pages                                         |
| 1085       | At beginning of cursor                                               |


## License
Multiple

```
Mozilla Public License Version 2.0
BSD 2-Clause license
```