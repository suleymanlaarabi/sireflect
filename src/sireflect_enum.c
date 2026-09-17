#include "sireflect.h"
#include "sireflect_error.h"
#include "sireflect_registry.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void skip_space(const char **cursor) {
    while (isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }
}

static bool parse_identifier(const char **cursor, char **out_name) {
    const char *start = *cursor;
    if (!isalpha((unsigned char)*start) && *start != '_') {
        return false;
    }
    (*cursor)++;
    while (isalnum((unsigned char)**cursor) || **cursor == '_') {
        (*cursor)++;
    }

    size_t length = (size_t)(*cursor - start);
    char *name = malloc(length + 1);
    if (name == NULL) {
        sireflect_error_set("failed to allocate enum value name");
        return false;
    }
    memcpy(name, start, length);
    name[length] = '\0';
    *out_name = name;
    return true;
}

static void free_values(sireflect_enum_value_t *values, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free((char *)values[i].name);
    }
    free(values);
}

static bool parse_values(
    const char *source,
    sireflect_enum_value_t **out_values,
    size_t *out_count
) {
    const char *cursor = source;
    sireflect_enum_value_t *values = NULL;
    size_t count = 0;
    int64_t previous = -1;

    skip_space(&cursor);
    if (*cursor++ != '{') {
        sireflect_error_set("enum values must start with '{'");
        return false;
    }

    for (;;) {
        skip_space(&cursor);
        if (*cursor == '}') {
            cursor++;
            break;
        }

        char *name = NULL;
        if (!parse_identifier(&cursor, &name)) {
            sireflect_error_set("expected enum value name");
            free_values(values, count);
            return false;
        }

        skip_space(&cursor);
        int64_t value;
        if (*cursor == '=') {
            cursor++;
            skip_space(&cursor);
            errno = 0;
            char *end = NULL;
            value = strtoll(cursor, &end, 0);
            if (end == cursor || errno == ERANGE) {
                sireflect_error_set("enum value must be an integer literal");
                free(name);
                free_values(values, count);
                return false;
            }
            cursor = end;
        } else {
            if (previous == INT64_MAX) {
                sireflect_error_set("enum value overflows int64_t");
                free(name);
                free_values(values, count);
                return false;
            }
            value = previous + 1;
        }

        sireflect_enum_value_t *next = realloc(values, (count + 1) * sizeof(*values));
        if (next == NULL) {
            sireflect_error_set("failed to allocate enum values");
            free(name);
            free_values(values, count);
            return false;
        }
        values = next;
        values[count++] = (sireflect_enum_value_t){ .name = name, .value = value };
        previous = value;

        skip_space(&cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == '}') {
            cursor++;
            break;
        }
        sireflect_error_set("expected ',' or '}' after enum value");
        free_values(values, count);
        return false;
    }

    skip_space(&cursor);
    if (*cursor != '\0') {
        sireflect_error_set("unexpected text after enum values");
        free_values(values, count);
        return false;
    }
    if (count == 0) {
        sireflect_error_set("enum must contain at least one value");
        free(values);
        return false;
    }

    *out_values = values;
    *out_count = count;
    return true;
}

sireflect_handle_t sireflect_try_register_enum(const sireflect_enum_desc_t *desc) {
    sireflect_error_clear();
    if (!sireflect_registry_is_initialized() || desc == NULL || desc->name == NULL ||
        desc->values == NULL || desc->size == 0 || desc->align == 0) {
        sireflect_error_set(
            sireflect_registry_is_initialized() ? "invalid enum descriptor" : "sireflect is not initialized"
        );
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(desc->name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(existing);
        if (type->kind != sireflect_kind_enum || type->size != desc->size || type->align != desc->align) {
            sireflect_error_set("existing type is incompatible with enum descriptor");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    sireflect_enum_value_t *values = NULL;
    size_t value_count = 0;
    if (!parse_values(desc->values, &values, &value_count)) {
        return SIREFLECT_INVALID_HANDLE;
    }
    return sireflect_registry_add_type(
        desc->name, sireflect_kind_enum, desc->size, desc->align, NULL, 0, values, value_count
    );
}

sireflect_handle_t sireflect_register_enum(const sireflect_enum_desc_t *desc) {
    sireflect_handle_t handle = sireflect_try_register_enum(desc);
    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "failed to register enum");
    return handle;
}
