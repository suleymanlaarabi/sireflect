#include "sireflect.h"
#include "sireflect_parser.h"
#include "sireflect_registry.h"

#include <assert.h>

sireflect_handle_t
sireflect_register_struct(sireflect_registry_t *reg, const sireflect_struct_desc_t *desc) {
    assert(reg != NULL);
    assert(desc->name != NULL);
    assert(desc->fields != NULL);
    assert(desc->align != 0);

    sireflect_handle_t existing = sireflect_type_by_name(reg, desc->name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(reg, existing);
        assert(type->kind == sireflect_kind_struct);
        assert(type->size == desc->size);
        assert(type->align == desc->align);
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;

    sireflect_parse_struct_fields(
        reg,
        desc->fields,
        &parsed_fields,
        &field_count,
        desc->size,
        desc->align
    );

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
