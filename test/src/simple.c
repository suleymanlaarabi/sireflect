#include "sireflect.h"
#include <sireflect_test.h>

#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

SIREFLECT_STRUCT(Empty, {});

SIREFLECT_STRUCT(Position, {
    f32 x;
    f32 y;
});

SIREFLECT_STRUCT(Mixed, {
    u8 a;
    f64 b;
    u16 c;
});

SIREFLECT_STRUCT(WithPtr, { Position *pos; });

SIREFLECT_STRUCT(NativeTypes, {
    short a;
    int b;
    long c;
    float d;
    double e;
});

typedef struct {
    u8 a;
    f32 values[4];
} ArraySyntax;

typedef struct {
    u8 a;
    u8 b;
} MultiDecl;

static void register_unknown_type(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "Bad", "{ Missing field; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_array_syntax(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(
            sireflect_struct_desc_t
        ){ "ArraySyntax", "{ u8 a; f32 values[4]; }", sizeof(ArraySyntax), _Alignof(ArraySyntax) }
    );
    sireflect_registry_fini(reg);
}

static void register_multi_decl(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(
            sireflect_struct_desc_t
        ){ "MultiDecl", "{ u8 a, b; }", sizeof(MultiDecl), _Alignof(MultiDecl) }
    );
    sireflect_registry_fini(reg);
}

static void expect_abort_message(
    void (*function)(void),
    const char *expected_a,
    const char *expected_b,
    const char *expected_c
) {
    int stderr_pipe[2];
    test_int(pipe(stderr_pipe), 0);

    pid_t child = fork();
    test_assert(child >= 0);

    if (child == 0) {
        close(stderr_pipe[0]);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[1]);

        function();
        _exit(42);
    }

    close(stderr_pipe[1]);

    char output[2048];
    size_t len = 0;
    ssize_t bytes_read = 0;

    while ((bytes_read = read(stderr_pipe[0], output + len, sizeof(output) - len - 1)) > 0) {
        len += (size_t)bytes_read;
        if (len >= sizeof(output) - 1) {
            break;
        }
    }

    output[len] = '\0';
    close(stderr_pipe[0]);

    int status = 0;
    test_int(waitpid(child, &status, 0), child);
    test_assert(WIFSIGNALED(status));
    test_int(WTERMSIG(status), SIGABRT);
    test_not_null(strstr(output, expected_a));
    test_not_null(strstr(output, expected_b));
    test_not_null(strstr(output, expected_c));
}

void simple_builtin_types(void) {
    sireflect_registry_t *reg = sireflect_registry_init();

    test_assert(sireflect_type_by_name(reg, "u8") != SIREFLECT_INVALID_HANDLE);
    test_assert(sireflect_type_by_name(reg, "ptr") != SIREFLECT_INVALID_HANDLE);
    test_uint(sireflect_type_size(reg, sireflect_type_by_name(reg, "f64")), sizeof(f64));
    test_str(sireflect_type_name(reg, sireflect_type_by_name(reg, "char")), "char");
    test_uint(sireflect_type_size(reg, sireflect_type_by_name(reg, "short")), sizeof(short));
    test_uint(sireflect_type_size(reg, sireflect_type_by_name(reg, "int")), sizeof(int));
    test_uint(sireflect_type_size(reg, sireflect_type_by_name(reg, "long")), sizeof(long));
    test_uint(
        sireflect_type_info(reg, sireflect_type_by_name(reg, "short"))->kind,
        sireflect_kind_short
    );
    test_uint(
        sireflect_type_info(reg, sireflect_type_by_name(reg, "int"))->kind,
        sireflect_kind_int
    );
    test_uint(
        sireflect_type_info(reg, sireflect_type_by_name(reg, "long"))->kind,
        sireflect_kind_long
    );

    sireflect_registry_fini(reg);
}

void simple_empty_struct(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, Empty);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_assert(type != SIREFLECT_INVALID_HANDLE);
    test_uint(sireflect_type_size(reg, type), sizeof(Empty));
    test_uint(fields->field_count, 0);
    test_null(fields->fields);

    sireflect_registry_fini(reg);
}

void simple_primitive_fields(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, Position);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 2);
    test_str(fields->fields[0].name, "x");
    test_uint(fields->fields[0].type, sireflect_type_by_name(reg, "f32"));
    test_uint(fields->fields[0].offset, offsetof(Position, x));
    test_uint(fields->fields[0].size, sizeof(f32));
    test_uint(fields->fields[1].offset, offsetof(Position, y));

    Position pos = { .x = 1.0f, .y = 2.0f };
    test_ptr(sireflect_field_ptr(reg, type, &pos, "y"), &pos.y);

    sireflect_registry_fini(reg);
}

void simple_mixed_alignment(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, Mixed);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 3);
    test_uint(fields->fields[0].offset, offsetof(Mixed, a));
    test_uint(fields->fields[1].offset, offsetof(Mixed, b));
    test_uint(fields->fields[2].offset, offsetof(Mixed, c));
    test_uint(sireflect_type_size(reg, type), sizeof(Mixed));

    sireflect_registry_fini(reg);
}

void simple_pointer_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, WithPtr);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "pos");

    test_not_null((void *)field);
    test_uint(field->type, sireflect_type_by_name(reg, "ptr"));
    test_uint(field->offset, offsetof(WithPtr, pos));
    test_uint(field->size, sizeof(ptr));

    sireflect_registry_fini(reg);
}

void simple_native_types(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, NativeTypes);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 5);

    test_str(fields->fields[0].name, "a");
    test_uint(fields->fields[0].type, sireflect_type_by_name(reg, "short"));
    test_uint(fields->fields[0].offset, offsetof(NativeTypes, a));
    test_uint(fields->fields[0].size, sizeof(short));
    test_uint(sireflect_type_info(reg, fields->fields[0].type)->kind, sireflect_kind_short);

    test_str(fields->fields[1].name, "b");
    test_uint(fields->fields[1].type, sireflect_type_by_name(reg, "int"));
    test_uint(fields->fields[1].offset, offsetof(NativeTypes, b));
    test_uint(fields->fields[1].size, sizeof(int));
    test_uint(sireflect_type_info(reg, fields->fields[1].type)->kind, sireflect_kind_int);

    test_str(fields->fields[2].name, "c");
    test_uint(fields->fields[2].type, sireflect_type_by_name(reg, "long"));
    test_uint(fields->fields[2].offset, offsetof(NativeTypes, c));
    test_uint(fields->fields[2].size, sizeof(long));
    test_uint(sireflect_type_info(reg, fields->fields[2].type)->kind, sireflect_kind_long);

    test_uint(fields->fields[3].type, sireflect_type_by_name(reg, "float"));
    test_uint(fields->fields[3].offset, offsetof(NativeTypes, d));
    test_uint(fields->fields[3].size, sizeof(float));

    test_uint(fields->fields[4].type, sireflect_type_by_name(reg, "double"));
    test_uint(fields->fields[4].offset, offsetof(NativeTypes, e));
    test_uint(fields->fields[4].size, sizeof(double));

    sireflect_registry_fini(reg);
}

void simple_field_copy(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, Position);

    Position pos = { 0 };
    f32 value = 42.0f;
    test_int(sireflect_field_copy(reg, type, &pos, "x", &value), 0);
    test_flt(pos.x, 42.0);
    test_null((void *)sireflect_field_info(reg, type, "missing"));
    test_int(sireflect_field_copy(reg, type, &pos, "missing", &value), -1);

    sireflect_registry_fini(reg);
}

void simple_unknown_type_asserts(void) {
    test_expect_abort();
    register_unknown_type();
}

void simple_array_syntax_asserts(void) {
    test_expect_abort();
    register_array_syntax();
}

void simple_multi_decl_asserts(void) {
    test_expect_abort();
    register_multi_decl();
}

void simple_unknown_type_diagnostic(void) {
    expect_abort_message(
        register_unknown_type,
        "unknown field type 'Missing'",
        "struct 'Bad', field 'field'",
        "line 1, column 3"
    );
}

void simple_array_syntax_diagnostic(void) {
    expect_abort_message(
        register_array_syntax,
        "unsupported syntax in reflected struct",
        "struct 'ArraySyntax', field 'values'",
        "actual unsupported token '['"
    );
}

void simple_multi_decl_diagnostic(void) {
    expect_abort_message(
        register_multi_decl,
        "unsupported syntax in reflected struct",
        "struct 'MultiDecl', field 'a'",
        "actual unsupported token ','"
    );
}
