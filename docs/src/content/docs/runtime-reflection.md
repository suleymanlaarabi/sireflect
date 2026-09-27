---
title: Runtime Reflection
description: Walk types and values, classify types, and attach semantic metadata.
---

## Walk a type graph

`sireflect_walk_type(root, flags, visitor, user)` calls the visitor before each
type's children. Struct fields and array element types are followed by default.
Pointer targets are followed only with `SIREFLECT_WALK_FOLLOW_POINTERS`.
Function pointer return types are included in the type graph. Enums and scalar
types are leaves.

The visit contains its `type`, borrowed `info`, parent handle, relation, depth,
and the field for a `SIREFLECT_WALK_FIELD` relation. By default, shared types are
reported at each occurrence. Add `SIREFLECT_WALK_DEDUPLICATE` to report each
handle once. Active cycles are cut even without deduplication. A visitor that
returns `false` stops immediately; the walk returns `false` with no error.

```c
static bool show_type(const sireflect_type_visit_t *visit, void *user) {
    (void)user;
    printf("%zu: %s\n", visit->depth, visit->info->name);
    return true;
}

sireflect_walk_type(type, SIREFLECT_WALK_FOLLOW_POINTERS, show_type, NULL);
```

The maximum nesting depth is 256. Exceeding it returns `false` and sets
`sireflect_error()`. Invalid handles, flags, or visitors also return a
recoverable error.

## Walk an object

`sireflect_walk_value` accepts a mutable object and visitor;
`sireflect_walk_const_value` accepts a const object and a distinct const visitor.
Both use the same traversal engine as the type walk. The value events are:

| Event | Meaning |
| --- | --- |
| `SIREFLECT_VALUE_ENTER_STRUCT` / `SIREFLECT_VALUE_LEAVE_STRUCT` | Surround a struct's fields. |
| `SIREFLECT_VALUE_FIELD` | Direct pointer to the field, before its own visit. |
| `SIREFLECT_VALUE_ENTER_ARRAY` / `SIREFLECT_VALUE_LEAVE_ARRAY` | Surround array elements. |
| `SIREFLECT_VALUE_ARRAY_ELEMENT` | Direct pointer to an element and its index. |
| `SIREFLECT_VALUE_LEAF` | Scalar or enum value. |
| `SIREFLECT_VALUE_POINTER` | Pointer storage, including NULL and function pointers; never dereferenced. |

The root has depth zero. A field or element has its parent depth plus one.
Pass zero flags for value walks. A NULL object is rejected. The caller remains
responsible for providing an object of the reflected C type and for respecting
`const` and `volatile` field qualifiers when writing.

## Classify and index

`sireflect_type_category` returns a stable category for valid type handles,
or `sireflect_category_invalid` for an invalid handle. Helpers include
`sireflect_type_is_scalar`, `sireflect_type_is_numeric_handle`,
`sireflect_type_is_integral`, `sireflect_type_is_floating`,
`sireflect_type_is_cstring`, and `sireflect_type_is_function_pointer`.
A C string is exactly a typed pointer to `char`; `char[N]` and raw `ptr` are
not C strings. Leading field qualifiers do not create separate type handles.

`sireflect_array_element_ptr` and `sireflect_array_element_mut_ptr` return an
element address from registered array metadata. They return NULL for a bad
handle, NULL array, or out-of-range index. For a matrix, first get a row from
the outer array, then an element from the row's array type.
`sireflect_enum_value_valid` tests whether an integer appears in an enum.

## Attach metadata

`sireflect_type_set_meta` and `sireflect_field_set_meta` store string, bool,
signed integer, unsigned integer, or double values. The library copies keys
and string values. It does not interpret any key. Read one value with
`sireflect_type_meta` or `sireflect_field_meta`, or iterate the borrowed
`sireflect_metas_t` view from `sireflect_type_metas` or
`sireflect_field_metas`.

```c
sireflect_meta_t hint = {
    .key = "hint",
    .kind = SIREFLECT_META_STRING,
    .value.string = "color",
};
sireflect_field_set_meta(type, "tint", &hint);
```

Replacing a key updates the same metadata object; the previous string value
is freed. Individual metadata pointers last until the final `sireflect_fini`,
while a view's `items` array can move when another key is added. Setters return
`false` and set `sireflect_error()` for invalid input or allocation failure.
No metadata pointers remain valid after the final release.
