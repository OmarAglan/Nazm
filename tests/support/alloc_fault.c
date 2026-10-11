#include "alloc_fault.h"
#include "alloc/sysalloc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NAZM_ALLOC_FAULT_INJECTION
#error "alloc_fault.c belongs only to builds with NAZM_ALLOC_FAULT_INJECTION"
#endif

static size_t points;
static size_t failures;
static size_t live;
static size_t armed_point;   /* 0 means disarmed */
static bool   armed_sticky;
static bool   configured;

static void report_at_exit(void) {
    fprintf(stderr, "alloc-fault: points=%lu failures=%lu live=%lu\n",
            (unsigned long)points, (unsigned long)failures,
            (unsigned long)live);
}

static bool environment_flag(const char *name) {
    const char *value = getenv(name);
    return value != NULL && value[0] != '\0'
        && !(value[0] == '0' && value[1] == '\0');
}

/* Read the environment once, before the first allocation point is counted. */
static void configure_from_environment(void) {
    if (configured) {
        return;
    }
    configured = true;

    const char *at = getenv("NAZM_ALLOC_FAULT_AT");
    if (at != NULL && at[0] != '\0') {
        char *end = NULL;
        unsigned long long value = strtoull(at, &end, 10);
        if (end != at && *end == '\0') {
            armed_point = (size_t)value;
            armed_sticky = environment_flag("NAZM_ALLOC_FAULT_STICKY");
        }
    }
    if (environment_flag("NAZM_ALLOC_FAULT_REPORT")) {
        atexit(report_at_exit);
    }
}

/* Count one allocation point and decide whether it fails. */
static bool next_point_fails(void) {
    configure_from_environment();
    points++;
    if (armed_point == 0) {
        return false;
    }
    if (points == armed_point || (armed_sticky && points > armed_point)) {
        failures++;
        return true;
    }
    return false;
}

void alloc_fault_reset(void) {
    configured = true;
    points = 0;
    failures = 0;
    armed_point = 0;
    armed_sticky = false;
}

void alloc_fault_arm(size_t point, bool sticky) {
    configured = true;
    armed_point = point;
    armed_sticky = sticky;
}

size_t alloc_fault_points(void) {
    return points;
}

size_t alloc_fault_failures(void) {
    return failures;
}

size_t alloc_fault_live(void) {
    return live;
}

void *nazm_sys_malloc(size_t size) {
    if (next_point_fails()) {
        errno = ENOMEM;
        return NULL;
    }
    void *block = malloc(size);
    if (block != NULL) {
        live++;
    }
    return block;
}

void *nazm_sys_calloc(size_t count, size_t size) {
    if (next_point_fails()) {
        errno = ENOMEM;
        return NULL;
    }
    void *block = calloc(count, size);
    if (block != NULL) {
        live++;
    }
    return block;
}

void nazm_sys_free(void *pointer) {
    if (pointer == NULL) {
        return;
    }
    live--;
    free(pointer);
}

bool nazm_sys_arena_fault(void) {
    return next_point_fails();
}
