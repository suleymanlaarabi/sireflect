#include "sireflect_registry.h"
#include "sireflect_error.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void sireflect_registry_reserve(sireflect_registry_t *reg, size_t min_cap) {
    sireflect_assert(reg != NULL, "registry must not be NULL");

    if (reg->type_cap >= min_cap) {
        return;
    }

    size_t new_cap = reg->type_cap == 0 ? 16 : reg->type_cap * 2;
    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    sireflect_type_info_t *types = realloc(reg->types, new_cap * sizeof(*types));
    sireflect_assert(types != NULL, "failed to allocate type metadata");

    reg->types = types;
    reg->type_cap = new_cap;
}

sireflect_handle_t sireflect_registry_add_type(
    sireflect_registry_t *reg,
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(name != NULL, "type name must not be NULL");
    sireflect_assert(size != 0 || kind == sireflect_kind_struct, "non-struct type size must not be zero");
    sireflect_assert(align != 0, "type alignment must not be zero");

    sireflect_registry_reserve(reg, reg->type_count + 1);

    const size_t index = reg->type_count++;
    reg->types[index] = (sireflect_type_info_t){
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

    return sireflect_handle_from_index(index);
}

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_registry_t *reg, sireflect_handle_t pointee_type) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(pointee_type != SIREFLECT_INVALID_HANDLE, "pointer pointee type must be valid");

    for (size_t i = 0; i < reg->type_count; i++) {
        const sireflect_type_info_t *type = &reg->types[i];
        if (type->kind == sireflect_kind_pointer && type->element_type == pointee_type) {
            return sireflect_handle_from_index(i);
        }
    }

    const sireflect_type_info_t *pointee = sireflect_registry_const_type_at(reg, pointee_type);
    sireflect_assert(pointee != NULL, "pointer pointee metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s*", pointee->name);
    sireflect_assert(name_len > 0, "failed to format pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s*", pointee->name);

    sireflect_handle_t pointer_type = sireflect_registry_add_type(
        reg,
        name,
        sireflect_kind_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *pointer_info = sireflect_registry_type_at(reg, pointer_type);
    pointer_info->element_type = pointee_type;

    return pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_registry_t *reg,
    sireflect_handle_t element_type,
    size_t element_count
) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(element_type != SIREFLECT_INVALID_HANDLE, "array element type must be valid");
    sireflect_assert(element_count != 0, "array element count must not be zero");

    for (size_t i = 0; i < reg->type_count; i++) {
        const sireflect_type_info_t *type = &reg->types[i];
        if (type->kind == sireflect_kind_array && type->element_type == element_type &&
            type->element_count == element_count) {
            return sireflect_handle_from_index(i);
        }
    }

    const sireflect_type_info_t *element = sireflect_registry_const_type_at(reg, element_type);
    sireflect_assert(element != NULL, "array element metadata must exist");
    sireflect_assert(element->size <= SIZE_MAX / element_count, "array type size overflows size_t");

    char *name = sireflect_format_array_type_name(element, element_count);

    sireflect_handle_t array_type = sireflect_registry_add_type(
        reg,
        name,
        sireflect_kind_array,
        element->size * element_count,
        element->align,
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *array_info = sireflect_registry_type_at(reg, array_type);
    array_info->element_type = element_type;
    array_info->element_count = element_count;

    return array_type;
}

#define add_type(name, kind)                                                                       \
    sireflect_registry_add_type(reg, #name, kind, sizeof(name), _Alignof(name), NULL, 0)

#define add_named_type(c_type, reflected_name, kind)                                               \
    sireflect_registry_add_type(reg, reflected_name, kind, sizeof(c_type), _Alignof(c_type), NULL, 0)

static inline void sireflect_register_builtin_types(sireflect_registry_t *reg) {
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

sireflect_registry_t *sireflect_registry_init(void) {
    sireflect_error_clear();

    sireflect_registry_t *reg = calloc(1, sizeof(*reg));
    sireflect_assert(reg != NULL, "registry must not be NULL");

    sireflect_register_builtin_types(reg);
    return reg;
}

void sireflect_registry_fini(sireflect_registry_t *reg) {
    sireflect_error_clear();

    if (reg == NULL) {
        return;
    }

    for (size_t i = 0; i < reg->type_count; i++) {
        sireflect_type_info_t *type = &reg->types[i];

        free((char *)type->name);

        for (size_t f = 0; f < type->fields.field_count; f++) {
            free((char *)type->fields.fields[f].name);
        }

        free(type->fields.fields);
    }

    free(reg->types);
    free(reg);
}

sireflect_handle_t sireflect_type_by_name(const sireflect_registry_t *reg, const char *name) {
    sireflect_error_clear();

    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(name != NULL, "type name must not be NULL");

    for (size_t i = 0; i < reg->type_count; i++) {
        if (strcmp(reg->types[i].name, name) == 0) {
            return sireflect_handle_from_index(i);
        }
    }

    return SIREFLECT_INVALID_HANDLE;
}

const sireflect_type_info_t *
sireflect_registry_const_type_at(const sireflect_registry_t *reg, sireflect_handle_t handle) {
    sireflect_assert(reg != NULL, "registry must not be NULL");

    const size_t index = sireflect_index_from_handle(handle);
    sireflect_assert(index < reg->type_count, "type handle is out of range");

    return &reg->types[index];
}

sireflect_type_info_t *
sireflect_registry_type_at(sireflect_registry_t *reg, sireflect_handle_t handle) {
    return (sireflect_type_info_t *)sireflect_registry_const_type_at(reg, handle);
}
