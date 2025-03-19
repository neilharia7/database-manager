#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "dberror.h"
#include <stdlib.h>
#include <string.h>

// Define a new error code for pinned pages
#define RC_PINNED_PAGES 100

/**
 * Internal management data for the buffer pool.
 * This structure holds the file handle for the page file and arrays for managing
 * the status of each page frame.
 */
typedef struct BP_MgmtData {
    SM_FileHandle fileHandle;      // Storage Manager file handle for the page file
    BM_PageHandle **pageFrames;    // Array of page frame pointers; initially NULL
    int *fixCounts;                // Array to count the number of clients (pins) per page frame
    bool *dirtyFlags;              // Array to indicate if a page frame has been modified
    void *strategyData;            // Extra parameters for the replacement strategy
    int numPages;                  // Number of pages in the buffer pool
} BP_MgmtData;

/**
 * Function: initBufferPool
 * ------------------------
 * Creates a new buffer pool that manages a page file. It initializes the pool
 * with numPages page frames (all initially empty) and assigns the specified
 * page replacement strategy. The page file with the name pageFileName must already exist.
 * stratData may carry additional parameters needed for the replacement strategy.
 *
 * Parameters:
 *   bm           - Pointer to the buffer pool structure to be initialized.
 *   pageFileName - Name of the page file to be managed.
 *   numPages     - Number of page frames in the buffer pool.
 *   strategy     - Page replacement strategy to be used (e.g., RS_FIFO, RS_LRU, etc.).
 *   stratData    - Extra parameter(s) for the replacement strategy (for instance, k for RS_LRU_K).
 *
 * Returns:
 *   RC_INVALID_PARAM             - If any of the input parameters are invalid.
 *   RC_MEMORY_ALLOCATION_ERROR   - If memory allocation fails.
 *   RC_OK                        - Operation was successful.
 */
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy, void *stratData) {
    if (bm == NULL || pageFileName == NULL || numPages <= 0) {
        return RC_INVALID_PARAM;
    }

    // Check if the page file already exists by attempting to open it
    SM_FileHandle fileHandle;
    RC rc = openPageFile((char*)pageFileName, &fileHandle);
    if (rc != RC_OK) {
        // The page file does not exist, which violates the precondition
        return rc;
    }

    // Create and initialize the internal management structure
    BP_MgmtData *mgmtData = (BP_MgmtData*) malloc(sizeof(BP_MgmtData));
    if (mgmtData == NULL) {
        return RC_MEMORY_ALLOCATION_ERROR;
    }

    // Initialize the management data
    mgmtData->fileHandle = fileHandle;
    mgmtData->numPages = numPages;
    mgmtData->strategyData = stratData;

    // Allocate arrays for page frames, fix counts, and dirty flags
    mgmtData->pageFrames = (BM_PageHandle**) calloc(numPages, sizeof(BM_PageHandle*));
    if (mgmtData->pageFrames == NULL) {
        free(mgmtData);
        return RC_MEMORY_ALLOCATION_ERROR;
    }

    mgmtData->fixCounts = (int*) calloc(numPages, sizeof(int));
    if (mgmtData->fixCounts == NULL) {
        free(mgmtData->pageFrames);
        free(mgmtData);
        return RC_MEMORY_ALLOCATION_ERROR;
    }

    mgmtData->dirtyFlags = (bool*) calloc(numPages, sizeof(bool));
    if (mgmtData->dirtyFlags == NULL) {
        free(mgmtData->fixCounts);
        free(mgmtData->pageFrames);
        free(mgmtData);
        return RC_MEMORY_ALLOCATION_ERROR;
    }

    // Initialize all page frames to NULL, fix counts to 0, and dirty flags to false
    int i;
    for (i = 0; i < numPages; i++) {
        mgmtData->pageFrames[i] = NULL;
        mgmtData->fixCounts[i] = 0;
        mgmtData->dirtyFlags[i] = FALSE;
    }

    // Allocate and copy the page file name into the BM_BufferPool structure
    bm->pageFile = strdup(pageFileName);
    if (bm->pageFile == NULL) {
        free(mgmtData->dirtyFlags);
        free(mgmtData->fixCounts);
        free(mgmtData->pageFrames);
        free(mgmtData);
        return RC_MEMORY_ALLOCATION_ERROR;
    }

    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = mgmtData;

    return RC_OK;
}

/**
 * Function: forceFlushPool
 * ------------------------
 * Causes all dirty pages with a fix count of 0 in the buffer pool to be written to disk.
 *
 * For each page frame:
 *   - If the frame is loaded and is dirty with no active pins, the page is written back to disk.
 *   - On successful write, the dirty flag is reset.
 *
 * Parameters:
 *   bm - Pointer to the buffer pool structure.
 *
 * Returns:
 *   RC_OK on success, or an appropriate error code if writing a page fails.
 */
RC forceFlushPool(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return RC_INVALID_PARAM;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    int i;

    for (i = 0; i < bm->numPages; i++) {
        // Check if the page frame is loaded
        if (mgmtData->pageFrames[i] != NULL) {
            // If the page is dirty and not pinned, write it back to disk
            if (mgmtData->dirtyFlags[i] && mgmtData->fixCounts[i] == 0) {
                RC rc = writeBlock(mgmtData->pageFrames[i]->pageNum,
                                  &mgmtData->fileHandle,
                                  mgmtData->pageFrames[i]->data);
                if (rc != RC_OK) {
                    return rc;
                }
                mgmtData->dirtyFlags[i] = FALSE;
            }
        }
    }

    return RC_OK;
}

/**
 * Function: shutdownBufferPool
 * ----------------------------
 * Destroys a buffer pool and frees all associated resources.
 *
 * The process involves:
 *   - Checking for any pinned pages. It is an error to shutdown a pool if any page is still pinned.
 *   - Flushing all dirty pages (with fix count 0) to disk.
 *   - Freeing all memory allocated for page frames and other auxiliary arrays.
 *   - Closing the page file.
 *
 * Parameters:
 *   bm - Pointer to the buffer pool structure.
 *
 * Returns:
 *   RC_OK on success, or an appropriate error code if shutdown fails.
 */
RC shutdownBufferPool(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return RC_INVALID_PARAM;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    int i;

    // Check for pinned pages; shutdown is an error if any page is still pinned
    for (i = 0; i < bm->numPages; i++) {
        if (mgmtData->fixCounts[i] > 0) {
            return RC_PINNED_PAGES;
        }
    }

    // Flush all dirty pages with fix count 0 to disk
    RC rc = forceFlushPool(bm);
    if (rc != RC_OK) {
        return rc;
    }

    // Close the page file
    rc = closePageFile(&mgmtData->fileHandle);
    if (rc != RC_OK) {
        return rc;
    }

    // Free memory associated with each page frame
    for (i = 0; i < bm->numPages; i++) {
        if (mgmtData->pageFrames[i] != NULL) {
            if (mgmtData->pageFrames[i]->data != NULL) {
                free(mgmtData->pageFrames[i]->data);
            }
            free(mgmtData->pageFrames[i]);
        }
    }

    // Free the arrays and management data structure
    free(mgmtData->dirtyFlags);
    free(mgmtData->fixCounts);
    free(mgmtData->pageFrames);
    free(mgmtData);
    bm->mgmtData = NULL;

    // Free the allocated page file name
    free(bm->pageFile);
    bm->pageFile = NULL;

    return RC_OK;
}

// TESTING
// TODO remove

#ifdef TEST_BUFFER_MGR
#include <stdio.h>

int main() {
    // This main function is for testing purposes
    BM_BufferPool bufferPool;
    RC rc = initBufferPool(&bufferPool, "test_pagefile.bin", 5, RS_LRU, NULL);
    if (rc != RC_OK) {
        printf("initBufferPool failed with error code: %d\n", rc);
        return -1;
    }

    // Additional test code adding pages, marking dirty pages, etc., would go here

    rc = shutdownBufferPool(&bufferPool);
    if (rc != RC_OK) {
        printf("shutdownBufferPool failed with error code: %d\n", rc);
        return -1;
    }

    printf("Buffer pool shutdown successfully.\n");
    return 0;
}
#endif