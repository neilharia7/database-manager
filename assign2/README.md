# Buffer Manager Implementation

## Overview

This project implements a buffer manager for a database system. The buffer manager manages a fixed number of pages in memory that represent blocks from a page file managed by the storage manager. The memory blocks managed by the buffer manager are called page frames, and the combination of a page file and the page frames storing pages from that file is called a Buffer Pool.

## Features

- Multiple buffer pools can be managed simultaneously (one buffer pool per page file)
- Support for different page replacement strategies (FIFO and LRU implemented)
- Page pinning and unpinning mechanism
- Dirty page tracking and management
- Fix count tracking to prevent eviction of pages in use
- Statistics tracking for buffer pool usage

## Data Structures

### BM_BufferPool

The `BM_BufferPool` structure stores information about a buffer pool:

- `pageFile`: The name of the page file associated with the buffer pool
- `numPages`: The size of the buffer pool (number of page frames)
- `strategy`: The page replacement strategy
- `mgmtData`: A pointer to bookkeeping data, including:
    - Pointer to the area in memory that stores the page frames
    - Data structures needed by the page replacement strategy

### BM_PageHandle

The `BM_PageHandle` structure stores information about a page:

- `pageNum`: The page number (position of the page in the page file)
- `data`: Pointer to the area in memory storing the content of the page

## Buffer Pool Functions

### `initBufferPool`

```c++
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
                 const int numPages, ReplacementStrategy strategy,
                 void *stratData);
```

Creates a new buffer pool with numPages page frames using the specified page replacement strategy. The pool is used to cache pages from the page file with name pageFileName. Initially, all page frames are empty. The page file should already exist.

### `shutdownBufferPool`

```c++
RC shutdownBufferPool(BM_BufferPool *const bm);
```
Destroys a buffer pool, freeing all resources associated with it. If the buffer pool contains any dirty pages, these pages are written back to disk before destroying the pool. It is an error to shutdown a buffer pool that has pinned pages.

### `forceFlushPool`

```c++
RC forceFlushPool(BM_BufferPool *const bm);
```

Causes all dirty pages (with fix count 0) from the buffer pool to be written to disk.

### markDirty

```c++
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page);
```

Marks a page as dirty, indicating that its contents have been modified and need to be written back to disk.

### unpinPage

```c++
RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page);
```
Unpins a page, reducing its fix count. If the fix count reaches zero, the page can be replaced in the buffer pool.

### forcePage

```c++
RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page);
```
Writes the current content of the page back to the page file on disk.


### pinPage
```c++
RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum);
```

Pins the page with the given page number into the buffer pool. If the page is already in the pool, it is returned. Otherwise, it is read from disk.


## Usage

To compile and test the buffer manager

```shell

make
./test_program_2
make clean

```

Contributions
Neil - Buffer Manager Interface Pooling & Documentation
Brandon - Page Management
Jinzhao - Statistic Functions

