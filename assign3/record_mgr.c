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

/**
 * Function: openTable
 * ------------------
 * Opens an existing table for operations.
 * This function initializes the buffer pool for the table and loads the table
 * metadata into memory.
 * @param rel	Table data structure to be initialized
 * @param name	Name of the table to open
 * @return
 *	-	RC_OK if table opening is successful
 *	-	Other error codes if buffer pool initialization fails
 */
RC openTable(RM_TableData *rel, char *name) {
	BM_BufferPool *bm = (BM_BufferPool *)malloc(sizeof(BM_BufferPool));
	BM_PageHandle *page = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
	RecordManager *mgmt = (RecordManager *)malloc(sizeof(RecordManager));
	TableMetadata *metadata;

	// Initialize buffer pool with pool size 10 (assumption)
	initBufferPool(bm, name, BUFFER_POOL_SIZE, RS_FIFO, NULL);

	// Pin first page (metadata page)
	pinPage(bm, page, 0);

	// Read metadata
	metadata = (TableMetadata *)page->data;

	// Set up record manager
	mgmt->bm = bm;
	mgmt->page = page;
	mgmt->currentSlot = 0;
	// Calculates total number of records can fit in the usable space of the page
	// + 1 -> slot occupancy flag, indicating whether a slot contains a valid record or is available (empty)
	mgmt->totalSlots = (PAGE_SIZE - sizeof(int)) / (metadata->recordSize + 1);
	mgmt->slotOccupied = (bool *)malloc(mgmt->totalSlots * sizeof(bool));

	// Set up table data
	rel->name = strdup(name);
	rel->schema = createSchema(
		metadata->schema.numAttr,
		metadata->schema.attrNames,
		metadata->schema.dataTypes,
		metadata->schema.typeLength,
		metadata->schema.keySize,
		metadata->schema.keyAttrs
	);
	rel->mgmtData = mgmt;

	// Unpin metadata page
	unpinPage(bm, page);

	return RC_OK;
}

/**
 * Function: closeTable
 * -------------------
 * Closes a previously opened table.
 * This function shuts down the buffer pool associated with the table and
 * nukes all memory allocated for table management.
 *
 * @param rel	Table data structure to close
 * @return
 *	-	RC_OK if table closing is successful
 */
RC closeTable(RM_TableData *rel) {
	RecordManager *mgmt = (RecordManager *)rel->mgmtData;

	// Force write any dirty pages
	forceFlushPool(mgmt->bm);

	// Shutdown buffer pool
	shutdownBufferPool(mgmt->bm);

	// Free allocated memory
	free(mgmt->slotOccupied);
	free(mgmt->page);
	free(mgmt->bm);
	free(mgmt);
	freeSchema(rel->schema);
	free(rel->name);

	rel->mgmtData = NULL;

	return RC_OK;
}

/**
 * Function: deleteTable
 * --------------------
 * Deletes a table and its associated page file.
 * This function removes the page file that stores the table data.
 *
 * @param name	Name of the table to delete
 * @return
 *	-	RC_OK if table (page) deletion is successful
 *	-	RC_FILE_NOT_FOUND if the table does not exist
 */
RC deleteTable(char *name) {
	if (name == NULL) {
		return RC_INVALID_PARAM;
	}
	// Simply delete the page file
	return destroyPageFile(name);
}