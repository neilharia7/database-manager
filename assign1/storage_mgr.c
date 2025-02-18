#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "storage_mgr.h"
#include "dberror.h"

RC readBlock(int pageNum, SM_FileHandle* fHandle, SM_PageHandle memPage) {
    FILE* file = fopen(fHandle->fileName, "r");
    if (file == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    /* Move to the correct position (pageNum * PAGE_SIZE) */
    if (fseek(file, pageNum * PAGE_SIZE, SEEK_SET) != 0) {
        fclose(file);
        return RC_READ_NON_EXISTING_PAGE;
    }

    /* Read the block into the memory page */
    size_t bytesRead = fread(memPage, 1, PAGE_SIZE, file);
    if (bytesRead != PAGE_SIZE) {
        fclose(file);
        return RC_READ_NON_EXISTING_PAGE;
    }

    fHandle->curPagePos = pageNum; // Update current page position on success
    fclose(file);
    return RC_OK;
}

/* Get the current block position (page number) */
int getBlockPos(SM_FileHandle* fHandle) {
    return fHandle->curPagePos;
}

/* Read the first block from the file */
RC readFirstBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    RC rc = readBlock(0, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = 0;
    }
    return rc;
}

/* Read the previous block from the current block */
RC readPreviousBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    int prevPageNum = fHandle->curPagePos - 1;
    if (prevPageNum < 0) {
        return RC_READ_NON_EXISTING_PAGE; // No previous page exists
    }
    RC rc = readBlock(prevPageNum, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = prevPageNum;
    }
    return rc;
}

/* Read the current block */
RC readCurrentBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    RC rc = readBlock(fHandle->curPagePos, fHandle, memPage);
    // Current page position remains the same on success
    return rc;
}

/* Read the next block from the current block */
RC readNextBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    int nextPageNum = fHandle->curPagePos + 1;
    if (nextPageNum >= fHandle->totalNumPages) {
        return RC_READ_NON_EXISTING_PAGE; // No next page exists
    }
    RC rc = readBlock(nextPageNum, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = nextPageNum;
    }
    return rc;
}

/* Read the last block from the file */
RC readLastBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    int lastPageNum = fHandle->totalNumPages - 1;
    RC rc = readBlock(lastPageNum, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = lastPageNum;
    }
    return rc;
}
