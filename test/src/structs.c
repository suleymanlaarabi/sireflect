#include <test.h>

void structs_empty_struct(void) {
    sireflect_test_impl_empty_struct();
}

void structs_primitive_fields(void) {
    sireflect_test_impl_primitive_fields();
}

void structs_mixed_alignment(void) {
    sireflect_test_impl_mixed_alignment();
}

void structs_field_copy(void) {
    sireflect_test_impl_field_copy();
}
