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

    // Additional fields for tracking IO and loaded page numbers
    int readIO;                    // Counter for read operations from disk
    int writeIO;                   // Counter for write operations to disk
    PageNumber *framePageNumbers;  // Track which page is loaded in each frame

    int *lastUsed;                 // LRU: store "last used clock" for each frame
    int lruClock;                  // Increments on each page pin to track recency
} BP_MgmtData;


/**
 * Helper function to find if a page is already in the pool.
 *
 * This function scans through all frames in the buffer pool to check if the
 * specified page is already loaded. This is essential for avoiding duplicate
 * loading of the same page.
 *
 * Parameters:
 *   mgmtData - Pointer to the buffer pool management data
 *   pageNum  - The page number to search for
 *
 * Returns:
 *   The frame index if the page is found, otherwise returns -1.
 */
static int findPageInPool(BP_MgmtData *mgmtData, PageNumber pageNum) {
    for (int i = 0; i < mgmtData->numPages; i++) {
        if (mgmtData->pageFrames[i] != NULL &&
            mgmtData->pageFrames[i]->pageNum == pageNum) {
            return i;
            }
    }
    return -1;
}

/**
 * Helper function to select a frame for replacement using LRU strategy.
 *
 * This function implements the page replacement policy by:
 * 1. First looking for any empty frame
 * 2. If no empty frames, selecting an unpinned frame with the oldest "last used" timestamp
 * 3. If the selected frame is dirty, writing it back to disk before replacement
 *
 * The LRU (Least Recently Used) strategy ensures that pages that haven't been
 * accessed for the longest time are replaced first.
 *
 * Parameters:
 *   mgmtData - Pointer to the buffer pool management data
 *
 * Returns:
 *   The frame index to use for the new page, or -1 if all frames are pinned.
 */
static int getFrameToReplace(BP_MgmtData *mgmtData) {
    // 1) Look for an empty frame first.
    for (int i = 0; i < mgmtData->numPages; i++) {
        if (mgmtData->pageFrames[i] == NULL) {
            return i;
        }
    }

    // 2) If none empty, pick the unpinned frame with the oldest (lowest) lastUsed.
    int victimIndex = -1;
    int oldestTime = __INT_MAX__; // or some sentinel large value
    for (int i = 0; i < mgmtData->numPages; i++) {
        if (mgmtData->fixCounts[i] == 0) {
            if (mgmtData->lastUsed[i] < oldestTime) {
                oldestTime = mgmtData->lastUsed[i];
                victimIndex = i;
            }
        }
    }

    // if victimIndex is still -1, all frames are pinned
    if (victimIndex == -1) {
        return -1;
    }

    // If it's dirty, write it out before replacement
    if (mgmtData->dirtyFlags[victimIndex]) {
        writeBlock(
            mgmtData->pageFrames[victimIndex]->pageNum,
            &mgmtData->fileHandle,
            mgmtData->pageFrames[victimIndex]->data
        );
        mgmtData->writeIO++;
        mgmtData->dirtyFlags[victimIndex] = false;
    }
    return victimIndex;
}


/**
 * Function: initBufferPool
 * ------------------------
 * Creates and initializes a new buffer pool for managing a page file.
 *
 * This function sets up all the necessary data structures for the buffer pool:
 * - Allocates memory for management data
 * - Opens the specified page file
 * - Initializes arrays for tracking page frames, fix counts, dirty flags, etc.
 * - Sets up the page replacement strategy
 *
 * The buffer pool starts with all frames empty (NULL) and will load pages
 * on demand as they are requested.
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

    SM_FileHandle fileHandle;
    RC rc = openPageFile((char*)pageFileName, &fileHandle);
    if (rc != RC_OK) {
        return rc;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) malloc(sizeof(BP_MgmtData));
    if (mgmtData == NULL) {
        return RC_MEMORY_ALLOCATION_ERROR;
    }

    mgmtData->fileHandle = fileHandle;
    mgmtData->numPages = numPages;
    mgmtData->strategyData = stratData;
    mgmtData->readIO = 0;
    mgmtData->writeIO = 0;

    mgmtData->pageFrames = (BM_PageHandle**) calloc(numPages, sizeof(BM_PageHandle*));
    mgmtData->fixCounts  = (int*) calloc(numPages, sizeof(int));
    mgmtData->dirtyFlags = (bool*) calloc(numPages, sizeof(bool));
    mgmtData->framePageNumbers = (PageNumber*) calloc(numPages, sizeof(PageNumber));
    mgmtData->lastUsed   = (int*) calloc(numPages, sizeof(int)); // LRU array
    mgmtData->lruClock   = 1; // start clock at 1, or 0, your choice

    if (!mgmtData->pageFrames || !mgmtData->fixCounts || !mgmtData->dirtyFlags
        || !mgmtData->framePageNumbers || !mgmtData->lastUsed) {
        // free everything allocated so far
        free(mgmtData->pageFrames);
        free(mgmtData->fixCounts);
        free(mgmtData->dirtyFlags);
        free(mgmtData->framePageNumbers);
        free(mgmtData->lastUsed);
        free(mgmtData);
        return RC_MEMORY_ALLOCATION_ERROR;
    }

    for (int i = 0; i < numPages; i++) {
        mgmtData->pageFrames[i]       = NULL;
        mgmtData->fixCounts[i]        = 0;
        mgmtData->dirtyFlags[i]       = false;
        mgmtData->framePageNumbers[i] = NO_PAGE;
        mgmtData->lastUsed[i]         = 0; // not used yet
    }

    bm->pageFile = strdup(pageFileName);
    if (bm->pageFile == NULL) {
        free(mgmtData->pageFrames);
        free(mgmtData->fixCounts);
        free(mgmtData->dirtyFlags);
        free(mgmtData->framePageNumbers);
        free(mgmtData->lastUsed);
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
 * Writes all dirty, unpinned pages from the buffer pool to disk.
 *
 * This function is used to synchronize the buffer pool with the disk by writing
 * back any modified pages that aren't currently in use. This helps ensure data
 * durability and frees up dirty frames for potential reuse.
 *
 * The function:
 * 1. Iterates through all frames in the buffer pool
 * 2. For each frame that is loaded, dirty, and has a fix count of 0:
 *    - Writes the page back to disk
 *    - Updates the write I/O counter
 *    - Resets the dirty flag
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
                mgmtData->writeIO++;
                mgmtData->dirtyFlags[i] = FALSE;
            }
        }
    }

    return RC_OK;
}

/**
 * Function: shutdownBufferPool
 * ----------------------------
 * Destroys a buffer pool and releases all associated resources.
 *
 * This function performs a complete cleanup of the buffer pool:
 * 1. Verifies that no pages are still pinned (returns error if any are)
 * 2. Flushes all dirty pages to disk to ensure data persistence
 * 3. Closes the underlying page file
 * 4. Frees all allocated memory for page frames and management structures
 *
 * This is typically called at the end of database operations to ensure
 * proper cleanup and that all changes are persisted to disk.
 *
 * Parameters:
 *   bm - Pointer to the buffer pool structure.
 *
 * Returns:
 *   RC_OK on success
 *   RC_PINNED_PAGES if any page is still pinned
 *   Other error codes if file operations fail
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
    free(mgmtData->framePageNumbers);
    free(mgmtData->dirtyFlags);
    free(mgmtData->fixCounts);
    free(mgmtData->pageFrames);
    free(mgmtData->lastUsed);
    free(mgmtData);
    bm->mgmtData = NULL;

    // Free the allocated page file name
    free(bm->pageFile);
    bm->pageFile = NULL;

    return RC_OK;
}

/**
 * Function: markDirty
 * -------------------
 * Marks a page as dirty in the buffer pool, indicating it has been modified.
 *
 * When a client modifies the content of a page, this function should be called
 * to indicate that the page needs to be written back to disk eventually.
 * The dirty flag helps the buffer manager determine which pages need to be
 * flushed to disk during eviction or when explicitly requested.
 *
 * Parameters:
 *   bm   - The buffer pool.
 *   page - The page handle referring to the page to mark dirty.
 *
 * Returns:
 *   RC_OK if successful
 *   RC_PAGE_NOT_FOUND if the page is not found in the pool
 *   RC_INVALID_PARAM for NULL parameters
 */
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (bm == NULL || bm->mgmtData == NULL || page == NULL) {
        return RC_INVALID_PARAM;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    int frameIndex = findPageInPool(mgmtData, page->pageNum);
    if (frameIndex == -1) {
        // Page not found in pool
        return RC_PAGE_NOT_FOUND;
    }

    mgmtData->dirtyFlags[frameIndex] = TRUE;
    return RC_OK;
}

/**
 * Function: unpinPage
 * -------------------
 * Decrements the fix count for a page, indicating the client is done with it.
 *
 * When a client finishes using a page, it should unpin it to allow the buffer
 * manager to eventually evict the page if needed. Unpinning doesn't immediately
 * remove the page from memory - it just makes it eligible for replacement
 * when its fix count reaches zero.
 *
 * This is essential for the page replacement algorithm to work correctly,
 * as only unpinned pages can be selected for eviction.
 *
 * Parameters:
 *   bm   - The buffer pool.
 *   page - The page to unpin.
 *
 * Returns:
 *   RC_OK if successful
 *   RC_PAGE_NOT_FOUND if the page isn't found
 *   RC_INVALID_PARAM for NULL parameters
 */
RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (bm == NULL || bm->mgmtData == NULL || page == NULL) {
        return RC_INVALID_PARAM;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    int frameIndex = findPageInPool(mgmtData, page->pageNum);
    if (frameIndex == -1) {
        // Page not found
        return RC_PAGE_NOT_FOUND;
    }

    if (mgmtData->fixCounts[frameIndex] > 0) {
        mgmtData->fixCounts[frameIndex]--;
    }
    return RC_OK;
}

/**
 * Function: forcePage
 * -------------------
 * Immediately writes a page to disk if it's dirty, regardless of fix count.
 *
 * This function provides a way to explicitly persist changes to a specific page
 * without waiting for normal buffer management operations. This can be useful
 * for ensuring data durability at specific points in a transaction or operation.
 *
 * Unlike forceFlushPool, this function:
 * 1. Operates on a single specific page
 * 2. Writes the page even if it has a non-zero fix count
 * 3. Only writes if the page is marked as dirty
 *
 * Parameters:
 *   bm   - The buffer pool.
 *   page - The page to force write to disk.
 *
 * Returns:
 *   RC_OK if successful
 *   RC_PAGE_NOT_FOUND if the page isn't found
 *   Other error codes if the write operation fails
 */
RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (bm == NULL || bm->mgmtData == NULL || page == NULL) {
        return RC_INVALID_PARAM;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    int frameIndex = findPageInPool(mgmtData, page->pageNum);
    if (frameIndex == -1) {
        return RC_PAGE_NOT_FOUND;
    }

    if (mgmtData->dirtyFlags[frameIndex]) {
        RC rc = writeBlock(page->pageNum, &mgmtData->fileHandle, mgmtData->pageFrames[frameIndex]->data);
        if (rc != RC_OK) {
            return rc;
        }
        mgmtData->writeIO++;
        mgmtData->dirtyFlags[frameIndex] = FALSE;
    }
    return RC_OK;
}

/**
 * Function: pinPage
 * -----------------
 * Loads a page into the buffer pool and pins it for client use.
 *
 * This is the primary function for clients to access pages. It:
 * 1. Checks if the requested page is already in memory
 *    - If yes, increments its fix count and updates LRU information
 * 2. If not in memory:
 *    - Finds a frame to use (empty or victim for replacement)
 *    - Loads the page from disk into the selected frame
 *    - Sets up tracking information (fix count, dirty flag, etc.)
 *
 * The page replacement strategy (LRU in this implementation) is used when
 * selecting a victim frame. Only unpinned pages can be selected as victims.
 *
 * Parameters:
 *   bm      - The buffer pool.
 *   page    - The page handle to be populated with the pinned page.
 *   pageNum - The page number to pin.
 *
 * Returns:
 *   RC_OK if successful
 *   RC_PAGE_NOT_FOUND if no free frame can be found (all pages pinned)
 *   Other error codes for memory allocation or disk read failures
 */
RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum) {
    if (bm == NULL || bm->mgmtData == NULL || pageNum < 0) {
        return RC_INVALID_PARAM;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    int frameIndex = findPageInPool(mgmtData, pageNum);
    if (frameIndex != -1) {
        // already in memory - just update fix count and LRU information
        mgmtData->fixCounts[frameIndex]++;
        mgmtData->lastUsed[frameIndex] = mgmtData->lruClock++; // LRU update
        page->pageNum = pageNum;
        page->data = mgmtData->pageFrames[frameIndex]->data;
        return RC_OK;
    } else {
        // page not in memory - need to load it
        // search for a victim frame using replacement strategy
        int victimFrame = getFrameToReplace(mgmtData);
        if (victimFrame == -1) {
            // all frames are pinned, cannot replace any
            return RC_PAGE_NOT_FOUND;
        }

        // free old handle if present
        if (mgmtData->pageFrames[victimFrame] != NULL) {
            free(mgmtData->pageFrames[victimFrame]->data);
            free(mgmtData->pageFrames[victimFrame]);
        }

        // allocate new page handle and memory for page data
        BM_PageHandle *newPage = (BM_PageHandle*) malloc(sizeof(BM_PageHandle));
        if (!newPage) {
            return RC_MEMORY_ALLOCATION_ERROR;
        }
        newPage->pageNum = pageNum;
        newPage->data = (char*) malloc(PAGE_SIZE);
        if (!newPage->data) {
            free(newPage);
            return RC_MEMORY_ALLOCATION_ERROR;
        }

        // read block from disk (or ensure capacity if needed)
        RC rc = readBlock(pageNum, &mgmtData->fileHandle, newPage->data);
        if (rc == RC_READ_NON_EXISTING_PAGE) {
            // page doesn't exist yet, ensure file has enough capacity
            rc = ensureCapacity(pageNum + 1, &mgmtData->fileHandle);
            if (rc != RC_OK) {
                free(newPage->data);
                free(newPage);
                return rc;
            }
            // try reading again after ensuring capacity
            rc = readBlock(pageNum, &mgmtData->fileHandle, newPage->data);
        }
        if (rc != RC_OK) {
            free(newPage->data);
            free(newPage);
            return rc;
        }

        // update buffer pool management data
        mgmtData->readIO++;
        mgmtData->pageFrames[victimFrame] = newPage;
        mgmtData->fixCounts[victimFrame]  = 1;
        mgmtData->dirtyFlags[victimFrame] = false;
        mgmtData->framePageNumbers[victimFrame] = pageNum;
        // LRU update - mark this as most recently used
        mgmtData->lastUsed[victimFrame] = mgmtData->lruClock++;

        // populate the client's page handle
        page->pageNum = pageNum;
        page->data = newPage->data;
        return RC_OK;
    }
}

/**
 * Function: getFrameContents
 * --------------------------
 * Returns an array showing which page is stored in each frame of the buffer pool.
 *
 * This function is primarily used for debugging and testing the buffer manager.
 * It provides visibility into the current state of the buffer pool by showing
 * which page (if any) is loaded in each frame.
 *
 * The returned array has one entry per frame in the buffer pool. Each entry contains:
 * - The page number of the page in that frame
 * - NO_PAGE if the frame is empty
 *
 * Parameters:
 *   bm - The buffer pool.
 *
 * Returns:
 *   PageNumber* array of size numPages, or NULL if parameters are invalid.
 */
PageNumber *getFrameContents(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    return mgmtData->framePageNumbers;
}

/**
 * Function: getDirtyFlags
 * -----------------------
 * Returns an array indicating which frames contain dirty pages.
 *
 * This function is used for debugging and testing the buffer manager.
 * It provides visibility into which pages have been modified and need
 * to be written back to disk eventually.
 *
 * The returned array has one boolean entry per frame in the buffer pool:
 * - TRUE if the page in that frame is dirty (modified)
 * - FALSE if the page is clean or the frame is empty
 *
 * Parameters:
 *   bm - The buffer pool.
 *
 * Returns:
 *   bool* array of size numPages, or NULL if parameters are invalid.
 */
bool *getDirtyFlags(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    return mgmtData->dirtyFlags;
}

/**
 * Function: getFixCounts
 * ----------------------
 * Returns an array showing the fix count for each frame in the buffer pool.
 *
 * This function is used for debugging and testing the buffer manager.
 * It provides visibility into how many clients are currently using each page,
 * which is critical for the page replacement algorithm.
 *
 * The fix count indicates:
 * - How many clients have pinned the page (are using it)
 * - Whether the page can be evicted (only if fix count is 0)
 *
 * Parameters:
 *   bm - The buffer pool.
 *
 * Returns:
 *   int* array of fix counts (size numPages), or NULL if parameters are invalid.
 */
int *getFixCounts(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }

    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    return mgmtData->fixCounts;
}

/**
 * Function: getNumReadIO
 * ----------------------
 * Returns the total number of pages read from disk since buffer pool initialization.
 *
 * This function provides a performance metric for evaluating the efficiency
 * of the buffer manager. A lower number of reads indicates better caching
 * performance, as more requests are being served from memory.
 *
 * The counter is incremented each time a page is read from disk during:
 * - Initial loading of a page that wasn't in memory
 * - Reloading a page that was evicted
 *
 * Parameters:
 *   bm - The buffer pool.
 *
 * Returns:
 *   Integer count of read operations, or -1 if parameters are invalid.
 */
int getNumReadIO(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;
    }
    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    return mgmtData->readIO;
}

/**
 * Function: getNumWriteIO
 * -----------------------
 * Returns the total number of pages written to disk since buffer pool initialization.
 *
 * This function provides another performance metric for the buffer manager.
 * It tracks how many times pages have been written back to disk, which happens:
 * - During page replacement when a dirty page is evicted
 * - When forcePage or forceFlushPool is called
 * - During buffer pool shutdown
 *
 * A higher write count may indicate more data modifications or more aggressive
 * page replacement.
 *
 * Parameters:
 *   bm - The buffer pool.
 *
 * Returns:
 *   Integer count of write operations, or -1 if parameters are invalid.
 */
int getNumWriteIO(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;
    }
    BP_MgmtData *mgmtData = (BP_MgmtData*) bm->mgmtData;
    return mgmtData->writeIO;
}


// TESTING
// TODO remove

#ifdef TEST_BUFFER_MGR
/**
 * Main function for testing the buffer manager implementation.
 *
 * This function demonstrates basic usage of the buffer manager:
 * 1. Initializing a buffer pool
 * 2. Pinning a page
 * 3. Marking a page as dirty
 * 4. Unpinning a page
 * 5. Forcing a page to disk
 * 6. Shutting down the buffer pool
 *
 * This is only compiled when TEST_BUFFER_MGR is defined.
 */
int main() {
    // This main function is for testing purposes
    BM_BufferPool bufferPool;
    RC rc = initBufferPool(&bufferPool, "test_pagefile.bin", 5, RS_LRU, NULL);
    if (rc != RC_OK) {
        printf("initBufferPool failed with error code: %d\n", rc);
        return -1;
    }

    // Additional test code for pinned pages, marking dirty pages, etc.
    BM_PageHandle page;
    rc = pinPage(&bufferPool, &page, 0);
    if (rc == RC_OK) {
        printf("Pinned page 0 successfully.\n");
        markDirty(&bufferPool, &page);
    }

    rc = unpinPage(&bufferPool, &page);
    if (rc == RC_OK) {
        printf("Unpinned page 0 successfully.\n");
    }

    rc = forcePage(&bufferPool, &page);
    if (rc == RC_OK) {
        printf("Forced page 0 to disk successfully.\n");
    }

    rc = shutdownBufferPool(&bufferPool);
    if (rc != RC_OK) {
        printf("shutdownBufferPool failed with error code: %d\n", rc);
        return -1;
    }

    printf("Buffer pool shutdown successfully.\n");
    return 0;
}
#endif