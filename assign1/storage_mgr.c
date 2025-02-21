#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "storage_mgr.h"

/*
 * Function: initStorageManager
 * ----------------------------
 *   A placeholder initialization function for the storage manager.
 *   Currently, no initialization is required.
 */
void initStorageManager(void) {
    // No initialization required for this storage manager implementation.
}

/*
 * Function: createPageFile
 * ------------------------
 *   Creates a new file with the specified file name.
 *   The file is initialized to one page in size (PAGE_SIZE bytes)
 *   and all bytes are set to zero.
 *
 * Parameters:
 *   fileName - A string representing the name of the file to be created.
 *
 * Returns:
 *   RC_FILE_ALREADY_EXISTING - The file already exists.
 *   RC_FILE_CREATION_FAILED  - The file could not be created.
 *   RC_WRITE_FAILED          - Writing zeroed page to file failed.
 *   RC_OK                    - Operation was successful.
 */
RC createPageFile(char *fileName) {
    // Open the file for reading to check if it exists
    FILE *filePointer = fopen(fileName, "r");
    if (filePointer != NULL) {
        // File exists, so close it and return an error code
        fclose(filePointer);
        return RC_FILE_ALREADY_EXISTING;
    }

    // Create the new file in binary write mode (read/write allowed)
    filePointer = fopen(fileName, "wb+");
    if (filePointer == NULL) {
        // File creation failed
        return RC_FILE_CREATION_FAILED;
    }

    // Allocate memory for one page and initialize all bytes to 0.
    char *pageData = (char *) calloc(PAGE_SIZE, sizeof(char));
    if (pageData == NULL) {
        // Memory allocation failure: close file and indicate failure.
        fclose(filePointer);
        return RC_FILE_CREATION_FAILED;
    }

    // Write the zeroed page to the file.
    if (fwrite(pageData, sizeof(char), PAGE_SIZE, filePointer) != PAGE_SIZE) {
        // If not all bytes could be written, clean up and return error.
        free(pageData);
        fclose(filePointer);
        return RC_WRITE_FAILED;
    }

    // Clean up: free allocated memory and close the file.
    free(pageData);
    fclose(filePointer);

    return RC_OK;
}

/*
 * Function: openPageFile
 * ----------------------
 *   Opens an existing page file.
 *
 * Parameters:
 *   fileName - A string representing the name of the file to be opened.
 *   fHandle  - A pointer to a SM_FileHandle structure which will be
 *              initialized with file details.
 *
 * Returns:
 *   RC_FILE_NOT_FOUND         - File does not exist or cannot be opened.
 *   RC_READ_NON_EXISTING_PAGE - File size is inconsistent with page size.
 *   RC_OK                     - Operation was successful.
 */
RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
    // Open the file in read/write binary mode.
    FILE *filePointer = fopen(fileName, "rb+");
    if (filePointer == NULL) {
        // File not found or permission issues.
        return RC_FILE_NOT_FOUND;
    }

    // Initialize file handle structure.
    fHandle->fileName = fileName;
    fHandle->mgmtInfo = filePointer;
    fHandle->curPagePos = 0;

    // Move file pointer to the end to calculate the file size.
    if (fseek(filePointer, 0, SEEK_END) != 0) {
        fclose(filePointer);
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Get the size of the file.
    long fileSize = ftell(filePointer);
    if (fileSize < 0) {
        fclose(filePointer);
        return RC_READ_NON_EXISTING_PAGE;
    }
    // Reset file pointer to beginning.
    rewind(filePointer);

    // Ensure file size is an exact multiple of PAGE_SIZE.
    if (fileSize % PAGE_SIZE != 0) {
        fclose(filePointer);
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Set the total number of pages in the file.
    fHandle->totalNumPages = fileSize / PAGE_SIZE;

    return RC_OK;
}

/*
 * Function: closePageFile
 * -----------------------
 *   Closes an open file and resets the file handle.
 *
 * Parameters:
 *   fHandle - A pointer to the SM_FileHandle structure representing
 *             the open file.
 *
 * Returns:
 *   RC_FILE_NOT_FOUND       - If file closing fails.
 *   RC_FILE_HANDLE_NOT_INIT - If the file handle is not correctly initialized.
 *   RC_OK                   - Operation was successful.
 */
RC closePageFile(SM_FileHandle *fHandle) {
    // Check if the file handle is valid.
    if (fHandle == NULL || fHandle->mgmtInfo == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    // Attempt to close the file.
    if (fclose(fHandle->mgmtInfo) != 0) {
        return RC_FILE_NOT_FOUND;
    }

    // Reset file handle management info.
    fHandle->mgmtInfo = NULL;

    return RC_OK;
}

/*
 * Function: destroyPageFile
 * -------------------------
 *   Deletes the file with the specified file name from the disk.
 *
 * Parameters:
 *   fileName - A string representing the name of the file to be destroyed.
 *
 * Returns:
 *   RC_FILE_NOT_FOUND - If the file does not exist or deletion fails.
 *   RC_OK             - Operation was successful.
 */
RC destroyPageFile(char *fileName) {
    // Remove the file from the file system.
    if (remove(fileName) != 0) {
        return RC_FILE_NOT_FOUND;
    }
    return RC_OK;
}

/*
 * Function: readBlock
 * -------------------
 *   Reads a block identified by the page number from the file into the memory.
 *
 * Parameters:
 *   pageNum  - The page number of the block to be read.
 *   fHandle  - The file handle structure containing file information.
 *   memPage  - A buffer where the read page content will be stored.
 *
 * Returns:
 *   RC_FILE_NOT_FOUND         - If the file cannot be opened.
 *   RC_READ_NON_EXISTING_PAGE - If the specified page does not exist.
 *   RC_OK                     - Operation was successful.
 */
RC readBlock(int pageNum, SM_FileHandle* fHandle, SM_PageHandle memPage) {
    // Open the file in read mode.
    FILE* file = fopen(fHandle->fileName, "r");
    if (file == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    // Move the file pointer to the start of the requested page.
    if (fseek(file, pageNum * PAGE_SIZE, SEEK_SET) != 0) {
        fclose(file);
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Read PAGE_SIZE bytes into memPage.
    size_t bytesRead = fread(memPage, 1, PAGE_SIZE, file);
    if (bytesRead != PAGE_SIZE) {
        fclose(file);
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Update current page position in file handle.
    fHandle->curPagePos = pageNum;
    fclose(file);
    return RC_OK;
}

/*
 * Function: getBlockPos
 * ---------------------
 *   Returns the current block (page) position in the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *
 * Returns:
 *   An integer corresponding to the current page position.
 */
int getBlockPos(SM_FileHandle* fHandle) {
    return fHandle->curPagePos;
}

/*
 * Function: readFirstBlock
 * ------------------------
 *   Reads the first block from the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *   memPage - Buffer to store the first page content.
 *
 * Returns:
 *   RC_OK or error code from readBlock.
 */
RC readFirstBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    RC rc = readBlock(0, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = 0;
    }
    return rc;
}

/*
 * Function: readPreviousBlock
 * ---------------------------
 *   Reads the block preceding the current block in the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *   memPage - Buffer to store the previous page content.
 *
 * Returns:
 *   RC_READ_NON_EXISTING_PAGE - If no previous block exists.
 *   RC_OK                   - Operation was successful.
 */
RC readPreviousBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    int prevPageNum = fHandle->curPagePos - 1;
    if (prevPageNum < 0) {
        return RC_READ_NON_EXISTING_PAGE; // No previous page exists.
    }
    RC rc = readBlock(prevPageNum, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = prevPageNum;
    }
    return rc;
}

/*
 * Function: readCurrentBlock
 * --------------------------
 *   Reads the current block from the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *   memPage - Buffer to store the current page content.
 *
 * Returns:
 *   RC_OK or appropriate error code from readBlock.
 */
RC readCurrentBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    // The current page is read without changing curPagePos.
    RC rc = readBlock(fHandle->curPagePos, fHandle, memPage);
    return rc;
}

/*
 * Function: readNextBlock
 * -----------------------
 *   Reads the block that follows the current block in the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *   memPage - Buffer to store the next page content.
 *
 * Returns:
 *   RC_READ_NON_EXISTING_PAGE - If no next block exists.
 *   RC_OK                   - Operation was successful.
 */
RC readNextBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    int nextPageNum = fHandle->curPagePos + 1;
    if (nextPageNum >= fHandle->totalNumPages) {
        return RC_READ_NON_EXISTING_PAGE; // No next page exists.
    }
    RC rc = readBlock(nextPageNum, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = nextPageNum;
    }
    return rc;
}

/*
 * Function: readLastBlock
 * -----------------------
 *   Reads the last block (page) from the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *   memPage - Buffer to store the last page content.
 *
 * Returns:
 *   RC_OK if the read operation is successful, or error code otherwise.
 */
RC readLastBlock(SM_FileHandle* fHandle, SM_PageHandle memPage) {
    int lastPageNum = fHandle->totalNumPages - 1;
    RC rc = readBlock(lastPageNum, fHandle, memPage);
    if (rc == RC_OK) {
        fHandle->curPagePos = lastPageNum;
    }
    return rc;
}

/*
 * Function: writeBlock
 * --------------------
 *   Writes a block to a specified page number in the file.
 *
 * Parameters:
 *   pageNum  - The page number where data will be written.
 *   fHandle  - The file handle structure.
 *   memPage  - The source buffer that contains data to write.
 *
 * Returns:
 *   RC_WRITE_FAILED    - If page number is invalid or write operation fails.
 *   RC_FILE_NOT_FOUND  - If the file cannot be opened.
 *   RC_OK              - Operation was successful.
 */
RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    // Validate if the page number is within acceptable range.
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        return RC_WRITE_FAILED;

    // Open the file in read/write mode.
    FILE *file = fopen(fHandle->fileName, "r+");
    if (!file)
        return RC_FILE_NOT_FOUND;

    // Move file pointer to the correct page location.
    fseek(file, pageNum * PAGE_SIZE, SEEK_SET);
    // Write PAGE_SIZE data from memPage to the file.
    fwrite(memPage, sizeof(char), PAGE_SIZE, file);
    fclose(file);

    return RC_OK;
}

/*
 * Function: writeCurrentBlock
 * ---------------------------
 *   Writes data to the current block (page) of the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *   memPage - Buffer that contains the data to be written.
 *
 * Returns:
 *   The result of writeBlock() called on the current page.
 */
RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

/*
 * Function: appendEmptyBlock
 * --------------------------
 *   Appends an empty block (page of PAGE_SIZE zero bytes) at the end of the file.
 *
 * Parameters:
 *   fHandle - The file handle structure.
 *
 * Returns:
 *   RC_FILE_NOT_FOUND - If the file cannot be opened for appending.
 *   RC_OK             - Operation was successful.
 */
RC appendEmptyBlock(SM_FileHandle *fHandle) {
    // Open file in append mode.
    FILE *file = fopen(fHandle->fileName, "a");
    if (!file)
        return RC_FILE_NOT_FOUND;

    // Prepare a zeroed page to be appended.
    char emptyPage[PAGE_SIZE];
    memset(emptyPage, 0, PAGE_SIZE);
    // Write the empty page.
    fwrite(emptyPage, sizeof(char), PAGE_SIZE, file);
    fclose(file);

    // Update the total number of pages in the file handle.
    fHandle->totalNumPages++;
    return RC_OK;
}

/*
 * Function: ensureCapacity
 * ------------------------
 *   Ensures that the file has at least the specified number of pages.
 *   If the file currently has fewer pages, empty pages are appended.
 *
 * Parameters:
 *   numberOfPages - The required minimum number of pages.
 *   fHandle       - The file handle structure.
 *
 * Returns:
 *   RC_OK - Operation was successful (capacity ensured).
 */
RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {
    // Append empty blocks until the file reaches the required capacity.
    while (fHandle->totalNumPages < numberOfPages)
        appendEmptyBlock(fHandle);
    return RC_OK;
}