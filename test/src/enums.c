#include <sireflect_test.h>

SIREFLECT_ENUM(Color, {
    COLOR_RED,
    COLOR_GREEN = 5,
    COLOR_BLUE,
    COLOR_NEGATIVE = -5,
    COLOR_AFTER_NEGATIVE,
});

SIREFLECT_STRUCT(WithColor, {
    Color color;
    Color *selected;
    Color palette[2];
});

void enums_values_and_queries(void) {
    sireflect_init();
    sireflect_handle_t type = sireflect(Color);
    const sireflect_type_info_t *info = sireflect_type_info(type);
    const sireflect_enum_values_t *values = sireflect_type_enum_values(type);

    test_uint(info->kind, sireflect_kind_enum);
    test_uint(info->size, sizeof(Color));
    test_uint(info->align, _Alignof(Color));
    test_assert(sireflect_type_is_enum(info));
    test_assert(!sireflect_type_is_struct(info));
    test_str(sireflect_kind_name(sireflect_kind_enum), "enum");
    test_uint(values->value_count, 5);
    test_str(values->values[0].name, "COLOR_RED");
    test_int(values->values[0].value, 0);
    test_int(values->values[1].value, 5);
    test_int(values->values[2].value, 6);
    test_int(values->values[3].value, -5);
    test_int(values->values[4].value, -4);
    test_str(sireflect_enum_value_by_name(type, "COLOR_GREEN")->name, "COLOR_GREEN");
    test_int(sireflect_enum_value_by_value(type, -5)->value, -5);
    test_assert(sireflect_enum_value_by_name(type, "MISSING") == NULL);
    test_assert(sireflect_enum_value_by_value(type, 99) == NULL);
    sireflect_fini();
}

void enums_struct_fields(void) {
    sireflect_init();
    sireflect_handle_t color = sireflect(Color);
    sireflect_handle_t type = sireflect(WithColor);
    const sireflect_fields_t *fields = sireflect_type_fields(type);
    const sireflect_type_info_t *palette = sireflect_type_info(fields->fields[2].type);

    test_uint(fields->fields[0].type, color);
    test_uint(sireflect_type_info(fields->fields[1].type)->element_type, color);
    test_uint(palette->kind, sireflect_kind_array);
    test_uint(palette->element_type, color);
    test_uint(palette->element_count, 2);
    sireflect_fini();
}

void enums_duplicate_enum_registration(void) {
    sireflect_init();
    sireflect_handle_t first = sireflect(Color);
    test_uint(sireflect(Color), first);
    test_uint(sireflect_try_register_enum(&sireflect_desc(Color)), first);
    sireflect_fini();
}
