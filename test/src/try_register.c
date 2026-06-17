#include <test.h>

void try_register_valid_struct(void) {
    sireflect_test_impl_try_register_valid_struct();
}

void try_register_invalid_descriptor(void) {
    sireflect_test_impl_try_register_invalid_descriptor();
}

void try_register_unknown_type_returns_invalid(void) {
    sireflect_test_impl_try_register_unknown_type_returns_invalid();
}

void try_register_array_errors_return_invalid(void) {
    sireflect_test_impl_try_register_array_errors_return_invalid();
}

void try_register_declarator_errors_return_invalid(void) {
    sireflect_test_impl_try_register_declarator_errors_return_invalid();
}

void try_register_existing_incompatible_type_returns_invalid(void) {
    sireflect_test_impl_try_register_existing_incompatible_type_returns_invalid();
}

void try_register_registry_usable_after_failure(void) {
    sireflect_test_impl_try_register_registry_usable_after_failure();
}
