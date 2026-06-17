#ifndef SIREFLECT_PARSER_H
#define SIREFLECT_PARSER_H

#include <sireflect.h>

bool sireflect_parse_struct_fields(
    sireflect_registry_t *reg,
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    bool fail_fast
);

#endif
