---
title: API Reference
description: Public Sireflect symbols from sireflect.h.
---

Include the public API with:

```c
#include <sireflect.h>
```

## Primitive aliases

Sireflect provides short aliases that are also understood by the parser:

```c
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;
typedef void *ptr;
```

## Core types

```c
typedef uint64_t sireflect_handle_t;

#define SIREFLECT_INVALID_HANDLE ((sireflect_handle_t)0)
```

Sireflect's internal process-wide context owns all metadata.
`sireflect_handle_t` identifies a type while that context is initialized.

## Assertions

```c
void sireflect_assert_fail(
    const char *condition,
    const char *message,
    const char *file,
    int line,
    const char *function
);

#define sireflect_assert(condition, message)
```

In debug builds, `sireflect_assert` reports the failed condition, message,
source location, and function, then aborts. When `NDEBUG` is defined,
`sireflect_assert` is compiled as a no-op.

## Type kind

```c
typedef enum {
    sireflect_kind_u8,
    sireflect_kind_u16,
    sireflect_kind_u32,
    sireflect_kind_u64,
    sireflect_kind_i8,
    sireflect_kind_i16,
    sireflect_kind_i32,
    sireflect_kind_i64,
    sireflect_kind_f32,
    sireflect_kind_f64,
    sireflect_kind_bool,
    sireflect_kind_char,
    sireflect_kind_short,
    sireflect_kind_int,
    sireflect_kind_long,
    sireflect_kind_ptr,
    sireflect_kind_struct,
    sireflect_kind_array,
    sireflect_kind_pointer,
    sireflect_kind_signed_char,
    sireflect_kind_unsigned_char,
    sireflect_kind_unsigned_short,
    sireflect_kind_unsigned_int,
    sireflect_kind_unsigned_long,
    sireflect_kind_long_long,
    sireflect_kind_unsigned_long_long,
    sireflect_kind_function_pointer,
    sireflect_kind_enum
} sireflect_kind_t;
```

```c
const char *sireflect_kind_name(sireflect_kind_t kind);
bool sireflect_is_numeric(sireflect_kind_t kind);
```

`sireflect_kind_name` returns a stable string for a kind, or `"unknown"` for an
invalid kind value.

`sireflect_is_numeric` returns `true` for integer and floating-point kinds,
including native C numeric kinds such as `char`, `short`, `int`, `long`,
`unsigned int`, and `long long`. It returns `false` for `bool`, `ptr`, typed
pointers, function pointers, structs, enums, arrays, and invalid kind values.

## Field qualifiers

```c
typedef enum {
    SIREFLECT_QUAL_CONST = 1u << 0,
    SIREFLECT_QUAL_VOLATILE = 1u << 1
} sireflect_qualifier_t;
```

Field metadata stores qualifiers as a bitmask.

## Field metadata

```c
typedef struct {
    const char *name;
    sireflect_handle_t type;
    size_t offset;
    size_t size;
    size_t align;
    uint32_t qualifiers;
} sireflect_field_info_t;
```

Field metadata is owned by Sireflect and returned as borrowed views.

## Field list

```c
typedef struct {
    sireflect_field_info_t *fields;
    size_t field_count;
} sireflect_fields_t;
```

This is a borrowed view over a type's fields.

## Enum metadata

```c
typedef struct {
    const char *name;
    int64_t value;
} sireflect_enum_value_t;

typedef struct {
    sireflect_enum_value_t *values;
    size_t value_count;
} sireflect_enum_values_t;
```

This is a borrowed view over an enum's enumerators.

## Type metadata

```c
typedef struct {
    const char *name;
    sireflect_kind_t kind;
    size_t size;
    size_t align;
    sireflect_fields_t fields;
    sireflect_enum_values_t enum_values;
    sireflect_handle_t element_type;
    size_t element_count;
} sireflect_type_info_t;
```

Struct types have `kind == sireflect_kind_struct`. Non-struct types have an
empty field list.

Enum types have `kind == sireflect_kind_enum`; `enum_values` contains their
enumerators and is empty for other types.

Array types have `kind == sireflect_kind_array`, `element_type` set to the
element type handle, and `element_count` set to the fixed array length. Typed
pointer types have `kind == sireflect_kind_pointer`, `element_type` set to the
pointee type handle, and `element_count == 0`. Function-pointer types have
`kind == sireflect_kind_function_pointer`, `element_type` set to the return
type handle, and `element_count == 0`. Other types use
`SIREFLECT_INVALID_HANDLE` and `0` for those members.

## Struct declaration macros

```c
#define SIREFLECT_STRUCT(name, ...)
#define SIREFLECT_ENUM(name, ...)
#define sireflect(name)
```

`SIREFLECT_STRUCT` declares a typedef struct and captures its field list as a
string. `SIREFLECT_ENUM` does the same for an enum's enumerator list.
`sireflect(name)` selects the appropriate registration function automatically.

Example:

```c
SIREFLECT_STRUCT(Position, {
    f32 x;
    f32 y;
});

sireflect_handle_t type = sireflect(Position);
```

```c
SIREFLECT_ENUM(Color, { COLOR_RED, COLOR_GREEN = 5, COLOR_BLUE });
sireflect_handle_t type = sireflect(Color);
```

## Sireflect lifecycle

```c
void sireflect_init(void);
void sireflect_fini(void);
```

The calls are reference-counted. The first `sireflect_init` initializes the
context and registers built-in primitive types. Metadata is destroyed only by
the matching final `sireflect_fini`.

## Register structs and enums

```c
const char *sireflect_error(void);

sireflect_handle_t sireflect_register_struct(const sireflect_struct_desc_t *desc);

sireflect_handle_t sireflect_try_register_struct(const sireflect_struct_desc_t *desc);

sireflect_handle_t sireflect_register_enum(const sireflect_enum_desc_t *desc);

sireflect_handle_t sireflect_try_register_enum(const sireflect_enum_desc_t *desc);

sireflect_handle_t sireflect_try_register_dynamic_struct(
    const char *name,
    const char *fields
);
```

Registers a struct type from a descriptor containing its name, textual field
list, size, and alignment. Most user code should call the
`sireflect(TypeName)` macro instead.

Returns the existing handle if the same struct was already registered.

`sireflect_register_struct` is the strict API: invalid descriptors or reflected
syntax fail through `sireflect_assert` in debug builds.
`sireflect_try_register_struct` returns `SIREFLECT_INVALID_HANDLE` for those
recoverable failures instead. Allocation failures remain non-recoverable
assertions.

`sireflect_try_register_dynamic_struct` registers a struct from a name and
field source, deriving its size and alignment from the registered field types.

Enum descriptors contain a name, textual enumerator list, size, and alignment.
Their syntax currently supports implicit values and explicit integer literals
(including negative, octal, and hexadecimal literals), but not C expressions.

After a recoverable failure, `sireflect_error` returns the current
library-owned error string, or `NULL` if no error is stored. Calling
`sireflect_error` does not clear or free the message, so repeated calls return
the same current error. The message is cleared by the next public `sireflect_*`
call except `sireflect_error`, and is also freed by the final `sireflect_fini`.
Callers must not free the returned pointer.

## Type lookup

```c
sireflect_handle_t sireflect_type_by_name(const char *name);
```

Returns a type handle by name, or `SIREFLECT_INVALID_HANDLE` if the type is not
registered.

## Type queries

```c
const sireflect_type_info_t *sireflect_type_info(sireflect_handle_t ref);

const sireflect_fields_t *sireflect_type_fields(sireflect_handle_t ref);

size_t sireflect_type_size(sireflect_handle_t ref);

const char *sireflect_type_name(sireflect_handle_t ref);

bool sireflect_type_is_struct(
    const sireflect_type_info_t *info
);

bool sireflect_type_is_enum(
    const sireflect_type_info_t *info
);

const sireflect_enum_values_t *sireflect_type_enum_values(sireflect_handle_t type);

const sireflect_enum_value_t *sireflect_enum_value_by_name(
    sireflect_handle_t type,
    const char *name
);

const sireflect_enum_value_t *sireflect_enum_value_by_value(
    sireflect_handle_t type,
    int64_t value
);

bool sireflect_type_is_array(
    const sireflect_type_info_t *info
);

bool sireflect_type_is_pointer(
    const sireflect_type_info_t *info
);

sireflect_handle_t sireflect_type_element(sireflect_handle_t ref);

size_t sireflect_type_element_count(sireflect_handle_t ref);

sireflect_handle_t sireflect_type_pointee(sireflect_handle_t ref);
```

These functions assert if `ref` is not a valid handle for the initialized
context.
`sireflect_type_is_struct`, `sireflect_type_is_enum`, `sireflect_type_is_array`, and
`sireflect_type_is_pointer` assert if `info` is `NULL`. Array element queries
assert if `ref` is not an array type. Pointer pointee queries assert if `ref` is
not a typed pointer type.

Enum queries assert when `type` is not an enum. Value lookups return `NULL`
when no enumerator matches.

## Field queries

```c
const sireflect_field_info_t *sireflect_field_info(sireflect_handle_t type,
    const char *field
);

sireflect_handle_t sireflect_field_type(sireflect_handle_t type,
    const char *field
);

size_t sireflect_field_size(sireflect_handle_t ref,
    const char *field
);
```

`sireflect_field_info` returns `NULL` when the field does not exist.
`sireflect_field_type` and `sireflect_field_size` assert when the field does not
exist.

## Field access

```c
const void *sireflect_field_ptr(sireflect_handle_t type,
    const void *obj,
    const char *field
);

void *sireflect_field_mut_ptr(sireflect_handle_t type,
    void *obj,
    const char *field
);
```

Returns a pointer to a field inside `obj`. The object must have the reflected C
type represented by `type`.

## Field copy

```c
int sireflect_field_copy(sireflect_handle_t type,
    void *obj,
    const char *field,
    const void *value
);
```

Copies `field.size` bytes from `value` into the selected field. Returns `0` on
success and `-1` if the field name does not exist.
