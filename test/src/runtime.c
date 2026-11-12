#include <test.h>
#include "sireflect_registry.h"

#include <stdio.h>
#include <string.h>

SIREFLECT_STRUCT(RuntimePoint, { int x; int y; });
SIREFLECT_STRUCT(RuntimePair, { RuntimePoint left; RuntimePoint right; });
SIREFLECT_STRUCT(RuntimeGrid, { RuntimePoint points[2]; int matrix[2][3]; });
SIREFLECT_STRUCT(RuntimePointers, { RuntimePoint *items[2]; char *name; const char *label; ptr raw; int (*callback)(); });
SIREFLECT_STRUCT(RuntimeNode, { RuntimeNode *next; int value; });
SIREFLECT_ENUM(RuntimeColor, { RuntimeRed = 1, RuntimeBlue = 4 });
SIREFLECT_STRUCT(RuntimeEnumHolder, { RuntimeColor color; });

static bool vector_leaf(const sireflect_const_value_visit_t *visit, void *user) {
    if (visit->event == SIREFLECT_VALUE_LEAF) {
        int *total = user;
        *total += *(const int *)visit->ptr;
    }
    return true;
}

void runtime_vector_types_and_walk(void) {
    sireflect_init();
    test_uint(sireflect_vector_type(0), 0);
    sireflect_handle_t element = sireflect_type_by_name("int");
    sireflect_handle_t vector = sireflect_vector_type(element);
    test_assert(vector != 0);
    test_uint(sireflect_vector_type(element), vector);
    test_int(sireflect_type_category(vector), sireflect_category_vector);
    test_str(sireflect_kind_name(sireflect_type_info(vector)->kind), "vector");
    test_assert(!sireflect_is_numeric(sireflect_kind_vector));
    int values[] = { 2, 7, 11 };
    sireflect_vector_t data = { values, 3 };
    int total = 0;
    test_assert(sireflect_walk_const_value(vector, &data, 0, vector_leaf, &total));
    test_int(total, 20);
    data.count = 0;
    test_assert(sireflect_walk_const_value(vector, &data, 0, vector_leaf, &total));
    test_int(total, 20);
    sireflect_transaction_t *transaction = sireflect_transaction_begin();
    sireflect_handle_t nested = sireflect_vector_type(vector);
    char name[256];
    snprintf(name, sizeof name, "%s", sireflect_type_name(nested));
    sireflect_transaction_abort(transaction);
    test_uint(sireflect_type_by_name(name), 0);
    test_uint(sireflect_vector_type(element), vector);
    sireflect_fini();
}

typedef struct {
    sireflect_handle_t sought;
    size_t count;
    size_t total;
    size_t max_depth;
    bool saw_field;
    bool saw_array;
    bool saw_pointer_target;
    bool saw_function_return;
} type_counts_t;

static bool count_type(const sireflect_type_visit_t *visit, void *user) {
    type_counts_t *counts = user;
    counts->total++;
    if (visit->type == counts->sought) counts->count++;
    if (visit->depth > counts->max_depth) counts->max_depth = visit->depth;
    if (visit->relation == SIREFLECT_WALK_FIELD) {
        counts->saw_field = visit->field != NULL;
        test_assert(visit->parent_type != SIREFLECT_INVALID_HANDLE);
    }
    if (visit->relation == SIREFLECT_WALK_ARRAY_ELEMENT) counts->saw_array = true;
    if (visit->relation == SIREFLECT_WALK_POINTER_TARGET) counts->saw_pointer_target = true;
    if (visit->relation == SIREFLECT_WALK_FUNCTION_RETURN) counts->saw_function_return = true;
    return true;
}

static bool stop_type(const sireflect_type_visit_t *visit, void *user) {
    size_t *count = user;
    (void)visit;
    (*count)++;
    return false;
}

void runtime_type_walk_shapes(void) {
    sireflect_init();
    sireflect_handle_t point = sireflect(RuntimePoint);
    sireflect_handle_t grid = sireflect(RuntimeGrid);
    sireflect_handle_t pointers = sireflect(RuntimePointers);
    sireflect_handle_t color = sireflect(RuntimeColor);
    sireflect_handle_t holder = sireflect(RuntimeEnumHolder);
    type_counts_t counts = { .sought = point };
    test_assert(sireflect_walk_type(grid, 0, count_type, &counts));
    test_assert(counts.count == 1 && counts.saw_array && counts.saw_field);
    test_assert(counts.max_depth >= 3);
    counts = (type_counts_t){ .sought = point };
    test_assert(sireflect_walk_type(pointers, 0, count_type, &counts));
    test_assert(counts.count == 0 && !counts.saw_pointer_target && counts.saw_array);
    test_assert(counts.saw_function_return);
    counts = (type_counts_t){ .sought = color };
    test_assert(sireflect_walk_type(holder, 0, count_type, &counts));
    test_assert(counts.count == 1);
    sireflect_fini();
}

void runtime_type_walk_cycles_and_dedup(void) {
    sireflect_init();
    sireflect_handle_t point = sireflect(RuntimePoint);
    sireflect_handle_t pair = sireflect(RuntimePair);
    sireflect_handle_t node = sireflect(RuntimeNode);
    type_counts_t counts = { .sought = point };
    test_assert(sireflect_walk_type(pair, 0, count_type, &counts));
    test_assert(counts.count == 2);
    counts = (type_counts_t){ .sought = point };
    test_assert(sireflect_walk_type(pair, SIREFLECT_WALK_DEDUPLICATE, count_type, &counts));
    test_assert(counts.count == 1);
    counts = (type_counts_t){ .sought = node };
    test_assert(sireflect_walk_type(node, SIREFLECT_WALK_FOLLOW_POINTERS, count_type, &counts));
    test_assert(counts.count == 1 && counts.total < 10);
    counts = (type_counts_t){ .sought = node };
    test_assert(sireflect_walk_type(node,
        SIREFLECT_WALK_FOLLOW_POINTERS | SIREFLECT_WALK_DEDUPLICATE, count_type, &counts));
    test_assert(counts.count == 1);
    counts = (type_counts_t){ .sought = point };
    test_assert(sireflect_walk_type(pair, SIREFLECT_WALK_FOLLOW_POINTERS, count_type, &counts));
    test_assert(counts.count == 2);
    sireflect_fini();
}

void runtime_type_walk_stop_and_errors(void) {
    sireflect_init();
    sireflect_handle_t point = sireflect(RuntimePoint);
    size_t count = 0;
    test_assert(!sireflect_walk_type(point, 0, stop_type, &count));
    test_assert(count == 1 && sireflect_error() == NULL);
    test_assert(!sireflect_walk_type(0, 0, stop_type, &count));
    test_assert(sireflect_error() != NULL);
    test_assert(!sireflect_walk_type(point, 8, stop_type, &count));
    test_assert(sireflect_error() != NULL);
    sireflect_fini();
}

typedef struct {
    RuntimeGrid *grid;
    size_t fields;
    size_t elements;
    size_t leaves;
} value_counts_t;

static bool mutate_value(const sireflect_value_visit_t *visit, void *user) {
    value_counts_t *counts = user;
    if (visit->event == SIREFLECT_VALUE_FIELD) {
        counts->fields++;
        if (visit->field && strcmp(visit->field->name, "points") == 0)
            test_assert(visit->ptr == &counts->grid->points);
        if (visit->field && strcmp(visit->field->name, "matrix") == 0)
            test_assert(visit->ptr == &counts->grid->matrix);
    }
    if (visit->event == SIREFLECT_VALUE_ARRAY_ELEMENT) counts->elements++;
    if (visit->event == SIREFLECT_VALUE_LEAF) {
        counts->leaves++;
        if (visit->ptr == &counts->grid->points[1].x) *(int *)visit->ptr = 42;
        if (visit->ptr == &counts->grid->matrix[1][2]) *(int *)visit->ptr = 99;
    }
    return true;
}

void runtime_value_walk_mutable(void) {
    sireflect_init();
    sireflect_handle_t point = sireflect(RuntimePoint);
    sireflect_handle_t grid = sireflect(RuntimeGrid);
    (void)point;
    RuntimeGrid object = {0};
    value_counts_t counts = { .grid = &object };
    test_assert(sireflect_walk_value(grid, &object, 0, mutate_value, &counts));
    test_assert(object.points[1].x == 42 && object.matrix[1][2] == 99);
    test_assert(counts.fields >= 6 && counts.elements == 10 && counts.leaves == 10);
    test_assert(!sireflect_walk_value(grid, NULL, 0, mutate_value, &counts));
    test_assert(sireflect_error() != NULL);
    sireflect_fini();
}

typedef struct { const RuntimePointers *object; size_t pointers; size_t leaves; } const_counts_t;
static bool read_const(const sireflect_const_value_visit_t *visit, void *user) {
    const_counts_t *counts = user;
    if (visit->event == SIREFLECT_VALUE_POINTER) {
        counts->pointers++;
        if (visit->field && strcmp(visit->field->name, "name") == 0)
            test_assert(visit->ptr == &counts->object->name);
    }
    if (visit->event == SIREFLECT_VALUE_LEAF) counts->leaves++;
    return true;
}
static bool stop_const(const sireflect_const_value_visit_t *visit, void *user) {
    (void)visit;
    ++*(size_t *)user;
    return false;
}

void runtime_value_walk_const_and_pointers(void) {
    sireflect_init();
    sireflect(RuntimePoint);
    sireflect_handle_t pointers = sireflect(RuntimePointers);
    sireflect(RuntimeColor);
    sireflect_handle_t holder = sireflect(RuntimeEnumHolder);
    RuntimePoint point = {1, 2};
    const RuntimePointers object = { .items = { NULL, &point }, .name = NULL,
        .label = "hi", .raw = &point, .callback = NULL };
    const_counts_t counts = { .object = &object };
    test_assert(sireflect_walk_const_value(pointers, &object, 0, read_const, &counts));
    test_assert(counts.pointers == 6 && counts.leaves == 0);
    RuntimeEnumHolder enum_object = { .color = RuntimeBlue };
    counts = (const_counts_t){0};
    test_assert(sireflect_walk_const_value(holder, &enum_object, 0, read_const, &counts));
    test_assert(counts.leaves == 1);
    size_t stopped = 0;
    test_assert(!sireflect_walk_const_value(holder, &enum_object, 0, stop_const, &stopped));
    test_assert(stopped == 1 && sireflect_error() == NULL);
    sireflect_fini();
}

void runtime_categories_and_arrays(void) {
    sireflect_init();
    sireflect_handle_t point = sireflect(RuntimePoint);
    sireflect_handle_t grid = sireflect(RuntimeGrid);
    sireflect_handle_t pointers = sireflect(RuntimePointers);
    sireflect_handle_t color = sireflect(RuntimeColor);
    test_assert(sireflect_type_category(point) == sireflect_category_struct);
    test_assert(sireflect_type_category(color) == sireflect_category_enum);
    test_assert(sireflect_type_is_scalar(color));
    test_assert(sireflect_enum_value_valid(color, 4) && !sireflect_enum_value_valid(color, 3));
    test_assert(sireflect_type_is_integral(sireflect_type_by_name("int")));
    test_assert(sireflect_type_is_floating(sireflect_type_by_name("float")));
    test_assert(sireflect_type_is_numeric_handle(sireflect_type_by_name("u8")));
    test_assert(sireflect_type_category(sireflect_type_by_name("bool")) == sireflect_category_boolean);
    test_assert(sireflect_type_category(0) == sireflect_category_invalid);
    const char *integer_names[] = { "u8", "u16", "u32", "u64", "i8", "i16",
        "i32", "i64", "char", "short", "int", "long", "signed char",
        "unsigned char", "unsigned short", "unsigned int", "unsigned long",
        "long long", "unsigned long long" };
    for (size_t i = 0; i < sizeof(integer_names) / sizeof(integer_names[0]); i++) {
        test_assert(sireflect_type_category(sireflect_type_by_name(integer_names[i])) ==
            sireflect_category_integer);
    }
    const char *floating_names[] = { "f32", "f64", "float", "double" };
    for (size_t i = 0; i < sizeof(floating_names) / sizeof(floating_names[0]); i++) {
        test_assert(sireflect_type_category(sireflect_type_by_name(floating_names[i])) ==
            sireflect_category_floating);
    }
    sireflect_handle_t name = sireflect_field_type(pointers, "name");
    sireflect_handle_t label = sireflect_field_type(pointers, "label");
    test_assert(name == label && sireflect_type_is_cstring(name));
    test_assert(!sireflect_type_is_cstring(sireflect_field_type(pointers, "raw")));
    test_assert(sireflect_type_is_function_pointer(sireflect_field_type(pointers, "callback")));
    RuntimeGrid object = {0};
    sireflect_handle_t points = sireflect_field_type(grid, "points");
    sireflect_handle_t matrix = sireflect_field_type(grid, "matrix");
    test_assert(sireflect_array_element_ptr(points, object.points, 1) == &object.points[1]);
    test_assert(sireflect_array_element_mut_ptr(points, object.points, 0) == &object.points[0]);
    sireflect_handle_t row = sireflect_type_element(matrix);
    const void *row_ptr = sireflect_array_element_ptr(matrix, object.matrix, 1);
    test_assert(row_ptr == &object.matrix[1]);
    test_assert(sireflect_array_element_ptr(row, row_ptr, 2) == &object.matrix[1][2]);
    test_assert(sireflect_array_element_ptr(points, object.points, 2) == NULL);
    test_assert(sireflect_array_element_ptr(point, object.points, 0) == NULL);
    test_assert(sireflect_array_element_ptr(points, NULL, 0) == NULL);
    RuntimePointers pointer_object = {0};
    sireflect_handle_t items = sireflect_field_type(pointers, "items");
    test_assert(sireflect_array_element_ptr(items, pointer_object.items, 1) ==
        &pointer_object.items[1]);
    sireflect_fini();
}

void runtime_metadata_ownership_and_errors(void) {
    sireflect_init();
    sireflect_handle_t point = sireflect(RuntimePoint);
    char key[] = "hint";
    char value[] = "color";
    sireflect_meta_t meta = { .key = key, .kind = SIREFLECT_META_STRING, .value.string = value };
    test_assert(sireflect_type_set_meta(point, &meta));
    key[0] = 'x'; value[0] = 'x';
    const sireflect_meta_t *stored = sireflect_type_meta(point, "hint");
    test_assert(stored && strcmp(stored->value.string, "color") == 0);
    test_assert(sireflect_type_metas(point)->count == 1);
    meta = (sireflect_meta_t){ .key = "hint", .kind = SIREFLECT_META_BOOL, .value.boolean = true };
    test_assert(sireflect_type_set_meta(point, &meta));
    test_assert(sireflect_type_meta(point, "hint") == stored);
    test_assert(stored->kind == SIREFLECT_META_BOOL && stored->value.boolean);
    meta = (sireflect_meta_t){ .key = "min", .kind = SIREFLECT_META_I64, .value.i64 = -3 };
    test_assert(sireflect_field_set_meta(point, "x", &meta));
    meta = (sireflect_meta_t){ .key = "max", .kind = SIREFLECT_META_U64, .value.u64 = 9 };
    test_assert(sireflect_field_set_meta(point, "x", &meta));
    meta = (sireflect_meta_t){ .key = "step", .kind = SIREFLECT_META_F64, .value.f64 = 0.5 };
    test_assert(sireflect_field_set_meta(point, "x", &meta));
    test_assert(sireflect_field_metas(point, "x")->count == 3);
    test_assert(sireflect_field_meta(point, "x", "min")->value.i64 == -3);
    test_assert(sireflect_field_meta(point, "x", "max")->value.u64 == 9);
    test_assert(sireflect_field_meta(point, "x", "step")->value.f64 == 0.5);
    test_assert(sireflect_type_meta(point, "absent") == NULL);
    test_assert(!sireflect_field_set_meta(point, "absent", &meta) && sireflect_error());
    test_assert(!sireflect_type_set_meta(0, &meta) && sireflect_error());
    meta.key = "";
    test_assert(!sireflect_type_set_meta(point, &meta) && sireflect_error());
    meta.key = "bad"; meta.kind = (sireflect_meta_kind_t)99;
    test_assert(!sireflect_type_set_meta(point, &meta) && sireflect_error());
    sireflect_fini();
}

void runtime_stable_handles_and_pointers(void) {
    sireflect_init();
    sireflect_handle_t point = sireflect(RuntimePoint);
    const sireflect_type_info_t *info = sireflect_type_info(point);
    for (int i = 0; i < 100; i++) {
        char name[32];
        snprintf(name, sizeof(name), "RuntimeDynamic%d", i);
        test_assert(sireflect_try_register_dynamic_struct(name, "{ int x; }") != 0);
    }
    test_assert(sireflect_type_info(point) == info);
    test_assert(strcmp(info->name, "RuntimePoint") == 0);
    sireflect_fini();
    sireflect_init();
    test_assert(sireflect_type_category(point) == sireflect_category_invalid);
    sireflect_fini();
}

void runtime_recursive_registration_rollback(void) {
    sireflect_init();
    sireflect_handle_t first = sireflect_type_by_name("int");
    test_assert(sireflect_try_register_dynamic_struct("RuntimeBad",
        "{ RuntimeBad *next; Missing field; }") == 0);
    test_assert(sireflect_error() != NULL);
    test_assert(sireflect_type_by_name("RuntimeBad") == 0);
    test_assert(sireflect_type_by_name("RuntimeBad*") == 0);
    test_assert(sireflect_type_by_name("int") == first);
    sireflect_handle_t node = sireflect_try_register_dynamic_struct("RuntimeBad",
        "{ RuntimeBad *next; int value; }");
    test_assert(node != 0);
    sireflect_handle_t pointer = sireflect_field_type(node, "next");
    test_assert(sireflect_type_pointee(pointer) == node);
    sireflect_fini();
}

void runtime_depth_limit(void) {
    sireflect_init();
    sireflect_handle_t type = sireflect_type_by_name("int");
    for (size_t i = 0; i < 258; i++) {
        type = sireflect_registry_get_or_add_pointer_type(type);
    }
    type_counts_t counts = {0};
    test_assert(!sireflect_walk_type(type, SIREFLECT_WALK_FOLLOW_POINTERS,
        count_type, &counts));
    test_assert(sireflect_error() != NULL);
    test_assert(counts.total == 257);
    sireflect_fini();
}

void runtime_registration_transactions(void) {
    sireflect_init();
    sireflect_handle_t integer = sireflect_type_by_name("int");
    const sireflect_type_info_t *before = sireflect_type_info(integer);
    sireflect_transaction_t *transaction = sireflect_transaction_begin();
    test_true(transaction != NULL);
    test_true(sireflect_transaction_begin() == NULL);
    sireflect_handle_t pending = sireflect_try_register_dynamic_struct("PendingPoint", "{ int x; int y; }");
    test_true(pending != SIREFLECT_INVALID_HANDLE);
    test_uint(sireflect_type_by_name("int"), integer);
    test_true(sireflect_type_info(integer) == before);
    test_uint(sireflect_type_info(pending)->fields.field_count, 2);
    test_true(sireflect_try_register_dynamic_struct("InvalidPoint", "{ Missing x; }") == SIREFLECT_INVALID_HANDLE);
    test_uint(sireflect_type_by_name("PendingPoint"), pending);
    sireflect_transaction_abort(transaction);
    test_uint(sireflect_type_by_name("PendingPoint"), SIREFLECT_INVALID_HANDLE);
    transaction = sireflect_transaction_begin();
    pending = sireflect_try_register_dynamic_struct("CommittedPoint", "{ int x; }");
    const sireflect_type_info_t *metadata = sireflect_type_info(pending);
    test_true(sireflect_transaction_commit(transaction));
    test_uint(sireflect_type_by_name("CommittedPoint"), pending);
    test_true(sireflect_type_info(pending) == metadata);
    test_true(sireflect_type_info(integer) == before);
    sireflect_fini();
}

void runtime_registration_savepoints(void) {
    sireflect_init();
    sireflect_transaction_t *transaction = sireflect_transaction_begin();
    test_true(transaction != NULL);
    sireflect_handle_t retained = sireflect_try_register_dynamic_struct("RetainedPoint", "{ int x; }");
    test_true(retained != SIREFLECT_INVALID_HANDLE);
    const sireflect_type_info_t *metadata = sireflect_type_info(retained);
    uint32_t mark = sireflect_transaction_mark(transaction);
    sireflect_handle_t temporary = sireflect_try_register_dynamic_struct("TemporaryPoint", "{ RetainedPoint value; }");
    test_true(temporary != SIREFLECT_INVALID_HANDLE);
    test_false(sireflect_transaction_rewind(transaction, sireflect_transaction_mark(transaction) + 1));
    test_true(sireflect_transaction_rewind(transaction, mark));
    test_uint(sireflect_type_by_name("TemporaryPoint"), SIREFLECT_INVALID_HANDLE);
    test_uint(sireflect_type_by_name("RetainedPoint"), retained);
    test_true(sireflect_type_info(retained) == metadata);
    temporary = sireflect_try_register_dynamic_struct("ReplacementPoint", "{ int y; }");
    test_true(temporary != SIREFLECT_INVALID_HANDLE);
    test_true(sireflect_transaction_commit(transaction));
    test_uint(sireflect_type_by_name("ReplacementPoint"), temporary);
    test_true(sireflect_type_info(retained) == metadata);
    test_uint(sireflect_type_by_name("TemporaryPoint"), SIREFLECT_INVALID_HANDLE);
    sireflect_fini();
}

void runtime_transactional_schema_replacement(void) {
    sireflect_init();
    sireflect_handle_t old = sireflect_try_register_dynamic_struct("Changing", "{ int x; }");
    sireflect_handle_t holder = sireflect_try_register_dynamic_struct("Holder", "{ Changing items[2]; Changing *pointer; }");
    sireflect_handle_t array = sireflect_type_fields(holder)->fields[0].type;
    sireflect_handle_t pointer = sireflect_type_fields(holder)->fields[1].type;
    const sireflect_type_info_t *retained = sireflect_type_info(old);
    test_uint(sireflect_try_replace_dynamic_struct("Changing", "{}"), 0);
    sireflect_transaction_t *transaction = sireflect_transaction_begin();
    uint32_t mark = sireflect_transaction_mark(transaction);
    test_uint(sireflect_try_replace_dynamic_struct("Changing", "{ Missing x; }"), 0);
    test_uint(sireflect_type_by_name("Changing"), old);
    sireflect_handle_t replacement = sireflect_try_replace_dynamic_struct("Changing", "{ double x; double y; }");
    test_true(replacement != 0 && replacement != old);
    test_uint(sireflect_type_by_name("Changing"), replacement);
    test_uint(sireflect_type_size(old), sizeof(int));
    test_true(sireflect_type_info(old) == retained);
    test_true(sireflect_transaction_rewind(transaction, mark));
    test_uint(sireflect_type_by_name("Changing"), old);
    replacement = sireflect_try_replace_dynamic_struct("Changing", "{ double x; double y; }");
    sireflect_handle_t updated = sireflect_try_replace_dynamic_struct("Holder", "{ Changing items[2]; Changing *pointer; }");
    const sireflect_fields_t *fields = sireflect_type_fields(updated);
    test_true(fields->fields[0].type != array);
    test_true(fields->fields[1].type != pointer);
    test_uint(sireflect_type_info(fields->fields[0].type)->element_type, replacement);
    test_uint(sireflect_type_info(fields->fields[1].type)->element_type, replacement);
    test_uint(sireflect_type_size(fields->fields[0].type), 4 * sizeof(double));
    test_true(sireflect_transaction_commit(transaction));
    test_uint(sireflect_type_by_name("Changing"), replacement);
    test_uint(sireflect_type_info(array)->element_type, old);
    test_uint(sireflect_type_info(pointer)->element_type, old);
    test_uint(sireflect_type_by_name("Holder"), updated);
    transaction = sireflect_transaction_begin();
    test_true(sireflect_try_replace_dynamic_struct("Changing", "{}") != 0);
    sireflect_transaction_abort(transaction);
    test_uint(sireflect_type_by_name("Changing"), replacement);
    test_uint(sireflect_type_by_name("Holder"), updated);
    sireflect_fini();
}

void runtime_qualified_type_names(void) {
    sireflect_init();
    sireflect_handle_t nested = sireflect_try_register_dynamic_struct("Space::Nested", "{ int value; }");
    test_true(nested != 0);
    sireflect_handle_t container = sireflect_try_register_dynamic_struct("Space::Container",
        "{ Space::Nested value; Space::Nested array[2]; Space::Nested *pointer; }");
    test_true(container != 0);
    const sireflect_fields_t *fields = sireflect_type_fields(container);
    test_uint(fields->fields[0].type, nested);
    test_uint(sireflect_type_info(fields->fields[1].type)->element_type, nested);
    test_uint(sireflect_type_info(fields->fields[2].type)->element_type, nested);
    test_uint(sireflect_try_register_dynamic_struct("Invalid", "{ int a::b; }"), 0);
    test_uint(sireflect_try_register_dynamic_struct("Invalid", "{ Space::Missing x; }"), 0);
    sireflect_fini();
}
