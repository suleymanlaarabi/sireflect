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
| `kind` | Built-in kind or `sireflect_kind_struct`. |
| `size` | Size in bytes. |
| `align` | Alignment in bytes. |
| `fields` | Field list for struct types, empty for non-struct types. |

Convenience functions are also available:

```c
const char *name = sireflect_type_name(reg, type);
size_t size = sireflect_type_size(reg, type);
const sireflect_fields_t *fields = sireflect_type_fields(reg, type);
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
```

For custom structs, `kind` is always `sireflect_kind_struct`.

## Metadata ownership

The registry owns all type names, field names, and field arrays. Returned
pointers are borrowed views.

Do not free metadata returned by Sireflect. Do not store metadata pointers past
`sireflect_registry_fini`.
