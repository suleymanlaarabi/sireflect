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

SIREFLECT_STRUCT(WithCharPtr, { char *name; });

SIREFLECT_STRUCT(WithRawPtr, { ptr raw; });

SIREFLECT_STRUCT(NativeTypes, {
    short a;
    int b;
    long c;
    float d;
    double e;
});

SIREFLECT_STRUCT(ArraySyntax, {
    u8 a;
    f32 values[4];
});

SIREFLECT_STRUCT(StructArray, {
    u8 tag;
    Position positions[2];
});

SIREFLECT_STRUCT(RepeatedArray, {
    f32 a[4];
    f32 b[4];
});

SIREFLECT_STRUCT(PointerArray, { Position *items[8]; });

SIREFLECT_STRUCT(Matrix, { f32 matrix[4][4]; });

SIREFLECT_STRUCT(PointerMatrix, { Position *items[2][3]; });

SIREFLECT_STRUCT(MultiplePrimitiveDeclarators, { f32 x, y; });

SIREFLECT_STRUCT(MultipleStructDeclarators, { Position a, b; });

SIREFLECT_STRUCT(MultipleArrayDeclarators, { int values[4], more[8]; });

SIREFLECT_STRUCT(MultiplePointerDeclarators, { Position *parent, *next; });

SIREFLECT_STRUCT(MultipleMixedDeclarators, { Position value, *ref; });

SIREFLECT_STRUCT(QualifiedScalars, {
    const f32 x;
    volatile int y;
    const volatile u32 flags;
});

SIREFLECT_STRUCT(QualifiedPointer, { const Position *parent; });

SIREFLECT_STRUCT(QualifiedMultipleDeclarators, { const f32 x, y; });

SIREFLECT_STRUCT(MultiTokenBuiltins, {
    signed char code;
    unsigned char byte;
    unsigned short small;
    unsigned int flags;
    unsigned long span;
    long long count;
    unsigned long long mask;
});

SIREFLECT_STRUCT(MultiTokenCombinations, {
    const unsigned int a, b;
    unsigned long long values[2];
    signed char *code;
});

static void register_unknown_type(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "Bad", "{ Missing field; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_empty_array(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "BadArray", "{ f32 values[]; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_zero_array(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "BadArray", "{ f32 values[0]; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_alpha_array_count(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "BadArray", "{ f32 values[abc]; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_missing_array_end(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "BadArray", "{ f32 values[4; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_nested_empty_array(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "BadArray", "{ f32 values[4][]; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_missing_declarator(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){ "BadDecl", "{ f32 x, ; }", sizeof(ptr), _Alignof(ptr) }
    );
    sireflect_registry_fini(reg);
}

static void register_post_pointer_qualifier(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect(reg, Position);
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){
            "BadPointerQualifier",
            "{ Position * const stable_parent; }",
            sizeof(ptr),
            _Alignof(ptr),
        }
    );
    sireflect_registry_fini(reg);
}

static void register_unsupported_type_specifier(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_register_struct(
        reg,
        &(sireflect_struct_desc_t){
            "BadTypeSpecifier",
            "{ unsigned signed int flags; }",
            sizeof(unsigned int),
            _Alignof(unsigned int),
        }
    );
    sireflect_registry_fini(reg);
}

static void check_null_type_is_struct(void) {
    (void)sireflect_type_is_struct(NULL);
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

void sireflect_test_impl_builtin_types(void) {
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

void sireflect_test_impl_empty_struct(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, Empty);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_assert(type != SIREFLECT_INVALID_HANDLE);
    test_uint(sireflect_type_size(reg, type), sizeof(Empty));
    test_uint(fields->field_count, 0);
    test_null(fields->fields);

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_primitive_fields(void) {
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

void sireflect_test_impl_mixed_alignment(void) {
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

void sireflect_test_impl_pointer_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, WithPtr);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "pos");

    test_not_null((void *)field);
    test_uint(field->offset, offsetof(WithPtr, pos));
    test_uint(field->size, sizeof(((WithPtr *)0)->pos));
    test_uint(field->align, _Alignof(ptr));

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, field->type);
    test_uint(pointer->kind, sireflect_kind_pointer);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, sireflect_type_by_name(reg, "Position"));
    test_uint(pointer->element_type, sireflect_type_pointee(reg, field->type));
    test_uint(pointer->element_count, 0);
    test_str(pointer->name, "Position*");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_pointer_compat_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, WithCharPtr);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "name");

    test_not_null((void *)field);
    test_uint(field->offset, offsetof(WithCharPtr, name));
    test_uint(field->size, sizeof(((WithCharPtr *)0)->name));
    test_uint(field->align, _Alignof(ptr));

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, field->type);
    test_uint(pointer->kind, sireflect_kind_pointer);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, sireflect_type_by_name(reg, "char"));
    test_uint(pointer->element_type, sireflect_type_pointee(reg, field->type));
    test_uint(pointer->element_count, 0);
    test_str(pointer->name, "char*");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_raw_pointer_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, WithRawPtr);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "raw");

    test_not_null((void *)field);
    test_uint(field->type, sireflect_type_by_name(reg, "ptr"));
    test_uint(field->offset, offsetof(WithRawPtr, raw));
    test_uint(field->size, sizeof(((WithRawPtr *)0)->raw));
    test_uint(field->align, _Alignof(ptr));

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, field->type);
    test_uint(pointer->kind, sireflect_kind_ptr);
    test_assert(!sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, SIREFLECT_INVALID_HANDLE);
    test_uint(pointer->element_count, 0);

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_native_types(void) {
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

void sireflect_test_impl_field_copy(void) {
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

void sireflect_test_impl_kind_helpers(void) {
    test_str(sireflect_kind_name(sireflect_kind_u8), "u8");
    test_str(sireflect_kind_name(sireflect_kind_i32), "i32");
    test_str(sireflect_kind_name(sireflect_kind_f64), "f64");
    test_str(sireflect_kind_name(sireflect_kind_bool), "bool");
    test_str(sireflect_kind_name(sireflect_kind_ptr), "ptr");
    test_str(sireflect_kind_name(sireflect_kind_pointer), "pointer");
    test_str(sireflect_kind_name(sireflect_kind_struct), "struct");
    test_str(sireflect_kind_name(sireflect_kind_array), "array");
    test_str(sireflect_kind_name((sireflect_kind_t)999), "unknown");

    test_assert(sireflect_is_numeric(sireflect_kind_u8));
    test_assert(sireflect_is_numeric(sireflect_kind_i64));
    test_assert(sireflect_is_numeric(sireflect_kind_f32));
    test_assert(sireflect_is_numeric(sireflect_kind_char));
    test_assert(sireflect_is_numeric(sireflect_kind_short));
    test_assert(sireflect_is_numeric(sireflect_kind_int));
    test_assert(sireflect_is_numeric(sireflect_kind_long));
    test_assert(!sireflect_is_numeric(sireflect_kind_bool));
    test_assert(!sireflect_is_numeric(sireflect_kind_ptr));
    test_assert(!sireflect_is_numeric(sireflect_kind_pointer));
    test_assert(!sireflect_is_numeric(sireflect_kind_struct));
    test_assert(!sireflect_is_numeric(sireflect_kind_array));
    test_assert(!sireflect_is_numeric((sireflect_kind_t)999));
}

void sireflect_test_impl_type_is_struct_helper(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t struct_type = sireflect(reg, Position);
    sireflect_handle_t int_type = sireflect_type_by_name(reg, "int");

    test_assert(sireflect_type_is_struct(sireflect_type_info(reg, struct_type)));
    test_assert(!sireflect_type_is_struct(sireflect_type_info(reg, int_type)));

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_type_is_struct_asserts(void) {
    test_expect_abort();
    check_null_type_is_struct();
}

void sireflect_test_impl_array_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, ArraySyntax);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "values");

    test_not_null((void *)field);
    test_str(field->name, "values");
    test_uint(field->offset, offsetof(ArraySyntax, values));
    test_uint(field->size, sizeof(((ArraySyntax *)0)->values));
    test_uint(field->align, _Alignof(f32));

    const sireflect_type_info_t *array = sireflect_type_info(reg, field->type);
    test_assert(sireflect_type_is_array(array));
    test_uint(array->kind, sireflect_kind_array);
    test_uint(array->element_type, sireflect_type_by_name(reg, "f32"));
    test_uint(array->element_count, 4);
    test_uint(sireflect_type_element(reg, field->type), sireflect_type_by_name(reg, "f32"));
    test_uint(sireflect_type_element_count(reg, field->type), 4);
    test_uint(array->size, sizeof(f32) * 4);
    test_uint(array->align, _Alignof(f32));
    test_str(array->name, "f32[4]");

    ArraySyntax obj = { 0 };
    test_ptr(sireflect_field_ptr(reg, type, &obj, "values"), &obj.values);

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_struct_array_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t position = sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, StructArray);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "positions");

    test_not_null((void *)field);
    test_uint(field->offset, offsetof(StructArray, positions));
    test_uint(field->size, sizeof(((StructArray *)0)->positions));

    const sireflect_type_info_t *array = sireflect_type_info(reg, field->type);
    test_assert(sireflect_type_is_array(array));
    test_uint(array->element_type, position);
    test_uint(array->element_count, 2);
    test_uint(array->size, sizeof(Position) * 2);
    test_uint(array->align, _Alignof(Position));
    test_str(array->name, "Position[2]");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_repeated_array_type(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, RepeatedArray);
    const sireflect_field_info_t *a = sireflect_field_info(reg, type, "a");
    const sireflect_field_info_t *b = sireflect_field_info(reg, type, "b");

    test_not_null((void *)a);
    test_not_null((void *)b);
    test_uint(a->type, b->type);

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_pointer_array_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t position = sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, PointerArray);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "items");

    test_not_null((void *)field);
    test_uint(field->offset, offsetof(PointerArray, items));
    test_uint(field->size, sizeof(((PointerArray *)0)->items));

    const sireflect_type_info_t *array = sireflect_type_info(reg, field->type);
    test_assert(sireflect_type_is_array(array));
    test_uint(array->element_count, 8);
    test_uint(array->size, sizeof(Position *) * 8);
    test_uint(array->align, _Alignof(ptr));
    test_str(array->name, "Position*[8]");

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, array->element_type);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, position);
    test_str(pointer->name, "Position*");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_matrix_array_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, Matrix);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "matrix");

    test_not_null((void *)field);
    test_uint(field->offset, offsetof(Matrix, matrix));
    test_uint(field->size, sizeof(((Matrix *)0)->matrix));

    const sireflect_type_info_t *outer = sireflect_type_info(reg, field->type);
    test_assert(sireflect_type_is_array(outer));
    test_uint(outer->element_count, 4);
    test_str(outer->name, "f32[4][4]");

    const sireflect_type_info_t *inner = sireflect_type_info(reg, outer->element_type);
    test_assert(sireflect_type_is_array(inner));
    test_uint(inner->element_type, sireflect_type_by_name(reg, "f32"));
    test_uint(inner->element_count, 4);
    test_uint(inner->size, sizeof(f32) * 4);
    test_uint(inner->align, _Alignof(f32));
    test_str(inner->name, "f32[4]");

    test_uint(outer->size, sizeof(f32) * 4 * 4);
    test_uint(outer->align, _Alignof(f32));

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_pointer_matrix_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t position = sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, PointerMatrix);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "items");

    test_not_null((void *)field);
    test_uint(field->offset, offsetof(PointerMatrix, items));
    test_uint(field->size, sizeof(((PointerMatrix *)0)->items));

    const sireflect_type_info_t *outer = sireflect_type_info(reg, field->type);
    test_assert(sireflect_type_is_array(outer));
    test_uint(outer->element_count, 2);
    test_str(outer->name, "Position*[2][3]");

    const sireflect_type_info_t *inner = sireflect_type_info(reg, outer->element_type);
    test_assert(sireflect_type_is_array(inner));
    test_uint(inner->element_count, 3);
    test_str(inner->name, "Position*[3]");

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, inner->element_type);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, position);

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_multiple_primitive_declarators(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, MultiplePrimitiveDeclarators);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);
    sireflect_handle_t f32_type = sireflect_type_by_name(reg, "f32");

    test_uint(fields->field_count, 2);
    test_str(fields->fields[0].name, "x");
    test_uint(fields->fields[0].type, f32_type);
    test_uint(fields->fields[0].offset, offsetof(MultiplePrimitiveDeclarators, x));
    test_uint(fields->fields[0].size, sizeof(f32));
    test_str(fields->fields[1].name, "y");
    test_uint(fields->fields[1].type, f32_type);
    test_uint(fields->fields[1].offset, offsetof(MultiplePrimitiveDeclarators, y));
    test_uint(fields->fields[1].size, sizeof(f32));

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_multiple_struct_declarators(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t position = sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, MultipleStructDeclarators);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 2);
    test_str(fields->fields[0].name, "a");
    test_uint(fields->fields[0].type, position);
    test_uint(fields->fields[0].offset, offsetof(MultipleStructDeclarators, a));
    test_uint(fields->fields[0].size, sizeof(Position));
    test_str(fields->fields[1].name, "b");
    test_uint(fields->fields[1].type, position);
    test_uint(fields->fields[1].offset, offsetof(MultipleStructDeclarators, b));
    test_uint(fields->fields[1].size, sizeof(Position));

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_multiple_array_declarators(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, MultipleArrayDeclarators);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 2);
    test_str(fields->fields[0].name, "values");
    test_uint(fields->fields[0].offset, offsetof(MultipleArrayDeclarators, values));
    test_uint(fields->fields[0].size, sizeof(((MultipleArrayDeclarators *)0)->values));
    test_str(fields->fields[1].name, "more");
    test_uint(fields->fields[1].offset, offsetof(MultipleArrayDeclarators, more));
    test_uint(fields->fields[1].size, sizeof(((MultipleArrayDeclarators *)0)->more));

    const sireflect_type_info_t *values = sireflect_type_info(reg, fields->fields[0].type);
    const sireflect_type_info_t *more = sireflect_type_info(reg, fields->fields[1].type);
    test_assert(sireflect_type_is_array(values));
    test_assert(sireflect_type_is_array(more));
    test_uint(values->element_type, sireflect_type_by_name(reg, "int"));
    test_uint(values->element_count, 4);
    test_str(values->name, "int[4]");
    test_uint(more->element_type, sireflect_type_by_name(reg, "int"));
    test_uint(more->element_count, 8);
    test_str(more->name, "int[8]");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_multiple_pointer_declarators(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t position = sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, MultiplePointerDeclarators);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 2);
    test_str(fields->fields[0].name, "parent");
    test_uint(fields->fields[0].offset, offsetof(MultiplePointerDeclarators, parent));
    test_uint(fields->fields[0].size, sizeof(((MultiplePointerDeclarators *)0)->parent));
    test_str(fields->fields[1].name, "next");
    test_uint(fields->fields[1].offset, offsetof(MultiplePointerDeclarators, next));
    test_uint(fields->fields[1].size, sizeof(((MultiplePointerDeclarators *)0)->next));
    test_uint(fields->fields[0].type, fields->fields[1].type);

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, fields->fields[0].type);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, position);
    test_str(pointer->name, "Position*");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_multiple_mixed_declarators(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t position = sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, MultipleMixedDeclarators);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 2);
    test_str(fields->fields[0].name, "value");
    test_uint(fields->fields[0].type, position);
    test_uint(fields->fields[0].offset, offsetof(MultipleMixedDeclarators, value));
    test_uint(fields->fields[0].size, sizeof(Position));
    test_str(fields->fields[1].name, "ref");
    test_uint(fields->fields[1].offset, offsetof(MultipleMixedDeclarators, ref));
    test_uint(fields->fields[1].size, sizeof(((MultipleMixedDeclarators *)0)->ref));

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, fields->fields[1].type);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, position);
    test_str(pointer->name, "Position*");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_qualified_scalar_fields(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, QualifiedScalars);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 3);

    test_str(fields->fields[0].name, "x");
    test_uint(fields->fields[0].type, sireflect_type_by_name(reg, "f32"));
    test_uint(fields->fields[0].offset, offsetof(QualifiedScalars, x));
    test_uint(fields->fields[0].size, sizeof(f32));
    test_uint(fields->fields[0].qualifiers, SIREFLECT_QUAL_CONST);

    test_str(fields->fields[1].name, "y");
    test_uint(fields->fields[1].type, sireflect_type_by_name(reg, "int"));
    test_uint(fields->fields[1].offset, offsetof(QualifiedScalars, y));
    test_uint(fields->fields[1].size, sizeof(int));
    test_uint(fields->fields[1].qualifiers, SIREFLECT_QUAL_VOLATILE);

    test_str(fields->fields[2].name, "flags");
    test_uint(fields->fields[2].type, sireflect_type_by_name(reg, "u32"));
    test_uint(fields->fields[2].offset, offsetof(QualifiedScalars, flags));
    test_uint(fields->fields[2].size, sizeof(u32));
    test_uint(
        fields->fields[2].qualifiers,
        SIREFLECT_QUAL_CONST | SIREFLECT_QUAL_VOLATILE
    );

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_qualified_pointer_field(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t position = sireflect(reg, Position);
    sireflect_handle_t type = sireflect(reg, QualifiedPointer);
    const sireflect_field_info_t *field = sireflect_field_info(reg, type, "parent");

    test_not_null((void *)field);
    test_uint(field->offset, offsetof(QualifiedPointer, parent));
    test_uint(field->size, sizeof(((QualifiedPointer *)0)->parent));
    test_uint(field->align, _Alignof(ptr));
    test_uint(field->qualifiers, SIREFLECT_QUAL_CONST);

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, field->type);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(pointer->element_type, position);
    test_str(pointer->name, "Position*");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_qualified_multiple_declarators(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, QualifiedMultipleDeclarators);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);
    sireflect_handle_t f32_type = sireflect_type_by_name(reg, "f32");

    test_uint(fields->field_count, 2);
    test_uint(fields->fields[0].type, f32_type);
    test_uint(fields->fields[0].offset, offsetof(QualifiedMultipleDeclarators, x));
    test_uint(fields->fields[0].qualifiers, SIREFLECT_QUAL_CONST);
    test_uint(fields->fields[1].type, f32_type);
    test_uint(fields->fields[1].offset, offsetof(QualifiedMultipleDeclarators, y));
    test_uint(fields->fields[1].qualifiers, SIREFLECT_QUAL_CONST);

    sireflect_registry_fini(reg);
}

static void expect_multi_token_builtin(
    sireflect_registry_t *reg,
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align
) {
    sireflect_handle_t handle = sireflect_type_by_name(reg, name);
    test_assert(handle != SIREFLECT_INVALID_HANDLE);

    const sireflect_type_info_t *type = sireflect_type_info(reg, handle);
    test_str(type->name, name);
    test_uint(type->kind, kind);
    test_uint(type->size, size);
    test_uint(type->align, align);
    test_assert(sireflect_is_numeric(type->kind));
}

void sireflect_test_impl_multi_token_builtin_registry(void) {
    sireflect_registry_t *reg = sireflect_registry_init();

    expect_multi_token_builtin(
        reg,
        "signed char",
        sireflect_kind_signed_char,
        sizeof(signed char),
        _Alignof(signed char)
    );
    expect_multi_token_builtin(
        reg,
        "unsigned char",
        sireflect_kind_unsigned_char,
        sizeof(unsigned char),
        _Alignof(unsigned char)
    );
    expect_multi_token_builtin(
        reg,
        "unsigned short",
        sireflect_kind_unsigned_short,
        sizeof(unsigned short),
        _Alignof(unsigned short)
    );
    expect_multi_token_builtin(
        reg,
        "unsigned int",
        sireflect_kind_unsigned_int,
        sizeof(unsigned int),
        _Alignof(unsigned int)
    );
    expect_multi_token_builtin(
        reg,
        "unsigned long",
        sireflect_kind_unsigned_long,
        sizeof(unsigned long),
        _Alignof(unsigned long)
    );
    expect_multi_token_builtin(
        reg,
        "long long",
        sireflect_kind_long_long,
        sizeof(long long),
        _Alignof(long long)
    );
    expect_multi_token_builtin(
        reg,
        "unsigned long long",
        sireflect_kind_unsigned_long_long,
        sizeof(unsigned long long),
        _Alignof(unsigned long long)
    );

    test_str(sireflect_kind_name(sireflect_kind_signed_char), "signed char");
    test_str(sireflect_kind_name(sireflect_kind_unsigned_int), "unsigned int");
    test_str(sireflect_kind_name(sireflect_kind_long_long), "long long");
    test_str(
        sireflect_kind_name(sireflect_kind_unsigned_long_long),
        "unsigned long long"
    );

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_multi_token_fields(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, MultiTokenBuiltins);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    test_uint(fields->field_count, 7);

    test_str(fields->fields[0].name, "code");
    test_uint(fields->fields[0].type, sireflect_type_by_name(reg, "signed char"));
    test_uint(fields->fields[0].offset, offsetof(MultiTokenBuiltins, code));
    test_uint(fields->fields[0].size, sizeof(signed char));
    test_uint(fields->fields[0].align, _Alignof(signed char));

    test_str(fields->fields[1].name, "byte");
    test_uint(fields->fields[1].type, sireflect_type_by_name(reg, "unsigned char"));
    test_uint(fields->fields[1].offset, offsetof(MultiTokenBuiltins, byte));
    test_uint(fields->fields[1].size, sizeof(unsigned char));
    test_uint(fields->fields[1].align, _Alignof(unsigned char));

    test_str(fields->fields[2].name, "small");
    test_uint(fields->fields[2].type, sireflect_type_by_name(reg, "unsigned short"));
    test_uint(fields->fields[2].offset, offsetof(MultiTokenBuiltins, small));
    test_uint(fields->fields[2].size, sizeof(unsigned short));
    test_uint(fields->fields[2].align, _Alignof(unsigned short));

    test_str(fields->fields[3].name, "flags");
    test_uint(fields->fields[3].type, sireflect_type_by_name(reg, "unsigned int"));
    test_uint(fields->fields[3].offset, offsetof(MultiTokenBuiltins, flags));
    test_uint(fields->fields[3].size, sizeof(unsigned int));
    test_uint(fields->fields[3].align, _Alignof(unsigned int));

    test_str(fields->fields[4].name, "span");
    test_uint(fields->fields[4].type, sireflect_type_by_name(reg, "unsigned long"));
    test_uint(fields->fields[4].offset, offsetof(MultiTokenBuiltins, span));
    test_uint(fields->fields[4].size, sizeof(unsigned long));
    test_uint(fields->fields[4].align, _Alignof(unsigned long));

    test_str(fields->fields[5].name, "count");
    test_uint(fields->fields[5].type, sireflect_type_by_name(reg, "long long"));
    test_uint(fields->fields[5].offset, offsetof(MultiTokenBuiltins, count));
    test_uint(fields->fields[5].size, sizeof(long long));
    test_uint(fields->fields[5].align, _Alignof(long long));

    test_str(fields->fields[6].name, "mask");
    test_uint(fields->fields[6].type, sireflect_type_by_name(reg, "unsigned long long"));
    test_uint(fields->fields[6].offset, offsetof(MultiTokenBuiltins, mask));
    test_uint(fields->fields[6].size, sizeof(unsigned long long));
    test_uint(fields->fields[6].align, _Alignof(unsigned long long));

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_multi_token_arrays_and_pointers(void) {
    sireflect_registry_t *reg = sireflect_registry_init();
    sireflect_handle_t type = sireflect(reg, MultiTokenCombinations);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);
    sireflect_handle_t unsigned_int_type = sireflect_type_by_name(reg, "unsigned int");
    sireflect_handle_t unsigned_long_long_type =
        sireflect_type_by_name(reg, "unsigned long long");
    sireflect_handle_t signed_char_type = sireflect_type_by_name(reg, "signed char");

    test_uint(fields->field_count, 4);
    test_uint(fields->fields[0].type, unsigned_int_type);
    test_uint(fields->fields[0].offset, offsetof(MultiTokenCombinations, a));
    test_uint(fields->fields[0].qualifiers, SIREFLECT_QUAL_CONST);
    test_uint(fields->fields[1].type, unsigned_int_type);
    test_uint(fields->fields[1].offset, offsetof(MultiTokenCombinations, b));
    test_uint(fields->fields[1].qualifiers, SIREFLECT_QUAL_CONST);

    const sireflect_type_info_t *array = sireflect_type_info(reg, fields->fields[2].type);
    test_assert(sireflect_type_is_array(array));
    test_uint(fields->fields[2].offset, offsetof(MultiTokenCombinations, values));
    test_uint(fields->fields[2].size, sizeof(((MultiTokenCombinations *)0)->values));
    test_uint(array->element_type, unsigned_long_long_type);
    test_uint(array->element_count, 2);
    test_str(array->name, "unsigned long long[2]");

    const sireflect_type_info_t *pointer = sireflect_type_info(reg, fields->fields[3].type);
    test_assert(sireflect_type_is_pointer(pointer));
    test_uint(fields->fields[3].offset, offsetof(MultiTokenCombinations, code));
    test_uint(fields->fields[3].size, sizeof(((MultiTokenCombinations *)0)->code));
    test_uint(pointer->element_type, signed_char_type);
    test_str(pointer->name, "signed char*");

    sireflect_registry_fini(reg);
}

void sireflect_test_impl_unknown_type_asserts(void) {
    test_expect_abort();
    register_unknown_type();
}

void sireflect_test_impl_empty_array_asserts(void) {
    test_expect_abort();
    register_empty_array();
}

void sireflect_test_impl_zero_array_asserts(void) {
    test_expect_abort();
    register_zero_array();
}

void sireflect_test_impl_alpha_array_count_asserts(void) {
    test_expect_abort();
    register_alpha_array_count();
}

void sireflect_test_impl_missing_array_end_asserts(void) {
    test_expect_abort();
    register_missing_array_end();
}

void sireflect_test_impl_nested_empty_array_asserts(void) {
    test_expect_abort();
    register_nested_empty_array();
}

void sireflect_test_impl_missing_declarator_asserts(void) {
    test_expect_abort();
    register_missing_declarator();
}

void sireflect_test_impl_post_pointer_qualifier_asserts(void) {
    test_expect_abort();
    register_post_pointer_qualifier();
}

void sireflect_test_impl_unsupported_type_specifier_asserts(void) {
    test_expect_abort();
    register_unsupported_type_specifier();
}

void sireflect_test_impl_unknown_type_diagnostic(void) {
    expect_abort_message(
        register_unknown_type,
        "unknown field type 'Missing'",
        "struct 'Bad', field 'field'",
        "line 1, column 3"
    );
}

void sireflect_test_impl_empty_array_diagnostic(void) {
    expect_abort_message(
        register_empty_array,
        "array element count is required",
        "struct 'BadArray', field 'values'",
        "actual ']' ']'"
    );
}

void sireflect_test_impl_zero_array_diagnostic(void) {
    expect_abort_message(
        register_zero_array,
        "array element count must be greater than zero",
        "struct 'BadArray', field 'values'",
        "actual integer '0'"
    );
}

void sireflect_test_impl_alpha_array_count_diagnostic(void) {
    expect_abort_message(
        register_alpha_array_count,
        "array element count must be a positive integer literal",
        "struct 'BadArray', field 'values'",
        "actual identifier 'abc'"
    );
}

void sireflect_test_impl_missing_array_end_diagnostic(void) {
    expect_abort_message(
        register_missing_array_end,
        "expected ']' after array element count",
        "struct 'BadArray', field 'values'",
        "actual ';' ';'"
    );
}

void sireflect_test_impl_nested_empty_array_diagnostic(void) {
    expect_abort_message(
        register_nested_empty_array,
        "array element count is required",
        "struct 'BadArray', field 'values'",
        "actual ']' ']'"
    );
}

void sireflect_test_impl_missing_declarator_diagnostic(void) {
    expect_abort_message(
        register_missing_declarator,
        "unexpected token while parsing field name",
        "struct 'BadDecl'",
        "actual ';' ';'"
    );
}

void sireflect_test_impl_post_pointer_qualifier_diagnostic(void) {
    expect_abort_message(
        register_post_pointer_qualifier,
        "unexpected token while parsing field name",
        "struct 'BadPointerQualifier'",
        "actual identifier 'const'"
    );
}

void sireflect_test_impl_unsupported_type_specifier_diagnostic(void) {
    expect_abort_message(
        register_unsupported_type_specifier,
        "unsupported type specifier sequence",
        "struct 'BadTypeSpecifier'",
        "actual identifier 'signed'"
    );
}
