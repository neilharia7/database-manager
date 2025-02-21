# Storage Manager 

## Overview
This module provides functions to read blocks (pages) from a file into memory as part of a storage management system. It includes functions for reading specific blocks, such as the first, previous, current, next, and last blocks, along with retrieving the current block position.

## Functions

**Parameters:**
- `fileName` - A string representing the name of the file.
- `pageNum` - The page number of the block to be read.
- `fHandle` - The file handle structure containing file information.
- `memPage` - A buffer where the read page content will be stored.

**Returns:**
- `RC_FILE_ALREADY_EXISTING` - The file already exists.
- `RC_FILE_CREATION_FAILED` - The file could not be created.
- `RC_WRITE_FAILED` - Writing page to file failed.
- `RC_FILE_NOT_FOUND` - If the file cannot be opened.
- `RC_FILE_HANDLE_NOT_INIT` - If the file handle is not correctly initialized.
- `RC_READ_NON_EXISTING_PAGE` - If the specified page does not exist.
- `RC_OK` - Operation was successful.

### `void initStorageManager(void)`
Placeholder for initialing function for the storage manager

### `RC createPageFile(char *fileName)`
Creates a new file with the specified file name.

### `RC openPageFile(char *fileName, SM_FileHandle *fHandle)`
Opens an existing page file.

### `RC closePageFile(SM_FileHandle *fHandle)`
Closes an open file and resets the file handle.

### `RC destroyPageFile(char *fileName)`
Deletes the file with the specified file name from the disk.

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

- initStorageManager, createPageFile, openPageFile, closePageFile, destroyPageFile: Neil Haria
- readBlock, getBlockPos, readFirstBlock, readPreviousBlock, readCurrentBlock, readNextBlock, readLastBlock: Jinzhao Qian
- 
- merge and test: together

## Testing
Commands to perform testing

```shell
make
./test_program
make clean
```