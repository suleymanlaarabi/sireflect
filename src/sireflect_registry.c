#include "sireflect_registry.h"
#include "sireflect_error.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sireflect_registry_t sireflect_global_registry;
static size_t sireflect_global_references;

bool sireflect_registry_is_initialized(void) {
    return sireflect_global_references != 0;
}

sireflect_registry_t *sireflect_registry_current(void) {
    sireflect_assert(sireflect_registry_is_initialized(), "sireflect must be initialized");
    return &sireflect_global_registry;
}

static char *sireflect_dup_cstr(const char *str) {
    sireflect_assert(str != NULL, "string must not be NULL");

    const size_t len = strlen(str);
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate string");

    memcpy(result, str, len + 1);
    return result;
}

static char *
sireflect_format_array_type_name(const sireflect_type_info_t *element, size_t element_count) {
    sireflect_assert(element != NULL, "array element metadata must exist");

    const char *suffix = strchr(element->name, '[');
    if (element->kind != sireflect_kind_array || suffix == NULL) {
        const int name_len = snprintf(NULL, 0, "%s[%zu]", element->name, element_count);
        sireflect_assert(name_len > 0, "failed to format array type name");

        char *name = malloc((size_t)name_len + 1);
        sireflect_assert(name != NULL, "failed to allocate array type name");
        snprintf(name, (size_t)name_len + 1, "%s[%zu]", element->name, element_count);
        return name;
    }

    const size_t prefix_len = (size_t)(suffix - element->name);
    const int count_len = snprintf(NULL, 0, "[%zu]", element_count);
    sireflect_assert(count_len > 0, "failed to format array dimension");

    const size_t suffix_len = strlen(suffix);
    char *name = malloc(prefix_len + (size_t)count_len + suffix_len + 1);
    sireflect_assert(name != NULL, "failed to allocate array type name");

    memcpy(name, element->name, prefix_len);
    snprintf(name + prefix_len, (size_t)count_len + 1, "[%zu]", element_count);
    memcpy(name + prefix_len + (size_t)count_len, suffix, suffix_len + 1);

    return name;
}

static sireflect_handle_t sireflect_handle_from_index(size_t index) {
    return (sireflect_handle_t)(index + 1);
}

static size_t sireflect_index_from_handle(sireflect_handle_t handle) {
    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "type handle must be valid");
    return (size_t)(handle - 1);
}

sireflect_handle_t sireflect_registry_add_type(
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
) {
    sireflect_registry_t *reg = sireflect_registry_current();

    sireflect_assert(name != NULL, "type name must not be NULL");
    sireflect_assert(size != 0 || kind == sireflect_kind_struct, "non-struct type size must not be zero");
    sireflect_assert(align != 0, "type alignment must not be zero");

    const sireflect_type_info_t type = {
        .name = sireflect_dup_cstr(name),
        .kind = kind,
        .size = size,
        .align = align,
        .fields =
            {
                .fields = fields,
                .field_count = field_count,
            },
        .element_type = SIREFLECT_INVALID_HANDLE,
        .element_count = 0,
    };
    sicore_vec_push(&reg->types, &type, sizeof(type));

    const uint32_t index = reg->types.size - 1;
    sicore_map_set(&reg->types_by_name, type.name, index);

    return sireflect_handle_from_index((size_t)index);
}

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_handle_t pointee_type) {
    sireflect_assert(pointee_type != SIREFLECT_INVALID_HANDLE, "pointer pointee type must be valid");

    const sireflect_type_info_t *pointee = sireflect_registry_const_type_at(pointee_type);
    sireflect_assert(pointee != NULL, "pointer pointee metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s*", pointee->name);
    sireflect_assert(name_len > 0, "failed to format pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s*", pointee->name);

    sireflect_handle_t existing = sireflect_registry_handle_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        free(name);
        return existing;
    }

    sireflect_handle_t pointer_type = sireflect_registry_add_type(
        name,
        sireflect_kind_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *pointer_info = sireflect_registry_type_at(pointer_type);
    pointer_info->element_type = pointee_type;

    return pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_function_pointer_type(
    sireflect_handle_t return_type
) {
    sireflect_assert(return_type != SIREFLECT_INVALID_HANDLE, "function return type must be valid");

    const sireflect_type_info_t *return_info = sireflect_registry_const_type_at(return_type);
    sireflect_assert(return_info != NULL, "function return type metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s(*)()", return_info->name);
    sireflect_assert(name_len > 0, "failed to format function pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate function pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s(*)()", return_info->name);

    sireflect_handle_t existing = sireflect_registry_handle_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        free(name);
        return existing;
    }

    sireflect_handle_t function_pointer_type = sireflect_registry_add_type(
        name,
        sireflect_kind_function_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *function_pointer_info =
        sireflect_registry_type_at(function_pointer_type);
    function_pointer_info->element_type = return_type;

    return function_pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_handle_t element_type,
    size_t element_count
) {
    sireflect_assert(element_type != SIREFLECT_INVALID_HANDLE, "array element type must be valid");
    sireflect_assert(element_count != 0, "array element count must not be zero");

    const sireflect_type_info_t *element = sireflect_registry_const_type_at(element_type);
    sireflect_assert(element != NULL, "array element metadata must exist");
    sireflect_assert(element->size <= SIZE_MAX / element_count, "array type size overflows size_t");

    char *name = sireflect_format_array_type_name(element, element_count);

    sireflect_handle_t existing = sireflect_registry_handle_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        free(name);
        return existing;
    }

    sireflect_handle_t array_type = sireflect_registry_add_type(
        name,
        sireflect_kind_array,
        element->size * element_count,
        element->align,
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *array_info = sireflect_registry_type_at(array_type);
    array_info->element_type = element_type;
    array_info->element_count = element_count;

    return array_type;
}

#define add_type(name, kind) \
    sireflect_registry_add_type(#name, kind, sizeof(name), _Alignof(name), NULL, 0)

#define add_named_type(c_type, reflected_name, kind) \
    sireflect_registry_add_type(reflected_name, kind, sizeof(c_type), _Alignof(c_type), NULL, 0)

static inline void sireflect_register_builtin_types(void) {
    add_type(u8, sireflect_kind_u8);
    add_type(u16, sireflect_kind_u16);
    add_type(u32, sireflect_kind_u32);
    add_type(u64, sireflect_kind_u64);
    add_type(i8, sireflect_kind_i8);
    add_type(i16, sireflect_kind_i16);
    add_type(i32, sireflect_kind_i32);
    add_type(i64, sireflect_kind_i64);
    add_type(f32, sireflect_kind_f32);
    add_type(f64, sireflect_kind_f64);
    add_type(bool, sireflect_kind_bool);
    add_type(char, sireflect_kind_char);
    add_type(ptr, sireflect_kind_ptr);

    add_type(uint8_t, sireflect_kind_u8);
    add_type(uint16_t, sireflect_kind_u16);
    add_type(uint32_t, sireflect_kind_u32);
    add_type(uint64_t, sireflect_kind_u64);
    add_type(int8_t, sireflect_kind_i8);
    add_type(int16_t, sireflect_kind_i16);
    add_type(int32_t, sireflect_kind_i32);
    add_type(int64_t, sireflect_kind_i64);

    add_type(float, sireflect_kind_f32);
    add_type(double, sireflect_kind_f64);
    add_type(short, sireflect_kind_short);
    add_type(int, sireflect_kind_int);
    add_type(long, sireflect_kind_long);

    add_named_type(signed char, "signed char", sireflect_kind_signed_char);
    add_named_type(unsigned char, "unsigned char", sireflect_kind_unsigned_char);
    add_named_type(unsigned short, "unsigned short", sireflect_kind_unsigned_short);
    add_named_type(unsigned int, "unsigned int", sireflect_kind_unsigned_int);
    add_named_type(unsigned long, "unsigned long", sireflect_kind_unsigned_long);
    add_named_type(long long, "long long", sireflect_kind_long_long);
    add_named_type(unsigned long long, "unsigned long long", sireflect_kind_unsigned_long_long);
}

void sireflect_init(void) {
    sireflect_error_clear();

    if (sireflect_global_references == 0) {
        sireflect_global_references = 1;
        sicore_vec_init(&sireflect_global_registry.types, sizeof(sireflect_type_info_t));
        sicore_map_init(&sireflect_global_registry.types_by_name);
        sireflect_register_builtin_types();
        return;
    }

    sireflect_assert(sireflect_global_references != SIZE_MAX, "sireflect reference count overflow");
    sireflect_global_references++;
}

static void sireflect_registry_clear(void) {
    sireflect_registry_t *reg = &sireflect_global_registry;

    for (uint32_t i = 0; i < reg->types.size; i++) {
        sireflect_type_info_t *type = sicore_vec_get_mut(&reg->types, i, sireflect_type_info_t);

        free((char *)type->name);

        for (size_t f = 0; f < type->fields.field_count; f++) {
            free((char *)type->fields.fields[f].name);
        }

        free(type->fields.fields);
    }

    sicore_map_fini(&reg->types_by_name);
    sicore_vec_fini(&reg->types);
    memset(reg, 0, sizeof(*reg));
}

void sireflect_fini(void) {
    sireflect_error_clear();

    sireflect_assert(sireflect_global_references != 0, "sireflect is not initialized");
    if (sireflect_global_references == 0) {
        return;
    }

    sireflect_global_references--;
    if (sireflect_global_references == 0) {
        sireflect_registry_clear();
    }
}

sireflect_handle_t sireflect_type_by_name(const char *name) {
    sireflect_error_clear();

    sireflect_registry_current();
    sireflect_assert(name != NULL, "type name must not be NULL");

    return sireflect_registry_handle_by_name(name);
}

sireflect_handle_t sireflect_registry_handle_by_name(const char *name) {
    const sireflect_registry_t *reg = sireflect_registry_current();
    const uint32_t index = sicore_map_get(&reg->types_by_name, name);

    if (index == UINT32_MAX) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_handle_from_index((size_t)index);
}

const sireflect_type_info_t *sireflect_registry_const_type_at(sireflect_handle_t handle) {
    const sireflect_registry_t *reg = sireflect_registry_current();

    const size_t index = sireflect_index_from_handle(handle);
    sireflect_assert(index < reg->types.size, "type handle is out of range");

    return sicore_vec_get(&reg->types, index, sireflect_type_info_t);
}

sireflect_type_info_t *sireflect_registry_type_at(sireflect_handle_t handle) {
    return (sireflect_type_info_t *)sireflect_registry_const_type_at(handle);
}
