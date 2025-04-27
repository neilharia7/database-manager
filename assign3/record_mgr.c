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
	RC rc = 0;

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
	return unpinPage(&tableMgmtData->bufferPool, &tableMgmtData->pageHandle);
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
	RC rc = -1;
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


/**
 * Function: startScan
 * ------------------
 * Initializes a scan operation on the table.
 * This function sets up the scan handle with the necessary data to perform
 * a sequential scan of the table records, optionally filtering by a condition.
 *
 * @param rel       Table data structure to scan
 * @param scan      Scan handle to be initialized
 * @param cond      Expression condition to filter records (can be NULL for all records)
 * @return
 *  -   RC_OK if scan initialization is successful
 */
RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond) {
	// Set the relation for the scan
	scan->rel = rel;

	// Allocate and initialize scan management data
	RMScanMgmtData *rmScanMgmtData = (RMScanMgmtData *) malloc(sizeof(RMScanMgmtData));

	// Initialize scan to start at the first data page (page 2) and first slot
	rmScanMgmtData->rid.page = 2;
	rmScanMgmtData->rid.slot = 0;
	rmScanMgmtData->count = 0;
	rmScanMgmtData->condition = cond;

	// Attach management data to scan handle
	scan->mgmtData = rmScanMgmtData;

	return RC_OK;
}

/**
 * Function: next
 * -------------
 * Retrieves the next record that satisfies the scan condition.
 * This function iterates through table records, evaluating each against
 * the scan condition until a match is found or the end of table is reached.
 *
 * @param scan      Scan handle containing scan state information
 * @param record    Record structure to populate with the next matching record
 * @return
 *  -   RC_OK if a matching record is found
 *  -   RC_RM_NO_MORE_TUPLES if no more matching records exist
 */
RC next(RM_ScanHandle *scan, Record *record) {
	RMScanMgmtData *scanMgmtData = (RMScanMgmtData *) scan->mgmtData;
	RMTableMgmtData *tmt = (RMTableMgmtData *) scan->rel->mgmtData;

	Value *result = (Value *) malloc(sizeof(Value));
	static char *data;

	int recordSize = tmt->recordSize + 1;
	int totalSlots = floor(PAGE_SIZE/recordSize);

	// Return early if table is empty
	if (tmt->numTuples == 0)
		return RC_RM_NO_MORE_TUPLES;

	// Scan through records until we find a match or reach the end
	while (scanMgmtData->count <= tmt->numTuples) {
		// Initialize scan position if this is the first call
		if (scanMgmtData->count <= 0) {
			scanMgmtData->rid.page = 2;
			scanMgmtData->rid.slot = 0;

			pinPage(&tmt->bufferPool, &scanMgmtData->pHandle, scanMgmtData->rid.page);
			data = scanMgmtData->pHandle.data;
		} else {
			// Move to next slot
			scanMgmtData->rid.slot++;

			// If we've reached the end of the page, move to next page
			if (scanMgmtData->rid.slot >= totalSlots) {
				scanMgmtData->rid.slot = 0;
				scanMgmtData->rid.page++;
			}

			pinPage(&tmt->bufferPool, &scanMgmtData->pHandle, scanMgmtData->rid.page);
			data = scanMgmtData->pHandle.data;
		}

		// Calculate address of record data (skip marker byte)
		data = data + (scanMgmtData->rid.slot * recordSize) + 1;

		// Set record ID
		record->id.page = scanMgmtData->rid.page;
		record->id.slot = scanMgmtData->rid.slot;
		scanMgmtData->count++;

		// Copy record data
		memcpy(record->data, data, recordSize-1);

		// Evaluate condition if one exists
		if (scanMgmtData->condition != NULL) {
			evalExpr(record, (scan->rel)->schema, scanMgmtData->condition, &result);
		} else {
			result->v.boolV = TRUE; // When no condition, return all records
		}

		// If condition is satisfied, return the record
		if (result->v.boolV == TRUE) {
			unpinPage(&tmt->bufferPool, &scanMgmtData->pHandle);
			return RC_OK;
		} else {
			unpinPage(&tmt->bufferPool, &scanMgmtData->pHandle);
		}
	}

	// Reset scan position for next scan
	scanMgmtData->rid.page = 2;
	scanMgmtData->rid.slot = 0;
	scanMgmtData->count = 0;
	return RC_RM_NO_MORE_TUPLES;
}

/**
 * Function: closeScan
 * ------------------
 * Closes a scan operation and frees associated resources.
 * This function unpins any pinned pages and frees the scan management data.
 *
 * @param scan      Scan handle to be closed
 * @return
 *  -   RC_OK if scan is successfully closed
 */
RC closeScan(RM_ScanHandle *scan) {
	RMScanMgmtData *rmScanMgmtData = (RMScanMgmtData *) scan->mgmtData;
	RMTableMgmtData *rmTableMgmtData = (RMTableMgmtData *) scan->rel->mgmtData;

	// Unpin page if scan was active
	if (rmScanMgmtData->count > 0) {
		unpinPage(&rmTableMgmtData->bufferPool, &rmTableMgmtData->pageHandle);
	}

	// Free scan management data
	free(scan->mgmtData);
	scan->mgmtData = NULL;
	return RC_OK;
}

/**
 * Function: getRecordSize
 * ----------------------
 * Calculates the size of a record based on the schema.
 * This function sums up the size of each attribute based on its data type.
 *
 * @param schema	Schema defining the record structure
 * @return
 *  -   Total size of the record in bytes
 */
int getRecordSize(Schema *schema) {
	int size = 0;

	// Calculate size based on each attribute's data type
	for (int i = 0; i < schema->numAttr; ++i) {
		switch (schema->dataTypes[i]) {
			case DT_STRING:
				size += schema->typeLength[i];
			break;
			case DT_INT:
				size += sizeof(int);
			break;
			case DT_FLOAT:
				size += sizeof(float);
			break;
			case DT_BOOL:
				size += sizeof(bool);
			break;
			default:
				break;
		}
	}
	return size;
}

/**
 * Function: createSchema
 * ---------------------
 * Creates a new schema with the given attributes.
 * This function allocates memory for the schema and initializes its fields.
 *
 * @param numAttr       Number of attributes in the schema
 * @param attrNames     Array of attribute names
 * @param dataTypes     Array of attribute data types
 * @param typeLength    Array of attribute type lengths (for strings)
 * @param keySize       Number of key attributes
 * @param keys          Array of key attribute indices
 * @return
 *  -   Pointer to the newly created schema
 */
Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys) {
	Schema *schema = malloc(sizeof(Schema));

	// Initialize schema fields
	schema->numAttr = numAttr;
	schema->attrNames = attrNames;
	schema->dataTypes = dataTypes;
	schema->typeLength = typeLength;
	schema->keySize = keySize;
	schema->keyAttrs = keys;

	return schema;
}

/**
 * Function: freeSchema
 * -------------------
 * Frees memory allocated for a schema.
 * This function releases all memory associated with the schema,
 * including attribute names, data types, type lengths, and key attributes.
 *
 * @param schema    Schema to be freed
 * @return
 *  -   RC_OK if schema is successfully freed
 */
RC freeSchema(Schema *schema) {
	if (schema) {
		// Free attribute names
		if (schema->attrNames) {
			for (int i = 0; i < schema->numAttr; i++) {
				if (schema->attrNames[i]) free(schema->attrNames[i]);
			}
			free(schema->attrNames);
		}

		// Free other schema components
		if (schema->dataTypes) free(schema->dataTypes);
		if (schema->typeLength) free(schema->typeLength);
		if (schema->keyAttrs) free(schema->keyAttrs);

		// Free schema structure itself
		free(schema);
	}
	return RC_OK;
}

/**
 * Function: createRecord
 * ---------------------
 * Creates a new record based on the given schema.
 * This function allocates memory for the record and initializes its fields.
 *
 * @param record    Pointer to store the newly created record
 * @param schema    Schema defining the record structure
 * @return
 *  -   RC_OK if record is successfully created
 */
RC createRecord(Record **record, Schema *schema) {
	int recordSize = getRecordSize(schema);

	// Allocate memory for record
	Record *newRecord = (Record *) malloc(sizeof(Record));
	newRecord->data = (char *) malloc(recordSize);

	// Initialize record ID to invalid values
	newRecord->id.page = -1;
	newRecord->id.slot = -1;

	// Set output parameter
	*record = newRecord;

	return RC_OK;
}

/**
 * Function: freeRecord
 * -------------------
 * Frees memory allocated for a record.
 * This function releases all memory associated with the record.
 *
 * @param record    Record to be freed
 * @return
 *  -   RC_OK if record is successfully freed
 */
RC freeRecord(Record *record) {
	if (record) {
		// Free record data
		free(record->data);

		// Free record structure itself
		free(record);
	}
	return RC_OK;
}

/**
 * Function: determineAttributeOffsetInRecord
 * ----------------------------------------
 * Calculates the byte offset of an attribute within a record.
 * This function computes the offset by summing the sizes of all
 * preceding attributes in the schema.
 *
 * @param schema    Schema defining the record structure
 * @param attrNum   Index of the attribute to find
 * @param result    Pointer to store the calculated offset
 * @return
 *  -   RC_OK if offset is successfully calculated
 */
RC determineAttributeOffsetInRecord(Schema *schema, int attrNum, int *result) {
	int offset = 0;
	int attrPos = 0;

	// Sum sizes of all attributes before the target attribute
	for (attrPos = 0; attrPos < attrNum; attrPos++) {
		switch (schema->dataTypes[attrPos]) {
			case DT_STRING:
				offset += schema->typeLength[attrPos];
			break;
			case DT_INT:
				offset += sizeof(int);
			break;
			case DT_FLOAT:
				offset += sizeof(float);
			break;
			case DT_BOOL:
				offset += sizeof(bool);
			break;
		}
	}

	// Store result
	*result = offset;
	return RC_OK;
}

/**
 * Function: getAttr
 * ----------------
 * Retrieves an attribute value from a record.
 * This function locates the attribute within the record using its offset
 * and copies its value into a newly allocated Value structure.
 *
 * @param record    Record to retrieve attribute from
 * @param schema    Schema defining the record structure
 * @param attrNum   Index of the attribute to retrieve
 * @param value     Pointer to store the retrieved value
 * @return
 *  -   RC_OK if attribute is successfully retrieved
 */
RC getAttr(Record *record, Schema *schema, int attrNum, Value **value) {
	int offset = 0;

	// Calculate attribute offset
	determineAttributeOffsetInRecord(schema, attrNum, &offset);

	// Allocate memory for value
	Value *tempValue = (Value *) malloc(sizeof(Value));

	// Get pointer to attribute data
	char *string = record->data;
	string += offset;

	// Extract value based on data type
	switch (schema->dataTypes[attrNum]) {
		case DT_INT:
			memcpy(&(tempValue->v.intV), string, sizeof(int));
		tempValue->dt = DT_INT;
		break;
		case DT_STRING:
			tempValue->dt = DT_STRING;
		int len = schema->typeLength[attrNum];
		tempValue->v.stringV = (char *) malloc(len + 1);
		strncpy(tempValue->v.stringV, string, len);
		tempValue->v.stringV[len] = '\0';
		break;
		case DT_FLOAT:
			tempValue->dt = DT_FLOAT;
		memcpy(&(tempValue->v.floatV), string, sizeof(float));
		break;
		case DT_BOOL:
			tempValue->dt = DT_BOOL;
		memcpy(&(tempValue->v.boolV), string, sizeof(bool));
		break;
	}

	// Set output parameter
	*value = tempValue;
	return RC_OK;
}

/**
 * It sets the attributes of the record to the value passed for the attribute determined by "attrNum"
                1) It uses "determineAttributOffsetInRecord" for determining the offset at which the attibute is stored
                2) It uses the attribute type to write the new value at the offset determined above
                3) Once the attribute is set, RC_OK is returned
 * @param record
 * @param schema
 * @param attrNum
 * @param value
 * @return
 */
RC setAttr(Record *record, Schema *schema, int attrNum, Value *value) {
	int offset = 0;

	// Calculate attribute offset
	determineAttributeOffsetInRecord(schema, attrNum, &offset);

	// Get pointer to attribute data
	char *data = record->data;
	data = data + offset;

	// Set value based on data type
	switch (schema->dataTypes[attrNum]) {
		case DT_INT:
			*(int *) data = value->v.intV;
		break;
		case DT_STRING: {
			char *buf;
			int len = schema->typeLength[attrNum];
			buf = (char *) malloc(len + 1);
			strncpy(buf, value->v.stringV, len);
			buf[len] = '\0';
			strncpy(data, buf, len);
			free(buf);
		}
		break;
		case DT_FLOAT:
			*(float *) data = value->v.floatV;
		break;
		case DT_BOOL:
			*(bool *) data = value->v.boolV;
		break;
	}

	return RC_OK;
}