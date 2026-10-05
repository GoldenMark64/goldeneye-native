#include <math.h>
#include <stdio.h>
#include <string.h>

typedef int s32;
typedef float f32;

#define M_TAU_F 6.28318530717958647692f

struct coord3d {
    f32 f[3];
};

typedef struct Mtxf {
    f32 m[4][4];
} Mtxf;

struct PadRecord {
    struct coord3d pos;
};

struct ModelNode {
    void *Data;
};

struct ModelFileHeader {
    struct ModelNode **Switches;
};

struct Model {
    struct ModelFileHeader *obj;
    f32 scale;
};

struct PropRecord {
    struct coord3d pos;
};

struct ObjectRecord {
    int unused;
};

typedef struct CCTVRecord {
    struct Model *model;
    struct PropRecord *prop;

    /* The native collision this regression is about. */
    s32 pad;
    s32 lookpad;

    Mtxf mtx;
    Mtxf unk84;

    f32 unkC4;
    f32 unkC8;
    f32 unkCC;
    f32 unkD0;
    s32 unkD4;
    f32 unkD8;
    f32 unkDC;

    s32 timer;
    s32 convert_to_f32;
    f32 unkE8;
} CCTVRecord;

static struct PadRecord pads[8];
static struct PadRecord boundpads[8];

struct {
    struct PadRecord *pads;
    struct PadRecord *boundpads;
} g_CurrentSetup = {
    pads,
    boundpads,
};

static int matrix_calls;
static f32 captured_target_x;

static void domakedefaultobj(s32 stage, struct ObjectRecord *obj, s32 cmdindex)
{
    (void)stage;
    (void)obj;
    (void)cmdindex;
}

static s32 isNotBoundPad(s32 pad)
{
    return pad < 100;
}

static s32 getBoundPadNum(s32 pad)
{
    return pad - 100;
}

static void mtx4RotateVecInPlace(Mtxf *mtx, struct coord3d *vec)
{
    (void)mtx;
    (void)vec;
}

static void matrix_4x4_set_basis_and_position_target(
    Mtxf *mtx,
    f32 x, f32 y, f32 z,
    f32 target_x, f32 target_y, f32 target_z,
    f32 up_x, f32 up_y, f32 up_z)
{
    (void)mtx;
    (void)x;
    (void)y;
    (void)z;
    (void)target_y;
    (void)target_z;
    (void)up_x;
    (void)up_y;
    (void)up_z;

    matrix_calls++;
    captured_target_x = target_x;
}

static void matrix_scalar_multiply(f32 scale, f32 *row)
{
    (void)scale;
    (void)row;
}

#include "cctv_production.inc"

static int failures;
static int checks;

static void check(int condition, const char *what)
{
    checks++;

    if (condition) {
        printf("  PASS  %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

static void reset_capture(void)
{
    matrix_calls = 0;
    captured_target_x = 9999.0f;
}

static CCTVRecord make_camera(
    struct Model *model,
    struct PropRecord *prop,
    s32 pad,
    s32 lookpad)
{
    CCTVRecord camera;

    memset(&camera, 0, sizeof(camera));
    camera.model = model;
    camera.prop = prop;
    camera.pad = pad;
    camera.lookpad = lookpad;

    /* Keep the test focused on target-pad selection. */
    camera.convert_to_f32 = 1;

    return camera;
}

int main(void)
{
    struct coord3d switch_position = {{0.0f, 0.0f, 0.0f}};
    struct ModelNode node = {&switch_position};
    struct ModelNode *switches[] = {&node};
    struct ModelFileHeader header = {switches};
    struct Model model = {&header, 1.0f};
    struct PropRecord prop;

    CCTVRecord camera;

    memset(&prop, 0, sizeof(prop));
    memset(pads, 0, sizeof(pads));
    memset(boundpads, 0, sizeof(boundpads));

    pads[1].pos.f[0] = 11.0f;
    pads[2].pos.f[0] = 22.0f;
    boundpads[1].pos.f[0] = 33.0f;

    puts("== unbound lookpad differs from inherited placement pad ==");
    camera = make_camera(&model, &prop, 1, 2);
    reset_capture();
    setupCctv(9, &camera, 0);

    check(matrix_calls == 1,
          "camera setup executes for a valid unbound lookpad");
    check(fabsf(captured_target_x - (-22.0f)) < 0.001f,
          "camera targets pads[lookpad], not pads[ObjectRecord.pad]");

    puts("\n== bound lookpad differs from inherited placement pad ==");
    camera = make_camera(&model, &prop, 1, 101);
    reset_capture();
    setupCctv(9, &camera, 1);

    check(matrix_calls == 1,
          "camera setup executes for a valid bound lookpad");
    check(fabsf(captured_target_x - (-33.0f)) < 0.001f,
          "camera resolves boundpads[getBoundPadNum(lookpad)]");

    puts("\n== inherited placement pad must not gate camera aiming ==");
    camera = make_camera(&model, &prop, -1, 2);
    reset_capture();
    setupCctv(9, &camera, 2);

    check(matrix_calls == 1,
          "valid lookpad still aims when inherited placement pad is -1");

    puts("\n== negative lookpad disables camera target setup ==");
    camera = make_camera(&model, &prop, 1, -1);
    reset_capture();
    setupCctv(9, &camera, 3);

    check(matrix_calls == 0,
          "negative lookpad suppresses target setup despite valid inherited pad");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
