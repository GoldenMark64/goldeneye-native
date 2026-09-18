#include "production_model_slots.inc"
extern void *calloc(size_t, size_t);

/* These stub external allocation and initialization dependencies. The pool
 * decisions and release operations above come from production functions. */
struct ModelSlot *g_ModelSlots;
struct AnimModelSlot *g_AnimModelSlots;
s32 g_MaxModelSlots;
s32 g_MaxAnimModelSlots;
s32 g_ModelIsLvResetting;
static void *last_rwdata;

void *mempAllocBytesInBank(u32 size, u8 bank)
{
    (void)bank;
    return calloc(1, size);
}

void modelInit(Model *model, ModelFileHeader *header, u32 *data)
{
    last_rwdata = data;
    model->obj = header;
    model->datas = (union ModelRwData **)data;
}

void animInit(Model *model, ModelFileHeader *header, u32 *data)
{
    modelInit(model, header, data);
}

static int checks;
static int failures;
static void check(int condition, const char *message)
{
    printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    checks++;
    failures += !condition;
}

static void cycle(int animated, ModelFileHeader *header)
{
    Model *first = NULL;
    void *backing = NULL;
    int i;
    for (i = 0; i < 12; ++i) {
        Model *model = animated ? modelmgrInstantiateModelWithAnim(header)
                                : modelmgrInstantiateModel(header);
        check(model != NULL, animated ? "animated allocation through cycle 12"
                                      : "normal allocation through cycle 12");
        check(model && (first == NULL || model == first),
              animated ? "animated slot reused" : "normal slot reused");
        check(model && last_rwdata != NULL,
              animated ? "animated rwdata initialized" : "normal rwdata initialized");
        check(model && (backing == NULL || last_rwdata == backing),
              animated ? "animated rwdata backing reused" : "normal rwdata backing reused");
        if (model) {
            if (!first) first = model;
            if (!backing) backing = last_rwdata;
            if (animated) clear_aircraft_model_obj(model);
            else clear_model_obj(model);
        } else if (animated) {
            puts("animated slot exhausted after release");
        }
    }
}

int main(void)
{
    ModelFileHeader header = {0};
    Model *fallback;
    Model *pooled;
    header.numRecords = 20;
    modelmgrAllocateModelSlots(0);
    modelmgrAllocateAnimModelSlots(0);
    cycle(1, &header);
    cycle(0, &header);

    g_ModelIsLvResetting = 1;
    fallback = modelmgrInstantiateModelWithAnim(&header);
    check(fallback != NULL, "animated fallback allocated");
    check(fallback && (void *)fallback != (void *)&g_AnimModelSlots[0],
          "animated fallback is outside pool");
    if (fallback) clear_aircraft_model_obj(fallback);
    g_ModelIsLvResetting = 0;
    pooled = modelmgrInstantiateModelWithAnim(&header);
    check(pooled != NULL, "pooled animation survives fallback release");
    if (pooled) clear_aircraft_model_obj(pooled);

    g_ModelIsLvResetting = 1;
    fallback = modelmgrInstantiateModel(&header);
    check(fallback != NULL, "normal fallback allocated");
    check(fallback && (void *)fallback != (void *)&g_ModelSlots[0],
          "normal fallback is outside pool");
    if (fallback) clear_model_obj(fallback);
    g_ModelIsLvResetting = 0;
    pooled = modelmgrInstantiateModel(&header);
    check(pooled != NULL, "pooled model survives fallback release");
    if (pooled) clear_model_obj(pooled);

    pooled = modelmgrInstantiateModelWithAnim(&header);
    g_ModelIsLvResetting = 1;
    fallback = modelmgrInstantiateModelWithAnim(&header);
    g_ModelIsLvResetting = 0;
    if (fallback) clear_aircraft_model_obj(fallback);
    {
        Model *second = modelmgrInstantiateModelWithAnim(&header);
        check(pooled && second && second != pooled,
              "animated fallback release leaves occupied pool slot claimed");
        if (second) clear_aircraft_model_obj(second);
    }
    if (pooled) clear_aircraft_model_obj(pooled);

    pooled = modelmgrInstantiateModel(&header);
    g_ModelIsLvResetting = 1;
    fallback = modelmgrInstantiateModel(&header);
    g_ModelIsLvResetting = 0;
    if (fallback) clear_model_obj(fallback);
    {
        Model *second = modelmgrInstantiateModel(&header);
        check(pooled && second && second != pooled,
              "normal fallback release leaves occupied pool slot claimed");
        if (second) clear_model_obj(second);
    }
    if (pooled) clear_model_obj(pooled);

    check(g_AnimModelSlots[0].unk02 >= header.numRecords,
          "animated slot capacity metadata preserved");
    check(g_ModelSlots[0].unk02 >= header.numRecords,
          "normal slot capacity metadata preserved");
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
