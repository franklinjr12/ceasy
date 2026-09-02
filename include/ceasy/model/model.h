#ifndef CEASY_MODEL_MODEL_H
#define CEASY_MODEL_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ceasy/string/string.h>

typedef struct Context Context;

typedef enum {
    MODEL_FIELD_STRING,
    MODEL_FIELD_INT64,
    MODEL_FIELD_BOOL
} ModelFieldType;

typedef struct ModelField {
    StringView name;
    ModelFieldType type;
    size_t offset;
    bool primary_key;
    bool insertable;
    bool updatable;
} ModelField;

typedef struct ModelDefinition {
    StringView name;
    StringView table_name;
    size_t size;
    const ModelField *fields;
    size_t field_count;
    StringView find_sql;
    StringView all_sql;
    StringView insert_sql;
    StringView update_sql;
    StringView delete_sql;
} ModelDefinition;

typedef enum {
    MODEL_RESULT_ERROR = -1,
    MODEL_RESULT_NOT_FOUND = 0,
    MODEL_RESULT_OK = 1
} ModelResult;

typedef struct {
    void *items;
    size_t length;
} ModelArray;

/* Find/all records and their String fields belong to request Arena. */
ModelResult model_find(Context *context, const ModelDefinition *definition,
                       int64_t id, void **record);
bool model_all(Context *context, const ModelDefinition *definition,
               ModelArray *result);
/* Insert/update reload String fields into request Arena; input struct storage
 * remains caller-owned. Destroy never frees the input struct. */
bool model_insert(Context *context, const ModelDefinition *definition,
                  void *record);
ModelResult model_update(Context *context, const ModelDefinition *definition,
                         void *record);
ModelResult model_destroy(Context *context, const ModelDefinition *definition,
                          void *record);

#endif
