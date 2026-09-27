---
title: Limits
description: Current Sireflect limitations and intentional non-goals.
---

Sireflect is a limited reflection layer. Its parser accepts only the subset of C
that the library can map to correct runtime metadata.

## Current limits

Sireflect does not support:

| Syntax or feature | Reason |
| --- | --- |
| Pointer-level qualifiers such as `TYPE * const field` | Pointer-specific qualifier metadata is not modeled separately. |
| `struct Name` spelling | The parser expects a registered type name, not a tagged type spelling. |
| Type specifier combinations outside the supported list, such as `signed int` or `long double` | The parser only canonicalizes a bounded set of common integer spellings. |
| Bitfields | Bit offsets and widths are not represented. |
| Packed structs | The layout validator assumes normal C alignment. |
| Attributes | Custom compiler layout attributes are outside the parser subset. |
| Function pointer parameters | Empty parameter lists are supported; parameter types and variadic signatures are not reflected yet. |

## Assertion policy

Invalid inputs generally fail with `assert`.

This applies to:

| Case | Behavior |
| --- | --- |
| Unknown type name | Assert during struct registration. |
| Unsupported syntax | Assert during parsing. |
| Invalid handle | Assert when reading type metadata. |
| Missing required field | Assert in functions such as `sireflect_field_type`. |

Use `sireflect_type_by_name` and `sireflect_field_info` when absence is expected
and should be handled manually.

## Self references

Structs may refer to themselves through typed pointer fields:

```c
SIREFLECT_STRUCT(Node, {
    Node *next;
});
```

`SIREFLECT_STRUCT` declares the named C tag and typedef before the body, and
registration reserves the type handle while its fields are parsed. A direct
by-value self field is rejected. Other distinct types still need to be
registered before use.

Use `ptr` when you only need to reflect the field as a raw pointer:

```c
SIREFLECT_STRUCT(Node, {
    ptr next;
});
```

## Thread safety

The public API does not provide synchronization. Serialize lifecycle calls,
registration, and metadata access externally when multiple threads use
Sireflect.
