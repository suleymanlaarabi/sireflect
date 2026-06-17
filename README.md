![SIREFLECT](docs/assets/banner.png)

[![Documentation](https://img.shields.io/badge/docs-sireflect-blue?style=for-the-badge&color=blue)](https://suleymanlaarabi.github.io/sireflect/)
[![actions](https://img.shields.io/github/actions/workflow/status/suleymanlaarabi/sireflect/docs.yml?branch=main&style=for-the-badge)](https://github.com/suleymanlaarabi/sireflect/actions?query=workflow%3ACI)

`sireflect` is a small runtime reflection library for C structs. It gives you
type handles, field metadata, field lookup, and raw field access without a
compiler plugin or an external code generator.

- Compact C API with zero runtime dependencies.
- Struct reflection through `SIREFLECT_STRUCT` and a registry owned by the app.
- Built-in metadata for primitive aliases such as `u8`, `i32`, `f32`, `bool`,
  `ptr`, native C numeric types, and common multi-token type names such as
  `unsigned int` and `long long`.
- Fixed-size arrays such as `float values[4]`, `Position points[8]`,
  `Position *items[8]`, and `f32 matrix[4][4]`.
- Field metadata with name, type handle, byte offset, size, alignment, and
  leading `const` / `volatile` qualifiers.
- Strict registration with debug diagnostics, plus recoverable registration
  through `sireflect_try_register_struct`.

```c
#include "sireflect.h"

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
        const sireflect_type_info_t *type = sireflect_type_info(reg, field->type);

        printf(
            "%s: type=%s offset=%zu size=%zu\n",
            field->name,
            sireflect_kind_name(type->kind),
            field->offset,
            field->size
        );
    }

    Position pos = { .x = 10.0f, .y = 20.0f };
    f32 *x = sireflect_field_mut_ptr(reg, position_type, &pos, "x");
    *x = 42.0f;

    sireflect_registry_fini(reg);
    return 0;
}
```

## What it supports

Sireflect intentionally supports a small declaration subset:

```c
TYPE field;
const TYPE field;
volatile TYPE field;
const volatile TYPE field;
TYPE a, b;
TYPE *field;
TYPE *a, *b;
TYPE a, *b;
TYPE field[N];
TYPE a[N], b[M];
TYPE field[N][M];
TYPE *field[N];
```

Pointer fields use typed pointer metadata. Use the explicit `ptr` alias when
you need a raw, untyped pointer field.

Unsupported syntax fails during strict registration with a debug assertion. Use
`sireflect_try_register_struct` when invalid reflected source should return
`SIREFLECT_INVALID_HANDLE` instead of aborting in debug builds.

## Documentation

- [Documentation](https://suleymanlaarabi.github.io/sireflect/)
- [Getting Started](https://suleymanlaarabi.github.io/sireflect/getting-started/)
- [API Reference](https://suleymanlaarabi.github.io/sireflect/reference/api/)
