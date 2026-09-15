#ifndef SIREFLECT_REGISTRY_H
#define SIREFLECT_REGISTRY_H

#include <sireflect.h>
#ifndef SICORE_H
#include <sicore.h>
#endif

typedef struct sireflect_registry_t sireflect_registry_t;

struct sireflect_registry_t {
    sicore_vec_t types;
    sicore_map_t types_by_name;
};

sireflect_registry_t *sireflect_registry_current(void);
bool sireflect_registry_is_initialized(void);

sireflect_handle_t sireflect_registry_add_type(
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
);

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_handle_t element_type,
    size_t element_count
);

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_handle_t pointee_type);


sireflect_handle_t sireflect_registry_get_or_add_function_pointer_type(
    sireflect_handle_t return_type
);

sireflect_handle_t sireflect_registry_handle_by_name(const char *name);

sireflect_type_info_t *sireflect_registry_type_at(sireflect_handle_t handle);

const sireflect_type_info_t *sireflect_registry_const_type_at(sireflect_handle_t handle);

#endif
