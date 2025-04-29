#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "storage_mgr.h"

#define CHECK_FILE_VALIDITY(fileHandle)  \
do{ \
  if(fileHandle == NULL || fileHandle->mgmtInfo == NULL){   \
   return  RC_FILE_HANDLE_NOT_INIT;   \
  } \
}while(0);

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
 *   RC_WRITE_FAILED          - Writing zeroed page to file failed.
 *   RC_OK                    - Operation was successful.
 */
RC createPageFile(char *fileName) {
	// Open the file for reading to check if it exists
	if (fopen(fileName, "r") != NULL) {
		return RC_FILE_ALREADY_EXISTS;
	}

	// Create the new file in binary write mode (read/write allowed)
	FILE *filePointer = fopen(fileName, "w");
	// Allocate memory for one page and initialize all bytes to 0.
	char *totalPages = calloc(PAGE_SIZE, PAGE_ELEMENT_SIZE);
	char *firstPage = calloc(PAGE_SIZE, PAGE_ELEMENT_SIZE);
	strcpy(totalPages, "1\n");

	// Write the zeroed page to the file.
	fwrite(totalPages, PAGE_ELEMENT_SIZE, PAGE_SIZE, filePointer);
	fwrite(firstPage, PAGE_ELEMENT_SIZE, PAGE_SIZE, filePointer);

	// Clean up: free allocated memory and close the file.
	free(totalPages);
	free(firstPage);
	fclose(filePointer);

	return RC_OK;
}

/*
 * Function: openPageFile
 * ----------------------
 *   Opens an existing page file and initializes the file handle with its metadata.
 *
 * Parameters:
 *   fileName  - A string representing the name of the file to be opened.
 *   fileHandle - A pointer to a SM_FileHandle structure which will be
 *                initialized with file details.
 *
 * Returns:
 *   RC_FILE_NOT_FOUND         - File does not exist or cannot be opened.
 *   RC_READ_NON_EXISTING_PAGE - Error reading page metadata.
 *   RC_OK                     - Operation was successful.
 */
RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
	// Validate input parameters
	if (fileName == NULL || fHandle == NULL) {
		return RC_FILE_NOT_FOUND;
	}

	// Open the file in read/write mode
	FILE *filePointer = fopen(fileName, "r+");
	if (filePointer == NULL) {
		return RC_FILE_NOT_FOUND;
	}

	// Allocate memory for reading the page count from the file header
	char *readPage = calloc(PAGE_SIZE, PAGE_ELEMENT_SIZE);
	if (readPage == NULL) {
		fclose(filePointer);
		return RC_READ_NON_EXISTING_PAGE;
	}

	// Read the first line containing the total number of pages
	if (fgets(readPage, PAGE_SIZE, filePointer) == NULL) {
		free(readPage);
		fclose(filePointer);
		return RC_READ_NON_EXISTING_PAGE;
	}

	// Parse the total number of pages
	char *pageCountStr = strtok(readPage, "\n");
	if (pageCountStr == NULL) {
		free(readPage);
		fclose(filePointer);
		return RC_READ_NON_EXISTING_PAGE;
	}

	// Initialize the file handle with the file metadata
	fHandle->fileName = fileName;
	fHandle->totalNumPages = atoi(pageCountStr);
	fHandle->curPagePos = 0;
	fHandle->mgmtInfo = filePointer;

	// Clean up and return success
	free(readPage);
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
	if (fHandle == NULL || fHandle->mgmtInfo == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if (fclose(fHandle->mgmtInfo) != 0)
		return RC_FILE_NOT_FOUND;

	// Reset file handle
	fHandle->totalNumPages = 0;
	fHandle->curPagePos = 0;
	fHandle->fileName = NULL;
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
 *   RC_FILE_HANDLE_NOT_INIT   - If the file handle is not correctly initialized.
 *   RC_READ_NON_EXISTING_PAGE - If the specified page number is out of bounds.
 *   RC_READ_FAILED            - If file seek or read operations fail.
 *   RC_OK                     - Operation was successful.
 */
RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
	// Check if the file handle is valid.
	if (fHandle == NULL || fHandle->mgmtInfo == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	// Validate page number
	if (pageNum < 0 || pageNum >= fHandle->totalNumPages) {
		return RC_READ_NON_EXISTING_PAGE;
	}

	// Calculate the position to seek to (add 1 to account for the header page)
	long seekPosition = (pageNum + 1) * PAGE_SIZE;

	// Move file pointer to the beginning of the requested page
	if (fseek(fHandle->mgmtInfo, seekPosition, SEEK_SET) != 0) {
		return RC_READ_FAILED;
	}

	// Read the page content into the provided memory buffer
	size_t elementsRead = fread(memPage, PAGE_ELEMENT_SIZE, PAGE_SIZE, fHandle->mgmtInfo);
	if (elementsRead != PAGE_SIZE) {
		return RC_READ_FAILED;
	}

	// Update current page position in the file handle
	fHandle->curPagePos = pageNum;

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
	return readBlock(fHandle->curPagePos, fHandle, memPage);
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
RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
	return readBlock(fHandle->curPagePos + 1, fHandle, memPage);
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
RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
	return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

/*
 * Function: writeBlockData
 * ------------------------
 *   Helper function that writes data from memory to a specific page in the file.
 *   Updates the current page position and flushes the file buffer.
 *
 * Parameters:
 *   pageNum    - The page number where data will be written.
 *   fileHandle - A pointer to the SM_FileHandle structure.
 *   memPage    - The source buffer containing data to write.
 *
 * Returns:
 *   RC_WRITE_FAILED - If the write operation fails.
 *   RC_OK           - Operation was successful.
 */
RC writeBlockData(int pageNum, SM_FileHandle *fileHandle, SM_PageHandle memPage) {
	// Write PAGE_SIZE data from memPage to the file
	size_t elementsWritten = fwrite(memPage, PAGE_ELEMENT_SIZE, PAGE_SIZE, fileHandle->mgmtInfo);
	if (elementsWritten != PAGE_SIZE) {
		return RC_WRITE_FAILED;
	}

	// Update current page position and ensure data is written to disk
	fileHandle->curPagePos = pageNum;
	fflush(fileHandle->mgmtInfo);

	return RC_OK;
}

/*
 * Function: writeBlock
 * --------------------
 *   Writes a block of data to a specified page number in the file.
 *   If the page doesn't exist but is just after the last page, it will be created.
 *
 * Parameters:
 *   pageNum    - The page number where data will be written.
 *   fileHandle - A pointer to the SM_FileHandle structure.
 *   memPage    - The source buffer containing data to write.
 *
 * Returns:
 *   RC_FILE_HANDLE_NOT_INIT     - If the file handle is not correctly initialized.
 *   RC_WRITE_NON_EXISTING_PAGE  - If the specified page number is out of bounds.
 *   RC_WRITE_FAILED             - If file seek or write operations fail.
 *   RC_OK                       - Operation was successful.
 */
RC writeBlock(int pageNum, SM_FileHandle *fileHandle, SM_PageHandle memPage) {
	// Validate file handle
	CHECK_FILE_VALIDITY(fileHandle);

	// Validate page number
	if (pageNum < 0 || pageNum > fileHandle->totalNumPages) {
		return RC_WRITE_NON_EXISTING_PAGE;
	}

	// If writing to the page just after the last page, append a new block first
	if (pageNum == fileHandle->totalNumPages) {
		RC appendResult = appendEmptyBlock(fileHandle);
		if (appendResult != RC_OK) {
			return appendResult;
		}
	}

	// Get current file position
	long currentPosition = ftell(fileHandle->mgmtInfo);

	// Calculate target position (add 1 to account for the header page)
	long targetPosition = (pageNum + 1) * PAGE_SIZE * PAGE_ELEMENT_SIZE;

	// If already at the correct position, write directly
	if (currentPosition == targetPosition) {
		return writeBlockData(pageNum, fileHandle, memPage);
	}

	// Otherwise, seek to the correct position and then write
	if (fseek(fileHandle->mgmtInfo, targetPosition, SEEK_SET) == 0) {
		return writeBlockData(pageNum, fileHandle, memPage);
	}

	return RC_WRITE_FAILED;
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
 *   Appends an empty block (page) filled with zeros to the end of the file.
 *   Updates the total number of pages in the file handle and writes the updated
 *   count to the file header.
 *
 * Parameters:
 *   fileHandle - A pointer to the SM_FileHandle structure.
 *
 * Returns:
 *   RC_FILE_HANDLE_NOT_INIT - If the file handle is not correctly initialized.
 *   RC_WRITE_FAILED         - If file operations (seek, write) fail.
 *   RC_OK                   - Operation was successful.
 */
RC appendEmptyBlock(SM_FileHandle *fileHandle) {
	// Validate file handle
	CHECK_FILE_VALIDITY(fileHandle);

	// Create an empty page filled with zeros
	char *emptyBlock = calloc(PAGE_SIZE, PAGE_ELEMENT_SIZE);
	if (emptyBlock == NULL) {
		return RC_WRITE_FAILED;
	}

	// Seek to the end of the file
	if (fseek(fileHandle->mgmtInfo, 0, SEEK_END) != 0) {
		free(emptyBlock);
		return RC_WRITE_FAILED;
	}

	// Write the empty block to the file
	size_t elementsWritten = fwrite(emptyBlock, PAGE_ELEMENT_SIZE, PAGE_SIZE, fileHandle->mgmtInfo);
	free(emptyBlock); // Free memory regardless of write success

	if (elementsWritten != PAGE_SIZE) {
		return RC_WRITE_FAILED;
	}

	// Update the total number of pages in the file handle
	fileHandle->totalNumPages++;

	// Update the page count in the file header
	if (fseek(fileHandle->mgmtInfo, 0, SEEK_SET) != 0) {
		return RC_WRITE_FAILED;
	}

	// Write the updated page count to the header
	fprintf(fileHandle->mgmtInfo, "%d\n", fileHandle->totalNumPages);

	// Ensure data is written to disk
	fflush(fileHandle->mgmtInfo);

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