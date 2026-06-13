
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'simple'
void simple_builtin_types(void);
void simple_empty_struct(void);
void simple_primitive_fields(void);
void simple_mixed_alignment(void);
void simple_pointer_field(void);
void simple_native_types(void);
void simple_field_copy(void);
void simple_kind_helpers(void);
void simple_type_is_struct_helper(void);
void simple_type_is_struct_asserts(void);
void simple_unknown_type_asserts(void);
void simple_array_syntax_asserts(void);
void simple_multi_decl_asserts(void);
void simple_unknown_type_diagnostic(void);
void simple_array_syntax_diagnostic(void);
void simple_multi_decl_diagnostic(void);

bake_test_case simple_testcases[] = {
    {
        "builtin_types",
        simple_builtin_types
    },
    {
        "empty_struct",
        simple_empty_struct
    },
    {
        "primitive_fields",
        simple_primitive_fields
    },
    {
        "mixed_alignment",
        simple_mixed_alignment
    },
    {
        "pointer_field",
        simple_pointer_field
    },
    {
        "native_types",
        simple_native_types
    },
    {
        "field_copy",
        simple_field_copy
    },
    {
        "kind_helpers",
        simple_kind_helpers
    },
    {
        "type_is_struct_helper",
        simple_type_is_struct_helper
    },
    {
        "type_is_struct_asserts",
        simple_type_is_struct_asserts
    },
    {
        "unknown_type_asserts",
        simple_unknown_type_asserts
    },
    {
        "array_syntax_asserts",
        simple_array_syntax_asserts
    },
    {
        "multi_decl_asserts",
        simple_multi_decl_asserts
    },
    {
        "unknown_type_diagnostic",
        simple_unknown_type_diagnostic
    },
    {
        "array_syntax_diagnostic",
        simple_array_syntax_diagnostic
    },
    {
        "multi_decl_diagnostic",
        simple_multi_decl_diagnostic
    }
};


static bake_test_suite suites[] = {
    {
        "simple",
        NULL,
        NULL,
        16,
        simple_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("sireflect.test", argc, argv, suites, 1);
}
