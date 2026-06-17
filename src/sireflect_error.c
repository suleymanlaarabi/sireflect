#include "sireflect_error.h"

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
