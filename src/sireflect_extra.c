#include "sireflect_error.h"
#include "sireflect_registry.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static sireflect_category_t category_of(sireflect_handle_t type) {
    sireflect_type_entry_t *entry = sireflect_registry_entry_at(type);
    if (entry == NULL) return sireflect_category_invalid;
    switch (entry->info.kind) {
    case sireflect_kind_bool: return sireflect_category_boolean;
    case sireflect_kind_f32:
    case sireflect_kind_f64: return sireflect_category_floating;
    case sireflect_kind_enum: return sireflect_category_enum;
    case sireflect_kind_struct: return sireflect_category_struct;
    case sireflect_kind_array: return sireflect_category_array;
    case sireflect_kind_function_pointer: return sireflect_category_function_pointer;
    case sireflect_kind_ptr: return sireflect_category_pointer;
    case sireflect_kind_pointer: {
        sireflect_type_entry_t *pointee = sireflect_registry_entry_at(entry->info.element_type);
        return pointee != NULL && pointee->info.kind == sireflect_kind_char
            ? sireflect_category_cstring : sireflect_category_pointer;
    }
    default: return sireflect_category_integer;
    }
}

sireflect_category_t sireflect_type_category(sireflect_handle_t type) {
    sireflect_error_clear();
    return category_of(type);
}

bool sireflect_type_is_numeric_handle(sireflect_handle_t type) {
    sireflect_error_clear();
    sireflect_category_t category = category_of(type);
    return category == sireflect_category_integer || category == sireflect_category_floating;
}

bool sireflect_type_is_scalar(sireflect_handle_t type) {
    sireflect_error_clear();
    sireflect_category_t category = category_of(type);
    return category == sireflect_category_boolean || category == sireflect_category_integer ||
        category == sireflect_category_floating || category == sireflect_category_enum;
}

bool sireflect_type_is_cstring(sireflect_handle_t type) {
    sireflect_error_clear();
    return category_of(type) == sireflect_category_cstring;
}

bool sireflect_type_is_integral(sireflect_handle_t type) {
    sireflect_error_clear();
    return category_of(type) == sireflect_category_integer;
}

bool sireflect_type_is_floating(sireflect_handle_t type) {
    sireflect_error_clear();
    return category_of(type) == sireflect_category_floating;
}

bool sireflect_type_is_function_pointer(sireflect_handle_t type) {
    sireflect_error_clear();
    return category_of(type) == sireflect_category_function_pointer;
}

static const void *array_element(sireflect_handle_t type, const void *array, size_t index) {
    sireflect_type_entry_t *entry = sireflect_registry_entry_at(type);
    if (entry == NULL || entry->info.kind != sireflect_kind_array || array == NULL ||
        index >= entry->info.element_count) return NULL;
    sireflect_type_entry_t *element = sireflect_registry_entry_at(entry->info.element_type);
    if (element == NULL || element->info.size == 0 ||
        entry->info.element_count > entry->info.size / element->info.size) return NULL;
    return (const unsigned char *)array + index * element->info.size;
}

const void *sireflect_array_element_ptr(sireflect_handle_t array_type,
    const void *array, size_t index) {
    sireflect_error_clear();
    return array_element(array_type, array, index);
}

void *sireflect_array_element_mut_ptr(sireflect_handle_t array_type,
    void *array, size_t index) {
    sireflect_error_clear();
    return (void *)array_element(array_type, array, index);
}

bool sireflect_enum_value_valid(sireflect_handle_t type, int64_t value) {
    sireflect_error_clear();
    sireflect_type_entry_t *entry = sireflect_registry_entry_at(type);
    if (entry == NULL || entry->info.kind != sireflect_kind_enum) return false;
    for (size_t i = 0; i < entry->info.enum_values.value_count; i++) {
        if (entry->info.enum_values.values[i].value == value) return true;
    }
    return false;
}

static char *dup_string(const char *src) {
    size_t len = strlen(src);
    char *copy = malloc(len + 1);
    if (copy != NULL) memcpy(copy, src, len + 1);
    return copy;
}

static const sireflect_meta_t *find_meta(const sireflect_meta_store_t *store, const char *key) {
    for (size_t i = 0; i < store->view.count; i++) {
        if (strcmp(store->view.items[i]->key, key) == 0) return store->view.items[i];
    }
    return NULL;
}

static bool set_meta(sireflect_meta_store_t *store, const sireflect_meta_t *meta) {
    if (meta == NULL || meta->key == NULL || meta->key[0] == '\0' ||
        meta->kind < SIREFLECT_META_STRING || meta->kind > SIREFLECT_META_F64 ||
        (meta->kind == SIREFLECT_META_STRING && meta->value.string == NULL)) {
        sireflect_error_set("invalid metadata value or key");
        return false;
    }
    char *key = dup_string(meta->key);
    char *string = meta->kind == SIREFLECT_META_STRING ? dup_string(meta->value.string) : NULL;
    if (key == NULL || (meta->kind == SIREFLECT_META_STRING && string == NULL)) {
        free(key);
        free(string);
        sireflect_error_set("failed to allocate metadata");
        return false;
    }
    sireflect_meta_t *existing = (sireflect_meta_t *)find_meta(store, meta->key);
    if (existing != NULL) {
        free((char *)existing->key);
        if (existing->kind == SIREFLECT_META_STRING) free((char *)existing->value.string);
        *existing = *meta;
        existing->key = key;
        if (meta->kind == SIREFLECT_META_STRING) existing->value.string = string;
        return true;
    }
    sireflect_meta_t *item = malloc(sizeof(*item));
    if (item == NULL) {
        free(key);
        free(string);
        sireflect_error_set("failed to allocate metadata");
        return false;
    }
    *item = *meta;
    item->key = key;
    if (meta->kind == SIREFLECT_META_STRING) item->value.string = string;
    if (store->view.count == SIZE_MAX / sizeof(*store->view.items)) {
        free(item);
        free(key);
        free(string);
        sireflect_error_set("metadata list is too large");
        return false;
    }
    const sireflect_meta_t **items = realloc((void *)store->view.items,
        (store->view.count + 1) * sizeof(*items));
    if (items == NULL) {
        free(item);
        free(key);
        free(string);
        sireflect_error_set("failed to allocate metadata list");
        return false;
    }
    store->view.items = items;
    items[store->view.count++] = item;
    return true;
}

static sireflect_meta_store_t *type_store(sireflect_handle_t type) {
    sireflect_type_entry_t *entry = sireflect_registry_entry_at(type);
    return entry == NULL ? NULL : &entry->type_meta;
}

static sireflect_meta_store_t *field_store(sireflect_handle_t type, const char *field) {
    sireflect_type_entry_t *entry = sireflect_registry_entry_at(type);
    if (entry == NULL || field == NULL) return NULL;
    for (size_t i = 0; i < entry->info.fields.field_count; i++) {
        if (strcmp(entry->info.fields.fields[i].name, field) == 0) return &entry->field_meta[i];
    }
    return NULL;
}

bool sireflect_type_set_meta(sireflect_handle_t type, const sireflect_meta_t *meta) {
    sireflect_error_clear();
    sireflect_meta_store_t *store = type_store(type);
    if (store == NULL) { sireflect_error_set("invalid metadata type"); return false; }
    return set_meta(store, meta);
}

const sireflect_meta_t *sireflect_type_meta(sireflect_handle_t type, const char *key) {
    sireflect_error_clear();
    sireflect_meta_store_t *store = type_store(type);
    return store == NULL || key == NULL ? NULL : find_meta(store, key);
}

const sireflect_metas_t *sireflect_type_metas(sireflect_handle_t type) {
    sireflect_error_clear();
    sireflect_meta_store_t *store = type_store(type);
    return store == NULL ? NULL : &store->view;
}

bool sireflect_field_set_meta(sireflect_handle_t type, const char *field,
    const sireflect_meta_t *meta) {
    sireflect_error_clear();
    sireflect_meta_store_t *store = field_store(type, field);
    if (store == NULL) { sireflect_error_set("unknown metadata field or type"); return false; }
    return set_meta(store, meta);
}

const sireflect_meta_t *sireflect_field_meta(sireflect_handle_t type,
    const char *field, const char *key) {
    sireflect_error_clear();
    sireflect_meta_store_t *store = field_store(type, field);
    return store == NULL || key == NULL ? NULL : find_meta(store, key);
}

const sireflect_metas_t *sireflect_field_metas(sireflect_handle_t type,
    const char *field) {
    sireflect_error_clear();
    sireflect_meta_store_t *store = field_store(type, field);
    return store == NULL ? NULL : &store->view;
}
