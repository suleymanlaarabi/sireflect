#include "sireflect_parser.h"
#include "sireflect.h"
#include "sireflect_error.h"
#include "sireflect_registry.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIREFLECT_MAX_ARRAY_DIMS 16

typedef enum {
    sireflect_token_ident,
    sireflect_token_integer,
    sireflect_token_lbrace,
    sireflect_token_rbrace,
    sireflect_token_lbracket,
    sireflect_token_rbracket,
    sireflect_token_star,
    sireflect_token_comma,
    sireflect_token_semicolon,
    sireflect_token_unknown,
    sireflect_token_end
} sireflect_token_kind_t;

typedef struct {
    sireflect_token_kind_t kind;
    const char *start;
    size_t len;
    size_t offset;
    size_t line;
    size_t column;
} sireflect_token_t;

typedef struct {
    const char *src;
    const char *struct_name;
    const char *field_start;
    size_t field_len;
    size_t pos;
    size_t line;
    size_t column;
    sireflect_token_t current;
    char message[512];
    bool failed;
    bool fail_fast;
} sireflect_parser_t;

typedef struct {
    const char *start;
    size_t len;
    char name[64];
    int has_name;
    size_t line;
    size_t column;
} sireflect_type_spec_t;

static inline int sireflect_is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }

static inline int sireflect_is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static inline int sireflect_token_is_ident(sireflect_token_t token, const char *text) {
    return token.kind == sireflect_token_ident && strlen(text) == token.len &&
           strncmp(token.start, text, token.len) == 0;
}

static inline int sireflect_token_is_qualifier(sireflect_token_t token) {
    return sireflect_token_is_ident(token, "const") || sireflect_token_is_ident(token, "volatile");
}

static inline void
sireflect_type_spec_set(sireflect_type_spec_t *type, sireflect_token_t token) {
    type->start = token.start;
    type->len = token.len;
    type->name[0] = '\0';
    type->has_name = 0;
    type->line = token.line;
    type->column = token.column;
}

static inline void sireflect_type_spec_set2(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    )
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline void sireflect_type_spec_set3(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second,
    sireflect_token_t third
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start,
        (int)third.len,
        third.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    );
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline const char *sireflect_token_kind_name(sireflect_token_kind_t kind) {
    switch (kind) {
    case sireflect_token_ident:
        return "identifier";
    case sireflect_token_integer:
        return "integer";
    case sireflect_token_lbrace:
        return "'{'";
    case sireflect_token_rbrace:
        return "'}'";
    case sireflect_token_lbracket:
        return "'['";
    case sireflect_token_rbracket:
        return "']'";
    case sireflect_token_star:
        return "'*'";
    case sireflect_token_comma:
        return "','";
    case sireflect_token_semicolon:
        return "';'";
    case sireflect_token_unknown:
        return "unsupported token";
    case sireflect_token_end:
        return "end of input";
    }

    return "unknown token";
}

static inline void
sireflect_token_display(sireflect_token_t token, char *buffer, size_t buffer_size) {
    if (token.kind == sireflect_token_end) {
        snprintf(buffer, buffer_size, "end of input");
        return;
    }

    if (token.len == 0) {
        snprintf(buffer, buffer_size, "%s", sireflect_token_kind_name(token.kind));
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s '%.*s'",
        sireflect_token_kind_name(token.kind),
        (int)token.len,
        token.start
    );
}

static inline void
sireflect_parser_context(sireflect_parser_t *parser, char *buffer, size_t buffer_size) {
    if (parser->field_start != NULL) {
        snprintf(
            buffer,
            buffer_size,
            "struct '%s', field '%.*s'",
            parser->struct_name != NULL ? parser->struct_name : "<unknown>",
            (int)parser->field_len,
            parser->field_start
        );
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "struct '%s'",
        parser->struct_name != NULL ? parser->struct_name : "<unknown>"
    );
}

static inline void
sireflect_parser_fail_at(sireflect_parser_t *parser, sireflect_token_t token, const char *message) {
    char actual[96];
    char context[160];

    sireflect_token_display(token, actual, sizeof(actual));
    sireflect_parser_context(parser, context, sizeof(context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "%s in %s at line %zu, column %zu: actual %s",
        message,
        context,
        token.line,
        token.column,
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_unexpected(
    sireflect_parser_t *parser,
    sireflect_token_kind_t expected,
    const char *context
) {
    char actual[96];
    char parser_context[160];

    sireflect_token_display(parser->current, actual, sizeof(actual));
    sireflect_parser_context(parser, parser_context, sizeof(parser_context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "unexpected token while parsing %s in %s at line %zu, column %zu: expected %s, actual %s",
        context,
        parser_context,
        parser->current.line,
        parser->current.column,
        sireflect_token_kind_name(expected),
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_advance(sireflect_parser_t *parser) {
    if (parser->src[parser->pos] == '\n') {
        parser->line++;
        parser->column = 1;
    } else {
        parser->column++;
    }

    parser->pos++;
}

static inline void sireflect_parser_next(sireflect_parser_t *parser) {
    const char *src = parser->src;

    while (isspace((unsigned char)src[parser->pos])) {
        sireflect_parser_advance(parser);
    }

    const size_t start = parser->pos;
    const size_t line = parser->line;
    const size_t column = parser->column;
    const char c = src[start];

    if (c == '\0') {
        parser->current = (sireflect_token_t){ sireflect_token_end, &src[start], 0, start, line, column };
        return;
    }

    if (sireflect_is_ident_start(c)) {
        sireflect_parser_advance(parser);
        while (sireflect_is_ident_char(src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_ident,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    if (isdigit((unsigned char)c)) {
        sireflect_parser_advance(parser);
        while (isdigit((unsigned char)src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_integer,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    sireflect_parser_advance(parser);

    switch (c) {
    case '{':
        parser->current = (sireflect_token_t){ sireflect_token_lbrace, &src[start], 1, start, line, column };
        return;
    case '}':
        parser->current = (sireflect_token_t){ sireflect_token_rbrace, &src[start], 1, start, line, column };
        return;
    case '[':
        parser->current =
            (sireflect_token_t){ sireflect_token_lbracket, &src[start], 1, start, line, column };
        return;
    case ']':
        parser->current =
            (sireflect_token_t){ sireflect_token_rbracket, &src[start], 1, start, line, column };
        return;
    case '*':
        parser->current = (sireflect_token_t){ sireflect_token_star, &src[start], 1, start, line, column };
        return;
    case ',':
        parser->current = (sireflect_token_t){ sireflect_token_comma, &src[start], 1, start, line, column };
        return;
    case ';':
        parser->current =
            (sireflect_token_t){ sireflect_token_semicolon, &src[start], 1, start, line, column };
        return;
    default:
        parser->current = (sireflect_token_t){ sireflect_token_unknown, &src[start], 1, start, line, column };
        sireflect_parser_fail_at(
            parser,
            parser->current,
            "unsupported syntax in reflected struct; supported fields are '<type> <name>;', '<type> <name>, <name>;', '<type> *<name>;', '<type> <name>[N][M];', and '<type> *<name>[N];'"
        );
    }
}

static inline void sireflect_parser_init(
    sireflect_parser_t *parser,
    const char *struct_name,
    const char *src,
    bool fail_fast
) {
    sireflect_assert(parser != NULL, "parser must not be NULL");
    sireflect_assert(struct_name != NULL, "parser struct name must not be NULL");
    sireflect_assert(src != NULL, "parser source must not be NULL");

    parser->src = src;
    parser->struct_name = struct_name;
    parser->field_start = NULL;
    parser->field_len = 0;
    parser->pos = 0;
    parser->line = 1;
    parser->column = 1;
    parser->message[0] = '\0';
    parser->failed = false;
    parser->fail_fast = fail_fast;
    sireflect_parser_next(parser);
}

static inline sireflect_token_t
sireflect_expect(sireflect_parser_t *parser, sireflect_token_kind_t kind, const char *context) {
    sireflect_token_t token = parser->current;
    if (token.kind != kind) {
        sireflect_parser_unexpected(parser, kind, context);
        return token;
    }
    sireflect_parser_next(parser);
    return token;
}

static inline sireflect_token_t sireflect_expect_field_name(sireflect_parser_t *parser) {
    sireflect_token_t token = parser->current;
    if (token.kind != sireflect_token_ident || sireflect_token_is_qualifier(token)) {
        sireflect_parser_unexpected(parser, sireflect_token_ident, "field name");
        return token;
    }

    parser->field_start = token.start;
    parser->field_len = token.len;
    sireflect_parser_next(parser);
    return token;
}

static inline uint32_t sireflect_parse_qualifiers(sireflect_parser_t *parser) {
    uint32_t qualifiers = 0;

    for (;;) {
        if (sireflect_token_is_ident(parser->current, "const")) {
            qualifiers |= SIREFLECT_QUAL_CONST;
            sireflect_parser_next(parser);
            continue;
        }

        if (sireflect_token_is_ident(parser->current, "volatile")) {
            qualifiers |= SIREFLECT_QUAL_VOLATILE;
            sireflect_parser_next(parser);
            continue;
        }

        return qualifiers;
    }
}

static inline void
sireflect_fail_unsupported_type_specifier(sireflect_parser_t *parser, sireflect_token_t token) {
    sireflect_parser_fail_at(
        parser,
        token,
        "unsupported type specifier sequence; supported multi-token types are 'signed char', 'unsigned char', 'unsigned short', 'unsigned int', 'unsigned long', 'long long', and 'unsigned long long'"
    );
}

static inline sireflect_type_spec_t sireflect_parse_type_specifier(sireflect_parser_t *parser) {
    sireflect_token_t first = sireflect_expect(parser, sireflect_token_ident, "field type");
    sireflect_type_spec_t type;
    sireflect_type_spec_set(&type, first);
    if (parser->failed) {
        return type;
    }

    if (sireflect_token_is_ident(first, "signed")) {
        if (!sireflect_token_is_ident(parser->current, "char")) {
            sireflect_fail_unsupported_type_specifier(parser, parser->current);
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "long")) {
        if (!sireflect_token_is_ident(parser->current, "long")) {
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "unsigned")) {
        sireflect_token_t second = parser->current;

        if (sireflect_token_is_ident(second, "char") || sireflect_token_is_ident(second, "short") ||
            sireflect_token_is_ident(second, "int")) {
            sireflect_parser_next(parser);
            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        if (sireflect_token_is_ident(second, "long")) {
            sireflect_parser_next(parser);

            if (sireflect_token_is_ident(parser->current, "long")) {
                sireflect_token_t third = parser->current;
                sireflect_parser_next(parser);
                sireflect_type_spec_set3(&type, first, second, third);
                return type;
            }

            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        sireflect_fail_unsupported_type_specifier(parser, second);
        return type;
    }

    return type;
}

static inline char *sireflect_dup_range(const char *start, size_t len) {
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate parser string");

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

static inline size_t
sireflect_parse_array_count(sireflect_parser_t *parser, sireflect_token_t token) {
    size_t count = 0;

    for (size_t i = 0; i < token.len; i++) {
        const unsigned int digit = (unsigned int)(token.start[i] - '0');
        if (count > (SIZE_MAX - digit) / 10) {
            sireflect_parser_fail_at(parser, token, "array element count overflows size_t");
            return 0;
        }
        count = count * 10 + digit;
    }

    if (count == 0) {
        sireflect_parser_fail_at(parser, token, "array element count must be greater than zero");
        return 0;
    }

    return count;
}

static inline size_t
sireflect_parse_array_dimensions(sireflect_parser_t *parser, size_t *counts, size_t max_count) {
    size_t count = 0;

    while (parser->current.kind == sireflect_token_lbracket) {
        sireflect_parser_next(parser);

        if (parser->current.kind == sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "array element count is required");
            return count;
        }

        if (parser->current.kind != sireflect_token_integer) {
            sireflect_parser_fail_at(
                parser,
                parser->current,
                "array element count must be a positive integer literal"
            );
            return count;
        }

        sireflect_token_t count_token = parser->current;
        sireflect_parser_next(parser);

        if (parser->current.kind != sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "expected ']' after array element count");
            return count;
        }

        if (count >= max_count) {
            sireflect_parser_fail_at(parser, count_token, "too many array dimensions");
            return count;
        }

        counts[count++] = sireflect_parse_array_count(parser, count_token);
        if (parser->failed) {
            return count;
        }
        sireflect_parser_next(parser);
    }

    return count;
}

static inline void sireflect_parse_declarator_shape(sireflect_parser_t *parser) {
    size_t counts[SIREFLECT_MAX_ARRAY_DIMS];

    parser->field_start = NULL;
    parser->field_len = 0;

    if (parser->current.kind == sireflect_token_star) {
        sireflect_parser_next(parser);
    }

    sireflect_expect_field_name(parser);
    if (parser->failed) {
        return;
    }
    (void)sireflect_parse_array_dimensions(parser, counts, SIREFLECT_MAX_ARRAY_DIMS);
}

static inline size_t sireflect_parse_declaration_shape(sireflect_parser_t *parser) {
    size_t count = 0;

    (void)sireflect_parse_qualifiers(parser);
    (void)sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator_shape(parser);
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline bool sireflect_count_fields(
    const char *struct_name,
    const char *fields_src,
    bool fail_fast,
    size_t *out_count
) {
    sireflect_parser_t parser;
    size_t count = 0;

    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        return false;
    }

    while (parser.current.kind != sireflect_token_rbrace) {
        count += sireflect_parse_declaration_shape(&parser);
        if (parser.failed) {
            return false;
        }
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        return false;
    }

    *out_count = count;
    return true;
}

static inline size_t sireflect_align_up(size_t value, size_t align) {
    sireflect_assert(align != 0, "alignment must not be zero");

    const size_t remainder = value % align;
    if (remainder == 0) {
        return value;
    }

    return value + align - remainder;
}

static inline sireflect_handle_t sireflect_resolve_field_type(
    sireflect_registry_t *reg,
    sireflect_parser_t *parser,
    sireflect_type_spec_t type
) {
    char *owned_name = NULL;
    const char *type_name = type.name;

    if (!type.has_name) {
        owned_name = sireflect_dup_range(type.start, type.len);
        type_name = owned_name;
    }

    sireflect_handle_t field_type = sireflect_type_by_name(reg, type_name);
    if (field_type == SIREFLECT_INVALID_HANDLE) {
        char context[160];

        sireflect_parser_context(parser, context, sizeof(context));
        snprintf(
            parser->message,
            sizeof(parser->message),
            "unknown field type '%s' in %s at line %zu, column %zu; register the type before this struct or use a supported primitive alias",
            type_name,
            context,
            type.line,
            type.column
        );
        free(owned_name);
        parser->failed = true;
        if (parser->fail_fast) {
            sireflect_assert(false, parser->message);
        }
        sireflect_error_set(parser->message);
        return SIREFLECT_INVALID_HANDLE;
    }

    free(owned_name);
    return field_type;
}

static inline void sireflect_parse_declarator(
    sireflect_registry_t *reg,
    sireflect_parser_t *parser,
    sireflect_field_info_t *field,
    sireflect_type_spec_t type,
    uint32_t qualifiers,
    size_t *offset,
    size_t *max_align
) {
    parser->field_start = NULL;
    parser->field_len = 0;

    int is_pointer = 0;
    size_t array_counts[SIREFLECT_MAX_ARRAY_DIMS];
    size_t array_dim_count = 0;

    if (parser->current.kind == sireflect_token_star) {
        is_pointer = 1;
        sireflect_parser_next(parser);
    }

    sireflect_token_t name_token = sireflect_expect_field_name(parser);
    if (parser->failed) {
        return;
    }
    array_dim_count =
        sireflect_parse_array_dimensions(parser, array_counts, SIREFLECT_MAX_ARRAY_DIMS);
    if (parser->failed) {
        return;
    }

    sireflect_handle_t field_type = sireflect_resolve_field_type(reg, parser, type);
    if (parser->failed) {
        return;
    }

    if (is_pointer) {
        field_type = sireflect_registry_get_or_add_pointer_type(reg, field_type);
    }

    for (size_t i = array_dim_count; i > 0; i--) {
        field_type = sireflect_registry_get_or_add_array_type(reg, field_type, array_counts[i - 1]);
    }

    const sireflect_type_info_t *type_info = sireflect_type_info(reg, field_type);
    sireflect_assert(type_info != NULL, "field type metadata must exist");

    field->name = sireflect_dup_range(name_token.start, name_token.len);
    field->type = field_type;
    field->size = type_info->size;
    field->align = type_info->align;
    field->offset = sireflect_align_up(*offset, field->align);
    field->qualifiers = qualifiers;

    *offset = field->offset + field->size;
    if (field->align > *max_align) {
        *max_align = field->align;
    }
}

static inline size_t sireflect_parse_declaration(
    sireflect_registry_t *reg,
    sireflect_parser_t *parser,
    sireflect_field_info_t *fields,
    size_t *offset,
    size_t *max_align
) {
    size_t count = 0;
    uint32_t qualifiers = sireflect_parse_qualifiers(parser);
    sireflect_type_spec_t type = sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator(
            reg,
            parser,
            &fields[count],
            type,
            qualifiers,
            offset,
            max_align
        );
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline void sireflect_free_parsed_fields(sireflect_field_info_t *fields, size_t field_count) {
    if (fields == NULL) {
        return;
    }

    for (size_t i = 0; i < field_count; i++) {
        free((char *)fields[i].name);
    }

    free(fields);
}

bool sireflect_parse_struct_fields(
    sireflect_registry_t *reg,
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    bool fail_fast
) {
    (void)struct_size;
    (void)struct_align;

    sireflect_assert(reg != NULL, "registry must not be NULL");
    sireflect_assert(struct_name != NULL, "struct name must not be NULL");
    sireflect_assert(fields_src != NULL, "field source must not be NULL");
    sireflect_assert(out_fields != NULL, "output field pointer must not be NULL");
    sireflect_assert(out_field_count != NULL, "output field count pointer must not be NULL");

    size_t field_count = 0;
    if (!sireflect_count_fields(struct_name, fields_src, fail_fast, &field_count)) {
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    sireflect_field_info_t *fields = NULL;

    if (field_count != 0) {
        fields = calloc(field_count, sizeof(*fields));
        sireflect_assert(fields != NULL, "failed to allocate field metadata");
    }

    sireflect_parser_t parser;
    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    size_t offset = 0;
    size_t max_align = 1;

    for (size_t i = 0; i < field_count;) {
        const size_t parsed_count =
            sireflect_parse_declaration(reg, &parser, &fields[i], &offset, &max_align);
        if (parser.failed) {
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
        i += parsed_count;
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

#ifndef NDEBUG
    {
        const size_t computed_size = sireflect_align_up(offset, struct_align);
        if (computed_size != struct_size) {
            if (fail_fast) {
                sireflect_assert(false, "computed struct size does not match C layout");
            }
            sireflect_error_set("computed struct size does not match C layout");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }

        if (max_align > struct_align) {
            if (fail_fast) {
                sireflect_assert(false, "computed field alignment exceeds struct alignment");
            }
            sireflect_error_set("computed field alignment exceeds struct alignment");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
    }
#endif

    *out_fields = fields;
    *out_field_count = field_count;
    return true;
}
