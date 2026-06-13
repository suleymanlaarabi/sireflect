---
title: Supported Syntax
description: Struct declaration syntax accepted by the Sireflect parser.
---

Sireflect parses the field source captured by `SIREFLECT_STRUCT`. The parser is
small on purpose and accepts only declarations that can be reflected with simple,
predictable layout rules.

## Supported declarations

Fields must use one of these forms:

```c
TYPE field;
TYPE *field;
TYPE field[N];
TYPE field[N][M];
TYPE *field[N];
```

Whitespace can appear around tokens:

```c
SIREFLECT_STRUCT(Node, {
    f32 x;
    f64 y;
    Position pos;
    Position *parent;
    Position* next;
    Position*next;
    float values[4];
    Position points[8];
    Position *items[8];
    f32 matrix[4][4];
});
```

For `TYPE *field`, Sireflect stores the field type as a typed pointer. The
pointed type can be inspected with `sireflect_type_pointee`. Use `ptr field;`
when you need raw, untyped pointer metadata.

For `TYPE field[N]`, Sireflect stores the field type as an array type. The array
type records the element type handle and element count. Multi-dimensional arrays
are represented as nested arrays, so `f32 matrix[4][4]` is an array of 4
elements whose element type is `f32[4]`.

For `TYPE *field[N]`, the field type is an array whose element type is a typed
pointer. The pointed type can be inspected from that element with
`sireflect_type_pointee`.

## Built-in type names

The registry creates handles for these built-in names:

| Name | C type |
| --- | --- |
| `u8` | `uint8_t` |
| `u16` | `uint16_t` |
| `u32` | `uint32_t` |
| `u64` | `uint64_t` |
| `i8` | `int8_t` |
| `i16` | `int16_t` |
| `i32` | `int32_t` |
| `i64` | `int64_t` |
| `f32` | `float` |
| `f64` | `double` |
| `bool` | `bool` |
| `char` | `char` |
| `short` | `short` |
| `int` | `int` |
| `long` | `long` |
| `float` | `float` |
| `double` | `double` |
| `ptr` | `void *` |

`f32` and `float` are separate reflected names with the same size and alignment.
The same applies to `f64` and `double`.

## Custom type names

Custom struct field types must already be registered before another struct uses
them by value or by pointer.

```c
SIREFLECT_STRUCT(Position, {
    f32 x;
    f32 y;
});

SIREFLECT_STRUCT(Transform, {
    Position position;
});

sireflect_registry_t *reg = sireflect_registry_init();
sireflect(reg, Position);
sireflect(reg, Transform);
```

Unknown type names assert during registration.

## Unsupported syntax

These declarations are not supported:

```c
f32 x, y;
const f32 x;
volatile f32 x;
struct Position pos;
unsigned int flags;
int bits : 3;
```

Unsupported syntax is a debug assertion failure. Sireflect does this to avoid
accepting declarations that would produce incomplete or incorrect metadata.

## Layout rules

Sireflect computes field offsets from the reflected field sizes and alignments,
then validates the result against the real `sizeof(Type)` and `_Alignof(Type)`.

This means packed structs, custom alignment attributes, bitfields, and other C
layout features outside the supported subset are not accepted.
