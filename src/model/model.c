#include "ceasy/model/model.h"

#include "ceasy/context.h"

#include <limits.h>
#include <string.h>

static bool model_field_size(ModelFieldType type, size_t *size) {
    if (size == NULL) {
        return false;
    }
    switch (type) {
    case MODEL_FIELD_STRING:
        *size = sizeof(String);
        return true;
    case MODEL_FIELD_INT64:
        *size = sizeof(int64_t);
        return true;
    case MODEL_FIELD_BOOL:
        *size = sizeof(bool);
        return true;
    default:
        return false;
    }
}

static bool model_field_address(const ModelDefinition *definition,
                                const ModelField *field, void *record,
                                void **address) {
    size_t field_size;

    if (definition == NULL || field == NULL || record == NULL ||
        address == NULL || !model_field_size(field->type, &field_size) ||
        field->offset > definition->size ||
        field_size > definition->size - field->offset) {
        return false;
    }
    *address = (unsigned char *)record + field->offset;
    return true;
}

static const ModelField *model_primary_key(const ModelDefinition *definition) {
    if (definition == NULL || definition->fields == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < definition->field_count; index++) {
        if (definition->fields[index].primary_key) {
            return &definition->fields[index];
        }
    }
    return NULL;
}

static bool model_valid_definition(const ModelDefinition *definition) {
    if (definition == NULL || definition->size == 0 ||
        definition->fields == NULL || definition->field_count == 0 ||
        definition->field_count > (size_t)INT_MAX ||
        definition->find_sql.data == NULL || definition->all_sql.data == NULL ||
        definition->insert_sql.data == NULL ||
        definition->update_sql.data == NULL ||
        definition->delete_sql.data == NULL ||
        model_primary_key(definition) == NULL) {
        return false;
    }
    for (size_t index = 0; index < definition->field_count; index++) {
        size_t field_size;
        if (!model_field_size(definition->fields[index].type, &field_size) ||
            definition->fields[index].offset > definition->size ||
            field_size > definition->size - definition->fields[index].offset) {
            return false;
        }
    }
    return true;
}

static bool model_map_row(Context *context, const ModelDefinition *definition,
                          DatabaseStatement *statement, void *record) {
    if (context == NULL || context->arena == NULL ||
        !model_valid_definition(definition) || statement == NULL ||
        record == NULL ||
        database_column_count(statement) != (int)definition->field_count) {
        return false;
    }
    for (size_t index = 0; index < definition->field_count; index++) {
        const ModelField *field = &definition->fields[index];
        void *address;

        if (!stringv_equal(database_column_name(statement, (int)index),
                           field->name)) {
            return false;
        }
        if (!model_field_address(definition, field, record, &address)) {
            return false;
        }
        switch (field->type) {
        case MODEL_FIELD_STRING:
            *(String *)address = string_from_in(
                context->arena, database_column_text(statement, (int)index));
            break;
        case MODEL_FIELD_INT64:
            *(int64_t *)address = database_column_int64(statement, (int)index);
            break;
        case MODEL_FIELD_BOOL:
            *(bool *)address =
                database_column_int64(statement, (int)index) != 0;
            break;
        default:
            return false;
        }
    }
    return true;
}

static bool model_bind_fields(DatabaseStatement *statement,
                              const ModelDefinition *definition, void *record,
                              bool insert) {
    int parameter = 1;

    for (size_t index = 0; index < definition->field_count; index++) {
        const ModelField *field = &definition->fields[index];
        void *address;
        bool selected = insert ? field->insertable : field->updatable;

        if (!selected) {
            continue;
        }
        if (!model_field_address(definition, field, record, &address)) {
            return false;
        }
        if (field->type == MODEL_FIELD_STRING) {
            if (!database_bind_text(statement, parameter,
                                    string_as_view((String *)address))) {
                return false;
            }
        } else if (field->type == MODEL_FIELD_INT64) {
            if (!database_bind_int64(statement, parameter,
                                     *(int64_t *)address)) {
                return false;
            }
        } else if (field->type == MODEL_FIELD_BOOL) {
            if (!database_bind_int64(statement, parameter,
                                     *(bool *)address ? 1 : 0)) {
                return false;
            }
        } else {
            return false;
        }
        parameter++;
    }
    return true;
}

static bool model_bind_field_count(const ModelDefinition *definition,
                                   bool insert, int *count) {
    size_t selected = 0;

    if (definition == NULL || count == NULL) {
        return false;
    }
    for (size_t index = 0; index < definition->field_count; index++) {
        bool enabled = insert ? definition->fields[index].insertable
                              : definition->fields[index].updatable;
        if (enabled) {
            if (selected == (size_t)INT_MAX) {
                return false;
            }
            selected++;
        }
    }
    *count = (int)selected;
    return true;
}

static bool model_set_primary_key(const ModelDefinition *definition,
                                  void *record, int64_t id) {
    const ModelField *field = model_primary_key(definition);
    void *address;

    if (field == NULL || field->type != MODEL_FIELD_INT64 ||
        !model_field_address(definition, field, record, &address)) {
        return false;
    }
    *(int64_t *)address = id;
    return true;
}

static bool model_get_primary_key(const ModelDefinition *definition,
                                  void *record, int64_t *id) {
    const ModelField *field = model_primary_key(definition);
    void *address;

    if (id == NULL || field == NULL || field->type != MODEL_FIELD_INT64 ||
        !model_field_address(definition, field, record, &address)) {
        return false;
    }
    *id = *(int64_t *)address;
    return true;
}

ModelResult model_find(Context *context, const ModelDefinition *definition,
                       int64_t id, void **record) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;
    void *loaded;

    if (record != NULL) {
        *record = NULL;
    }
    if (record == NULL || context == NULL || context->database == NULL ||
        context->arena == NULL || !model_valid_definition(definition) ||
        !database_prepare(context->database, &statement,
                          definition->find_sql) ||
        !database_bind_int64(&statement, 1, id)) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_ERROR;
    }
    step = database_step(&statement);
    if (step == DATABASE_STEP_DONE) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_NOT_FOUND;
    }
    if (step != DATABASE_STEP_ROW) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_ERROR;
    }
    loaded = arena_alloc_zero(context->arena, definition->size);
    if (loaded == NULL ||
        !model_map_row(context, definition, &statement, loaded)) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_ERROR;
    }
    database_statement_destroy(&statement);
    *record = loaded;
    return MODEL_RESULT_OK;
}

bool model_all(Context *context, const ModelDefinition *definition,
               ModelArray *result) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;
    void *items = NULL;
    size_t length = 0;
    size_t capacity = 0;

    if (result != NULL) {
        result->items = NULL;
        result->length = 0;
    }
    if (result == NULL || context == NULL || context->database == NULL ||
        context->arena == NULL || !model_valid_definition(definition) ||
        !database_prepare(context->database, &statement, definition->all_sql)) {
        database_statement_destroy(&statement);
        return false;
    }
    while ((step = database_step(&statement)) == DATABASE_STEP_ROW) {
        if (length == capacity) {
            size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
            void *new_items;

            if (new_capacity < capacity ||
                new_capacity > SIZE_MAX / definition->size) {
                database_statement_destroy(&statement);
                return false;
            }
            new_items =
                arena_alloc(context->arena, new_capacity * definition->size);
            if (new_items == NULL) {
                database_statement_destroy(&statement);
                return false;
            }
            if (items != NULL && length > 0) {
                memcpy(new_items, items, length * definition->size);
            }
            items = new_items;
            capacity = new_capacity;
        }
        void *record = (unsigned char *)items + length * definition->size;
        memset(record, 0, definition->size);
        if (!model_map_row(context, definition, &statement, record)) {
            database_statement_destroy(&statement);
            return false;
        }
        length++;
    }
    database_statement_destroy(&statement);
    if (step == DATABASE_STEP_ERROR) {
        return false;
    }
    result->items = items;
    result->length = length;
    return true;
}

bool model_insert(Context *context, const ModelDefinition *definition,
                  void *record) {
    DatabaseStatement statement = {0};
    int64_t id;
    void *loaded = NULL;

    if (context == NULL || context->database == NULL ||
        context->arena == NULL || record == NULL ||
        !model_valid_definition(definition) ||
        !database_prepare(context->database, &statement,
                          definition->insert_sql) ||
        !model_bind_fields(&statement, definition, record, true) ||
        !database_execute(&statement)) {
        database_statement_destroy(&statement);
        return false;
    }
    database_statement_destroy(&statement);
    id = database_last_insert_id(context->database);
    if (id <= 0 || !model_set_primary_key(definition, record, id) ||
        model_find(context, definition, id, &loaded) != MODEL_RESULT_OK) {
        return false;
    }
    memcpy(record, loaded, definition->size);
    return true;
}

ModelResult model_update(Context *context, const ModelDefinition *definition,
                         void *record) {
    DatabaseStatement statement = {0};
    int64_t id;
    void *loaded = NULL;
    int field_count;

    if (context == NULL || context->database == NULL ||
        context->arena == NULL || record == NULL ||
        !model_valid_definition(definition) ||
        !model_get_primary_key(definition, record, &id) || id <= 0 ||
        !model_bind_field_count(definition, false, &field_count) ||
        !database_prepare(context->database, &statement,
                          definition->update_sql) ||
        !model_bind_fields(&statement, definition, record, false) ||
        !database_bind_int64(&statement, field_count + 1, id) ||
        !database_execute(&statement)) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_ERROR;
    }
    database_statement_destroy(&statement);
    if (database_changes(context->database) == 0) {
        return MODEL_RESULT_NOT_FOUND;
    }
    if (model_find(context, definition, id, &loaded) != MODEL_RESULT_OK) {
        return MODEL_RESULT_ERROR;
    }
    memcpy(record, loaded, definition->size);
    return MODEL_RESULT_OK;
}

ModelResult model_destroy(Context *context, const ModelDefinition *definition,
                          void *record) {
    DatabaseStatement statement = {0};
    int64_t id;

    if (context == NULL || context->database == NULL || record == NULL ||
        !model_valid_definition(definition) ||
        !model_get_primary_key(definition, record, &id) || id <= 0 ||
        !database_prepare(context->database, &statement,
                          definition->delete_sql) ||
        !database_bind_int64(&statement, 1, id) ||
        !database_execute(&statement)) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_ERROR;
    }
    database_statement_destroy(&statement);
    return database_changes(context->database) == 0 ? MODEL_RESULT_NOT_FOUND
                                                    : MODEL_RESULT_OK;
}
