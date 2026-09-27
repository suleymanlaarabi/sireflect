#include "sireflect_error.h"
#include "sireflect_registry.h"

#include <stdint.h>
#include <stdlib.h>

#define SIREFLECT_WALK_MAX_DEPTH 256

typedef enum { walk_types, walk_const_values, walk_mut_values } walk_mode_t;

typedef struct {
    walk_mode_t mode;
    uint32_t flags;
    void *user;
    sireflect_type_visitor_t type_visitor;
    sireflect_const_value_visitor_t const_visitor;
    sireflect_value_visitor_t mut_visitor;
    sireflect_handle_t active[SIREFLECT_WALK_MAX_DEPTH + 1];
    unsigned char *seen;
    sireflect_handle_t first_handle;
} walk_state_t;

static bool emit_value(walk_state_t *state, sireflect_value_event_t event,
    sireflect_handle_t type, const sireflect_type_info_t *info,
    const sireflect_field_info_t *field, const void *ptr, void *mut_ptr,
    size_t index, size_t depth) {
    if (state->mode == walk_const_values) {
        sireflect_const_value_visit_t visit = {
            .event = event, .type = type, .info = info, .field = field,
            .ptr = ptr, .index = index, .depth = depth
        };
        return state->const_visitor(&visit, state->user);
    }
    sireflect_value_visit_t visit = {
        .event = event, .type = type, .info = info, .field = field,
        .ptr = mut_ptr, .index = index, .depth = depth
    };
    return state->mut_visitor(&visit, state->user);
}

static bool walk_node(walk_state_t *state, sireflect_handle_t type,
    sireflect_walk_relation_t relation, const sireflect_field_info_t *field,
    sireflect_handle_t parent, const void *ptr, void *mut_ptr,
    size_t index, size_t depth) {
    sireflect_type_entry_t *entry = sireflect_registry_entry_at(type);
    if (entry == NULL) {
        sireflect_error_set("invalid type in reflection graph");
        return false;
    }
    if (depth > SIREFLECT_WALK_MAX_DEPTH) {
        sireflect_error_set("reflection walk depth limit exceeded");
        return false;
    }
    const sireflect_type_info_t *info = &entry->info;
    if (state->mode == walk_types) {
        for (size_t i = 0; i < depth; i++) {
            if (state->active[i] == type) return true;
        }
        if (state->seen != NULL) {
            size_t slot = (size_t)(type - state->first_handle);
            if (state->seen[slot]) return true;
            state->seen[slot] = 1;
        }
        state->active[depth] = type;
        sireflect_type_visit_t visit = {
            .type = type, .info = info, .relation = relation,
            .field = field, .parent_type = parent, .depth = depth
        };
        if (!state->type_visitor(&visit, state->user)) return false;
    } else {
        sireflect_value_event_t event = SIREFLECT_VALUE_LEAF;
        if (info->kind == sireflect_kind_struct) event = SIREFLECT_VALUE_ENTER_STRUCT;
        else if (info->kind == sireflect_kind_array) event = SIREFLECT_VALUE_ENTER_ARRAY;
        else if (info->kind == sireflect_kind_pointer || info->kind == sireflect_kind_ptr ||
            info->kind == sireflect_kind_function_pointer) event = SIREFLECT_VALUE_POINTER;
        else event = SIREFLECT_VALUE_LEAF;
        if (!emit_value(state, event, type, info, field, ptr, mut_ptr, index, depth)) return false;
    }

    if (info->kind == sireflect_kind_struct) {
        for (size_t i = 0; i < info->fields.field_count; i++) {
            if (depth == SIREFLECT_WALK_MAX_DEPTH) {
                sireflect_error_set("reflection walk depth limit exceeded");
                return false;
            }
            const sireflect_field_info_t *child = &info->fields.fields[i];
            if (child->offset > info->size || child->size > info->size - child->offset) {
                sireflect_error_set("invalid reflected field bounds");
                return false;
            }
            const void *child_ptr = ptr ? (const unsigned char *)ptr + child->offset : NULL;
            void *child_mut_ptr = mut_ptr ? (unsigned char *)mut_ptr + child->offset : NULL;
            sireflect_type_entry_t *child_entry = sireflect_registry_entry_at(child->type);
            if (child_entry == NULL) {
                sireflect_error_set("invalid reflected field type");
                return false;
            }
            if (state->mode != walk_types &&
                !emit_value(state, SIREFLECT_VALUE_FIELD, child->type,
                    &child_entry->info, child,
                    child_ptr, child_mut_ptr, 0, depth + 1)) return false;
            if (!walk_node(state, child->type, SIREFLECT_WALK_FIELD, child, type,
                child_ptr, child_mut_ptr, 0, depth + 1)) return false;
        }
        if (state->mode != walk_types &&
            !emit_value(state, SIREFLECT_VALUE_LEAVE_STRUCT, type, info, field,
                ptr, mut_ptr, index, depth)) return false;
    } else if (info->kind == sireflect_kind_array) {
        sireflect_type_entry_t *element = sireflect_registry_entry_at(info->element_type);
        if (element == NULL || element->info.size == 0 || info->element_count == 0 ||
            info->element_count > info->size / element->info.size) {
            sireflect_error_set("invalid reflected array metadata");
            return false;
        }
        size_t count = state->mode == walk_types ? 1 : info->element_count;
        for (size_t i = 0; i < count; i++) {
            if (depth == SIREFLECT_WALK_MAX_DEPTH) {
                sireflect_error_set("reflection walk depth limit exceeded");
                return false;
            }
            size_t offset = i * element->info.size;
            const void *child_ptr = ptr ? (const unsigned char *)ptr + offset : NULL;
            void *child_mut_ptr = mut_ptr ? (unsigned char *)mut_ptr + offset : NULL;
            if (state->mode != walk_types &&
                !emit_value(state, SIREFLECT_VALUE_ARRAY_ELEMENT, info->element_type,
                    &element->info, NULL, child_ptr, child_mut_ptr, i, depth + 1)) return false;
            if (!walk_node(state, info->element_type, SIREFLECT_WALK_ARRAY_ELEMENT,
                NULL, type, child_ptr, child_mut_ptr, i, depth + 1)) return false;
        }
        if (state->mode != walk_types &&
            !emit_value(state, SIREFLECT_VALUE_LEAVE_ARRAY, type, info, field,
                ptr, mut_ptr, index, depth)) return false;
    } else if (state->mode == walk_types && info->kind == sireflect_kind_pointer &&
        (state->flags & SIREFLECT_WALK_FOLLOW_POINTERS)) {
        if (!walk_node(state, info->element_type, SIREFLECT_WALK_POINTER_TARGET,
            NULL, type, NULL, NULL, 0, depth + 1)) return false;
    } else if (state->mode == walk_types && info->kind == sireflect_kind_function_pointer) {
        if (!walk_node(state, info->element_type, SIREFLECT_WALK_FUNCTION_RETURN,
            NULL, type, NULL, NULL, 0, depth + 1)) return false;
    }
    return true;
}

bool sireflect_walk_type(sireflect_handle_t root, uint32_t flags,
    sireflect_type_visitor_t visitor, void *user) {
    sireflect_error_clear();
    if (visitor == NULL || sireflect_registry_entry_at(root) == NULL ||
        (flags & ~(SIREFLECT_WALK_FOLLOW_POINTERS | SIREFLECT_WALK_DEDUPLICATE))) {
        sireflect_error_set("invalid type walk arguments");
        return false;
    }
    walk_state_t state = { .mode = walk_types, .flags = flags,
        .user = user, .type_visitor = visitor,
        .first_handle = sireflect_registry_current()->first_handle };
    if (flags & SIREFLECT_WALK_DEDUPLICATE) {
        state.seen = calloc(sireflect_registry_current()->types.size, 1);
        if (state.seen == NULL) {
            sireflect_error_set("failed to allocate type walk state");
            return false;
        }
    }
    bool result = walk_node(&state, root, SIREFLECT_WALK_ROOT,
        NULL, SIREFLECT_INVALID_HANDLE, NULL, NULL, 0, 0);
    free(state.seen);
    return result;
}

static bool walk_value_common(sireflect_handle_t type, const void *value,
    void *mut_value, uint32_t flags, walk_state_t *state) {
    sireflect_error_clear();
    if (sireflect_registry_entry_at(type) == NULL || value == NULL || flags != 0 ||
        (state->mode == walk_const_values ? state->const_visitor == NULL : state->mut_visitor == NULL)) {
        sireflect_error_set("invalid value walk arguments");
        return false;
    }
    return walk_node(state, type, SIREFLECT_WALK_ROOT, NULL,
        SIREFLECT_INVALID_HANDLE, value, mut_value, 0, 0);
}

bool sireflect_walk_value(sireflect_handle_t type, void *value, uint32_t flags,
    sireflect_value_visitor_t visitor, void *user) {
    walk_state_t state = { .mode = walk_mut_values, .user = user, .mut_visitor = visitor };
    return walk_value_common(type, value, value, flags, &state);
}

bool sireflect_walk_const_value(sireflect_handle_t type, const void *value, uint32_t flags,
    sireflect_const_value_visitor_t visitor, void *user) {
    walk_state_t state = { .mode = walk_const_values, .user = user, .const_visitor = visitor };
    return walk_value_common(type, value, NULL, flags, &state);
}
