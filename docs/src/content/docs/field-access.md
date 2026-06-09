---
title: Field Access
description: Query field metadata and access fields inside objects.
---

Sireflect field functions operate on a reflected type handle and a field name.

## Query a field

```c
const sireflect_field_info_t *field =
    sireflect_field_info(reg, position_type, "x");

if (field != NULL) {
    printf("x offset=%zu size=%zu\n", field->offset, field->size);
}
```

`sireflect_field_info` returns `NULL` if the field is not found.

For required fields, use the convenience functions:

```c
sireflect_handle_t field_type =
    sireflect_field_type(reg, position_type, "x");

size_t field_size =
    sireflect_field_size(reg, position_type, "x");
```

These functions assert if the field does not exist.

## Get a const field pointer

```c
const f32 *x = sireflect_field_ptr(reg, position_type, &position, "x");
```

`sireflect_field_ptr` returns `const void *`. Cast it to the expected C type.

The object pointer must point to a live object of the reflected type. Sireflect
does not verify this at runtime.

## Get a mutable field pointer

```c
f32 *x = sireflect_field_mut_ptr(reg, position_type, &position, "x");
*x = 12.0f;
```

Use the mutable variant when you need to write through the returned pointer.

## Copy a value

```c
f32 value = 12.0f;

if (sireflect_field_copy(reg, position_type, &position, "x", &value) != 0) {
    /* Missing field. */
}
```

The copy operation is byte-based. It copies `field.size` bytes from `value` into
the destination field. Pass a pointer to a value with the correct size and
representation.

## Example: print fields

```c
static void print_type_fields(const sireflect_registry_t *reg, sireflect_handle_t type) {
    const sireflect_fields_t *fields = sireflect_type_fields(reg, type);

    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        const char *type_name = sireflect_type_name(reg, field->type);

        printf("%s %s; /* offset=%zu size=%zu */\n",
            type_name,
            field->name,
            field->offset,
            field->size);
    }
}
```
