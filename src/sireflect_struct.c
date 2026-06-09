#include "sireflect_parser.h"
#include "sireflect_registry.h"

#include <assert.h>

sireflect_handle_t sireflect_register_struct(
    sireflect_registry_t *reg,
    const char *name,
    const char *fields_src,
    size_t size,
    size_t align
) {
    assert(reg != NULL);
    assert(name != NULL);
    assert(fields_src != NULL);
    assert(align != 0);

    sireflect_handle_t existing = sireflect_type_by_name(reg, name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(reg, existing);
        assert(type->kind == sireflect_kind_struct);
        assert(type->size == size);
        assert(type->align == align);
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;

    sireflect_parse_struct_fields(reg, fields_src, &parsed_fields, &field_count, size, align);

    return sireflect_registry_add_type(
        reg,
        name,
        sireflect_kind_struct,
        size,
        align,
        parsed_fields,
        field_count
    );
}
