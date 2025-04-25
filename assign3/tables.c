#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
#include "record_mgr.h"


static char **copyAttrNames(char **attrNames, int numAttr) {
    if (numAttr <= 0 || attrNames == NULL) {
        printf("Invalid arguments for copyAttrNames\n");
        return NULL;
    }
    char **copy = (char **)malloc(numAttr * sizeof(char *));
    if (!copy) {
        printf("Memory allocation failed for attribute names\n");
        return NULL;
    }
    for (int i = 0; i < numAttr; i++) {
        if (attrNames[i] == NULL) {
            printf("Attribute name is NULL at index %d\n", i);
            copy[i] = NULL;
        } else {
            copy[i] = strdup(attrNames[i]);
            if (!copy[i]) {
                printf("Memory allocation failed for attribute name at index %d\n", i);
                // Optionally, free allocated memory before return
                for (int j = 0; j < i; j++) free(copy[j]);
                free(copy);
                return NULL;
            }
        }
    }
    return copy;
}


Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys) {
    Schema *schema = (Schema *)malloc(sizeof(Schema));
    schema->numAttr = numAttr;
    schema->attrNames = copyAttrNames(attrNames, numAttr);

    schema->dataTypes = (DataType *)malloc(numAttr * sizeof(DataType));
    memcpy(schema->dataTypes, dataTypes, numAttr * sizeof(DataType));

    schema->typeLength = (int *)malloc(numAttr * sizeof(int));
    memcpy(schema->typeLength, typeLength, numAttr * sizeof(int));

    schema->keyAttrs = (int *)malloc(keySize * sizeof(int));
    memcpy(schema->keyAttrs, keys, keySize * sizeof(int));
    schema->keySize = keySize;

    return schema;
}

RC freeSchema(Schema *schema) {
    if (!schema) return RC_OK;
    for (int i = 0; i < schema->numAttr; i++) {
        free(schema->attrNames[i]);
    }
    free(schema->attrNames);
    free(schema->dataTypes);
    free(schema->typeLength);
    free(schema->keyAttrs);
    free(schema);
    return RC_OK;
}

// Calculate the size of a record for a given schema
int getRecordSize(Schema *schema) {
    int size = 0;
    for (int i = 0; i < schema->numAttr; i++) {
        switch (schema->dataTypes[i]) {
            case DT_INT:
                size += sizeof(int);
                break;
            case DT_FLOAT:
                size += sizeof(float);
                break;
            case DT_BOOL:
                size += sizeof(bool);
                break;
            case DT_STRING:
                size += schema->typeLength[i];
                break;
        }
    }
    return size;
}

// Stub implementations for debug/read methods
Value *stringToValue(char *value) { return NULL; }
char *serializeTableInfo(RM_TableData *rel) { return NULL; }
char *serializeTableContent(RM_TableData *rel) { return NULL; }
char *serializeSchema(Schema *schema) { return NULL; }
char *serializeRecord(Record *record, Schema *schema) { return NULL; }
char *serializeAttr(Record *record, Schema *schema, int attrNum) { return NULL; }
char *serializeValue(Value *val) { return NULL; }
