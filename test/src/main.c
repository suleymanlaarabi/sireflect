
/* A friendly warning from bake.test
 * ----------------------------------------------------------------------------
 * This file is generated. To add/remove testcases modify the 'project.json' of
 * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!
 * ----------------------------------------------------------------------------
 */

#include <test.h>

// Testsuite 'types'
void types_builtin_types(void);
void types_native_types(void);
void types_kind_helpers(void);
void types_type_is_struct_helper(void);
void types_type_is_struct_asserts(void);

// Testsuite 'structs'
void structs_empty_struct(void);
void structs_primitive_fields(void);
void structs_mixed_alignment(void);
void structs_field_copy(void);

// Testsuite 'pointers'
void pointers_pointer_field(void);
void pointers_pointer_compat_field(void);

// Testsuite 'arrays'
void arrays_array_field(void);
void arrays_struct_array_field(void);
void arrays_repeated_array_type(void);
void arrays_pointer_array_field(void);
void arrays_matrix_array_field(void);
void arrays_pointer_matrix_field(void);

// Testsuite 'parser_errors'
void parser_errors_unknown_type_asserts(void);
void parser_errors_empty_array_asserts(void);
void parser_errors_zero_array_asserts(void);
void parser_errors_alpha_array_count_asserts(void);
void parser_errors_missing_array_end_asserts(void);
void parser_errors_nested_empty_array_asserts(void);
void parser_errors_multi_decl_asserts(void);
void parser_errors_unknown_type_diagnostic(void);
void parser_errors_empty_array_diagnostic(void);
void parser_errors_zero_array_diagnostic(void);
void parser_errors_alpha_array_count_diagnostic(void);
void parser_errors_missing_array_end_diagnostic(void);
void parser_errors_nested_empty_array_diagnostic(void);
void parser_errors_multi_decl_diagnostic(void);

bake_test_case types_testcases[] = {
    {
        "builtin_types",
        types_builtin_types
    },
    {
        "native_types",
        types_native_types
    },
    {
        "kind_helpers",
        types_kind_helpers
    },
    {
        "type_is_struct_helper",
        types_type_is_struct_helper
    },
    {
        "type_is_struct_asserts",
        types_type_is_struct_asserts
    }
};

bake_test_case structs_testcases[] = {
    {
        "empty_struct",
        structs_empty_struct
    },
    {
        "primitive_fields",
        structs_primitive_fields
    },
    {
        "mixed_alignment",
        structs_mixed_alignment
    },
    {
        "field_copy",
        structs_field_copy
    }
};

bake_test_case pointers_testcases[] = {
    {
        "pointer_field",
        pointers_pointer_field
    },
    {
        "pointer_compat_field",
        pointers_pointer_compat_field
    }
};

bake_test_case arrays_testcases[] = {
    {
        "array_field",
        arrays_array_field
    },
    {
        "struct_array_field",
        arrays_struct_array_field
    },
    {
        "repeated_array_type",
        arrays_repeated_array_type
    },
    {
        "pointer_array_field",
        arrays_pointer_array_field
    },
    {
        "matrix_array_field",
        arrays_matrix_array_field
    },
    {
        "pointer_matrix_field",
        arrays_pointer_matrix_field
    }
};

bake_test_case parser_errors_testcases[] = {
    {
        "unknown_type_asserts",
        parser_errors_unknown_type_asserts
    },
    {
        "empty_array_asserts",
        parser_errors_empty_array_asserts
    },
    {
        "zero_array_asserts",
        parser_errors_zero_array_asserts
    },
    {
        "alpha_array_count_asserts",
        parser_errors_alpha_array_count_asserts
    },
    {
        "missing_array_end_asserts",
        parser_errors_missing_array_end_asserts
    },
    {
        "nested_empty_array_asserts",
        parser_errors_nested_empty_array_asserts
    },
    {
        "multi_decl_asserts",
        parser_errors_multi_decl_asserts
    },
    {
        "unknown_type_diagnostic",
        parser_errors_unknown_type_diagnostic
    },
    {
        "empty_array_diagnostic",
        parser_errors_empty_array_diagnostic
    },
    {
        "zero_array_diagnostic",
        parser_errors_zero_array_diagnostic
    },
    {
        "alpha_array_count_diagnostic",
        parser_errors_alpha_array_count_diagnostic
    },
    {
        "missing_array_end_diagnostic",
        parser_errors_missing_array_end_diagnostic
    },
    {
        "nested_empty_array_diagnostic",
        parser_errors_nested_empty_array_diagnostic
    },
    {
        "multi_decl_diagnostic",
        parser_errors_multi_decl_diagnostic
    }
};


static bake_test_suite suites[] = {
    {
        "types",
        NULL,
        NULL,
        5,
        types_testcases
    },
    {
        "structs",
        NULL,
        NULL,
        4,
        structs_testcases
    },
    {
        "pointers",
        NULL,
        NULL,
        2,
        pointers_testcases
    },
    {
        "arrays",
        NULL,
        NULL,
        6,
        arrays_testcases
    },
    {
        "parser_errors",
        NULL,
        NULL,
        14,
        parser_errors_testcases
    }
};

int main(int argc, char *argv[]) {
    return bake_test_run("sireflect.test", argc, argv, suites, 5);
}
