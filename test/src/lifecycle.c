#include <test.h>

void lifecycle_nested_init_fini(void) {
    sireflect_test_impl_nested_init_fini();
}

void lifecycle_fini_without_init_asserts(void) {
    sireflect_test_impl_fini_without_init_asserts();
}

void lifecycle_try_register_before_init(void) {
    sireflect_test_impl_try_register_before_init();
}
