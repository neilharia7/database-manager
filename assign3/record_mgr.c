#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tgmath.h>
#include "record_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"


// Structure to manage table metadata and buffer pool
typedef struct RMTableMgmtData {

	int numTuples;	// Number of tuples (records) in the table
	int firstFreePageNumber;	// First free page number for inserting new records
	int recordSize;	// Size of each record in bytes
	BM_PageHandle pageHandle;	// Buffer manager page handle for metadata operations
	BM_BufferPool bufferPool;	// Buffer pool for managing table pages
} RMTableMgmtData;

/* RMScanMgmtData stores scan details and condition */
typedef struct RMScanMgmtData {

	BM_PageHandle pHandle;
	RID rid; // current row that is being scanned
	int count; // no. of tuples scanned till now
	Expr *condition; // expression to be checked

} RMScanMgmtData;

/**
 * Function: initRecordManager
 * ---------------------------
 * Initializes the record manager. Nothing more.
 * @param mgmtData
 * @return
 *	-	RC_OK if initialization is successful
 */
RC initRecordManager(void * mgmtData) {
	// No initialization required for this implementation
	return RC_OK;
}

/**
 * Function: shutdownRecordManager
 * ------------------------------
 * Shuts down the record manager
 * This function is called when the record manager is no longer needed.
 * @return
 *	-	RC_OK if shutdown is successful
 */

RC shutdownRecordManager() {
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
	SM_FileHandle fHandle;
	RC rc;

	// Create a new page file for the table
	if ((rc = createPageFile(name)) != RC_OK)
		return rc;

	// Open the newly created page file
	if ((rc = openPageFile(name, &fHandle)) != RC_OK)
		return rc;

	// Buffer to hold metadata to be written to the first page
	char data[PAGE_SIZE];
	char *metaData = data;
	memset(metaData, 0, PAGE_SIZE);

	// Write number of tuples (initially 0)
	*(int *) metaData = 0;
	metaData += sizeof(int);

	// Write first free page number (page 2, since page 0 is metadata, page 1 is for schema/metadata)
	*(int *) metaData = 2;
	metaData += sizeof(int);

	// Write record size (computed from schema)
	*(int *) metaData = getRecordSize(schema);
	metaData += sizeof(int);

	// Write number of attributes in the schema
	*(int *) metaData = schema->numAttr;
	metaData += sizeof(int);

	// Write schema attribute information for each attribute
	for (int i = 0; i < schema->numAttr; i++) {
		// Assuming max field name is 20
		strncpy(metaData, schema->attrNames[i], 20);
		metaData += 20;

		// Write attribute data type (as integer)
		*(int *) metaData = (int) schema->dataTypes[i];
		metaData += sizeof(int);

		// Write attribute type length (for strings, etc.)
		*(int *) metaData = (int) schema->typeLength[i];
		metaData += sizeof(int);
	}

	// Write key attribute indices (for primary key)
	for (int i = 0; i < schema->keySize; i++) {
		*(int *) metaData = schema->keyAttrs[i];
		metaData += sizeof(int);
	}

	// Write the metadata buffer to page 1 of the file
	if ((rc = writeBlock(1, &fHandle, data)) != RC_OK)
		return rc;

	if ((rc = closePageFile(&fHandle)) != RC_OK)
		return rc;

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
	RC rc = 0;

	// Allocate memory for schema and table management data
	Schema *schema = (Schema *) malloc(sizeof(Schema));
	RMTableMgmtData *tableMgmtData = (RMTableMgmtData *) malloc(sizeof(RMTableMgmtData));
	rel->mgmtData = tableMgmtData;
	rel->name = name;

	// Initialize buffer pool for the table (using LRU replacement strategy, 10 pages)
	if ((rc = initBufferPool(&tableMgmtData->bufferPool, name, 10, RS_LRU, NULL)) != RC_OK) {
		return rc;
	}
	// Pin the metadata page (page 1) to read table metadata
	if ((rc = pinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle, 1)) != RC_OK) {
		// pinning the 0th page which has metadata
		return rc;
	}
	SM_PageHandle metaData = (char *) tableMgmtData->pageHandle.data;

	/**
	 * Read
	 *	number of tuples
	 *	first free page number
	 *	record size
	 *	number of attributes
	 * from metadata
	 */
	tableMgmtData->numTuples = *(int *) metaData;
	metaData += sizeof(int);
	tableMgmtData->firstFreePageNumber = *(int *) metaData;
	metaData += sizeof(int);
	tableMgmtData->recordSize = *(int *) metaData;
	metaData += sizeof(int);
	schema->numAttr = *(int *) metaData;
	metaData += sizeof(int);

	// Allocate memory for schema attribute arrays
	schema->attrNames = (char **) malloc(sizeof(char *) * schema->numAttr);
	schema->dataTypes = (DataType *) malloc(sizeof(DataType) * schema->numAttr);
	schema->typeLength = (int *) malloc(sizeof(int) * schema->numAttr);

	// Read attribute information for each attribute
	for (int i = 0; i < schema->numAttr; i++) {
		// (max 20 bytes + null terminator)
		schema->attrNames[i] = (char *) malloc(21);
		strncpy(schema->attrNames[i], metaData, 20);
		schema->attrNames[i][20] = '\0';	// -> null termination
		metaData += 20;

		schema->dataTypes[i] = *(int *) metaData;
		metaData += sizeof(int);

		schema->typeLength[i] = *(int *) metaData;
		metaData += sizeof(int);
	}
	schema->keySize = *(int *) metaData;
	metaData += sizeof(int);

	// Allocate memory for key attributes (primary key)
	schema->keyAttrs = (int *) malloc(sizeof(int) * schema->keySize);
	for (int i = 0; i < schema->keySize; i++) {
		schema->keyAttrs[i] = *(int *) metaData;
		metaData += sizeof(int);
	}
	rel->schema = schema;

	// Unpin the metadata page after reading
	if ((rc = unpinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle)) != RC_OK) {
		return rc;
	}
	return RC_OK;
}


/**
 * Function: closeTable
 * ------------------
 * Closes an open table, writing back any updated metadata and shutting down the buffer pool.
 * @param rel	Table data structure to be closed
 * @return
 *	-	RC_OK if table closing is successful
 *	-	Other error codes if buffer pool shutdown fails
 */
RC closeTable(RM_TableData *rel) {
	RC rc;
	SM_FileHandle fHandle;
	RMTableMgmtData *tableMgmtData = rel->mgmtData;

	// Pin the metadata page to update number of tuples
	if ((rc == pinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle, 1)) != RC_OK)
		return rc;

	char *metaData = tableMgmtData->pageHandle.data;
	// Update number of tuples in metadata
	*(int *) metaData = tableMgmtData->numTuples;

	// Mark the metadata page as dirty (modified)
	markDirty(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle);

	// Unpin the metadata page after updating
	if ((rc = unpinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle)) != RC_OK)
		return rc;

	// Shutdown the buffer pool for the table
	if ((rc = shutdownBufferPool(&tableMgmtData->bufferPool)) != RC_OK)
		return rc;

	// Clear management data pointer
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
 *	-	Other error codes if destroyPageFile fails
 */
RC deleteTable(char *name) {
	// Destroy the page file associated with the table
	return destroyPageFile(name);
}

/**
 * Function: getNumTuples
 * ---------------------
 * Returns the total number of records present in the table.
 * This value is fetched from the table management data.
 *
 * @param rel	Table data structure
 * @return
 *	-	The number of tuples (records) in the table
 */
int getNumTuples(RM_TableData *rel) {

	RMTableMgmtData *rmTableMgmtData = rel->mgmtData;
	return rmTableMgmtData->numTuples;
}

/**
 * Function: insertRecord
 * ---------------------
 * Inserts a new record into the table.
 * Pins the first free page where records can be inserted
 * Checks if there is enough space in this page to insert the record
 * If not, it moves to the next page to find a free slot
 * Writes the new record at the free slot and updates RID information
 * Marks the page as dirty
 * Unpins the page
 * Increments the number of tuples
 *
 * @param rel	Table data structure
 * @param record	Record to be inserted
 * @return
 *	-	RC_OK - If record insertion is successful
 */
RC insertRecord(RM_TableData *rel, Record *record) {
    RMTableMgmtData *tableMgmtData = rel->mgmtData;
    int recordSize = tableMgmtData->recordSize + 1;
    RID *rid = &record->id;
    rid->page = tableMgmtData->firstFreePageNumber;
    rid->slot = -1;

    // Pin the first free page
    RC rc = pinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle, rid->page);
    if (rc != RC_OK) {
        return rc;
    }

    char *data = tableMgmtData->pageHandle.data;

    // Calculate total slots per page
    int totalSlots = floor(PAGE_SIZE / recordSize);

    // Find a free slot in the current page
    for (int i = 0; i < totalSlots; i++) {
        if (data[i * recordSize] != '#') {
            rid->slot = i;
            break;
        }
    }

    // If no free slot found in current page, search in subsequent pages
    while (rid->slot == -1) {
        unpinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle);
        rid->page++; // Increment page number

        rc = pinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle, rid->page);
        if (rc != RC_OK) {
            return rc;
        }

        data = tableMgmtData->pageHandle.data;

        for (int i = 0; i < totalSlots; i++) {
            if (data[i * recordSize] != '#') {
                tableMgmtData->firstFreePageNumber = rid->page;
                rid->slot = i;
                break;
            }
        }
    }

    // Mark the page as dirty
    rc = markDirty(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle);
    if (rc != RC_OK) {
        unpinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle);
        return rc;
    }

    // Write record to the slot
    char *slotAddress = data + (rid->slot * recordSize);
    *slotAddress = '#'; // Mark slot as occupied
    slotAddress++;
    memcpy(slotAddress, record->data, recordSize - 1);

    // Unpin the page
    rc = unpinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle);
    if (rc != RC_OK) {
        return rc;
    }

    // Update tuple count and record ID
    tableMgmtData->numTuples++;
    record->id = *rid;

    return RC_OK;
}

/**
 * Function: deleteRecord
 * ---------------------
 * Deletes a record from the table.
 * Pins the page where records is located
 * Marks the record as deleted by replacing "#" with "$" (tombstone)
 * Decrements the number of tuples
 * Marks the page as dirty
 * Unpins the page
 *
 * @param rel	Table data structure
 * @param id	Record to be inserted
 * @return
 *	-	RC_OK - If record deletion is successful
 */
RC deleteRecord(RM_TableData *rel, RID id) {
	RMTableMgmtData *rmTableMgmtData = rel->mgmtData;

	// Pin the page containing the record
	RC rc = pinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle, id.page);
	if (rc != RC_OK) {
		return rc;
	}

	// Update number of tuples
	rmTableMgmtData->numTuples--;

	// Calculate record size and slot address
	int recordSize = rmTableMgmtData->recordSize + 1;
	char *data = rmTableMgmtData->pageHandle.data;
	char *slotAddress = data + (id.slot * recordSize);

	// Set tombstone '$' for deleted record
	*slotAddress = '$';

	// Mark the page as dirty and unpin
	rc = markDirty(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
	if (rc != RC_OK) {
		unpinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
		return rc;
	}

	rc = unpinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
	return rc;
}

/**
 * Function: updateRecord
 * ---------------------
 * Updates an existing record in the table.
 * Pins the page where the record is located
 * Finds the slot in the page where the record is stored
 * Updates the record data at that location
 * Marks the page as dirty
 * Unpins the page
 *
 * @param rel	Table data structure
 * @param record	Record to be inserted
 * @return
 *	-	RC_OK - If record insertion is successful
 */
RC updateRecord(RM_TableData *rel, Record *record) {
	RMTableMgmtData *rmTableMgmtData = rel->mgmtData;

	// Pin the page containing the record
	RC rc = pinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle, record->id.page);
	if (rc != RC_OK) {
		return rc;
	}

	// Calculate record size and data address
	int recordSize = rmTableMgmtData->recordSize + 1;
	char *data = rmTableMgmtData->pageHandle.data;
	char *recordAddress = data + (record->id.slot * recordSize) + 1; // +1 to skip the marker

	// Update record data
	memcpy(recordAddress, record->data, recordSize - 1);

	// Mark the page as dirty
	if ((rc = markDirty(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle)) != RC_OK) {
		unpinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
		return rc;
	}

	// Unpin the page
	return unpinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
}

/**
 * Function: getRecord
 * ------------------
 * Retrieves a record from the table.
 * Validates the input parameters
 * Pins the page containing the record
 * Locates the record using the slot ID
 * Checks if the record exists (marked with "#")
 * Copies the record data to the provided record structure
 * Unpins the page
 *
 * @param rel	Table data structure
 * @param id	Record ID to retrieve
 * @param record	Record to be inserted
 * @return
 *	-	RC_OK - If record insertion is successful
 */
RC getRecord(RM_TableData *rel, RID id, Record *record) {
	RC rc;
	RMTableMgmtData *rmTableMgmtData = rel->mgmtData;

	// Pin the page containing the record
	if ((rc = pinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle, id.page)) != RC_OK) {
		return rc;
	}

	// Calculate record size and slot address
	int recordSize = rmTableMgmtData->recordSize + 1;
	char *recordSlotAddress = rmTableMgmtData->pageHandle.data + (id.slot * recordSize);

	// Check if record exists (marked with "#")
	if (*recordSlotAddress != '#') {
		unpinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
		return RC_TUPLE_WIT_RID_ON_EXISTING;
	}

	// Copy record data to the provided record structure
	memcpy(record->data, recordSlotAddress + 1, recordSize - 1);
	record->id = id;

	// Unpin the page
	return unpinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
}
