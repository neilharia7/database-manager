# Storage Manager 

## Overview
This module provides functions to read blocks (pages) from a file into memory as part of a storage management system. It includes functions for reading specific blocks, such as the first, previous, current, next, and last blocks, along with retrieving the current block position.

## Functions

**Parameters:**
- `pageNum` - The page number of the block to be read.
- `fHandle` - The file handle structure containing file information.
- `memPage` - A buffer where the read page content will be stored.

**Returns:**
- `RC_FILE_NOT_FOUND` - If the file cannot be opened.
- `RC_READ_NON_EXISTING_PAGE` - If the specified page does not exist.
- `RC_OK` - Operation was successful.

### `RC readBlock(int pageNum, SM_FileHandle* fHandle, SM_PageHandle memPage)`
Reads a block identified by the page number from the file into memory.

### `int getBlockPos(SM_FileHandle* fHandle)`
Returns the current block (page) position in the file.

### `RC readFirstBlock(SM_FileHandle* fHandle, SM_PageHandle memPage)`
Reads the first block from the file.

### `RC readPreviousBlock(SM_FileHandle* fHandle, SM_PageHandle memPage)`
Reads the block preceding the current block in the file.

### `RC readCurrentBlock(SM_FileHandle* fHandle, SM_PageHandle memPage)`
Reads the current block from the file.

### `RC readNextBlock(SM_FileHandle* fHandle, SM_PageHandle memPage)`
Reads the block that follows the current block in the file.

### `RC readLastBlock(SM_FileHandle* fHandle, SM_PageHandle memPage)`
Reads the last block (page) from the file.

## Error Codes
- `RC_OK` - Operation successful.
- `RC_FILE_NOT_FOUND` - File cannot be opened.
- `RC_READ_NON_EXISTING_PAGE` - Requested page does not exist.

## Usage Example



## Contributions
- 
- readBlock, getBlockPos, readFirstBlock, readPreviousBlock, readCurrentBlock, readNextBlock, readLastBlock: Jinzhao Qian
-
- merge and test: together