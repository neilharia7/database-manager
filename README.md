# Database Management

This project implements a comprehensive database management system with multiple components working together to provide efficient data storage and retrieval capabilities. The system consists of a Record Manager that builds upon a Buffer Manager to handle database operations while optimizing memory usage.

## System Architecture

The database management system is structured in layers:

1. **Record Manager**: Provides CRUD operations on records stored in page files
2. **Buffer Manager**: Manages pages in memory with efficient caching strategies
3. **Storage Manager**: Handles the underlying page file operations

## Components

### Record Manager

The Record Manager provides an interface for table and record operations:

- Table creation and deletion with schema initialization
- Record insertion, deletion (using tombstoning), updating, and retrieval
- Sequential scans with optional conditional predicates
- Schema management and record layout calculation
- Integration with Buffer Manager for efficient page caching

### Buffer Manager

The Buffer Manager optimizes memory usage by:

- Managing multiple buffer pools simultaneously (one per page file)
- Supporting different page replacement strategies (FIFO, LRU)
- Implementing page pinning and unpinning mechanisms
- Tracking dirty pages for efficient writes
- Maintaining fix counts to prevent eviction of pages in use
- Collecting statistics on buffer pool usage

### Storage Manager

The Storage Manager provides low-level file operations:

- Creating, opening, and closing page files
- Reading and writing blocks (pages) to disk
- Appending empty blocks to files
- Managing file metadata (total pages, current position)
- Ensuring capacity for growing files
- Sequential block access (first, previous, current, next, last)

## Project Structure

```
/
├── record_manager/       # RM implementation
│   ├── record_mgr.c      # Functionality
│   ├── record_mgr.h      # RM interface
│   └── README.md         # Docs
├── buffer_manager/       # BM implementation
│   ├── buffer_mgr.c      # Functionality
│   ├── buffer_mgr.h      # BMinterface
│   └── README.md         # Docs
├── storage_manager/      # SM implementation
│   ├── storage_mgr.c     # Functionality
│   ├── storage_mgr.h     # SM interface
│   └── README.md         # Docs
└── README.md             
```

## Building and Testing


For storage manager

```bash

cd storage_manager && make && ./test_program
make clean
```

For buffer manager

```bash

cd buffer_manager && make && ./test_program_2
make clean
```

For record manager

```bash

cd buffer_manager && make && ./test_program_3
make clean
```

## Contributors

- **Neil**
- **Brandon**
- **Jinzhao**