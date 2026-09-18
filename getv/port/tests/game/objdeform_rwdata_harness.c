/* ROM-free ABI regression: compile the production objDeform slot expression and
 * production modelGetNodeRwData with synthetic word-indexed runtime storage. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef int32_t s32;
typedef struct Vertex { int marker; } Vertex;
typedef struct ModelFileHeader { int unused; } ModelFileHeader;

enum {
    MODELNODE_OPCODE_HEADER = 1,
    MODELNODE_OPCODE_DLCOLLISION = 2,
    MODELNODE_OPCODE_OP07 = 3,
    MODELNODE_OPCODE_LOD = 4,
    MODELNODE_OPCODE_SWITCH = 5,
    MODELNODE_OPCODE_BSP = 6,
    MODELNODE_OPCODE_OP11 = 7,
    MODELNODE_OPCODE_GUNFIRE = 8,
    MODELNODE_OPCODE_HEAD = 9
};

typedef struct ModelRoData_DisplayList_CollisionRecord { u16 RwDataIndex; }
    ModelRoData_DisplayList_CollisionRecord;
typedef struct ModelRwData_HeadPlaceholderRecord {
    ModelFileHeader *ModelFileHeader;
    void *RwDatas;
} ModelRwData_HeadPlaceholderRecord;
union ModelRoData {
    struct { u16 RwDataIndex; } Header, Op07, LOD, Switch, BSP, Op11,
        Gunfire, HeadPlaceholder;
    ModelRoData_DisplayList_CollisionRecord DisplayListCollisions;
};
union ModelRwData {
    struct { Vertex *Vertices; } DisplayListCollisions;
    ModelRwData_HeadPlaceholderRecord HeadPlaceholder;
};
typedef struct ModelNode {
    int Opcode;
    union ModelRoData *Data;
    struct ModelNode *Parent;
} ModelNode;
typedef struct Model { union ModelRwData **datas; } Model;

#include "production_model_accessor.inc"

static Vertex **production_objdeform_slot(Model *model, ModelNode *nodeCopy,
                                          ModelRoData_DisplayList_CollisionRecord *rodata)
{
    Vertex **vtxslot;
#include "production_objdeform_assignment.inc"
    return vtxslot;
}

static int checks;
static int failures;
static void check(int condition, const char *label)
{
    checks++;
    printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    failures += !condition;
}

int main(void)
{
    _Alignas(16) unsigned char root_storage[64];
    _Alignas(16) unsigned char attached_storage[64];
    Vertex root_vertex = { 11 }, attached_vertex = { 22 }, decoy = { 33 };
    ModelRoData_DisplayList_CollisionRecord collision_ro = { 2 };
    union ModelRoData head_ro = { .HeadPlaceholder = { 0 } };
    ModelNode root_node = { MODELNODE_OPCODE_DLCOLLISION,
                            (union ModelRoData *)&collision_ro, 0 };
    ModelNode head_node = { MODELNODE_OPCODE_HEAD, &head_ro, 0 };
    Model model = { (union ModelRwData **)root_storage };
    Vertex **actual;
    Vertex **expected;
    Vertex **wrong;

    memset(root_storage, 0, sizeof root_storage);
    memset(attached_storage, 0, sizeof attached_storage);
    if (sizeof(void *) != 8) {
        fputs("FAIL: regression requires a 64-bit native pointer ABI\n", stderr);
        return 2;
    }
    expected = (Vertex **)(void *)(root_storage + 2 * sizeof(u32));
    wrong = (Vertex **)(void *)(root_storage + 2 * sizeof(void *));
    *expected = &root_vertex;
    *wrong = &decoy;
    actual = production_objdeform_slot(&model, &root_node, &collision_ro);
    check((unsigned char *)expected - root_storage == 8, "RwDataIndex=2 means byte offset 8");
    check((unsigned char *)wrong - root_storage == 16, "native pointer index means byte offset 16");
#if EXPECT_FIXED
    check(actual == expected, "production objDeform chooses the four-byte slot");
    check(*actual == &root_vertex, "production objDeform reads the intended vertex pointer");
#else
    check(actual == wrong, "unchanged objDeform chooses the wrong native slot");
    check(*actual == &decoy, "unchanged objDeform reads the decoy vertex pointer");
#endif
    check((Vertex **)(void *)modelGetNodeRwData(&model, &root_node) == expected,
          "production modelGetNodeRwData uses a four-byte word stride");

    /* An attached head changes the pool that owns the DLCOLLISION record. */
    ((ModelRwData_HeadPlaceholderRecord *)(void *)root_storage)->RwDatas = attached_storage;
    root_node.Parent = &head_node;
    expected = (Vertex **)(void *)(attached_storage + 2 * sizeof(u32));
    wrong = (Vertex **)(void *)(root_storage + 2 * sizeof(void *));
    *expected = &attached_vertex;
    actual = production_objdeform_slot(&model, &root_node, &collision_ro);
#if EXPECT_FIXED
    check(actual == expected, "production objDeform follows attached head pool");
    check(*actual == &attached_vertex, "attached pool yields intended vertex pointer");
#else
    check(actual == wrong, "unchanged objDeform bypasses attached head pool");
    check(actual != expected, "unchanged address differs from attached word slot");
#endif
    check((Vertex **)(void *)modelGetNodeRwData(&model, &root_node) == expected,
          "production accessor resolves attached head pool");
    printf("mode=%s checks=%d failures=%d\n", EXPECT_FIXED ? "fixed" : "vulnerable",
           checks, failures);
    return checks == 8 && failures == 0 ? 0 : 1;
}
