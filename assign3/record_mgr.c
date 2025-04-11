#include <stdlib.h>
#include <string.h>
#include "record_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"


// Metadata of table stored at the beginning of the first page
typedef struct TableMetadata {
	int numTuples;          // Number of records in the table
	int firstFreePage;      // First page with free space
	int numPages;           // Total number of pages in the table
	int recordSize;         // Size of each record in bytes
	Schema schema;          // Schema of the table
} TableMetadata;

typedef struct RecordManager {
	BM_BufferPool *bm;      // Buffer pool for the table
	BM_PageHandle *page;    // Current page being used
	int currentSlot;        // Current slot being used
	bool *slotOccupied;     // Array to track occupied slots
	int scanCount;          // Number of records scanned
	Expr *scanCondition;    // Condition for scan
	int totalSlots;         // Total slots per page
} RecordManager;

/**
 * Function: initRecordManager
 * ---------------------------
 * Initializes the record manager.
 * This function sets up the necessary data structures and initializes the underlying
 * storage manager to prepare the system for record operations.
 *
 * @param mgmtData	TODO add info
 * @return
 *	-	RC_OK if initialization is successful
 */
RC initRecordManager(void *mgmtData) {
	// Initialize storage manager
	initStorageManager();
	return RC_OK;
}

/**
 * Function: shutdownRecordManager
 * ------------------------------
 * Shuts down the record manager and frees all associated resources.
 * This function is called when the record manager is no longer needed.
 * TODO
 * @return
 *	-	RC_OK if shutdown is successful
 */
RC shutdownRecordManager() {
	// TODO
	return RC_OK;
}

/**
 * Function: createTable
 * --------------------
 * Creates a new table with the given name and schema.
 * This function creates a new page file to store the table data and initializes
 * the first page with table metadata including the schema.
 * @param name		Name of the table to create (used as the page file name)
 * @param schema	Schema of the table to create
 * @return
 *	-	RC_OK if table creation is successful
 */
RC createTable(char *name, Schema *schema) {
	SM_FileHandle fileHandle;
	char *page = malloc(PAGE_SIZE);
	TableMetadata metadata;
	int recordSize = getRecordSize(schema);

	// Initialize metadata
	metadata.numTuples = 0;
	metadata.firstFreePage = 1; // First page after metadata
	metadata.numPages = 1;      // Start with one page (metadata page)
	metadata.recordSize = recordSize;

	// Copy schema to metadata
	metadata.schema = *schema;

	// Write metadata to first page
	memcpy(page, &metadata, sizeof(TableMetadata));

	// Create file with given name and write first page
	createPageFile(name);
	openPageFile(name, &fileHandle);
	writeBlock(0, &fileHandle, page);
	closePageFile(&fileHandle);

	free(page);
	return RC_OK;
}