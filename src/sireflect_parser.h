#ifndef SIREFLECT_PARSER_H
#define SIREFLECT_PARSER_H

#include <sireflect.h>

bool sireflect_parse_struct_fields(
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    size_t *out_struct_size,
    size_t *out_struct_align,
    bool validate_layout,
    bool fail_fast
);

#endif
