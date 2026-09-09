/* Mouse displacement is an angle, not a stick velocity. Keep fractions until the
 * game consumes a sample; never clamp travel to a per-tick turning speed. */
#ifndef GE_MOUSE_LOOK_H
#define GE_MOUSE_LOOK_H

typedef struct GeMouseLook {
    double yaw, pitch;
    int context; /* 0: discard, 1: on-foot gameplay, 2: classic vehicle controls */
} GeMouseLook;

static void geMouseLookClear(GeMouseLook *look)
{
    look->yaw = look->pitch = 0.0;
}

static void geMouseLookAdd(GeMouseLook *look, int dx, int dy, int sensitivity)
{
    if (look->context == 1) {
        /* 0.1 degree/count at 100%; independent of frame time and event batching. */
        look->yaw += (double)dx * sensitivity * 0.001;
        look->pitch -= (double)dy * sensitivity * 0.001;
    }
}

static int geMouseLookTake(GeMouseLook *look, int context, float *yaw, float *pitch)
{
    *yaw = *pitch = 0.0f;
    if (context != look->context || context != 1) geMouseLookClear(look);
    look->context = context;
    if (context != 1) return 0;
    *yaw = (float)look->yaw;
    *pitch = (float)look->pitch;
    geMouseLookClear(look);
    return 1;
}
#endif
