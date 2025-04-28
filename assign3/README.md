## Overview

This project implements a Record Manager for a simple database system. The Record Manager uses the Buffer Manager to handle pages in memory and provides CRUD operations on records stored in page files. It manages table metadata, record insertion, deletion, updating, retrieval, and scanning with optional filtering.

## Features

- Table creation and deletion with schema initialization
- Record insertion, deletion (tombstoning), updating, and retrieval
- Sequential scans with optional conditional predicates
- Schema management and record layout calculation
- Integration with Buffer Manager for efficient page caching

## Data Structures

### `RMTableMgmtData`

Stores table-level metadata and buffer pool information:

- `int numTuples` — Total number of records in the table
- `int firstFreePageNumber` — Next page to search for free slots
- `int recordSize` — Size of each record (bytes)
- `BM_PageHandle pageHandle` — Handle for pinned pages
- `BM_BufferPool bufferPool` — Buffer pool for table pages

### `RMScanMgmtData`

Stores state for an active scan:

- `BM_PageHandle pHandle` — Handle for the current page
- `RID rid` — Current record identifier (page, slot)
- `int count` — Number of records scanned so far
- `Expr *condition` — Optional filter expression

## Record Manager Functions

### Initialization & Shutdown

```c
RC initRecordManager(void *mgmtData);
RC shutdownRecordManager(void);
```

- initRecordManager — Sets up any global state (no-op in this implementation).
- shutdownRecordManager — Cleans up the record manager (no-op).

### Table Management

```c
RC createTable(char *name, Schema *schema);
RC openTable(RM_TableData *rel, char *name);
RC closeTable(RM_TableData *rel);
RC deleteTable(char *name);
int getNumTuples(RM_TableData *rel);
```

- createTable — Creates a new page file and writes schema and metadata to the first page.
- openTable — Initializes RM_TableData, loads metadata into RMTableMgmtData, and pins the metadata page.
- closeTable — Updates metadata (tuple count), flushes dirty metadata, and shuts down the buffer pool.
- deleteTable — Destroys the underlying page file.
- getNumTuples — Returns the current total record count.

### Record Operations

```c
RC insertRecord(RM_TableData *rel, Record *record);
RC deleteRecord(RM_TableData *rel, RID id);
RC updateRecord(RM_TableData *rel, Record *record);
RC getRecord(RM_TableData *rel, RID id, Record *record);
```

- insertRecord — Finds a free slot, writes the record, marks the page dirty, and updates tuple count.
- deleteRecord — Pins the page, writes a tombstone ('$'), decrements tuple count, and marks dirty.
- updateRecord — Overwrites an existing record’s data and marks the page dirty.
- getRecord — Retrieves record data into the provided Record structure.

### Scanning

```c
RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond);
RC next(RM_ScanHandle *scan, Record *record);
RC closeScan(RM_ScanHandle *scan);
```

- startScan — Initializes scan state at the first data page and slot, with an optional condition.
- next — Iterates through pages and slots, evaluates the condition, and returns the next matching record.
- closeScan — Unpins any pinned pages and frees scan management data.

### Schema & Record Utilities

```c
int getRecordSize(Schema *schema);
Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys);
RC freeSchema(Schema *schema);
RC createRecord(Record **record, Schema *schema);
RC freeRecord(Record *record);
RC determineAttributeOffsetInRecord(Schema *schema, int attrNum, int *result);
RC getAttr(Record *record, Schema *schema, int attrNum, Value **value);
RC setAttr(Record *record, Schema *schema, int attrNum, Value *value);
```

- getRecordSize — Computes the byte size of a record given its schema.
- createSchema / freeSchema — Allocate and deallocate Schema structures.
- createRecord / freeRecord — Allocate and deallocate Record instances.
- determineAttributeOffsetInRecord — Computes the byte offset for a given attribute in a record.
- getAttr / setAttr — Extract and assign attribute values within a record.

## Usage

To compile and test the Record Manager with the accompanying Buffer and Storage Managers:

```bash
make
./test_program_3  # runs unit tests for record manager
make clean
```

## Contributions

- Neil Initialization & Shutdown; Table Management

- Brandon Record Operations; Scanning

- Jinzhao Schema & Record Utilities; Documentation


