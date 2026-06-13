#include "sireflect_parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    sireflect_token_ident,
    sireflect_token_lbrace,
    sireflect_token_rbrace,
    sireflect_token_star,
    sireflect_token_semicolon,
    sireflect_token_end
} sireflect_token_kind_t;

typedef struct {
    sireflect_token_kind_t kind;
    const char *start;
    size_t len;
} sireflect_token_t;

typedef struct {
    const char *src;
    size_t pos;
    sireflect_token_t current;
} sireflect_parser_t;

static inline int sireflect_is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }

static inline int sireflect_is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static inline void sireflect_parser_next(sireflect_parser_t *parser) {
    const char *src = parser->src;

    while (isspace((unsigned char)src[parser->pos])) {
        parser->pos++;
    }

    const size_t start = parser->pos;
    const char c = src[start];

    if (c == '\0') {
        parser->current = (sireflect_token_t){ sireflect_token_end, &src[start], 0 };
        return;
    }

    if (sireflect_is_ident_start(c)) {
        parser->pos++;
        while (sireflect_is_ident_char(src[parser->pos])) {
            parser->pos++;
        }

        parser->current = (sireflect_token_t){
            sireflect_token_ident,
            &src[start],
            parser->pos - start,
        };
        return;
    }

    parser->pos++;

    switch (c) {
    case '{':
        parser->current = (sireflect_token_t){ sireflect_token_lbrace, &src[start], 1 };
        return;
    case '}':
        parser->current = (sireflect_token_t){ sireflect_token_rbrace, &src[start], 1 };
        return;
    case '*':
        parser->current = (sireflect_token_t){ sireflect_token_star, &src[start], 1 };
        return;
    case ';':
        parser->current = (sireflect_token_t){ sireflect_token_semicolon, &src[start], 1 };
        return;
    default:
        sireflect_assert(false, "unsupported token in reflected struct");
    }
}

static inline void sireflect_parser_init(sireflect_parser_t *parser, const char *src) {
    sireflect_assert(parser != NULL, "parser must not be NULL");
    sireflect_assert(src != NULL, "parser source must not be NULL");

    parser->src = src;
    parser->pos = 0;
    sireflect_parser_next(parser);
}

static inline sireflect_token_t
sireflect_expect(sireflect_parser_t *parser, sireflect_token_kind_t kind) {
    sireflect_token_t token = parser->current;
    sireflect_assert(token.kind == kind, "unexpected token in reflected struct");
    sireflect_parser_next(parser);
    return token;
}

static inline char *sireflect_dup_range(const char *start, size_t len) {
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate parser string");

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

static inline void sireflect_parse_field_shape(sireflect_parser_t *parser) {
    sireflect_expect(parser, sireflect_token_ident);

    if (parser->current.kind == sireflect_token_star) {
        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_ident);
    sireflect_expect(parser, sireflect_token_semicolon);
}

static inline size_t sireflect_count_fields(const char *fields_src) {
    sireflect_parser_t parser;
    size_t count = 0;

    sireflect_parser_init(&parser, fields_src);
    sireflect_expect(&parser, sireflect_token_lbrace);

    while (parser.current.kind != sireflect_token_rbrace) {
        sireflect_parse_field_shape(&parser);
        count++;
    }

    sireflect_expect(&parser, sireflect_token_rbrace);
    sireflect_expect(&parser, sireflect_token_end);

    return count;
}

static inline size_t sireflect_align_up(size_t value, size_t align) {
    sireflect_assert(align != 0, "alignment must not be zero");

    const size_t remainder = value % align;
    if (remainder == 0) {
        return value;
    }

    return value + align - remainder;
}

static inline void sireflect_parse_field(
    sireflect_registry_t *reg,
    sireflect_parser_t *parser,
    sireflect_field_info_t *field,
    size_t *offset,
    size_t *max_align
) {
    sireflect_token_t type_token = sireflect_expect(parser, sireflect_token_ident);
    int is_pointer = 0;

    if (parser->current.kind == sireflect_token_star) {
        is_pointer = 1;
        sireflect_parser_next(parser);
    }

    sireflect_token_t name_token = sireflect_expect(parser, sireflect_token_ident);
    sireflect_expect(parser, sireflect_token_semicolon);

    char *type_name = sireflect_dup_range(type_token.start, type_token.len);
    sireflect_handle_t field_type = sireflect_type_by_name(reg, type_name);
    sireflect_assert(field_type != SIREFLECT_INVALID_HANDLE, "unknown field type");

    if (is_pointer) {
        field_type = sireflect_type_by_name(reg, "ptr");
        sireflect_assert(field_type != SIREFLECT_INVALID_HANDLE, "built-in ptr type is missing");
    }

    const sireflect_type_info_t *type_info = sireflect_type_info(reg, field_type);
    sireflect_assert(type_info != NULL, "field type metadata must exist");

    free(type_name);

    field->name = sireflect_dup_range(name_token.start, name_token.len);
    field->type = field_type;
    field->size = type_info->size;
    field->align = type_info->align;
    field->offset = sireflect_align_up(*offset, field->align);

    *offset = field->offset + field->size;
    if (field->align > *max_align) {
        *max_align = field->align;
    }
}

void sireflect_parse_struct_fields(
    sireflect_registry_t *reg,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align
) {
    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(fields_src != NULL, "field source must not be NULL");
    sireflect_assert(out_fields != NULL, "output field pointer must not be NULL");
    sireflect_assert(out_field_count != NULL, "output field count pointer must not be NULL");

    const size_t field_count = sireflect_count_fields(fields_src);
    sireflect_field_info_t *fields = NULL;

    if (field_count != 0) {
        fields = calloc(field_count, sizeof(*fields));
        sireflect_assert(fields != NULL, "failed to allocate field metadata");
    }

    sireflect_parser_t parser;
    sireflect_parser_init(&parser, fields_src);
    sireflect_expect(&parser, sireflect_token_lbrace);

    size_t offset = 0;
    size_t max_align = 1;

    for (size_t i = 0; i < field_count; i++) {
        sireflect_parse_field(reg, &parser, &fields[i], &offset, &max_align);
    }

    sireflect_expect(&parser, sireflect_token_rbrace);
    sireflect_expect(&parser, sireflect_token_end);

    const size_t computed_size = sireflect_align_up(offset, struct_align);
    sireflect_assert(computed_size == struct_size, "computed struct size does not match C layout");
    sireflect_assert(max_align <= struct_align, "computed field alignment exceeds struct alignment");

    *out_fields = fields;
    *out_field_count = field_count;
}
