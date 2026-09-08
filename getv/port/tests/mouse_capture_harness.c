/* Compiled by tools/tests/test_mouse_capture.py with unchanged production source sections.
 * Real SDL headers verify the interface; these device stubs cannot capture the user's cursor. */
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_input.h"
#include "ge_mouse_accum.h"
#include "ge_console_input.c"

static int window_storage, other_storage;
static SDL_Window *wnd = (SDL_Window *)&window_storage;
static SDL_Window *other = (SDL_Window *)&other_storage;
static SDL_Window *keyboard_focus, *mouse_focus;
static Uint8 keys[SDL_NUM_SCANCODES];
static Uint32 buttons;
static SDL_bool relative;
static int motion_x, motion_y, attempts, fail_mode, idle;
static SDL_Event queue[8];
static int queue_count, queue_index;

const Uint8 *SDL_GetKeyboardState(int *n) { return keys; }
SDL_Window *SDL_GetKeyboardFocus(void) { return keyboard_focus; }
SDL_Window *SDL_GetMouseFocus(void) { return mouse_focus; }
Uint32 SDL_GetWindowID(SDL_Window *window) { return window == wnd ? 7 : 8; }
void SDL_GetWindowSize(SDL_Window *window, int *w, int *h) { *w = 640; *h = 480; }
Uint32 SDL_GetMouseState(int *x, int *y) { return buttons; }
SDL_bool SDL_GetRelativeMouseMode(void) { return relative; }
const char *SDL_GetError(void) { return "simulated capture failure"; }
int SDL_SetRelativeMouseMode(SDL_bool enabled)
{
    attempts++;
    if (fail_mode == 1) return -1;
    if (fail_mode == 0) relative = enabled;
    return 0;
}
Uint32 SDL_GetRelativeMouseState(int *x, int *y)
{
    if (x) *x = motion_x;
    if (y) *y = motion_y;
    motion_x = motion_y = 0;
    return buttons;
}
int SDL_PollEvent(SDL_Event *event)
{
    if (queue_index == queue_count) return 0;
    *event = queue[queue_index++];
    if (event->type == SDL_MOUSEBUTTONDOWN) buttons |= SDL_BUTTON(event->button.button);
    if (event->type == SDL_MOUSEBUTTONUP) buttons &= ~SDL_BUTTON(event->button.button);
    if (event->type == SDL_WINDOWEVENT && event->window.windowID == 7) {
        if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            keyboard_focus = mouse_focus = NULL;
        if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            keyboard_focus = mouse_focus = wnd;
    }
    return 1;
}

static int geKeyboardIdle(void) { return idle; }
int gePortInputDebugLevel(void) { return 0; }
#include "mouse.inc"

/* Dependencies of gfx_sdl_handle_events unrelated to mouse handoff. Console ownership itself
 * uses production ge_console_input.c above; its ImGui event adapter is represented here. */
static struct { int exiting_fullscreen, x, y, w, h, settings_changed; } configWindow;
#define IS_FULLSCREEN() 0
static void (*kb_all_keys_up)(void);
static int gePortImguiConsoleOpen(void) { return geConsoleInputOpen(); }
static int gePortImguiEvent(void *event) { return geConsoleInputCaptureActive(); }
static void gfx_sdl_onkeydown(int code) {}
static void gfx_sdl_onkeyup(int code) {}
static void game_exit(void) {}
static void gfx_sdl_set_fullscreen(void) {}
static void gfx_sdl_reset_dimension_and_pos(void) {}
#include "events.inc"

static int failures;
static void check(int condition, const char *name)
{
    printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    failures += !condition;
}
static struct GePadState poll(void)
{
    struct GePadState out = {0};
    geMousePoll(0, &out);
    return out;
}
static void release_buttons(void) { buttons = 0; (void)poll(); }
static void escape(int held) { keys[SDL_SCANCODE_ESCAPE] = held; (void)poll(); }
static void release_cursor(void) { escape(1); escape(0); }
static SDL_Event click_event(Uint32 type, int button, Uint32 id, int x, int y)
{
    SDL_Event event = {0};
    event.type = type;
    event.button.button = button;
    event.button.windowID = id;
    event.button.x = x;
    event.button.y = y;
    return event;
}
static void dispatch(SDL_Event event)
{
    queue[0] = event; queue_count = 1; queue_index = 0;
    gfx_sdl_handle_events();
}
static void click(void) { dispatch(click_event(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 7, 100, 100)); }
static void focus_event(int kind, Uint32 id)
{
    SDL_Event event = {0};
    event.type = SDL_WINDOWEVENT;
    event.window.event = kind;
    event.window.windowID = id;
    dispatch(event);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    const char *scenario = argv[1];
    printf("scenario: %s\n", scenario);
    unsetenv("GETV_MOUSE_SELFTEST"); unsetenv("GETV_MOUSE_SELFTEST_Y");
    setenv("GETV_MOUSE", strcmp(scenario, "disabled") == 0 ? "0" : "1", 1);
    setenv("GETV_MOUSE_SENS", "100", 1); setenv("GETV_MOUSE_INVERT", "0", 1);
    keyboard_focus = mouse_focus = wnd;
    geConsoleInputReset();
    if (strcmp(scenario, "idle") == 0) idle = 1;
    if (strcmp(scenario, "selftest-x") == 0) setenv("GETV_MOUSE_SELFTEST", "12", 1);
    if (strcmp(scenario, "selftest-y") == 0) setenv("GETV_MOUSE_SELFTEST_Y", "12", 1);
    if (strcmp(scenario, "unfocused-start") == 0) keyboard_focus = mouse_focus = NULL;
    struct GePadState out = poll();

    if (strcmp(scenario, "disabled") == 0 || strcmp(scenario, "idle") == 0 ||
        strncmp(scenario, "selftest-", 9) == 0) {
        check(attempts == 0, "initial poll never captures automated or disabled mouse");
        click();
        check(attempts == 0 && !relative, "click cannot capture automated or disabled mouse");
        if (strcmp(scenario, "selftest-x") == 0) check(out.rx != 0, "horizontal selftest still drives look");
        if (strcmp(scenario, "selftest-y") == 0) check(out.ry != 0, "vertical selftest still drives look");
        if (strcmp(scenario, "disabled") != 0) {
            ge_mouse_pend_x = 12000;
            focus_event(SDL_WINDOWEVENT_FOCUS_LOST, 7);
            check(ge_mouse_pend_x == 12000 && attempts == 0,
                  "focus loss does not alter automated mouse carry or capture");
        }
        return failures != 0;
    }
    if (strcmp(scenario, "unfocused-start") == 0) {
        check(attempts == 0, "unfocused first poll does not request capture");
        focus_event(SDL_WINDOWEVENT_FOCUS_GAINED, 7);
        click(); out = poll();
        check(relative && !out.rtrigger, "first focused click captures without firing");
        return failures != 0;
    }
    check(relative, "initial mouse capture");

    if (strcmp(scenario, "focus") == 0) {
        focus_event(SDL_WINDOWEVENT_FOCUS_LOST, 8);
        check(relative, "another window losing focus does not release game mouse");
        motion_x = 24; ge_mouse_pend_x = 4000;
        focus_event(SDL_WINDOWEVENT_FOCUS_LOST, 7);
        check(!relative, "game focus loss releases logical capture");
        check(ge_mouse_pend_x == 0 && motion_x == 0, "focus loss clears stale motion and carry");
        focus_event(SDL_WINDOWEVENT_FOCUS_GAINED, 7);
        check(!relative, "focus alone does not recapture");
        click(); out = poll();
        check(relative && !out.rtrigger && !out.rx, "activating click resumes without shot or jump");
        return failures != 0;
    }

    release_cursor();
    check(!relative, "Escape releases mouse");
    int before = attempts;
    if (strcmp(scenario, "ownership") == 0) {
        dispatch(click_event(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 7, 100, 100));
        check(attempts == before, "right click does not recapture");
        release_buttons();
        dispatch(click_event(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 8, 100, 100));
        check(attempts == before, "another window click does not recapture");
        release_buttons();
        const int coords[][2] = {{-1, 100}, {640, 100}, {100, -1}, {100, 480}};
        for (int i = 0; i < 4; i++) {
            dispatch(click_event(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 7, coords[i][0], coords[i][1]));
            check(attempts == before, "out-of-client click does not recapture");
            release_buttons();
        }
        mouse_focus = other; click();
        check(attempts == before, "mouse outside game does not recapture");
        mouse_focus = wnd; release_buttons();
        keyboard_focus = NULL; click();
        check(attempts == before, "unfocused click does not request capture");
        keyboard_focus = wnd; release_buttons();
        buttons = SDL_BUTTON_LMASK; (void)poll();
        check(attempts == before, "held drag without click event does not recapture");
        release_buttons();
        geConsoleInputSetOpen(1); gePortInputConsoleCapture(1); before = attempts;
        click(); out = poll();
        check(attempts == before && !out.rtrigger, "open console owns the click");
        geConsoleInputSetOpen(0); gePortInputConsoleCapture(0); before = attempts;
        click(); out = poll();
        check(attempts == before && !out.rtrigger, "console close quarantine owns held click");
        release_buttons(); click();
        check(relative, "fresh click works after console quarantine ends");
        return failures != 0;
    }
    if (strcmp(scenario, "failure") == 0) {
        fail_mode = 1; click();
        check(!relative && attempts == before + 1, "SDL capture failure leaves mouse released");
        check(!ge_mouse_capture_wanted, "failed capture does not claim capture intent");
        release_buttons(); fail_mode = 2; before = attempts; click();
        check(!relative && attempts == before + 1, "successful return with wrong mode is not accepted");
        release_buttons(); fail_mode = 0; click();
        check(relative && ge_mouse_capture_wanted, "fresh click retries failed capture");
        release_buttons(); fail_mode = 1; escape(1);
        check(relative && ge_mouse_capture_wanted, "failed Escape release preserves actual state and intent");
        return failures != 0;
    }

    motion_x = 72; motion_y = 48; ge_mouse_pend_x = 12000; ge_mouse_pend_y = 6000;
    click();
    check(relative && attempts == before + 1, "click event recaptures after Escape");
    out = poll();
    check(!out.rx && !out.ry, "recapture discards pre-capture motion and carry");
    check(!out.rtrigger && !out.rt_raw, "resume click does not fire");
    motion_x = 12; out = poll();
    check(out.rx != 0 && !out.rtrigger, "mouse look resumes while resume click is held");
    buttons |= SDL_BUTTON_RMASK; out = poll();
    check(!out.rtrigger && !out.ltrigger, "mouse actions stay blocked until all buttons release");
    release_buttons(); click(); out = poll();
    check(out.rtrigger && out.rt_raw == 32767, "fresh click after release fires normally");
    release_buttons(); release_cursor();
    queue[0] = click_event(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 7, 100, 100);
    queue[1] = click_event(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 7, 100, 100);
    queue_count = 2; queue_index = 0; gfx_sdl_handle_events(); out = poll();
    check(relative && !out.rtrigger, "click down and up between polls still recaptures without firing");
    for (int i = 0; i < 3; i++) {
        release_buttons(); release_cursor(); click(); out = poll();
        check(relative && !out.rtrigger, "repeated Escape-click cycle stays usable");
    }
    release_buttons(); escape(1); before = attempts; escape(1);
    check(!relative && attempts == before, "held Escape does not flap capture");
    escape(0); buttons = SDL_BUTTON_LMASK; escape(1); out = poll();
    check(relative && !out.rtrigger, "second Escape recaptures without leaking a held click");
    return failures != 0;
}
