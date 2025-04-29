# Storage Manager 

## Overview
This module provides functions to read blocks (pages) from a file into memory as part of a storage management system. It includes functions for reading specific blocks, such as the first, previous, current, next, and last blocks, along with retrieving the current block position.

## Description

Our project implements a simple storage manager in C. We added functionality for file handling and read/write functions


#### File handling
    In our file handling part of this project, we added createPageFile which creates a new one page blank file.
    
    Next is openPageFile which simply opens the file or returns errors based on what is wrong
    
    closePageFile is used to close an open file, which will reset the file handle as well. it also has error handling similar to openpage
    
    destroyPageFile completely removes a specified file from the file system


#### Read functions
    In our read functions, we have readBlock which opens a file in read mode.
    
    getBlockPosition just returns the current page position from the file
    
    readFirstBlock only reads the first block from a file while readPreviousBlock reads only the block before the current block
    
    There are three other functions that work similarly, which are readCurrentBlock, readNextBlock, and readLastBlock, which all do exactly as their names suggest using readBlock and some varioation of page position, fhandle and mempage

#### Write functions
    In our write functions we started with writeBlock, which allows the user to write to a block on a specific page number in a selected file. 
    
    Next is writeCurrentBlock which just writes to the block without specifying to change locations
    
    appendEmptyBlock then allows us to add a completely blank block to the end of the selected file
    
    Lastly, we have ensureCapacity which we use to see if the selected file has enough pages based of what we need. It will append extra pages to meet the requirements if neccessary. 

## Contributions

Jinzhao - Read functions

Brandon - Write functions

Neil - File handling functions

## Testing
Commands to perform testing

```shell
make
./test_program
make clean
```