---
title: Type Metadata
description: Working with reflected type handles and type information.
---

Types are represented by `sireflect_handle_t`. A handle is an integer id owned by
a registry. Valid handles start at `1`; `SIREFLECT_INVALID_HANDLE` is `0`.

Handles are only meaningful with the registry that created them.

## Look up a type

```c
sireflect_handle_t f32_type = sireflect_type_by_name(reg, "f32");

if (f32_type == SIREFLECT_INVALID_HANDLE) {
    /* Type is not registered. */
}
```

Built-in types are registered by `sireflect_registry_init`. Struct types are
registered by calling `sireflect(reg, TypeName)` or `sireflect_register_struct`.

## Read type information

```c
const sireflect_type_info_t *info = sireflect_type_info(reg, type);

printf("%s size=%zu align=%zu\n", info->name, info->size, info->align);
```

`sireflect_type_info_t` contains:

| Member | Meaning |
| --- | --- |
| `name` | Reflected type name. |
| `kind` | Built-in kind, `sireflect_kind_struct`, `sireflect_kind_array`, or `sireflect_kind_pointer`. |
| `size` | Size in bytes. |
| `align` | Alignment in bytes. |
| `fields` | Field list for struct types, empty for non-struct types. |
| `element_type` | Element type handle for array types, pointee type handle for typed pointer types, otherwise `SIREFLECT_INVALID_HANDLE`. |
| `element_count` | Element count for array types, otherwise `0`. Pointer types also use `0`. |

Convenience functions are also available:

```c
const char *name = sireflect_type_name(reg, type);
size_t size = sireflect_type_size(reg, type);
const sireflect_fields_t *fields = sireflect_type_fields(reg, type);
bool is_struct = sireflect_type_is_struct(info);
bool is_array = sireflect_type_is_array(info);
bool is_pointer = sireflect_type_is_pointer(info);
```

## Type kinds

`sireflect_kind_t` describes what a reflected type is:

```c
sireflect_kind_u8
sireflect_kind_u16
sireflect_kind_u32
sireflect_kind_u64
sireflect_kind_i8
sireflect_kind_i16
sireflect_kind_i32
sireflect_kind_i64
sireflect_kind_f32
sireflect_kind_f64
sireflect_kind_bool
sireflect_kind_char
sireflect_kind_short
sireflect_kind_int
sireflect_kind_long
sireflect_kind_ptr
sireflect_kind_struct
sireflect_kind_array
sireflect_kind_pointer
```

For custom structs, `kind` is always `sireflect_kind_struct`.
For fixed-size arrays, `kind` is `sireflect_kind_array`.
For typed pointers, `kind` is `sireflect_kind_pointer`.

Array types are created when registering fields such as:

```c
SIREFLECT_STRUCT(Path, {
    Position points[8];
});
```

The field's `type` points to an array type. Use the array helpers to inspect the
element:

```c
const sireflect_type_info_t *field_type = sireflect_type_info(reg, field->type);

if (sireflect_type_is_array(field_type)) {
    sireflect_handle_t element = sireflect_type_element(reg, field->type);
    size_t count = sireflect_type_element_count(reg, field->type);
}
```

Simple pointer fields keep the legacy `ptr` type for compatibility:

```c
SIREFLECT_STRUCT(Node, {
    Position *parent;
});
```

Typed pointer types are created when registering arrays of pointers:

```c
SIREFLECT_STRUCT(NodeChildren, {
    Position *children[8];
});
```

Arrays of pointers are inspected by walking from the array type to its element
type first:

```c
sireflect_handle_t current = field->type;
const sireflect_type_info_t *field_type = sireflect_type_info(reg, current);

if (sireflect_type_is_array(field_type)) {
    current = sireflect_type_element(reg, current);
    field_type = sireflect_type_info(reg, current);
}

if (sireflect_type_is_pointer(field_type)) {
    sireflect_handle_t pointee = sireflect_type_pointee(reg, current);
}
```

Use `sireflect_kind_name` when showing kinds in logs or UI:

```c
printf("kind=%s\n", sireflect_kind_name(info->kind));
```

Use `sireflect_is_numeric` to classify integer and floating-point kinds:

```c
if (sireflect_is_numeric(info->kind)) {
    /* Handle numeric values. */
}
```

## Metadata ownership

The registry owns all type names, field names, and field arrays. Returned
pointers are borrowed views.

Do not free metadata returned by Sireflect. Do not store metadata pointers past
`sireflect_registry_fini`.
