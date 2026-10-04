#ifndef GE_GDL_SOURCE_H
#define GE_GDL_SOURCE_H

#include <stdint.h>

enum GeGdlSourceKind {
    GE_GDL_SOURCE_NONE = 0,
    GE_GDL_SOURCE_BG_MISC = 1,
    GE_GDL_SOURCE_BG_PRIMARY = 2,
    GE_GDL_SOURCE_BG_SECONDARY = 3,
    GE_GDL_SOURCE_MODEL_CONVERTED = 10,
    GE_GDL_SOURCE_MODEL_GUNDL_PRIMARY = 11,
    GE_GDL_SOURCE_MODEL_GUNDL_SECONDARY = 12,
    GE_GDL_SOURCE_MODEL_COLLISION_DYNAMIC = 13,
    GE_GDL_SOURCE_MODEL_COLLISION_SECONDARY = 14,
    GE_GDL_SOURCE_MODEL_ROTTEX_PRIMARY = 15,
};

struct GeGdlSourceInfo {
    uint32_t kind;
    int32_t id;
    uint32_t aux;
    uintptr_t owner;
    char name[32];
};

void gePortGdlSourceRegister(const void *target, uint32_t kind, int32_t id,
                             const void *owner, uint32_t aux, const char *name);
int gePortGdlSourceLookup(const void *target, struct GeGdlSourceInfo *out);

#endif
