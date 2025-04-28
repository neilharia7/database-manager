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
- initRecordManager — Initializes the record manager. 
- shutdownRecordManager — Shuts down the record manager. Called when the record manager is no longer needed.

### Table Management

```c
RC createTable(char *name, Schema *schema);
RC openTable(RM_TableData *rel, char *name);
RC closeTable(RM_TableData *rel);
RC deleteTable(char *name);
int getNumTuples(RM_TableData *rel);
```
- createTable — Creates a new table with the given name and schema. Creates a new page file and initializes the first page with table metadata including the schema.
- openTable — Opens an existing table for operations. Initializes the buffer pool and loads table metadata into memory.
- closeTable — Closes an open table, writing back any updated metadata and shutting down the buffer pool.
- deleteTable — Deletes a table and its associated page file.
- getNumTuples — Returns the total number of records present in the table.

### Record Operations

```c
RC insertRecord(RM_TableData *rel, Record *record);
RC deleteRecord(RM_TableData *rel, RID id);
RC updateRecord(RM_TableData *rel, Record *record);
RC getRecord(RM_TableData *rel, RID id, Record *record);
```
- insertRecord — Pins the next free page, finds a slot, writes the record (updating RID), marks the page dirty, unpins it, and increments tuple count.
- deleteRecord — Pins the record’s page, replaces '#' with '$', decrements tuple count, marks the page dirty, and unpins it.
- updateRecord — Pins the record’s page, updates its data, marks the page dirty, and unpins it.
- getRecord — Pins the record’s page, verifies the '#' marker, copies data into the Record structure, and unpins it.

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

To compile and test the Record Manager on test_assign3_1

```bash
make
./test_program_3  
make clean
```

To compile and test the Record Manager on test_expr
(need to change filename Makefile_expr to Makefile first)
```bash
make
./test_program_expr  # runs unit tests for record manager
make clean
```

## Contributions

- Neil Initialization & Shutdown; Table Management

- Brandon Record Operations; Scanning

- Jinzhao Schema & Record Utilities; Documentation


