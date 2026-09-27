#include "sireflect_error.h"

#include <stdlib.h>
#include <string.h>

static char *sireflect_current_error = NULL;
static bool sireflect_error_owned = false;
static char sireflect_out_of_memory_error[] = "failed to allocate error message";

static char *sireflect_error_dup(const char *message) {
    sireflect_assert(message != NULL, "error message must not be NULL");

    const size_t len = strlen(message);
    char *copy = malloc(len + 1);
    if (copy != NULL) memcpy(copy, message, len + 1);
    return copy;
}

void sireflect_error_clear(void) {
    if (sireflect_error_owned) free(sireflect_current_error);
    sireflect_current_error = NULL;
    sireflect_error_owned = false;
}

void sireflect_error_set(const char *message) {
    sireflect_error_clear();

    if (message == NULL) {
        return;
    }

    sireflect_current_error = sireflect_error_dup(message);
    if (sireflect_current_error == NULL) {
        sireflect_current_error = sireflect_out_of_memory_error;
    } else {
        sireflect_error_owned = true;
    }
}

const char *sireflect_error(void) {
    return sireflect_current_error;
}
