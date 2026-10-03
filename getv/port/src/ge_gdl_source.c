#include "ge_gdl_source.h"

#include <stddef.h>
#include <string.h>

#define GE_GDL_SOURCE_SLOTS 8192u

struct GeGdlSourceEntry {
    uintptr_t target;
    struct GeGdlSourceInfo info;
};

static struct GeGdlSourceEntry ge_gdl_sources[GE_GDL_SOURCE_SLOTS];

static uint32_t geGdlSourceHash(uintptr_t p)
{
    p >>= 4;
    p ^= p >> 17;
    p ^= p >> 9;
    return (uint32_t)p & (GE_GDL_SOURCE_SLOTS - 1u);
}

void gePortGdlSourceRegister(const void *target, uint32_t kind, int32_t id,
                             const void *owner, uint32_t aux, const char *name)
{
    uintptr_t key = (uintptr_t)target;
    uint32_t start;
    uint32_t probe;

    if (key == 0) return;

    start = geGdlSourceHash(key);
    for (probe = 0; probe < GE_GDL_SOURCE_SLOTS; probe++) {
        struct GeGdlSourceEntry *e = &ge_gdl_sources[(start + probe) & (GE_GDL_SOURCE_SLOTS - 1u)];
        if (e->target == 0 || e->target == key) {
            if (e->target == 0) memset(e, 0, sizeof(*e));
            e->target = key;
            e->info.kind = kind;
            e->info.id = id;
            e->info.aux = aux;
            e->info.owner = (uintptr_t)owner;
            if (name != NULL && *name != '\0') {
                size_t n = strlen(name);
                if (n >= sizeof(e->info.name)) n = sizeof(e->info.name) - 1;
                memcpy(e->info.name, name, n);
                e->info.name[n] = '\0';
            }
            return;
        }
    }
}

int gePortGdlSourceLookup(const void *target, struct GeGdlSourceInfo *out)
{
    uintptr_t key = (uintptr_t)target;
    uint32_t start;
    uint32_t probe;

    if (key == 0 || out == NULL) return 0;

    start = geGdlSourceHash(key);
    for (probe = 0; probe < GE_GDL_SOURCE_SLOTS; probe++) {
        const struct GeGdlSourceEntry *e = &ge_gdl_sources[(start + probe) & (GE_GDL_SOURCE_SLOTS - 1u)];
        if (e->target == key) {
            *out = e->info;
            return 1;
        }
        if (e->target == 0) return 0;
    }
    return 0;
}
