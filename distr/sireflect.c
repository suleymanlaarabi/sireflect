#include "sireflect.h"

#if SICORE_HAS_MAP
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#define SICORE_MAP_SSE2 1
#elif defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#define SICORE_MAP_NEON 1
#endif

#define SICORE_GROUP_WIDTH 16u
#define SICORE_INITIAL_CAPACITY 16u
#define SICORE_CTRL_EMPTY UINT8_C(0x80)
#define SICORE_CTRL_DELETED UINT8_C(0xfe)

/* 16 octets sur ABI 64 bits: 1/4 de ligne de cache de 64 octets. */
typedef struct {
    const char *key;
    uint32_t value;
    uint32_t key_length;
} sicore_map_entry_t;

/*
 * Hash de chaîne basé sur wyhash final v4 (domaine public / Unlicense), adapté
 * et préfixé pour rester entièrement interne à cette unité de compilation.
 */
static const uint64_t sicore_hash_secret[5] = { UINT64_C(0xa0761d6478bd642f),
                                                UINT64_C(0xe7037ed1a0b428db),
                                                UINT64_C(0x8ebc6af09c88c6e3),
                                                UINT64_C(0x589965cc75374cc3),
                                                UINT64_C(0x1d8e4e27c47d124f) };

static inline void sicore_mul128(uint64_t *a, uint64_t *b) {
#if defined(__SIZEOF_INT128__)
    __uint128_t r = (__uint128_t)(*a) * (*b);
    *a = (uint64_t)r;
    *b = (uint64_t)(r >> 64);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
    *a = _umul128(*a, *b, b);
#else
    const uint64_t ah = *a >> 32;
    const uint64_t al = (uint32_t)*a;
    const uint64_t bh = *b >> 32;
    const uint64_t bl = (uint32_t)*b;
    const uint64_t rh = ah * bh;
    const uint64_t rm0 = ah * bl;
    const uint64_t rm1 = bh * al;
    const uint64_t rl = al * bl;
    const uint64_t t = rl + (rm0 << 32);
    uint64_t carry = t < rl;
    const uint64_t lo = t + (rm1 << 32);
    carry += lo < t;
    *a = lo;
    *b = rh + (rm0 >> 32) + (rm1 >> 32) + carry;
#endif
}

static inline uint64_t sicore_mix(uint64_t a, uint64_t b) {
    sicore_mul128(&a, &b);
    return a ^ b;
}

static inline uint64_t sicore_read64(const uint8_t *p) {
    uint64_t v;
    memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#if defined(_MSC_VER)
    v = _byteswap_uint64(v);
#else
    v = __builtin_bswap64(v);
#endif
#endif
    return v;
}

static inline uint64_t sicore_read32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#if defined(_MSC_VER)
    v = _byteswap_ulong((unsigned long)v);
#else
    v = __builtin_bswap32(v);
#endif
#endif
    return v;
}

static inline uint64_t sicore_read3(const uint8_t *p, size_t len) {
    return ((uint64_t)p[0] << 16) | ((uint64_t)p[len >> 1] << 8) | (uint64_t)p[len - 1];
}

static inline uint64_t
sicore_hash_finish16(const uint8_t *p, uint64_t len, uint64_t seed, size_t remaining) {
    uint64_t a;
    uint64_t b;

    if (remaining <= 8) {
        if (remaining >= 4) {
            a = sicore_read32(p);
            b = sicore_read32(p + remaining - 4);
        } else if (remaining != 0) {
            a = sicore_read3(p, remaining);
            b = 0;
        } else {
            a = 0;
            b = 0;
        }
    } else {
        a = sicore_read64(p);
        b = sicore_read64(p + remaining - 8);
    }

    return sicore_mix(sicore_hash_secret[1] ^ len, sicore_mix(a ^ sicore_hash_secret[1], b ^ seed));
}

static inline uint64_t sicore_hash_bytes(const uint8_t *p, size_t len) {
    size_t remaining = len;
    uint64_t seed = sicore_hash_secret[0];

    if (SICORE_UNLIKELY(remaining > 64)) {
        uint64_t seed2 = seed;
        do {
            seed =
                sicore_mix(sicore_read64(p) ^ sicore_hash_secret[1], sicore_read64(p + 8) ^ seed) ^
                sicore_mix(
                    sicore_read64(p + 16) ^ sicore_hash_secret[2],
                    sicore_read64(p + 24) ^ seed
                );
            seed2 = sicore_mix(
                        sicore_read64(p + 32) ^ sicore_hash_secret[3],
                        sicore_read64(p + 40) ^ seed2
                    ) ^
                    sicore_mix(
                        sicore_read64(p + 48) ^ sicore_hash_secret[4],
                        sicore_read64(p + 56) ^ seed2
                    );
            p += 64;
            remaining -= 64;
        } while (remaining > 64);
        seed ^= seed2;
    }

    while (remaining > 16) {
        seed = sicore_mix(sicore_read64(p) ^ sicore_hash_secret[1], sicore_read64(p + 8) ^ seed);
        p += 16;
        remaining -= 16;
    }

    return sicore_hash_finish16(p, (uint64_t)len, seed, remaining);
}

static inline uint64_t sicore_hash_string(const char *key, uint32_t *length) {
    const uint32_t len = (uint32_t)strlen(key);
    *length = len;
    return sicore_hash_bytes((const uint8_t *)key, len);
}

static inline uint32_t sicore_ctz32(uint32_t x) {
#if defined(_MSC_VER)
    unsigned long bit;
    _BitScanForward(&bit, x);
    return (uint32_t)bit;
#else
    return (uint32_t)__builtin_ctz(x);
#endif
}

#if defined(SICORE_MAP_SSE2)
static inline uint32_t sicore_match_byte(const uint8_t *ctrl, uint8_t byte) {
    const __m128i group = _mm_loadu_si128((const __m128i *)(const void *)ctrl);
    const __m128i wanted = _mm_set1_epi8((char)byte);
    return (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(group, wanted));
}
#elif defined(SICORE_MAP_NEON)
static inline uint32_t sicore_match_byte(const uint8_t *ctrl, uint8_t byte) {
    static const uint8_t weights_data[16] = { 1, 2, 4, 8, 16, 32, 64, 128,
                                              1, 2, 4, 8, 16, 32, 64, 128 };
    const uint8x16_t group = vld1q_u8(ctrl);
    const uint8x16_t equal = vceqq_u8(group, vdupq_n_u8(byte));
    const uint8x16_t bits = vandq_u8(equal, vld1q_u8(weights_data));
    const uint32_t low = vaddv_u8(vget_low_u8(bits));
    const uint32_t high = vaddv_u8(vget_high_u8(bits));
    return low | (high << 8);
}
#else
static inline uint32_t sicore_match_byte(const uint8_t *ctrl, uint8_t byte) {
    uint32_t mask = 0;
    for (uint32_t i = 0; i < SICORE_GROUP_WIDTH; ++i) {
        mask |= (uint32_t)(ctrl[i] == byte) << i;
    }
    return mask;
}
#endif

static inline uint32_t sicore_max_load(uint32_t capacity) {
    return capacity - (capacity >> 3); /* 87,5 % */
}

static inline uint8_t sicore_hash_h2(uint64_t hash) { return (uint8_t)(hash & UINT64_C(0x7f)); }

static inline uint32_t sicore_hash_group(uint64_t hash, uint32_t group_mask) {
    return (uint32_t)(hash >> 7) & group_mask;
}

static inline void sicore_allocate(sicore_map_t *map, uint32_t capacity) {
    const size_t ctrl_bytes = capacity;
    const size_t entries_bytes = (size_t)capacity * sizeof(sicore_map_entry_t);
    uint8_t *const block = (uint8_t *)malloc(ctrl_bytes + entries_bytes);

    memset(block, SICORE_CTRL_EMPTY, ctrl_bytes);

    map->ctrl = block;
    map->entries = block + ctrl_bytes;
    map->size = 0;
    map->capacity = capacity;
    map->growth_left = sicore_max_load(capacity);
    map->group_mask = (capacity / SICORE_GROUP_WIDTH) - 1u;
}

static inline void sicore_insert_absent_hashed(
    sicore_map_t *map,
    const char *key,
    uint32_t value,
    uint32_t key_length,
    uint64_t hash
) {
    sicore_map_entry_t *const entries = (sicore_map_entry_t *)map->entries;
    const uint8_t h2 = sicore_hash_h2(hash);
    uint32_t group = sicore_hash_group(hash, map->group_mask);
    uint32_t probe = 0;

    for (;;) {
        const uint32_t base = group * SICORE_GROUP_WIDTH;
        const uint32_t empties = sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY);

        if (empties != 0) {
            const uint32_t index = base + sicore_ctz32(empties);
            entries[index].key = key;
            entries[index].value = value;
            entries[index].key_length = key_length;
            map->ctrl[index] = h2;
            ++map->size;
            --map->growth_left;
            return;
        }

        ++probe;
        group = (group + probe) & map->group_mask;
    }
}

static inline uint32_t
sicore_find_index(const sicore_map_t *map, const char *key, uint32_t key_length, uint64_t hash) {
    const sicore_map_entry_t *const entries = (const sicore_map_entry_t *)map->entries;
    const uint8_t h2 = sicore_hash_h2(hash);
    uint32_t group = sicore_hash_group(hash, map->group_mask);
    uint32_t probe = 0;

    for (;;) {
        const uint32_t base = group * SICORE_GROUP_WIDTH;
        uint32_t candidates = sicore_match_byte(map->ctrl + base, h2);

        while (candidates != 0) {
            const uint32_t bit = sicore_ctz32(candidates);
            const uint32_t index = base + bit;
            const char *const candidate_key = entries[index].key;

            if (candidate_key == key || (entries[index].key_length == key_length &&
                                         memcmp(candidate_key, key, key_length) == 0)) {
                return index;
            }
            candidates &= candidates - 1u;
        }

        if (sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY) != 0) {
            return UINT32_MAX;
        }

        ++probe;
        group = (group + probe) & map->group_mask;
    }
}

void sicore_map_init(sicore_map_t *map) { sicore_allocate(map, SICORE_INITIAL_CAPACITY); }

void sicore_map_fini(sicore_map_t *map) { free(map->ctrl); }

SICORE_HOT uint32_t sicore_map_get(const sicore_map_t *map, const char *key) {
    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);
    const uint32_t index = sicore_find_index(map, key, key_length, hash);
    return index == UINT32_MAX ? UINT32_MAX
                               : ((const sicore_map_entry_t *)map->entries)[index].value;
}

SICORE_HOT bool sicore_map_has(const sicore_map_t *map, const char *key) {
    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);
    return sicore_find_index(map, key, key_length, hash) != UINT32_MAX;
}

static void sicore_rehash(sicore_map_t *map, uint32_t new_capacity) {
    sicore_map_t rebuilt;
    const uint32_t old_capacity = map->capacity;
    uint8_t *const old_ctrl = map->ctrl;
    sicore_map_entry_t *const old_entries = (sicore_map_entry_t *)map->entries;

    sicore_allocate(&rebuilt, new_capacity);

    for (uint32_t i = 0; i < old_capacity; ++i) {
        if (old_ctrl[i] < SICORE_CTRL_EMPTY) {
            const char *const key = old_entries[i].key;

            sicore_insert_absent_hashed(
                &rebuilt,
                key,
                old_entries[i].value,
                old_entries[i].key_length,
                sicore_hash_bytes((const uint8_t *)key, old_entries[i].key_length)
            );
        }
    }

    free(old_ctrl);
    *map = rebuilt;
}

SICORE_HOT void sicore_map_set(sicore_map_t *map, const char *key, uint32_t value) {
    sicore_map_entry_t *entries = (sicore_map_entry_t *)map->entries;

    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);
    const uint8_t h2 = sicore_hash_h2(hash);

    uint32_t group = sicore_hash_group(hash, map->group_mask);
    uint32_t probe = 0;
    uint32_t first_deleted = UINT32_MAX;

    for (;;) {
        const uint32_t base = group * SICORE_GROUP_WIDTH;
        uint32_t candidates = sicore_match_byte(map->ctrl + base, h2);

        while (candidates != 0) {
            const uint32_t bit = sicore_ctz32(candidates);
            const uint32_t index = base + bit;
            const char *const candidate_key = entries[index].key;

            if (candidate_key == key || (entries[index].key_length == key_length &&
                                         memcmp(candidate_key, key, key_length) == 0)) {
                entries[index].value = value;
                return;
            }

            candidates &= candidates - 1u;
        }

        if (first_deleted == UINT32_MAX) {
            const uint32_t deleted = sicore_match_byte(map->ctrl + base, SICORE_CTRL_DELETED);

            if (deleted != 0) {
                first_deleted = base + sicore_ctz32(deleted);
            }
        }

        const uint32_t empties = sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY);

        if (empties != 0) {
            if (first_deleted != UINT32_MAX) {
                entries[first_deleted].key = key;
                entries[first_deleted].value = value;
                entries[first_deleted].key_length = key_length;

                map->ctrl[first_deleted] = h2;
                ++map->size;
                return;
            }

            if (SICORE_UNLIKELY(map->growth_left == 0)) {
                const uint32_t max_load = sicore_max_load(map->capacity);

                sicore_rehash(map, map->size < max_load ? map->capacity : map->capacity << 1);

                sicore_insert_absent_hashed(map, key, value, key_length, hash);

                return;
            }

            const uint32_t index = base + sicore_ctz32(empties);

            entries[index].key = key;
            entries[index].value = value;
            entries[index].key_length = key_length;

            map->ctrl[index] = h2;
            ++map->size;
            --map->growth_left;
            return;
        }

        ++probe;
        group = (group + probe) & map->group_mask;
    }
}

SICORE_HOT bool sicore_map_unset(sicore_map_t *map, const char *key) {
    uint32_t key_length;
    const uint64_t hash = sicore_hash_string(key, &key_length);

    const uint32_t index = sicore_find_index(map, key, key_length, hash);

    if (index == UINT32_MAX) {
        return false;
    }

    const uint32_t base = index & ~(SICORE_GROUP_WIDTH - 1u);

    --map->size;

    if (sicore_match_byte(map->ctrl + base, SICORE_CTRL_EMPTY) != 0) {
        map->ctrl[index] = SICORE_CTRL_EMPTY;
        ++map->growth_left;
    } else {
        map->ctrl[index] = SICORE_CTRL_DELETED;
    }

    return true;
}

#endif

#if SICORE_HAS_VEC
#include <stdlib.h>
#include <string.h>

void sicore_vec_init(sicore_vec_t *vec, uint32_t element_size) {
    vec->data = malloc(element_size);
    vec->size = 0;
    vec->capacity = 1;
}

void sicore_vec_init_w_size(sicore_vec_t *vec, uint32_t element_size, uint32_t size) {
    vec->data = malloc(element_size * size);
    vec->size = 0;
    vec->capacity = size;
}

void sicore_vec_fini(sicore_vec_t *vec) { free(vec->data); }

void sicore_vec_grow(sicore_vec_t *vec, uint32_t element_size) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, element_size * vec->capacity);
}

void sicore_vec_push(sicore_vec_t *vec, const void *element, const uint32_t element_size) {
    if (SICORE_UNLIKELY(vec->size >= vec->capacity)) {
        sicore_vec_grow(vec, element_size);
    }
    memcpy((uint8_t *)vec->data + (vec->size * element_size), element, element_size);
    vec->size++;
}

void sicore_vec_ensure(sicore_vec_t *vec, uint32_t count, const uint32_t element_size) {
    if (count <= vec->size)
        return;
    while (vec->capacity < count)
        sicore_vec_grow(vec, element_size);
    memset((uint8_t *)vec->data + vec->size * element_size, 0, (count - vec->size) * element_size);
    vec->size = count;
}

void sicore_vec_remove_fast(sicore_vec_t *vec, uint32_t index, const uint32_t element_size) {
    if (index < vec->size - 1) {
        void *dst = (uint8_t *)vec->data + (index * element_size);
        const void *src = (uint8_t *)vec->data + ((vec->size - 1) * element_size);
        memcpy(dst, src, element_size);
    }
    vec->size--;
}

bool sicore_vec_contains_u16(const sicore_vec_t *vec, const uint16_t value) {
    sicore_vec_iter(vec, uint16_t, current, {
        if (*current == value) {
            return true;
        }
    });
    return false;
}

static inline void sicore_vec_remove_fast_u16(sicore_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint16_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void sicore_vec_remove_u16(sicore_vec_t *vec, const uint16_t value) {
    sicore_vec_iter(vec, uint16_t, current, {
        if (*current == value) {
            sicore_vec_remove_fast_u16(vec, i);
            return;
        }
    });
}

static inline void sicore_vec_remove_fast_u64(sicore_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint64_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void sicore_vec_remove_u64(sicore_vec_t *vec, uint64_t value) {
    sicore_vec_iter(vec, uint64_t, current, {
        if (*current == value) {
            sicore_vec_remove_fast_u64(vec, i);
            return;
        }
    });
}
#endif

#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>

void sireflect_assert_fail(
    const char *condition,
    const char *message,
    const char *file,
    int line,
    const char *function
) {
    fprintf(stderr, "sireflect assertion failed: %s\n", message != NULL ? message : condition);
    fprintf(stderr, "  condition: %s\n", condition != NULL ? condition : "(unknown)");
    fprintf(stderr, "  location: %s:%d\n", file != NULL ? file : "(unknown)", line);
    fprintf(stderr, "  function: %s\n", function != NULL ? function : "(unknown)");
    abort();
}
#endif

#ifndef SIREFLECT_ERROR_H
#define SIREFLECT_ERROR_H

void sireflect_error_clear(void);
void sireflect_error_set(const char *message);

#endif

#include <stdlib.h>
#include <string.h>

static char *sireflect_current_error = NULL;

static char *sireflect_error_dup(const char *message) {
    sireflect_assert(message != NULL, "error message must not be NULL");

    const size_t len = strlen(message);
    char *copy = malloc(len + 1);
    sireflect_assert(copy != NULL, "failed to allocate error message");

    memcpy(copy, message, len + 1);
    return copy;
}

void sireflect_error_clear(void) {
    free(sireflect_current_error);
    sireflect_current_error = NULL;
}

void sireflect_error_set(const char *message) {
    sireflect_error_clear();

    if (message == NULL) {
        return;
    }

    sireflect_current_error = sireflect_error_dup(message);
}

const char *sireflect_error(void) {
    return sireflect_current_error;
}

const sireflect_field_info_t *
sireflect_field_info(sireflect_handle_t type, const char *field) {
    sireflect_error_clear();

    sireflect_assert(field != NULL, "field name must not be NULL");

    const sireflect_fields_t *fields = sireflect_type_fields(type);
    for (size_t i = 0; i < fields->field_count; i++) {
        if (strcmp(fields->fields[i].name, field) == 0) {
            return &fields->fields[i];
        }
    }

    return NULL;
}

sireflect_handle_t
sireflect_field_type(sireflect_handle_t type, const char *field) {
    sireflect_error_clear();

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    sireflect_assert(info != NULL, "field must exist");
    return info->type;
}

size_t
sireflect_field_size(sireflect_handle_t ref, const char *field) {
    sireflect_error_clear();

    const sireflect_field_info_t *info = sireflect_field_info(ref, field);
    sireflect_assert(info != NULL, "field must exist");
    return info->size;
}

const void *sireflect_field_ptr(
    sireflect_handle_t type,
    const void *obj,
    const char *field
) {
    sireflect_error_clear();

    sireflect_assert(obj != NULL, "object pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    sireflect_assert(info != NULL, "field must exist");

    return (const unsigned char *)obj + info->offset;
}

void *sireflect_field_mut_ptr(
    sireflect_handle_t type,
    void *obj,
    const char *field
) {
    sireflect_error_clear();

    sireflect_assert(obj != NULL, "object pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    sireflect_assert(info != NULL, "field must exist");

    return (unsigned char *)obj + info->offset;
}

int sireflect_field_copy(
    sireflect_handle_t type,
    void *obj,
    const char *field,
    const void *value
) {
    sireflect_error_clear();

    sireflect_assert(value != NULL, "source value pointer must not be NULL");

    const sireflect_field_info_t *info = sireflect_field_info(type, field);
    if (info == NULL) {
        return -1;
    }

    memcpy(sireflect_field_mut_ptr(type, obj, field), value, info->size);
    return 0;
}

#ifndef SIREFLECT_PARSER_H
#define SIREFLECT_PARSER_H

bool sireflect_parse_struct_fields(
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    size_t *out_struct_size,
    size_t *out_struct_align,
    bool validate_layout,
    bool fail_fast
);

#endif

#ifndef SIREFLECT_REGISTRY_H
#define SIREFLECT_REGISTRY_H

#ifndef SICORE_H
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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#define SIREFLECT_MAX_ARRAY_DIMS 16

typedef enum {
    sireflect_token_ident,
    sireflect_token_integer,
    sireflect_token_lbrace,
    sireflect_token_rbrace,
    sireflect_token_lbracket,
    sireflect_token_rbracket,
    sireflect_token_lparen,
    sireflect_token_rparen,
    sireflect_token_star,
    sireflect_token_comma,
    sireflect_token_semicolon,
    sireflect_token_unknown,
    sireflect_token_end
} sireflect_token_kind_t;

typedef struct {
    sireflect_token_kind_t kind;
    const char *start;
    size_t len;
    size_t offset;
    size_t line;
    size_t column;
} sireflect_token_t;

typedef struct {
    const char *src;
    const char *struct_name;
    const char *field_start;
    size_t field_len;
    size_t pos;
    size_t line;
    size_t column;
    sireflect_token_t current;
    char message[512];
    bool failed;
    bool fail_fast;
} sireflect_parser_t;

typedef struct {
    const char *start;
    size_t len;
    char name[64];
    int has_name;
    size_t line;
    size_t column;
} sireflect_type_spec_t;

static inline int sireflect_is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }

static inline int sireflect_is_ident_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static inline int sireflect_token_is_ident(sireflect_token_t token, const char *text) {
    return token.kind == sireflect_token_ident && strlen(text) == token.len &&
           strncmp(token.start, text, token.len) == 0;
}

static inline int sireflect_token_is_qualifier(sireflect_token_t token) {
    return sireflect_token_is_ident(token, "const") || sireflect_token_is_ident(token, "volatile");
}

static inline void
sireflect_type_spec_set(sireflect_type_spec_t *type, sireflect_token_t token) {
    type->start = token.start;
    type->len = token.len;
    type->name[0] = '\0';
    type->has_name = 0;
    type->line = token.line;
    type->column = token.column;
}

static inline void sireflect_type_spec_set2(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    )
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline void sireflect_type_spec_set3(
    sireflect_type_spec_t *type,
    sireflect_token_t first,
    sireflect_token_t second,
    sireflect_token_t third
) {
    const int len = snprintf(
        type->name,
        sizeof(type->name),
        "%.*s %.*s %.*s",
        (int)first.len,
        first.start,
        (int)second.len,
        second.start,
        (int)third.len,
        third.start
    );
    (void)len;
    sireflect_indebug(
        sireflect_assert(len > 0 && (size_t)len < sizeof(type->name), "type specifier is too long");
    );
    type->start = NULL;
    type->len = 0;
    type->has_name = 1;
    type->line = first.line;
    type->column = first.column;
}

static inline const char *sireflect_token_kind_name(sireflect_token_kind_t kind) {
    switch (kind) {
    case sireflect_token_ident:
        return "identifier";
    case sireflect_token_integer:
        return "integer";
    case sireflect_token_lbrace:
        return "'{'";
    case sireflect_token_rbrace:
        return "'}'";
    case sireflect_token_lbracket:
        return "'['";
    case sireflect_token_rbracket:
        return "']'";
    case sireflect_token_lparen:
        return "'('";
    case sireflect_token_rparen:
        return "')'";
    case sireflect_token_star:
        return "'*'";
    case sireflect_token_comma:
        return "','";
    case sireflect_token_semicolon:
        return "';'";
    case sireflect_token_unknown:
        return "unsupported token";
    case sireflect_token_end:
        return "end of input";
    }

    return "unknown token";
}

static inline void
sireflect_token_display(sireflect_token_t token, char *buffer, size_t buffer_size) {
    if (token.kind == sireflect_token_end) {
        snprintf(buffer, buffer_size, "end of input");
        return;
    }

    if (token.len == 0) {
        snprintf(buffer, buffer_size, "%s", sireflect_token_kind_name(token.kind));
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s '%.*s'",
        sireflect_token_kind_name(token.kind),
        (int)token.len,
        token.start
    );
}

static inline void
sireflect_parser_context(sireflect_parser_t *parser, char *buffer, size_t buffer_size) {
    if (parser->field_start != NULL) {
        snprintf(
            buffer,
            buffer_size,
            "struct '%s', field '%.*s'",
            parser->struct_name != NULL ? parser->struct_name : "<unknown>",
            (int)parser->field_len,
            parser->field_start
        );
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "struct '%s'",
        parser->struct_name != NULL ? parser->struct_name : "<unknown>"
    );
}

static inline void
sireflect_parser_fail_at(sireflect_parser_t *parser, sireflect_token_t token, const char *message) {
    char actual[96];
    char context[160];

    sireflect_token_display(token, actual, sizeof(actual));
    sireflect_parser_context(parser, context, sizeof(context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "%s in %s at line %zu, column %zu: actual %s",
        message,
        context,
        token.line,
        token.column,
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_unexpected(
    sireflect_parser_t *parser,
    sireflect_token_kind_t expected,
    const char *context
) {
    char actual[96];
    char parser_context[160];

    sireflect_token_display(parser->current, actual, sizeof(actual));
    sireflect_parser_context(parser, parser_context, sizeof(parser_context));

    snprintf(
        parser->message,
        sizeof(parser->message),
        "unexpected token while parsing %s in %s at line %zu, column %zu: expected %s, actual %s",
        context,
        parser_context,
        parser->current.line,
        parser->current.column,
        sireflect_token_kind_name(expected),
        actual
    );

    parser->failed = true;
    if (parser->fail_fast) {
        sireflect_assert(false, parser->message);
    }
    sireflect_error_set(parser->message);
}

static inline void sireflect_parser_advance(sireflect_parser_t *parser) {
    if (parser->src[parser->pos] == '\n') {
        parser->line++;
        parser->column = 1;
    } else {
        parser->column++;
    }

    parser->pos++;
}

static inline void sireflect_parser_next(sireflect_parser_t *parser) {
    const char *src = parser->src;

    while (isspace((unsigned char)src[parser->pos])) {
        sireflect_parser_advance(parser);
    }

    const size_t start = parser->pos;
    const size_t line = parser->line;
    const size_t column = parser->column;
    const char c = src[start];

    if (c == '\0') {
        parser->current = (sireflect_token_t){ sireflect_token_end, &src[start], 0, start, line, column };
        return;
    }

    if (sireflect_is_ident_start(c)) {
        sireflect_parser_advance(parser);
        while (sireflect_is_ident_char(src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_ident,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    if (isdigit((unsigned char)c)) {
        sireflect_parser_advance(parser);
        while (isdigit((unsigned char)src[parser->pos])) {
            sireflect_parser_advance(parser);
        }

        parser->current = (sireflect_token_t){
            sireflect_token_integer,
            &src[start],
            parser->pos - start,
            start,
            line,
            column,
        };
        return;
    }

    sireflect_parser_advance(parser);

    switch (c) {
    case '{':
        parser->current = (sireflect_token_t){ sireflect_token_lbrace, &src[start], 1, start, line, column };
        return;
    case '}':
        parser->current = (sireflect_token_t){ sireflect_token_rbrace, &src[start], 1, start, line, column };
        return;
    case '[':
        parser->current =
            (sireflect_token_t){ sireflect_token_lbracket, &src[start], 1, start, line, column };
        return;
    case ']':
        parser->current =
            (sireflect_token_t){ sireflect_token_rbracket, &src[start], 1, start, line, column };
        return;
    case '(':
        parser->current =
            (sireflect_token_t){ sireflect_token_lparen, &src[start], 1, start, line, column };
        return;
    case ')':
        parser->current =
            (sireflect_token_t){ sireflect_token_rparen, &src[start], 1, start, line, column };
        return;
    case '*':
        parser->current = (sireflect_token_t){ sireflect_token_star, &src[start], 1, start, line, column };
        return;
    case ',':
        parser->current = (sireflect_token_t){ sireflect_token_comma, &src[start], 1, start, line, column };
        return;
    case ';':
        parser->current =
            (sireflect_token_t){ sireflect_token_semicolon, &src[start], 1, start, line, column };
        return;
    default:
        parser->current = (sireflect_token_t){ sireflect_token_unknown, &src[start], 1, start, line, column };
        sireflect_parser_fail_at(
            parser,
            parser->current,
            "unsupported syntax in reflected struct; supported fields are '<type> <name>;', '<type> <name>, <name>;', '<type> *<name>;', '<type> (*<name>)();', '<type> <name>[N][M];', and '<type> *<name>[N];'"
        );
    }
}

static inline void sireflect_parser_init(
    sireflect_parser_t *parser,
    const char *struct_name,
    const char *src,
    bool fail_fast
) {
    sireflect_assert(parser != NULL, "parser must not be NULL");
    sireflect_assert(struct_name != NULL, "parser struct name must not be NULL");
    sireflect_assert(src != NULL, "parser source must not be NULL");

    parser->src = src;
    parser->struct_name = struct_name;
    parser->field_start = NULL;
    parser->field_len = 0;
    parser->pos = 0;
    parser->line = 1;
    parser->column = 1;
    parser->message[0] = '\0';
    parser->failed = false;
    parser->fail_fast = fail_fast;
    sireflect_parser_next(parser);
}

static inline sireflect_token_t
sireflect_expect(sireflect_parser_t *parser, sireflect_token_kind_t kind, const char *context) {
    sireflect_token_t token = parser->current;
    if (token.kind != kind) {
        sireflect_parser_unexpected(parser, kind, context);
        return token;
    }
    sireflect_parser_next(parser);
    return token;
}

static inline sireflect_token_t sireflect_expect_field_name(sireflect_parser_t *parser) {
    sireflect_token_t token = parser->current;
    if (token.kind != sireflect_token_ident || sireflect_token_is_qualifier(token)) {
        sireflect_parser_unexpected(parser, sireflect_token_ident, "field name");
        return token;
    }

    parser->field_start = token.start;
    parser->field_len = token.len;
    sireflect_parser_next(parser);
    return token;
}

static inline uint32_t sireflect_parse_qualifiers(sireflect_parser_t *parser) {
    uint32_t qualifiers = 0;

    for (;;) {
        if (sireflect_token_is_ident(parser->current, "const")) {
            qualifiers |= SIREFLECT_QUAL_CONST;
            sireflect_parser_next(parser);
            continue;
        }

        if (sireflect_token_is_ident(parser->current, "volatile")) {
            qualifiers |= SIREFLECT_QUAL_VOLATILE;
            sireflect_parser_next(parser);
            continue;
        }

        return qualifiers;
    }
}

static inline void
sireflect_fail_unsupported_type_specifier(sireflect_parser_t *parser, sireflect_token_t token) {
    sireflect_parser_fail_at(
        parser,
        token,
        "unsupported type specifier sequence; supported multi-token types are 'signed char', 'unsigned char', 'unsigned short', 'unsigned int', 'unsigned long', 'long long', and 'unsigned long long'"
    );
}

static inline sireflect_type_spec_t sireflect_parse_type_specifier(sireflect_parser_t *parser) {
    sireflect_token_t first = sireflect_expect(parser, sireflect_token_ident, "field type");
    sireflect_type_spec_t type;
    sireflect_type_spec_set(&type, first);
    if (parser->failed) {
        return type;
    }

    if (sireflect_token_is_ident(first, "signed")) {
        if (!sireflect_token_is_ident(parser->current, "char")) {
            sireflect_fail_unsupported_type_specifier(parser, parser->current);
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "long")) {
        if (!sireflect_token_is_ident(parser->current, "long")) {
            return type;
        }

        sireflect_token_t second = parser->current;
        sireflect_parser_next(parser);
        sireflect_type_spec_set2(&type, first, second);
        return type;
    }

    if (sireflect_token_is_ident(first, "unsigned")) {
        sireflect_token_t second = parser->current;

        if (sireflect_token_is_ident(second, "char") || sireflect_token_is_ident(second, "short") ||
            sireflect_token_is_ident(second, "int")) {
            sireflect_parser_next(parser);
            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        if (sireflect_token_is_ident(second, "long")) {
            sireflect_parser_next(parser);

            if (sireflect_token_is_ident(parser->current, "long")) {
                sireflect_token_t third = parser->current;
                sireflect_parser_next(parser);
                sireflect_type_spec_set3(&type, first, second, third);
                return type;
            }

            sireflect_type_spec_set2(&type, first, second);
            return type;
        }

        sireflect_fail_unsupported_type_specifier(parser, second);
        return type;
    }

    return type;
}

static inline char *sireflect_dup_range(const char *start, size_t len) {
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate parser string");

    memcpy(result, start, len);
    result[len] = '\0';

    return result;
}

static inline size_t
sireflect_parse_array_count(sireflect_parser_t *parser, sireflect_token_t token) {
    size_t count = 0;

    for (size_t i = 0; i < token.len; i++) {
        const unsigned int digit = (unsigned int)(token.start[i] - '0');
        if (count > (SIZE_MAX - digit) / 10) {
            sireflect_parser_fail_at(parser, token, "array element count overflows size_t");
            return 0;
        }
        count = count * 10 + digit;
    }

    if (count == 0) {
        sireflect_parser_fail_at(parser, token, "array element count must be greater than zero");
        return 0;
    }

    return count;
}

static inline size_t
sireflect_parse_array_dimensions(sireflect_parser_t *parser, size_t *counts, size_t max_count) {
    size_t count = 0;

    while (parser->current.kind == sireflect_token_lbracket) {
        sireflect_parser_next(parser);

        if (parser->current.kind == sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "array element count is required");
            return count;
        }

        if (parser->current.kind != sireflect_token_integer) {
            sireflect_parser_fail_at(
                parser,
                parser->current,
                "array element count must be a positive integer literal"
            );
            return count;
        }

        sireflect_token_t count_token = parser->current;
        sireflect_parser_next(parser);

        if (parser->current.kind != sireflect_token_rbracket) {
            sireflect_parser_fail_at(parser, parser->current, "expected ']' after array element count");
            return count;
        }

        if (count >= max_count) {
            sireflect_parser_fail_at(parser, count_token, "too many array dimensions");
            return count;
        }

        counts[count++] = sireflect_parse_array_count(parser, count_token);
        if (parser->failed) {
            return count;
        }
        sireflect_parser_next(parser);
    }

    return count;
}

static inline void sireflect_parse_declarator_shape(sireflect_parser_t *parser) {
    size_t counts[SIREFLECT_MAX_ARRAY_DIMS];

    parser->field_start = NULL;
    parser->field_len = 0;

    if (parser->current.kind == sireflect_token_lparen) {
        sireflect_expect(parser, sireflect_token_lparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_star, "function pointer declarator");
        (void)sireflect_expect_field_name(parser);
        sireflect_expect(parser, sireflect_token_rparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_lparen, "function pointer parameters");
        sireflect_expect(parser, sireflect_token_rparen, "function pointer parameters");
    } else {
        if (parser->current.kind == sireflect_token_star) {
            sireflect_parser_next(parser);
        }

        sireflect_expect_field_name(parser);
    }

    (void)sireflect_parse_array_dimensions(parser, counts, SIREFLECT_MAX_ARRAY_DIMS);
}

static inline size_t sireflect_parse_declaration_shape(sireflect_parser_t *parser) {
    size_t count = 0;

    (void)sireflect_parse_qualifiers(parser);
    (void)sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator_shape(parser);
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline bool sireflect_count_fields(
    const char *struct_name,
    const char *fields_src,
    bool fail_fast,
    size_t *out_count
) {
    sireflect_parser_t parser;
    size_t count = 0;

    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        return false;
    }

    while (parser.current.kind != sireflect_token_rbrace) {
        count += sireflect_parse_declaration_shape(&parser);
        if (parser.failed) {
            return false;
        }
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        return false;
    }

    *out_count = count;
    return true;
}

static inline size_t sireflect_align_up(size_t value, size_t align) {
    sireflect_assert(align != 0, "alignment must not be zero");

    const size_t remainder = value % align;
    if (remainder == 0) {
        return value;
    }

    return value + align - remainder;
}

static inline sireflect_handle_t sireflect_resolve_field_type(
    sireflect_parser_t *parser,
    sireflect_type_spec_t type
) {
    char *owned_name = NULL;
    const char *type_name = type.name;

    if (!type.has_name) {
        owned_name = sireflect_dup_range(type.start, type.len);
        type_name = owned_name;
    }

    sireflect_handle_t field_type = sireflect_type_by_name(type_name);
    if (field_type == SIREFLECT_INVALID_HANDLE) {
        char context[160];

        sireflect_parser_context(parser, context, sizeof(context));
        snprintf(
            parser->message,
            sizeof(parser->message),
            "unknown field type '%s' in %s at line %zu, column %zu; register the type before this struct or use a supported primitive alias",
            type_name,
            context,
            type.line,
            type.column
        );
        free(owned_name);
        parser->failed = true;
        if (parser->fail_fast) {
            sireflect_assert(false, parser->message);
        }
        sireflect_error_set(parser->message);
        return SIREFLECT_INVALID_HANDLE;
    }

    free(owned_name);
    return field_type;
}

static inline void sireflect_parse_declarator(
    sireflect_parser_t *parser,
    sireflect_field_info_t *field,
    sireflect_type_spec_t type,
    uint32_t qualifiers,
    size_t *offset,
    size_t *max_align
) {
    parser->field_start = NULL;
    parser->field_len = 0;

    int is_pointer = 0;
    int is_function_pointer = 0;
    size_t array_counts[SIREFLECT_MAX_ARRAY_DIMS];
    size_t array_dim_count = 0;
    sireflect_token_t name_token;

    if (parser->current.kind == sireflect_token_lparen) {
        is_function_pointer = 1;
        sireflect_expect(parser, sireflect_token_lparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_star, "function pointer declarator");
        name_token = sireflect_expect_field_name(parser);
        sireflect_expect(parser, sireflect_token_rparen, "function pointer declarator");
        sireflect_expect(parser, sireflect_token_lparen, "function pointer parameters");
        sireflect_expect(parser, sireflect_token_rparen, "function pointer parameters");
    } else {
        if (parser->current.kind == sireflect_token_star) {
            is_pointer = 1;
            sireflect_parser_next(parser);
        }

        name_token = sireflect_expect_field_name(parser);
    }
    if (parser->failed) {
        return;
    }

    array_dim_count =
        sireflect_parse_array_dimensions(parser, array_counts, SIREFLECT_MAX_ARRAY_DIMS);
    if (parser->failed) {
        return;
    }

    sireflect_handle_t field_type = sireflect_resolve_field_type(parser, type);
    if (parser->failed) {
        return;
    }

    if (is_function_pointer) {
        field_type = sireflect_registry_get_or_add_function_pointer_type(field_type);
    } else if (is_pointer) {
        field_type = sireflect_registry_get_or_add_pointer_type(field_type);
    }

    for (size_t i = array_dim_count; i > 0; i--) {
        field_type = sireflect_registry_get_or_add_array_type(field_type, array_counts[i - 1]);
    }

    const sireflect_type_info_t *type_info = sireflect_type_info(field_type);
    sireflect_assert(type_info != NULL, "field type metadata must exist");

    field->name = sireflect_dup_range(name_token.start, name_token.len);
    field->type = field_type;
    field->size = type_info->size;
    field->align = type_info->align;
    field->offset = sireflect_align_up(*offset, field->align);
    field->qualifiers = qualifiers;

    *offset = field->offset + field->size;
    if (field->align > *max_align) {
        *max_align = field->align;
    }
}

static inline size_t sireflect_parse_declaration(
    sireflect_parser_t *parser,
    sireflect_field_info_t *fields,
    size_t *offset,
    size_t *max_align
) {
    size_t count = 0;
    uint32_t qualifiers = sireflect_parse_qualifiers(parser);
    sireflect_type_spec_t type = sireflect_parse_type_specifier(parser);
    if (parser->failed) {
        return 0;
    }

    for (;;) {
        sireflect_parse_declarator(
            parser,
            &fields[count],
            type,
            qualifiers,
            offset,
            max_align
        );
        if (parser->failed) {
            return 0;
        }
        count++;

        if (parser->current.kind != sireflect_token_comma) {
            break;
        }

        sireflect_parser_next(parser);
    }

    sireflect_expect(parser, sireflect_token_semicolon, "field terminator");
    if (parser->failed) {
        return 0;
    }
    return count;
}

static inline void sireflect_free_parsed_fields(sireflect_field_info_t *fields, size_t field_count) {
    if (fields == NULL) {
        return;
    }

    for (size_t i = 0; i < field_count; i++) {
        free((char *)fields[i].name);
    }

    free(fields);
}

bool sireflect_parse_struct_fields(
    const char *struct_name,
    const char *fields_src,
    sireflect_field_info_t **out_fields,
    size_t *out_field_count,
    size_t struct_size,
    size_t struct_align,
    size_t *out_struct_size,
    size_t *out_struct_align,
    bool validate_layout,
    bool fail_fast
) {
    sireflect_assert(struct_name != NULL, "struct name must not be NULL");
    sireflect_assert(fields_src != NULL, "field source must not be NULL");
    sireflect_assert(out_fields != NULL, "output field pointer must not be NULL");
    sireflect_assert(out_field_count != NULL, "output field count pointer must not be NULL");
    sireflect_assert(out_struct_size != NULL, "output struct size must not be NULL");
    sireflect_assert(out_struct_align != NULL, "output struct alignment must not be NULL");

    size_t field_count = 0;
    if (!sireflect_count_fields(struct_name, fields_src, fail_fast, &field_count)) {
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    sireflect_field_info_t *fields = NULL;

    if (field_count != 0) {
        fields = calloc(field_count, sizeof(*fields));
        sireflect_assert(fields != NULL, "failed to allocate field metadata");
    }

    sireflect_parser_t parser;
    sireflect_parser_init(&parser, struct_name, fields_src, fail_fast);
    sireflect_expect(&parser, sireflect_token_lbrace, "struct field list start");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

    size_t offset = 0;
    size_t max_align = 1;

    for (size_t i = 0; i < field_count;) {
        const size_t parsed_count =
            sireflect_parse_declaration(&parser, &fields[i], &offset, &max_align);
        if (parser.failed) {
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
        i += parsed_count;
    }

    sireflect_expect(&parser, sireflect_token_rbrace, "struct field list end");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }
    sireflect_expect(&parser, sireflect_token_end, "trailing input after struct field list");
    if (parser.failed) {
        sireflect_free_parsed_fields(fields, field_count);
        *out_fields = NULL;
        *out_field_count = 0;
        return false;
    }

#ifndef NDEBUG
    if (validate_layout) {
        const size_t computed_size = sireflect_align_up(offset, struct_align);
        if (computed_size != struct_size) {
            if (fail_fast) {
                sireflect_assert(false, "computed struct size does not match C layout");
            }
            sireflect_error_set("computed struct size does not match C layout");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }

        if (max_align > struct_align) {
            if (fail_fast) {
                sireflect_assert(false, "computed field alignment exceeds struct alignment");
            }
            sireflect_error_set("computed field alignment exceeds struct alignment");
            sireflect_free_parsed_fields(fields, field_count);
            *out_fields = NULL;
            *out_field_count = 0;
            return false;
        }
    }
#else
    (void)struct_size;
    (void)struct_align;
    (void)validate_layout;
#endif

    *out_fields = fields;
    *out_field_count = field_count;
    *out_struct_align = max_align;
    *out_struct_size = sireflect_align_up(offset, max_align);
    return true;
}

static sireflect_registry_t sireflect_global_registry;
static size_t sireflect_global_references;

bool sireflect_registry_is_initialized(void) {
    return sireflect_global_references != 0;
}

sireflect_registry_t *sireflect_registry_current(void) {
    sireflect_assert(sireflect_registry_is_initialized(), "sireflect must be initialized");
    return &sireflect_global_registry;
}

static char *sireflect_dup_cstr(const char *str) {
    sireflect_assert(str != NULL, "string must not be NULL");

    const size_t len = strlen(str);
    char *result = malloc(len + 1);
    sireflect_assert(result != NULL, "failed to allocate string");

    memcpy(result, str, len + 1);
    return result;
}

static char *
sireflect_format_array_type_name(const sireflect_type_info_t *element, size_t element_count) {
    sireflect_assert(element != NULL, "array element metadata must exist");

    const char *suffix = strchr(element->name, '[');
    if (element->kind != sireflect_kind_array || suffix == NULL) {
        const int name_len = snprintf(NULL, 0, "%s[%zu]", element->name, element_count);
        sireflect_assert(name_len > 0, "failed to format array type name");

        char *name = malloc((size_t)name_len + 1);
        sireflect_assert(name != NULL, "failed to allocate array type name");
        snprintf(name, (size_t)name_len + 1, "%s[%zu]", element->name, element_count);
        return name;
    }

    const size_t prefix_len = (size_t)(suffix - element->name);
    const int count_len = snprintf(NULL, 0, "[%zu]", element_count);
    sireflect_assert(count_len > 0, "failed to format array dimension");

    const size_t suffix_len = strlen(suffix);
    char *name = malloc(prefix_len + (size_t)count_len + suffix_len + 1);
    sireflect_assert(name != NULL, "failed to allocate array type name");

    memcpy(name, element->name, prefix_len);
    snprintf(name + prefix_len, (size_t)count_len + 1, "[%zu]", element_count);
    memcpy(name + prefix_len + (size_t)count_len, suffix, suffix_len + 1);

    return name;
}

static sireflect_handle_t sireflect_handle_from_index(size_t index) {
    return (sireflect_handle_t)(index + 1);
}

static size_t sireflect_index_from_handle(sireflect_handle_t handle) {
    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "type handle must be valid");
    return (size_t)(handle - 1);
}

sireflect_handle_t sireflect_registry_add_type(
    const char *name,
    sireflect_kind_t kind,
    size_t size,
    size_t align,
    sireflect_field_info_t *fields,
    size_t field_count
) {
    sireflect_registry_t *reg = sireflect_registry_current();

    sireflect_assert(name != NULL, "type name must not be NULL");
    sireflect_assert(size != 0 || kind == sireflect_kind_struct, "non-struct type size must not be zero");
    sireflect_assert(align != 0, "type alignment must not be zero");

    const sireflect_type_info_t type = {
        .name = sireflect_dup_cstr(name),
        .kind = kind,
        .size = size,
        .align = align,
        .fields =
            {
                .fields = fields,
                .field_count = field_count,
            },
        .element_type = SIREFLECT_INVALID_HANDLE,
        .element_count = 0,
    };
    sicore_vec_push(&reg->types, &type, sizeof(type));

    const uint32_t index = reg->types.size - 1;
    sicore_map_set(&reg->types_by_name, type.name, index);

    return sireflect_handle_from_index((size_t)index);
}

sireflect_handle_t
sireflect_registry_get_or_add_pointer_type(sireflect_handle_t pointee_type) {
    sireflect_assert(pointee_type != SIREFLECT_INVALID_HANDLE, "pointer pointee type must be valid");

    const sireflect_type_info_t *pointee = sireflect_registry_const_type_at(pointee_type);
    sireflect_assert(pointee != NULL, "pointer pointee metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s*", pointee->name);
    sireflect_assert(name_len > 0, "failed to format pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s*", pointee->name);

    sireflect_handle_t existing = sireflect_registry_handle_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        free(name);
        return existing;
    }

    sireflect_handle_t pointer_type = sireflect_registry_add_type(
        name,
        sireflect_kind_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *pointer_info = sireflect_registry_type_at(pointer_type);
    pointer_info->element_type = pointee_type;

    return pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_function_pointer_type(
    sireflect_handle_t return_type
) {
    sireflect_assert(return_type != SIREFLECT_INVALID_HANDLE, "function return type must be valid");

    const sireflect_type_info_t *return_info = sireflect_registry_const_type_at(return_type);
    sireflect_assert(return_info != NULL, "function return type metadata must exist");

    const int name_len = snprintf(NULL, 0, "%s(*)()", return_info->name);
    sireflect_assert(name_len > 0, "failed to format function pointer type name");

    char *name = malloc((size_t)name_len + 1);
    sireflect_assert(name != NULL, "failed to allocate function pointer type name");
    snprintf(name, (size_t)name_len + 1, "%s(*)()", return_info->name);

    sireflect_handle_t existing = sireflect_registry_handle_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        free(name);
        return existing;
    }

    sireflect_handle_t function_pointer_type = sireflect_registry_add_type(
        name,
        sireflect_kind_function_pointer,
        sizeof(ptr),
        _Alignof(ptr),
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *function_pointer_info =
        sireflect_registry_type_at(function_pointer_type);
    function_pointer_info->element_type = return_type;

    return function_pointer_type;
}

sireflect_handle_t sireflect_registry_get_or_add_array_type(
    sireflect_handle_t element_type,
    size_t element_count
) {
    sireflect_assert(element_type != SIREFLECT_INVALID_HANDLE, "array element type must be valid");
    sireflect_assert(element_count != 0, "array element count must not be zero");

    const sireflect_type_info_t *element = sireflect_registry_const_type_at(element_type);
    sireflect_assert(element != NULL, "array element metadata must exist");
    sireflect_assert(element->size <= SIZE_MAX / element_count, "array type size overflows size_t");

    char *name = sireflect_format_array_type_name(element, element_count);

    sireflect_handle_t existing = sireflect_registry_handle_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        free(name);
        return existing;
    }

    sireflect_handle_t array_type = sireflect_registry_add_type(
        name,
        sireflect_kind_array,
        element->size * element_count,
        element->align,
        NULL,
        0
    );
    free(name);

    sireflect_type_info_t *array_info = sireflect_registry_type_at(array_type);
    array_info->element_type = element_type;
    array_info->element_count = element_count;

    return array_type;
}

#define add_type(name, kind) \
    sireflect_registry_add_type(#name, kind, sizeof(name), _Alignof(name), NULL, 0)

#define add_named_type(c_type, reflected_name, kind) \
    sireflect_registry_add_type(reflected_name, kind, sizeof(c_type), _Alignof(c_type), NULL, 0)

static inline void sireflect_register_builtin_types(void) {
    add_type(u8, sireflect_kind_u8);
    add_type(u16, sireflect_kind_u16);
    add_type(u32, sireflect_kind_u32);
    add_type(u64, sireflect_kind_u64);
    add_type(i8, sireflect_kind_i8);
    add_type(i16, sireflect_kind_i16);
    add_type(i32, sireflect_kind_i32);
    add_type(i64, sireflect_kind_i64);
    add_type(f32, sireflect_kind_f32);
    add_type(f64, sireflect_kind_f64);
    add_type(bool, sireflect_kind_bool);
    add_type(char, sireflect_kind_char);
    add_type(ptr, sireflect_kind_ptr);

    add_type(uint8_t, sireflect_kind_u8);
    add_type(uint16_t, sireflect_kind_u16);
    add_type(uint32_t, sireflect_kind_u32);
    add_type(uint64_t, sireflect_kind_u64);
    add_type(int8_t, sireflect_kind_i8);
    add_type(int16_t, sireflect_kind_i16);
    add_type(int32_t, sireflect_kind_i32);
    add_type(int64_t, sireflect_kind_i64);

    add_type(float, sireflect_kind_f32);
    add_type(double, sireflect_kind_f64);
    add_type(short, sireflect_kind_short);
    add_type(int, sireflect_kind_int);
    add_type(long, sireflect_kind_long);

    add_named_type(signed char, "signed char", sireflect_kind_signed_char);
    add_named_type(unsigned char, "unsigned char", sireflect_kind_unsigned_char);
    add_named_type(unsigned short, "unsigned short", sireflect_kind_unsigned_short);
    add_named_type(unsigned int, "unsigned int", sireflect_kind_unsigned_int);
    add_named_type(unsigned long, "unsigned long", sireflect_kind_unsigned_long);
    add_named_type(long long, "long long", sireflect_kind_long_long);
    add_named_type(unsigned long long, "unsigned long long", sireflect_kind_unsigned_long_long);
}

void sireflect_init(void) {
    sireflect_error_clear();

    if (sireflect_global_references == 0) {
        sireflect_global_references = 1;
        sicore_vec_init(&sireflect_global_registry.types, sizeof(sireflect_type_info_t));
        sicore_map_init(&sireflect_global_registry.types_by_name);
        sireflect_register_builtin_types();
        return;
    }

    sireflect_assert(sireflect_global_references != SIZE_MAX, "sireflect reference count overflow");
    sireflect_global_references++;
}

static void sireflect_registry_clear(void) {
    sireflect_registry_t *reg = &sireflect_global_registry;

    for (uint32_t i = 0; i < reg->types.size; i++) {
        sireflect_type_info_t *type = sicore_vec_get_mut(&reg->types, i, sireflect_type_info_t);

        free((char *)type->name);

        for (size_t f = 0; f < type->fields.field_count; f++) {
            free((char *)type->fields.fields[f].name);
        }

        free(type->fields.fields);
    }

    sicore_map_fini(&reg->types_by_name);
    sicore_vec_fini(&reg->types);
    memset(reg, 0, sizeof(*reg));
}

void sireflect_fini(void) {
    sireflect_error_clear();

    sireflect_assert(sireflect_global_references != 0, "sireflect is not initialized");
    if (sireflect_global_references == 0) {
        return;
    }

    sireflect_global_references--;
    if (sireflect_global_references == 0) {
        sireflect_registry_clear();
    }
}

sireflect_handle_t sireflect_type_by_name(const char *name) {
    sireflect_error_clear();

    sireflect_registry_current();
    sireflect_assert(name != NULL, "type name must not be NULL");

    return sireflect_registry_handle_by_name(name);
}

sireflect_handle_t sireflect_registry_handle_by_name(const char *name) {
    const sireflect_registry_t *reg = sireflect_registry_current();
    const uint32_t index = sicore_map_get(&reg->types_by_name, name);

    if (index == UINT32_MAX) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_handle_from_index((size_t)index);
}

const sireflect_type_info_t *sireflect_registry_const_type_at(sireflect_handle_t handle) {
    const sireflect_registry_t *reg = sireflect_registry_current();

    const size_t index = sireflect_index_from_handle(handle);
    sireflect_assert(index < reg->types.size, "type handle is out of range");

    return sicore_vec_get(&reg->types, index, sireflect_type_info_t);
}

sireflect_type_info_t *sireflect_registry_type_at(sireflect_handle_t handle) {
    return (sireflect_type_info_t *)sireflect_registry_const_type_at(handle);
}

sireflect_handle_t
sireflect_try_register_struct(const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    if (!sireflect_registry_is_initialized() || desc == NULL || desc->name == NULL || desc->fields == NULL ||
        desc->align == 0) {
        sireflect_error_set(
            sireflect_registry_is_initialized() ? "invalid struct descriptor"
                                                 : "sireflect is not initialized"
        );
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(desc->name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        const sireflect_type_info_t *type = sireflect_type_info(existing);
        if (type->kind != sireflect_kind_struct || type->size != desc->size ||
            type->align != desc->align) {
            sireflect_error_set("existing type is incompatible with struct descriptor");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;
    size_t parsed_size = 0;
    size_t parsed_align = 0;

    if (!sireflect_parse_struct_fields(
        desc->name,
        desc->fields,
        &parsed_fields,
        &field_count,
        desc->size,
        desc->align,
        &parsed_size,
        &parsed_align,
        true,
        false
    )) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_registry_add_type(
        desc->name,
        sireflect_kind_struct,
        desc->size,
        desc->align,
        parsed_fields,
        field_count
    );
}

sireflect_handle_t
sireflect_register_struct(const sireflect_struct_desc_t *desc) {
    sireflect_error_clear();

    sireflect_assert(desc != NULL, "struct descriptor must not be NULL");
    sireflect_assert(desc->name != NULL, "struct descriptor name must not be NULL");
    sireflect_assert(desc->fields != NULL, "struct descriptor fields must not be NULL");
    sireflect_assert(desc->align != 0, "struct descriptor alignment must not be zero");

    sireflect_handle_t handle = SIREFLECT_INVALID_HANDLE;

    if (sireflect_registry_is_initialized() && desc != NULL && desc->name != NULL && desc->fields != NULL &&
        desc->align != 0) {
        sireflect_handle_t existing = sireflect_type_by_name(desc->name);
        if (existing != SIREFLECT_INVALID_HANDLE) {
            const sireflect_type_info_t *type = sireflect_type_info(existing);
            if (type->kind != sireflect_kind_struct || type->size != desc->size ||
                type->align != desc->align) {
                sireflect_assert(type->kind == sireflect_kind_struct, "existing type must be a struct");
                sireflect_assert(
                    type->size == desc->size,
                    "existing struct size must match descriptor"
                );
                sireflect_assert(
                    type->align == desc->align,
                    "existing struct alignment must match descriptor"
                );
                return SIREFLECT_INVALID_HANDLE;
            }
            return existing;
        }

        sireflect_field_info_t *parsed_fields = NULL;
        size_t field_count = 0;
        size_t parsed_size = 0;
        size_t parsed_align = 0;

        if (sireflect_parse_struct_fields(
                desc->name,
                desc->fields,
                &parsed_fields,
                &field_count,
                desc->size,
                desc->align,
                &parsed_size,
                &parsed_align,
                true,
                true
            )) {
            handle = sireflect_registry_add_type(
                desc->name,
                sireflect_kind_struct,
                desc->size,
                desc->align,
                parsed_fields,
                field_count
            );
        }
    }

    sireflect_assert(handle != SIREFLECT_INVALID_HANDLE, "failed to register struct");
    return handle;
}

sireflect_handle_t sireflect_try_register_dynamic_struct(
    const char *name,
    const char *fields
) {
    sireflect_error_clear();

    if (!sireflect_registry_is_initialized() || name == NULL || fields == NULL) {
        sireflect_error_set(
            sireflect_registry_is_initialized() ? "invalid dynamic struct descriptor"
                                                 : "sireflect is not initialized"
        );
        return SIREFLECT_INVALID_HANDLE;
    }

    sireflect_handle_t existing = sireflect_type_by_name(name);
    if (existing != SIREFLECT_INVALID_HANDLE) {
        if (!sireflect_type_is_struct(sireflect_type_info(existing))) {
            sireflect_error_set("existing type is not a struct");
            return SIREFLECT_INVALID_HANDLE;
        }
        return existing;
    }

    sireflect_field_info_t *parsed_fields = NULL;
    size_t field_count = 0;
    size_t size = 0;
    size_t align = 0;

    if (!sireflect_parse_struct_fields(
            name,
            fields,
            &parsed_fields,
            &field_count,
            0,
            1,
            &size,
            &align,
            false,
            false
        )) {
        return SIREFLECT_INVALID_HANDLE;
    }

    return sireflect_registry_add_type(
        name,
        sireflect_kind_struct,
        size,
        align,
        parsed_fields,
        field_count
    );
}

const char *sireflect_kind_name(sireflect_kind_t kind) {
    sireflect_error_clear();

    switch (kind) {
    case sireflect_kind_u8:
        return "u8";
    case sireflect_kind_u16:
        return "u16";
    case sireflect_kind_u32:
        return "u32";
    case sireflect_kind_u64:
        return "u64";
    case sireflect_kind_i8:
        return "i8";
    case sireflect_kind_i16:
        return "i16";
    case sireflect_kind_i32:
        return "i32";
    case sireflect_kind_i64:
        return "i64";
    case sireflect_kind_f32:
        return "f32";
    case sireflect_kind_f64:
        return "f64";
    case sireflect_kind_bool:
        return "bool";
    case sireflect_kind_char:
        return "char";
    case sireflect_kind_short:
        return "short";
    case sireflect_kind_int:
        return "int";
    case sireflect_kind_long:
        return "long";
    case sireflect_kind_ptr:
        return "ptr";
    case sireflect_kind_pointer:
        return "pointer";
    case sireflect_kind_struct:
        return "struct";
    case sireflect_kind_array:
        return "array";
    case sireflect_kind_signed_char:
        return "signed char";
    case sireflect_kind_unsigned_char:
        return "unsigned char";
    case sireflect_kind_unsigned_short:
        return "unsigned short";
    case sireflect_kind_unsigned_int:
        return "unsigned int";
    case sireflect_kind_unsigned_long:
        return "unsigned long";
    case sireflect_kind_long_long:
        return "long long";
    case sireflect_kind_unsigned_long_long:
        return "unsigned long long";
    case sireflect_kind_function_pointer:
        return "function pointer";
    }

    return "unknown";
}

bool sireflect_is_numeric(sireflect_kind_t kind) {
    sireflect_error_clear();

    switch (kind) {
    case sireflect_kind_u8:
    case sireflect_kind_u16:
    case sireflect_kind_u32:
    case sireflect_kind_u64:
    case sireflect_kind_i8:
    case sireflect_kind_i16:
    case sireflect_kind_i32:
    case sireflect_kind_i64:
    case sireflect_kind_f32:
    case sireflect_kind_f64:
    case sireflect_kind_char:
    case sireflect_kind_short:
    case sireflect_kind_int:
    case sireflect_kind_long:
    case sireflect_kind_signed_char:
    case sireflect_kind_unsigned_char:
    case sireflect_kind_unsigned_short:
    case sireflect_kind_unsigned_int:
    case sireflect_kind_unsigned_long:
    case sireflect_kind_long_long:
    case sireflect_kind_unsigned_long_long:
        return true;
    case sireflect_kind_bool:
    case sireflect_kind_ptr:
    case sireflect_kind_pointer:
    case sireflect_kind_struct:
    case sireflect_kind_array:
    case sireflect_kind_function_pointer:
        return false;
    }

    return false;
}

const sireflect_type_info_t *
sireflect_type_info(sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_registry_const_type_at(ref);
}

const sireflect_fields_t *
sireflect_type_fields(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    return &type->fields;
}

size_t sireflect_type_size(sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_type_info(ref)->size;
}

const char *sireflect_type_name(sireflect_handle_t ref) {
    sireflect_error_clear();

    return sireflect_type_info(ref)->name;
}

bool sireflect_type_is_struct(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_struct;
}

bool sireflect_type_is_array(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_array;
}

bool sireflect_type_is_pointer(const sireflect_type_info_t *info) {
    sireflect_error_clear();

    sireflect_assert(info != NULL, "type metadata must not be NULL");
    return info->kind == sireflect_kind_pointer || info->kind == sireflect_kind_function_pointer;
}

sireflect_handle_t
sireflect_type_element(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_type;
}

size_t
sireflect_type_element_count(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    sireflect_assert(type->kind == sireflect_kind_array, "type must be an array");
    return type->element_count;
}

sireflect_handle_t
sireflect_type_pointee(sireflect_handle_t ref) {
    sireflect_error_clear();

    const sireflect_type_info_t *type = sireflect_type_info(ref);
    sireflect_assert(
        type->kind == sireflect_kind_pointer || type->kind == sireflect_kind_function_pointer,
        "type must be a typed pointer"
    );
    return type->element_type;
}

