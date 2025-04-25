#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "record_mgr.h"
#include "test_helper.h"

// Helper: create a simple schema for testing
Schema *testSchema() {
    char **attrNames = (char **) malloc(3 * sizeof(char *));
    attrNames[0] = strdup("a");
    attrNames[1] = strdup("b");
    attrNames[2] = strdup("c");

    DataType *dataTypes = (DataType *) malloc(3 * sizeof(DataType));
    dataTypes[0] = DT_INT;
    dataTypes[1] = DT_STRING;
    dataTypes[2] = DT_INT;

    int *typeLength = (int *) malloc(3 * sizeof(int));
    typeLength[0] = 0;
    typeLength[1] = 4;
    typeLength[2] = 0;

    int *keys = (int *) malloc(sizeof(int));
    keys[0] = 0;

    return createSchema(3, attrNames, dataTypes, typeLength, 1, keys);
}

void test_initRecordManager() {
    testName = "test initRecordManager";
    RC rc = initRecordManager(NULL);
    ASSERT_TRUE(rc == RC_OK, "initRecordManager should return RC_OK");
    TEST_DONE();
}

void test_shutdownRecordManager() {
    testName = "test shutdownRecordManager";
    RC rc = shutdownRecordManager();
    ASSERT_TRUE(rc == RC_OK, "shutdownRecordManager should return RC_OK");
    TEST_DONE();
}

void test_createTable_and_deleteTable() {
    testName = "test createTable and deleteTable";
    Schema *schema = testSchema();
    char *tableName = "test_table_rm";

    RC rc = createTable(tableName, schema);
    ASSERT_TRUE(rc == RC_OK, "createTable should return RC_OK");

    // Try deleting the table
    rc = deleteTable(tableName);
    ASSERT_TRUE(rc == RC_OK, "deleteTable should return RC_OK");

    // Try deleting again (should fail)
    rc = deleteTable(tableName);
    ASSERT_TRUE(rc == RC_FILE_NOT_FOUND, "deleteTable on non-existent table should return RC_FILE_NOT_FOUND");

    freeSchema(schema);
    TEST_DONE();
}

void test_openTable_and_closeTable() {
    testName = "test openTable and closeTable";
    Schema *schema = testSchema();
    char *tableName = "test_table_rm2";
    RM_TableData table;

    // Create table first
    RC rc = createTable(tableName, schema);
    ASSERT_TRUE(rc == RC_OK, "createTable should return RC_OK");

    // Open table
    rc = openTable(&table, tableName);
    ASSERT_TRUE(rc == RC_OK, "openTable should return RC_OK");
    ASSERT_TRUE(table.name != NULL, "table.name should not be NULL");
    ASSERT_TRUE(table.schema != NULL, "table.schema should not be NULL");
    ASSERT_TRUE(table.mgmtData != NULL, "table.mgmtData should not be NULL");

    // Close table
    rc = closeTable(&table);
    ASSERT_TRUE(rc == RC_OK, "closeTable should return RC_OK");

    // Clean up
    rc = deleteTable(tableName);
    ASSERT_TRUE(rc == RC_OK, "deleteTable should return RC_OK");

    freeSchema(schema);
    TEST_DONE();
}

int main(void) {
    test_initRecordManager();
    test_shutdownRecordManager();
    test_createTable_and_deleteTable();
    test_openTable_and_closeTable();
    return 0;
}
