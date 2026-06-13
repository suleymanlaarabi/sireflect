#ifndef SIREFLECT_H
#define SIREFLECT_H

/* This generated file contains includes for project dependencies. */
#include "sireflect/bake_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Marks generated helper symbols as intentionally unused. */
#if defined(__GNUC__) || defined(__clang__)
#define SIREFLECT_UNUSED __attribute__((unused))
#else
#define SIREFLECT_UNUSED
#endif

/* Primitive aliases understood by the parser. */
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

/* Opaque registry that owns reflected type metadata. */
typedef struct sireflect_registry_t sireflect_registry_t;

/* Type handle. Valid handles start at 1. */
typedef uint64_t sireflect_handle_t;

/* Invalid type handle. */
#define SIREFLECT_INVALID_HANDLE ((sireflect_handle_t)0)

/* Reports a failed Sireflect debug assertion. */
#ifndef NDEBUG
void sireflect_assert_fail(
    const char *condition,
    const char *message,
    const char *file,
    int line,
    const char *function
);

#define sireflect_assert(condition, message)                                                        \
    ((condition)                                                                                    \
         ? (void)0                                                                                  \
         : sireflect_assert_fail(#condition, message, __FILE__, __LINE__, __func__))
#else
#define sireflect_assert(condition, message) ((void)0)
#endif

/* Built-in kind of a reflected type. */
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
    sireflect_kind_pointer
} sireflect_kind_t;

/* Returns a stable string for a kind, or "unknown" for an invalid kind value. */
const char *sireflect_kind_name(sireflect_kind_t kind);

/* Returns true for integer and floating-point kinds. */
bool sireflect_is_numeric(sireflect_kind_t kind);

/* Metadata for a single reflected field. */
typedef struct {
    /* Field name as written in the reflected struct. */
    const char *name;

    /* Handle of the field type. Primitive types also have handles. */
    sireflect_handle_t type;

    /* Byte offset of the field from the start of the object. */
    size_t offset;

    /* Size of the field in bytes. */
    size_t size;

    /* Alignment of the field in bytes. */
    size_t align;
} sireflect_field_info_t;

/* Borrowed view over the fields of a reflected type. */
typedef struct {
    sireflect_field_info_t *fields;
    size_t field_count;
} sireflect_fields_t;

/* Metadata for a reflected type. */
typedef struct {
    /* Type name. */
    const char *name;

    /* Built-in kind of this type. */
    sireflect_kind_t kind;

    /* Size of the type in bytes. */
    size_t size;

    /* Alignment of the type in bytes. */
    size_t align;

    /* Fields owned by this type. Empty for non-struct types. */
    sireflect_fields_t fields;

    /* Element type for array types or pointee type for pointer types. */
    sireflect_handle_t element_type;

    /* Element count for array types, otherwise 0. */
    size_t element_count;
} sireflect_type_info_t;

typedef struct {
    const char *name;
    const char *fields;
    size_t size;
    size_t align;
} sireflect_struct_desc_t;

/* Transforms a struct type name into its descriptor symbol name. */
#define sireflect_desc(type) __##type##_desc

/*
 * Declares a struct and stores its source field list for registration.
 * Use sireflect(reg, name) to register the generated metadata.
 */
#define SIREFLECT_STRUCT(type_name, ...)                                                           \
    typedef struct __VA_ARGS__ type_name;                                                          \
    SIREFLECT_UNUSED static const sireflect_struct_desc_t sireflect_desc(type_name) = {            \
        .name = #type_name,                                                                        \
        .fields = #__VA_ARGS__,                                                                    \
        .size = sizeof(type_name),                                                                 \
        .align = _Alignof(type_name)                                                               \
    };

/* Registers a struct declared with SIREFLECT_STRUCT. */
#define sireflect(reg, name) sireflect_register_struct(reg, &sireflect_desc(name))

/* Creates a reflection registry. */
sireflect_registry_t *sireflect_registry_init(void);

/* Destroys a registry and all metadata it owns. */
void sireflect_registry_fini(sireflect_registry_t *reg);

/*
 * Registers a struct type from its textual field list.
 * Returns the existing handle if the same type was already registered.
 */
sireflect_handle_t
sireflect_register_struct(sireflect_registry_t *reg, const sireflect_struct_desc_t *desc);

/* Finds a type handle by name, or SIREFLECT_INVALID_HANDLE if missing. */
sireflect_handle_t sireflect_type_by_name(const sireflect_registry_t *reg, const char *name);

/* Returns metadata for a type handle. */
const sireflect_type_info_t *
sireflect_type_info(const sireflect_registry_t *reg, sireflect_handle_t ref);

/* Returns the fields of a type. */
const sireflect_fields_t *
sireflect_type_fields(const sireflect_registry_t *reg, sireflect_handle_t ref);

/* Returns the size of a type in bytes. */
size_t sireflect_type_size(const sireflect_registry_t *reg, sireflect_handle_t ref);

/* Returns the name of a type. */
const char *sireflect_type_name(const sireflect_registry_t *reg, sireflect_handle_t ref);

/* Returns true when type metadata describes a struct type. */
bool sireflect_type_is_struct(const sireflect_type_info_t *info);

/* Returns true when type metadata describes an array type. */
bool sireflect_type_is_array(const sireflect_type_info_t *info);

/* Returns true when type metadata describes a typed pointer type. */
bool sireflect_type_is_pointer(const sireflect_type_info_t *info);

/* Returns the element type handle of an array type. */
sireflect_handle_t
sireflect_type_element(const sireflect_registry_t *reg, sireflect_handle_t ref);

/* Returns the element count of an array type. */
size_t
sireflect_type_element_count(const sireflect_registry_t *reg, sireflect_handle_t ref);

/* Returns the pointee type handle of a typed pointer type. */
sireflect_handle_t
sireflect_type_pointee(const sireflect_registry_t *reg, sireflect_handle_t ref);

/* Finds metadata for a field by name. */
const sireflect_field_info_t *
sireflect_field_info(const sireflect_registry_t *reg, sireflect_handle_t type, const char *field);

/* Returns the type handle of a field. */
sireflect_handle_t
sireflect_field_type(const sireflect_registry_t *reg, sireflect_handle_t type, const char *field);

/* Returns the size of a field in bytes. */
size_t
sireflect_field_size(const sireflect_registry_t *reg, sireflect_handle_t ref, const char *field);

/* Returns a const pointer to a field inside an object. */
const void *sireflect_field_ptr(
    const sireflect_registry_t *reg,
    sireflect_handle_t type,
    const void *obj,
    const char *field
);

/* Returns a mutable pointer to a field inside an object. */
void *sireflect_field_mut_ptr(
    const sireflect_registry_t *reg,
    sireflect_handle_t type,
    void *obj,
    const char *field
);

/* Copies value bytes into a field. Returns 0 on success. */
int sireflect_field_copy(
    const sireflect_registry_t *reg,
    sireflect_handle_t type,
    void *obj,
    const char *field,
    const void *value
);

#ifdef __cplusplus
}
#endif

#endif
