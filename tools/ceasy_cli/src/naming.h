#ifndef CEASY_CLI_NAMING_H
#define CEASY_CLI_NAMING_H

#include <stdbool.h>
#include <stddef.h>

#include <ceasy/model/model.h>

#define CEASY_NAME_CAPACITY 128
#define CEASY_FIELD_CAPACITY 64

typedef struct {
    char type_name[CEASY_NAME_CAPACITY];
    char singular[CEASY_NAME_CAPACITY];
    char plural[CEASY_NAME_CAPACITY];
    char table_name[CEASY_NAME_CAPACITY];
    char file_stem[CEASY_NAME_CAPACITY];
    char include_guard[CEASY_NAME_CAPACITY];
} Naming;

typedef struct {
    char name[CEASY_NAME_CAPACITY];
    ModelFieldType type;
    char type_name[16];
} FieldSpec;

bool naming_model(const char *model_name, Naming *result, char *error,
                  size_t error_size);
bool naming_parse_fields(int argc, char **argv, FieldSpec *fields,
                         size_t *field_count, char *error, size_t error_size);
const char *naming_c_type(ModelFieldType type);
const char *naming_sql_type(ModelFieldType type);

#endif
