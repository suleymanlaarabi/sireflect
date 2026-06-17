---
title: Getting Started
description: Register a reflected struct and inspect its metadata.
---

This page shows the normal Sireflect flow:

1. Declare a reflected struct with `SIREFLECT_STRUCT`.
2. Create a registry with `sireflect_registry_init`.
3. Register the struct with `sireflect(reg, TypeName)`.
4. Read type and field metadata through the registry.
5. Destroy the registry with `sireflect_registry_fini`.

## Declare a reflected struct

```c
#include <sireflect.h>

SIREFLECT_STRUCT(Position, {
    f32 x;
    f32 y;
});
```

`SIREFLECT_STRUCT` expands to a normal typedef:

```c
typedef struct {
    f32 x;
    f32 y;
} Position;
```

It also stores the field source as a string so Sireflect can parse it when the
type is registered.

## Create a registry

```c
sireflect_registry_t *reg = sireflect_registry_init();
```

The registry owns all reflected metadata. During initialization it registers the
built-in primitive names understood by the parser, such as `u8`, `f32`, `int`,
`float`, `double`, `char`, `bool`, and `ptr`.

Destroy the registry when reflection metadata is no longer needed:

```c
sireflect_registry_fini(reg);
```

All metadata pointers returned by the registry become invalid after this call.

## Register a type

```c
sireflect_handle_t position = sireflect(reg, Position);
```

The `sireflect` macro calls `sireflect_register_struct` with the type name, the
captured field source, `sizeof(Position)`, and `_Alignof(Position)`.

Registering the same struct again returns the existing handle after validating
that the size and alignment still match.

Use `sireflect_try_register_struct` directly when invalid reflected source
should return `SIREFLECT_INVALID_HANDLE` instead of triggering the strict debug
assertions used by `sireflect_register_struct`. After a failed try-register,
`sireflect_error()` returns the current library-owned error string. The pointer
is valid until the next public `sireflect_*` call except `sireflect_error()`, or
until `sireflect_registry_fini()`.

## Inspect fields

```c
const sireflect_fields_t *fields = sireflect_type_fields(reg, position);

for (size_t i = 0; i < fields->field_count; i++) {
    const sireflect_field_info_t *field = &fields->fields[i];
    printf("%s: offset=%zu size=%zu align=%zu\n",
        field->name,
        field->offset,
        field->size,
        field->align);
}
```

Each field stores:

| Member | Meaning |
| --- | --- |
| `name` | Field name as written in the struct. |
| `type` | Handle of the reflected field type. |
| `offset` | Byte offset from the start of the object. |
| `size` | Field size in bytes. |
| `align` | Field alignment in bytes. |

## Read and write an object field

```c
Position pos = { .x = 1.0f, .y = 2.0f };

f32 *x = sireflect_field_mut_ptr(reg, position, &pos, "x");
*x = 5.0f;

const f32 *y = sireflect_field_ptr(reg, position, &pos, "y");
printf("y=%f\n", (double)*y);
```

`sireflect_field_ptr` and `sireflect_field_mut_ptr` return raw pointers. Cast the
result to the expected C type after checking the field metadata when needed.

## Copy bytes into a field

```c
f32 value = 42.0f;
int rc = sireflect_field_copy(reg, position, &pos, "x", &value);
```

`sireflect_field_copy` copies exactly `field.size` bytes from `value` into the
field. It returns `0` on success and `-1` if the field name does not exist.
