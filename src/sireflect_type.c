#include "sireflect_registry.h"

const char *sireflect_kind_name(sireflect_kind_t kind) {
    switch (kind) {
    case sireflect_kind_u8:
        return "u8";
    case sireflect_kind_u16:
        return "u16";
    case sireflect_kind_u32:
        return "u32";
    case sireflect_kind_u64:
        return "u64";
    case sireflect_kind_i8:
        return "i8";
    case sireflect_kind_i16:
        return "i16";
    case sireflect_kind_i32:
        return "i32";
    case sireflect_kind_i64:
        return "i64";
    case sireflect_kind_f32:
        return "f32";
    case sireflect_kind_f64:
        return "f64";
    case sireflect_kind_bool:
        return "bool";
    case sireflect_kind_char:
        return "char";
    case sireflect_kind_short:
        return "short";
    case sireflect_kind_int:
        return "int";
    case sireflect_kind_long:
        return "long";
    case sireflect_kind_ptr:
        return "ptr";
    case sireflect_kind_struct:
        return "struct";
    case sireflect_kind_array:
        return "array";
    }

    return "unknown";
}

bool sireflect_is_numeric(sireflect_kind_t kind) {
    switch (kind) {
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
    case sireflect_kind_char:
    case sireflect_kind_short:
    case sireflect_kind_int:
    case sireflect_kind_long:
        return true;
    case sireflect_kind_bool:
    case sireflect_kind_ptr:
    case sireflect_kind_struct:
    case sireflect_kind_array:
        return false;
    }

    return false;
}

const sireflect_type_info_t *
sireflect_type_info(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    return sireflect_registry_const_type_at(reg, ref);
}

const sireflect_fields_t *
sireflect_type_fields(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    const sireflect_type_info_t *type = sireflect_type_info(reg, ref);
    return &type->fields;
}

size_t sireflect_type_size(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    return sireflect_type_info(reg, ref)->size;
}

const char *sireflect_type_name(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    return sireflect_type_info(reg, ref)->name;
}

bool sireflect_type_is_struct(const sireflect_type_info_t *info) {
    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_struct;
}

bool sireflect_type_is_array(const sireflect_type_info_t *info) {
    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_array;
}

sireflect_handle_t
sireflect_type_element(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    const sireflect_type_info_t *type = sireflect_type_info(reg, ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_type;
}

size_t
sireflect_type_element_count(const sireflect_registry_t *reg, sireflect_handle_t ref) {
    const sireflect_type_info_t *type = sireflect_type_info(reg, ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_count;
}
