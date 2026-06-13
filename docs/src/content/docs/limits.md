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
| `const` / `volatile` | Qualifiers are not stored in field metadata. |
| `struct Name` spelling | The parser expects a single registered type name token. |
| `unsigned int` | Multi-token type names are not supported. |
| Bitfields | Bit offsets and widths are not represented. |
| Packed structs | The layout validator assumes normal C alignment. |
| Attributes | Custom compiler layout attributes are outside the parser subset. |
| Function pointers | Function declarator syntax is outside the parser subset. |

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

In the current implementation, a type must already exist in the registry before
it can be used as a field type. This also affects pointer declarations:

```c
SIREFLECT_STRUCT(Node, {
    Node *next;
});
```

This form requires `Node` to be known before parsing the fields, which is not
supported yet. A future version can support this by inserting a placeholder type
before parsing.

Use `ptr` when you only need to reflect the field as a raw pointer:

```c
SIREFLECT_STRUCT(Node, {
    ptr next;
});
```

## Thread safety

The public API does not provide synchronization. Treat a registry as externally
synchronized if multiple threads can register types or read metadata while a
registration is in progress.
