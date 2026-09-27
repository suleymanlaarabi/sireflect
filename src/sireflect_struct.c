#include "sireflect.h"
#include "sireflect_error.h"
#include "sireflect_parser.h"
#include "sireflect_registry.h"

#include <stdlib.h>

static sireflect_handle_t sireflect_register_new_struct(
    const char *name, const char *source, size_t size, size_t align,
    bool validate_layout, bool fail_fast
) {
    sireflect_registry_t *reg = sireflect_registry_current();
    const size_t checkpoint = reg->types.size;
    sireflect_handle_t handle = sireflect_registry_add_type(
        name, sireflect_kind_struct, size, align, NULL, 0, NULL, 0
    );
    sireflect_field_info_t *fields = NULL;
    size_t field_count = 0;
    size_t parsed_size = 0;
    size_t parsed_align = 0;
    if (!sireflect_parse_struct_fields(name, source, &fields, &field_count,
        size, align, &parsed_size, &parsed_align, validate_layout, fail_fast)) {
        sireflect_registry_rollback(checkpoint);
        return SIREFLECT_INVALID_HANDLE;
    }
    if (!sireflect_registry_finish_struct(handle, fields, field_count,
        validate_layout ? size : parsed_size, validate_layout ? align : parsed_align)) {
        for (size_t i = 0; i < field_count; i++) free((char *)fields[i].name);
        free(fields);
        sireflect_registry_rollback(checkpoint);
        sireflect_error_set("failed to allocate field metadata stores");
        return SIREFLECT_INVALID_HANDLE;
    }
    return handle;
}

sireflect_handle_t
sireflect_try_register_struct(const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    if (!sireflect_registry_is_initialized() || desc == NULL || desc->name == NULL || desc->fields == NULL ||
        desc->align == 0) {
        sireflect_error_set(
            sireflect_registry_is_initialized() ? "invalid struct descriptor"
                                                 : "sireflect is not initialized"
        );
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(desc->name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(existing);
        if (type->kind != sireflect_kind_struct || type->size != desc->size ||
            type->align != desc->align) {
            sireflect_error_set("existing type is incompatible with struct descriptor");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    return sireflect_register_new_struct(desc->name, desc->fields,
        desc->size, desc->align, true, false);
}

sireflect_handle_t
sireflect_register_struct(const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    sireflect_assert(desc != NULL, "struct descriptor must not be NULL");
    sireflect_assert(desc->name != NULL, "struct descriptor name must not be NULL");
    sireflect_assert(desc->fields != NULL, "struct descriptor fields must not be NULL");
    sireflect_assert(desc->align != 0, "struct descriptor alignment must not be zero");

    sireflect_handle_t handle = SIREFLECT_INVALID_HANDLE;

    if (sireflect_registry_is_initialized() && desc != NULL && desc->name != NULL && desc->fields != NULL &&
        desc->align != 0) {
        sireflect_handle_t existing = sireflect_type_by_name(desc->name);
        if (existing != SIREFLECT_INVALID_HANDLE) {
            const sireflect_type_info_t *type = sireflect_type_info(existing);
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

        handle = sireflect_register_new_struct(desc->name, desc->fields,
            desc->size, desc->align, true, true);
    }

    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "failed to register struct");
    return handle;
}

sireflect_handle_t sireflect_try_register_dynamic_struct(
    const char *name,
    const char *fields
) {
    sireflect_error_clear();

    if (!sireflect_registry_is_initialized() || name == NULL || fields == NULL) {
        sireflect_error_set(
            sireflect_registry_is_initialized() ? "invalid dynamic struct descriptor"
                                                 : "sireflect is not initialized"
        );
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        if (!sireflect_type_is_struct(sireflect_type_info(existing))) {
            sireflect_error_set("existing type is not a struct");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    return sireflect_register_new_struct(name, fields, 0, 1, false, false);
}
