---
title: Sireflect
description: Small runtime reflection for C structs.
---

Sireflect is a small C reflection library for projects that want runtime type
metadata without requiring a C compiler plugin or an external code generator.

The public API is exposed by:

```c
#include <sireflect.h>
```

Sireflect is intentionally limited. It reflects structs declared through
`SIREFLECT_STRUCT`, parses their field list at registration time, and stores
metadata in a registry owned by the application.

## What it provides

| Feature | Description |
| --- | --- |
| Type registry | Stores primitive and struct metadata behind stable handles. |
| Struct registration | Registers a struct declared with `SIREFLECT_STRUCT`. |
| Field metadata | Provides field name, type handle, byte offset, size, and alignment. |
| Field access | Returns pointers to fields inside a live object. |
| Byte copy | Copies raw bytes into a field by name. |

## Minimal example

```c
#include <sireflect.h>
#include <stdio.h>

SIREFLECT_STRUCT(Position, {
    f32 x;
    f32 y;
});

int main(void) {
    sireflect_registry_t *reg = sireflect_registry_init();

    sireflect_handle_t position_type = sireflect(reg, Position);
    const sireflect_fields_t *fields = sireflect_type_fields(reg, position_type);

    for (size_t i = 0; i < fields->field_count; i++) {
        const sireflect_field_info_t *field = &fields->fields[i];
        printf("%s: offset=%zu size=%zu\n", field->name, field->offset, field->size);
    }

    Position pos = { .x = 10.0f, .y = 20.0f };
    f32 *x = sireflect_field_mut_ptr(reg, position_type, &pos, "x");
    *x = 42.0f;

    sireflect_registry_fini(reg);
    return 0;
}
```

## Design model

Sireflect does not try to parse all of C. It supports a small declaration subset
that is easy to validate:

```c
TYPE field;
TYPE *field;
```

Unsupported declarations assert during registration. This is deliberate: if a
field cannot be reflected correctly, the program should fail early instead of
silently producing wrong metadata.

## Next steps

1. Read [Getting Started](./getting-started/) for the normal setup flow.
2. Read [Supported Syntax](./supported-syntax/) before declaring reflected structs.
3. Read [Field Access](./field-access/) to use reflection data with objects.
4. Use the [API Reference](./reference/api/) as a compact symbol list.
