#include "sireflect.h"
#include "sireflect_parser.h"
#include "sireflect_registry.h"

sireflect_handle_t
sireflect_try_register_struct(sireflect_registry_t *reg, const sireflect_struct_desc_t *desc) {
    if (reg == NULL || desc == NULL || desc->name == NULL || desc->fields == NULL ||
        desc->align == 0) {
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(reg, desc->name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(reg, existing);
        if (type->kind != sireflect_kind_struct || type->size != desc->size ||
            type->align != desc->align) {
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;

    if (!sireflect_parse_struct_fields(
        reg,
        desc->name,
        desc->fields,
        &parsed_fields,
        &field_count,
        desc->size,
        desc->align,
        false
    )) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_registry_add_type(
        reg,
        desc->name,
        sireflect_kind_struct,
        desc->size,
        desc->align,
        parsed_fields,
        field_count
    );
}

sireflect_handle_t
sireflect_register_struct(sireflect_registry_t *reg, const sireflect_struct_desc_t *desc) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(desc != NULL, "struct descriptor must not be NULL");
    sireflect_assert(desc->name != NULL, "struct descriptor name must not be NULL");
    sireflect_assert(desc->fields != NULL, "struct descriptor fields must not be NULL");
    sireflect_assert(desc->align != 0, "struct descriptor alignment must not be zero");

    sireflect_handle_t handle = SIREFLECT_INVALID_HANDLE;

    if (reg != NULL && desc != NULL && desc->name != NULL && desc->fields != NULL &&
        desc->align != 0) {
        sireflect_handle_t existing = sireflect_type_by_name(reg, desc->name);
        if (existing != SIREFLECT_INVALID_HANDLE) {
            const sireflect_type_info_t *type = sireflect_type_info(reg, existing);
            if (type->kind != sireflect_kind_struct || type->size != desc->size ||
                type->align != desc->align) {
                sireflect_assert(type->kind == sireflect_kind_struct, "existing type must be a struct");
                sireflect_assert(
                    type->size == desc->size,
                    "existing struct size must match descriptor"
                );
                sireflect_assert(
                    type->align == desc->align,
                    "existing struct alignment must match descriptor"
                );
                return SIREFLECT_INVALID_HANDLE;
            }
            return existing;
        }

        sireflect_field_info_t *parsed_fields = NULL;
        size_t field_count = 0;

        if (sireflect_parse_struct_fields(
                reg,
                desc->name,
                desc->fields,
                &parsed_fields,
                &field_count,
                desc->size,
                desc->align,
                true
            )) {
            handle = sireflect_registry_add_type(
                reg,
                desc->name,
                sireflect_kind_struct,
                desc->size,
                desc->align,
                parsed_fields,
                field_count
            );
        }
    }

    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "failed to register struct");
    return handle;
}
