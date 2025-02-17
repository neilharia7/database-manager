#include "storage_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        return RC_WRITE_FAILED;

    FILE *file = fopen(fHandle->fileName, "r+");
    if (!file) return RC_FILE_NOT_FOUND;

    fseek(file, pageNum * PAGE_SIZE, SEEK_SET);
    fwrite(memPage, sizeof(char), PAGE_SIZE, file);
    fclose(file);

    return RC_OK;
}


RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

RC appendEmptyBlock(SM_FileHandle *fHandle) {
    FILE *file = fopen(fHandle->fileName, "a");
    if (!file) return RC_FILE_NOT_FOUND;

    char emptyPage[PAGE_SIZE];
    memset(emptyPage, 0, PAGE_SIZE);
    fwrite(emptyPage, sizeof(char), PAGE_SIZE, file);
    fclose(file);

    fHandle->totalNumPages++;
    return RC_OK;
}

RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {
    while (fHandle->totalNumPages < numberOfPages)
        appendEmptyBlock(fHandle);
    return RC_OK;
}