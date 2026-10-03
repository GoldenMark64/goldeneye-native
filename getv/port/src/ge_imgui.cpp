/* Dear ImGui developer overlay and console for the native build.
 *
 * ImGui is C++ and the rest of the port is C, so ge_imgui.h is the only renderer-facing
 * boundary. The command parser, queue and results stay in ge_console.c; this file is only a
 * bounded UI producer. Submitted work still executes at gePortConsoleGameTick(), never here.
 *
 * OpenGL uses ImGui's fixed-function GL2 backend because Fast3D deliberately creates a legacy
 * 2.1 context. Fast3D leaves a shader, VBO and generic vertex attributes bound, so those states
 * are saved, cleared and restored around the overlay draw. Metal uses the helpers in
 * gfx_metal.mm to append a load-preserving pass to the same drawable before it is presented.
 *
 * GETV_IMGUI_PROBE=<frame> is the smoke-test hook. OpenGL counts pixels changed by the overlay;
 * Metal reports whether draw data was encoded, plus the common draw-list/vertex counts.
 */

#include "ge_imgui.h"
#include "ge_imgui_policy.h"
#include "ge_diagnostic_capture.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#if defined(RAPI_METAL)
#include "../fast3d/gfx_metal.h"
#else
#define GL_GLEXT_PROTOTYPES 1
#if defined(_WIN32)
#define GLEW_STATIC
#include <GL/glew.h>
#endif
#if defined(USE_GLES)
#include <SDL2/SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif
#endif

namespace {

int ge_diagnostic_event(void *sdl_event)
{
    if (sdl_event == NULL) return 0;
    SDL_Event *event = (SDL_Event *)sdl_event;

    if ((event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) &&
        event->key.keysym.scancode == SDL_SCANCODE_F3) {
        if (event->type == SDL_KEYDOWN && event->key.repeat == 0) {
            gePortDiagnosticRequest();
#if defined(RAPI_METAL)
            if (gePortDiagnosticScreenshotPath() != NULL) {
                gePortMetalArmDiagnosticCapture();
            }
#endif
        }
        return 1;
    }
    return 0;
}

#if !defined(RAPI_METAL)
int ge_diagnostic_write_bmp(const char *path, const unsigned char *pixels, int width, int height)
{
    const int pad = (4 - (width * 3) % 4) % 4;
    const unsigned long image_size = (unsigned long)(width * 3 + pad) * height;
    const unsigned long file_size = 54 + image_size;
    static const unsigned char zero[3] = {0, 0, 0};
    unsigned char header[54] = {0};
    FILE *out = fopen(path, "wb");

    if (out == NULL) {
        fprintf(stderr, "[getv][diag] screenshot fopen failed for '%s': %s\n",
                path, strerror(errno));
        fflush(stderr);
        return 0;
    }

    header[0] = 'B'; header[1] = 'M';
    header[2] = (unsigned char)file_size;
    header[3] = (unsigned char)(file_size >> 8);
    header[4] = (unsigned char)(file_size >> 16);
    header[5] = (unsigned char)(file_size >> 24);
    header[10] = 54; header[14] = 40;
    header[18] = (unsigned char)width;
    header[19] = (unsigned char)(width >> 8);
    header[20] = (unsigned char)(width >> 16);
    header[21] = (unsigned char)(width >> 24);
    header[22] = (unsigned char)height;
    header[23] = (unsigned char)(height >> 8);
    header[24] = (unsigned char)(height >> 16);
    header[25] = (unsigned char)(height >> 24);
    header[26] = 1; header[28] = 24;
    header[34] = (unsigned char)image_size;
    header[35] = (unsigned char)(image_size >> 8);
    header[36] = (unsigned char)(image_size >> 16);
    header[37] = (unsigned char)(image_size >> 24);
    fwrite(header, 1, sizeof header, out);

    for (int y = 0; y < height; y++) {
        const unsigned char *row = pixels + (size_t)y * width * 3;
        for (int x = 0; x < width; x++) {
            unsigned char bgr[3] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
            fwrite(bgr, 1, sizeof bgr, out);
        }
        if (pad) fwrite(zero, 1, (size_t)pad, out);
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "[getv][diag] screenshot fclose failed for '%s': %s\n",
                path, strerror(errno));
        fflush(stderr);
        return 0;
    }
    return 1;
}

void ge_diagnostic_capture_opengl(void)
{
    const char *path = gePortDiagnosticScreenshotPath();
    SDL_Window *window;
    unsigned char *pixels;
    GLint old_pack_alignment = 4;
    int width = 0;
    int height = 0;
    int ok;

    if (path == NULL) return;

    window = SDL_GL_GetCurrentWindow();
    if (window != NULL) SDL_GL_GetDrawableSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        gePortDiagnosticScreenshotComplete(0, 0, 0);
        return;
    }

    pixels = (unsigned char *)malloc((size_t)width * height * 3);
    if (pixels == NULL) {
        gePortDiagnosticScreenshotComplete(0, 0, 0);
        return;
    }

    glGetIntegerv(GL_PACK_ALIGNMENT, &old_pack_alignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_PACK_ALIGNMENT, old_pack_alignment);

    ok = ge_diagnostic_write_bmp(path, pixels, width, height);
    free(pixels);
    gePortDiagnosticScreenshotComplete(ok, width, height);
    if (ok) {
        fprintf(stderr, "[getv][diag] screenshot -> %s (%dx%d)\n", path, width, height);
        fflush(stderr);
    }
}
#else
void ge_diagnostic_capture_opengl(void) {}
#endif

} /* namespace */

#if defined(GE_WITH_IMGUI)

#include "ge_console.h"
#include "ge_console_input.h"
#include "ge_console_pause.h"
#include "port_input.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"

extern "C" unsigned long gePlayerTick(void);
extern "C" unsigned long gePortRenderedFrame(void);

#if defined(RAPI_METAL)
extern "C" int  gePortMetalImguiInit(void);
extern "C" void gePortMetalImguiBeginPass(void);
extern "C" int  gePortMetalImguiRenderDrawData(void *draw_data);
extern "C" void gePortMetalImguiEndPass(void);
extern "C" void gePortMetalImguiShutdown(void);
#else
#include "imgui_impl_opengl2.h"
#endif

namespace {

constexpr int GE_IMGUI_ATTRIBS = 8;
constexpr int GE_CONSOLE_UI_HISTORY = 32;
constexpr int GE_CONSOLE_UI_COMPLETIONS = 16;

bool g_active;
GeImguiPolicy g_policy;
bool g_frame_open;
bool g_focus_console_input;
bool g_skip_toggle_text;
unsigned long g_frames;
int g_probe_frame = -2;
SDL_Scancode g_toggle_scancode = SDL_SCANCODE_UNKNOWN;
char g_console_line[GE_CONSOLE_MAX_LINE];
char g_help_filter[64];
char g_input_history[GE_CONSOLE_UI_HISTORY][GE_CONSOLE_MAX_LINE];
int g_input_history_count;
int g_input_history_pos = -1;

bool ge_env_on(const char *name)
{
    const char *e = getenv(name);
    return e != NULL && *e != '\0' && strcmp(e, "0") != 0;
}

SDL_Scancode ge_console_toggle_scancode()
{
    if (g_toggle_scancode != SDL_SCANCODE_UNKNOWN) return g_toggle_scancode;

    const char *name = getenv("GETV_CONSOLE_KEY");
    if (name == NULL || *name == '\0' || SDL_strcasecmp(name, "grave") == 0 ||
        SDL_strcasecmp(name, "backquote") == 0) {
        g_toggle_scancode = SDL_SCANCODE_GRAVE;
    } else {
        g_toggle_scancode = SDL_GetScancodeFromName(name);
        if (g_toggle_scancode == SDL_SCANCODE_UNKNOWN) {
            printf("[getv][console] unknown GETV_CONSOLE_KEY \"%s\"; using backquote\n", name);
            g_toggle_scancode = SDL_SCANCODE_GRAVE;
        }
    }
    return g_toggle_scancode;
}

const char *ge_console_toggle_name()
{
    const char *name = SDL_GetScancodeName(ge_console_toggle_scancode());
    return (name != NULL && *name != '\0') ? name : "backquote";
}

void ge_console_set_open(bool open)
{
    if (open == (geConsoleInputOpen() != 0)) return;
    geConsoleInputSetOpen(open ? 1 : 0);
    geConsolePauseRequest(open ? 1 : 0);
    gePortInputConsoleCapture(open ? 1 : 0);
    g_focus_console_input = open;
    g_input_history_pos = -1;
    printf("[getv][console] %s (toggle: %s)\n", open ? "open" : "closed",
           ge_console_toggle_name());
    fflush(stdout);
}

bool ge_console_input_event(Uint32 type)
{
    switch (type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            return true;
        default:
            return false;
    }
}

void ge_console_history_add(const char *line)
{
    if (line == NULL || *line == '\0') return;
    if (g_input_history_count > 0 &&
        strcmp(g_input_history[g_input_history_count - 1], line) == 0) return;

    if (g_input_history_count == GE_CONSOLE_UI_HISTORY) {
        memmove(g_input_history[0], g_input_history[1],
                sizeof(g_input_history[0]) * (GE_CONSOLE_UI_HISTORY - 1));
        g_input_history_count--;
    }
    snprintf(g_input_history[g_input_history_count], sizeof(g_input_history[0]), "%s", line);
    g_input_history_count++;
}

int ge_console_input_callback(ImGuiInputTextCallbackData *data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (g_input_history_pos < 0) g_input_history_pos = g_input_history_count - 1;
            else if (g_input_history_pos > 0) g_input_history_pos--;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (g_input_history_pos >= 0 && ++g_input_history_pos >= g_input_history_count)
                g_input_history_pos = -1;
        }
        const char *replacement =
            g_input_history_pos >= 0 ? g_input_history[g_input_history_pos] : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, replacement);
    } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
        GeConsoleCompletion matches[GE_CONSOLE_UI_COMPLETIONS];
        int truncated = 0;
        unsigned int total = geConsoleComplete(data->Buf, matches,
                                               GE_CONSOLE_UI_COMPLETIONS, &truncated);
        if (total == 1) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, matches[0].name);
            data->InsertChars(data->BufTextLen, " ");
        }
        (void)truncated;
    }
    return 0;
}

bool ge_contains_case_insensitive(const char *text, const char *needle)
{
    if (needle == NULL || *needle == '\0') return true;
    if (text == NULL) return false;
    const size_t n = strlen(needle);
    for (const char *p = text; *p != '\0'; p++) {
        if (SDL_strncasecmp(p, needle, n) == 0) return true;
    }
    return false;
}

void ge_draw_dev_overlay()
{
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 132.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("GoldenEye -- dev overlay")) {
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("frame   %lu", g_frames);
        ImGui::Text("fps     %.1f  (%.2f ms/frame)",
                    (double)io.Framerate,
                    1000.0 / (double)(io.Framerate > 0.0f ? io.Framerate : 1.0f));
        ImGui::Text("display %.0f x %.0f", (double)io.DisplaySize.x, (double)io.DisplaySize.y);
        ImGui::Text("console %s  [%s]", geConsoleInputOpen() ? "OPEN" : "closed",
                    ge_console_toggle_name());
        ImGui::Text("policy  %s", geConsolePauseStateName(geConsolePauseState()));
    }
    ImGui::End();
}

void ge_draw_result(const GeConsoleResult &result)
{
    ImVec4 colour(0.75f, 0.80f, 0.85f, 1.0f);
    if (result.severity == GE_CONSOLE_SEVERITY_WARNING)
        colour = ImVec4(1.0f, 0.78f, 0.25f, 1.0f);
    else if (result.severity == GE_CONSOLE_SEVERITY_ERROR)
        colour = ImVec4(1.0f, 0.38f, 0.32f, 1.0f);

    const char *message = result.message[0] != '\0'
        ? result.message : geConsoleStatusName(result.status);
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::TextWrapped("[tick %llu / frame %llu] %s: %s",
                       (unsigned long long)(result.execution_tick
                           ? result.execution_tick : result.submission_tick),
                       (unsigned long long)(result.execution_frame
                           ? result.execution_frame : result.submission_frame),
                       geConsoleStatusName(result.status), message);
    ImGui::PopStyleColor();

    GeConsoleCommandSpec spec;
    if (result.command_id != 0 && geConsoleCommandById(result.command_id, &spec) &&
        (spec.flags & GE_CONSOLE_CMD_DIAGNOSTIC_SAFE) != 0) {
        ImGui::SameLine();
        ImGui::PushID((int)result.sequence);
        if (ImGui::SmallButton("copy")) ImGui::SetClipboardText(message);
        ImGui::PopID();
    }
}

void ge_draw_console()
{
    if (!geConsoleInputOpen()) return;

    ImGui::SetNextWindowPos(ImVec2(45.0f, 45.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(780.0f, 520.0f), ImGuiCond_FirstUseEver);
    bool open = true;
    if (ImGui::Begin("GoldenEye developer console", &open, ImGuiWindowFlags_NoCollapse)) {
        GeConsoleHistoryInfo info;
        geConsoleHistoryInfo(&info);
        ImGui::Text("Pause policy: %s", geConsolePauseStateName(geConsolePauseState()));
        ImGui::TextDisabled("structured results %u/%u, dropped %llu; raw input stays in memory only",
                            info.count, info.capacity, (unsigned long long)info.dropped);

        if (ImGui::BeginChild("console_scrollback", ImVec2(0.0f, 235.0f), true,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            if (info.count == 0) ImGui::TextDisabled("No results yet.");
            for (unsigned int i = 0; i < info.count; i++) {
                GeConsoleResult result;
                if (geConsoleResultAt(i, &result)) ge_draw_result(result);
            }
        }
        ImGui::EndChild();

        ImGui::SeparatorText("Searchable command help");
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##console_help", "filter command names or summaries",
                                 g_help_filter, sizeof(g_help_filter));
        if (ImGui::BeginChild("console_help", ImVec2(0.0f, 105.0f), true)) {
            unsigned int count = geConsoleCommandCount();
            if (count == 0) ImGui::TextDisabled("No command handlers registered in this build yet.");
            for (unsigned int i = 0; i < count; i++) {
                GeConsoleCommandSpec spec;
                if (!geConsoleCommandAt(i, &spec)) continue;
                if (!ge_contains_case_insensitive(spec.name, g_help_filter) &&
                    !ge_contains_case_insensitive(spec.summary, g_help_filter)) continue;
                ImGui::Text("%s", spec.name);
                ImGui::SameLine();
                ImGui::TextDisabled("-- %s", spec.summary);
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (g_focus_console_input) {
            ImGui::SetKeyboardFocusHere();
            g_focus_console_input = false;
        }
        ImGui::SetNextItemWidth(-1.0f);
        const ImGuiInputTextFlags flags =
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackCompletion |
            ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputTextWithHint("##console_command", "command (Tab completes, Up/Down history)",
                                     g_console_line, sizeof(g_console_line), flags,
                                     ge_console_input_callback)) {
            ge_console_history_add(g_console_line);
            (void)geConsoleSubmit(g_console_line, (uint64_t)gePlayerTick(),
                                  (uint64_t)gePortRenderedFrame(), NULL);
            g_console_line[0] = '\0';
            g_input_history_pos = -1;
            g_focus_console_input = true;
        }
    }
    ImGui::End();
    if (!open) ge_console_set_open(false);
}

#if !defined(RAPI_METAL)
void ge_imgui_probe_rect(int *x, int *y, int *w, int *h)
{
    int fw = 0, fh = 0;
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (win) SDL_GL_GetDrawableSize(win, &fw, &fh);
    if (fw <= 0 || fh <= 0) { *x = *y = *w = *h = 0; return; }
    int rw = fw < 900 ? fw : 900;
    int rh = fh < 600 ? fh : 600;
    *x = 0; *y = fh - rh; *w = rw; *h = rh;
}
#endif

} /* namespace */

extern "C" void gePortImguiInit(void *window, void *glctx)
{
    if (g_active) return;
    g_policy = geImguiPolicyResolve(1, getenv("GETV_IMGUI"));
    if (!g_policy.console_ui_enabled) return;
    if (window == NULL) {
        printf("[getv][imgui] no window -- console and developer overlay unavailable\n");
        fflush(stdout);
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = NULL;

#if defined(RAPI_METAL)
    if (!ImGui_ImplSDL2_InitForMetal((SDL_Window *)window)) {
        printf("[getv][imgui] SDL2 Metal backend init FAILED -- overlay OFF\n");
        ImGui::DestroyContext();
        return;
    }
    if (!gePortMetalImguiInit()) {
        printf("[getv][imgui] Metal backend init FAILED -- overlay OFF\n");
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        return;
    }
#else
    if (!ImGui_ImplSDL2_InitForOpenGL((SDL_Window *)window, glctx)) {
        printf("[getv][imgui] SDL2 backend init FAILED -- overlay OFF\n");
        ImGui::DestroyContext();
        return;
    }
    if (!ImGui_ImplOpenGL2_Init()) {
        printf("[getv][imgui] GL2 backend init FAILED -- overlay OFF\n");
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        return;
    }
#endif

    geConsoleInputReset();
    geConsolePauseReset();
    g_active = true;
    (void)ge_console_toggle_scancode();
    printf("[getv][imgui] console UI ON; developer overlay %s -- Dear ImGui %s, "
           "backends: %s + %s\n",
           g_policy.developer_overlay_enabled ? "ON" : "off", IMGUI_VERSION,
           ImGui::GetIO().BackendPlatformName ? ImGui::GetIO().BackendPlatformName : "?",
           ImGui::GetIO().BackendRendererName ? ImGui::GetIO().BackendRendererName : "?");
#if defined(RAPI_METAL)
    printf("[getv][imgui] renderer=Metal; console toggle=%s\n", ge_console_toggle_name());
#else
    printf("[getv][imgui] GL_VERSION=%s; console toggle=%s\n",
           (const char *)glGetString(GL_VERSION), ge_console_toggle_name());
#endif
    fflush(stdout);

    if (ge_env_on("GETV_CONSOLE_OPEN")) ge_console_set_open(true);
}

extern "C" void gePortImguiNewFrame(void)
{
    if (!g_active || g_frame_open ||
        (!g_policy.developer_overlay_enabled && !geConsoleInputOpen())) return;
#if !defined(RAPI_METAL)
    ImGui_ImplOpenGL2_NewFrame();
#endif
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    g_frame_open = true;
    g_frames++;
    if (g_policy.developer_overlay_enabled) ge_draw_dev_overlay();
    ge_draw_console();
}

extern "C" void gePortImguiRender(void)
{
    ge_diagnostic_capture_opengl();
    if (!g_active || !g_frame_open) return;
    g_frame_open = false;
    ImGui::Render();
    ImDrawData *dd = ImGui::GetDrawData();
    if (dd == NULL) return;

    if (g_probe_frame == -2) {
        const char *e = getenv("GETV_IMGUI_PROBE");
        g_probe_frame = (e && *e) ? atoi(e) : -1;
    }
    const bool probing = g_probe_frame > 0 && (long)g_frames == (long)g_probe_frame;

#if defined(RAPI_METAL)
    gePortMetalImguiBeginPass();
    int submitted = gePortMetalImguiRenderDrawData(dd);
    gePortMetalImguiEndPass();
    if (probing) {
        printf("[getv][imgui] probe frame %lu: Metal draw %s, %d draw list(s), %d vertices%s\n",
               g_frames, submitted ? "submitted" : "NOT submitted",
               dd->CmdListsCount, dd->TotalVtxCount,
               geConsoleInputOpen() ? ", console open" : "");
        fflush(stdout);
    }
#else
    int px = 0, py = 0, pw = 0, ph = 0;
    unsigned char *before = NULL;
    if (probing) {
        ge_imgui_probe_rect(&px, &py, &pw, &ph);
        if (pw > 0 && ph > 0) {
            before = (unsigned char *)malloc((size_t)pw * ph * 3);
            if (before) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(px, py, pw, ph, GL_RGB, GL_UNSIGNED_BYTE, before);
            }
        }
    }

    GLint last_program = 0, last_array_buffer = 0, last_element_buffer = 0;
    GLint last_active_texture = GL_TEXTURE0;
    GLint attrib_enabled[GE_IMGUI_ATTRIBS];
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
    for (int i = 0; i < GE_IMGUI_ATTRIBS; i++) {
        attrib_enabled[i] = 0;
        glGetVertexAttribiv((GLuint)i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attrib_enabled[i]);
    }
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < GE_IMGUI_ATTRIBS; i++)
        if (attrib_enabled[i]) glDisableVertexAttribArray((GLuint)i);

    ImGui_ImplOpenGL2_RenderDrawData(dd);

    for (int i = 0; i < GE_IMGUI_ATTRIBS; i++)
        if (attrib_enabled[i]) glEnableVertexAttribArray((GLuint)i);
    glActiveTexture((GLenum)last_active_texture);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_element_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_array_buffer);
    glUseProgram((GLuint)last_program);

    if (probing) {
        unsigned long changed = 0;
        if (before) {
            unsigned char *after = (unsigned char *)malloc((size_t)pw * ph * 3);
            if (after) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(px, py, pw, ph, GL_RGB, GL_UNSIGNED_BYTE, after);
                for (size_t i = 0; i < (size_t)pw * ph; i++) {
                    if (before[i * 3] != after[i * 3] ||
                        before[i * 3 + 1] != after[i * 3 + 1] ||
                        before[i * 3 + 2] != after[i * 3 + 2]) changed++;
                }
                free(after);
            }
            free(before);
        }
        printf("[getv][imgui] probe frame %lu: rect %dx%d at (%d,%d), "
               "%lu pixels changed, %d draw list(s), %d vertices%s\n",
               g_frames, pw, ph, px, py, changed, dd->CmdListsCount, dd->TotalVtxCount,
               geConsoleInputOpen() ? ", console open" : "");
        fflush(stdout);
    }
#endif
}

extern "C" int gePortImguiEvent(void *sdl_event)
{
    if (ge_diagnostic_event(sdl_event)) return 1;
    if (!g_active || sdl_event == NULL) return 0;
    SDL_Event *event = (SDL_Event *)sdl_event;

    if (event->type == SDL_KEYDOWN && event->key.repeat == 0 &&
        event->key.keysym.scancode == ge_console_toggle_scancode()) {
        ge_console_set_open(!geConsoleInputOpen());
        g_skip_toggle_text = true;
        return 1;
    }
    if (event->type == SDL_TEXTINPUT && g_skip_toggle_text) {
        g_skip_toggle_text = false;
        return 1;
    }
    if (event->type == SDL_KEYUP &&
        event->key.keysym.scancode == ge_console_toggle_scancode()) {
        /* Function keys and some layouts emit no text event for the toggle. Do not let a stale
         * skip flag eat the first real character typed after that key is released. */
        g_skip_toggle_text = false;
    }
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.scancode != ge_console_toggle_scancode()) {
        g_skip_toggle_text = false;
    }

    if (geConsoleInputOpen()) {
        ImGui_ImplSDL2_ProcessEvent(event);
        return ge_console_input_event(event->type) ? 1 : 0;
    }
    if (geConsoleInputCaptureActive() && ge_console_input_event(event->type)) {
        /* A key may be released after the window closes but before gameplay's release
         * quarantine ends. Feed that KEYUP to ImGui too so its backend cannot retain a
         * phantom held key the next time the console opens. */
        ImGui_ImplSDL2_ProcessEvent(event);
        return 1;
    }

    if (g_policy.developer_overlay_enabled) ImGui_ImplSDL2_ProcessEvent(event);
    return 0;
}

extern "C" void gePortImguiShutdown(void)
{
    if (!g_active) return;
    if (geConsoleInputOpen()) {
        geConsolePauseRequest(0);
        gePortInputConsoleCapture(0);
    }
    geConsoleInputReset();
    g_active = false;
    g_policy.console_ui_enabled = 0;
    g_policy.developer_overlay_enabled = 0;
    g_frame_open = false;
#if defined(RAPI_METAL)
    gePortMetalImguiShutdown();
#else
    ImGui_ImplOpenGL2_Shutdown();
#endif
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    printf("[getv][imgui] console UI shut down after %lu rendered UI frames\n", g_frames);
    fflush(stdout);
}

extern "C" int gePortImguiActive(void) { return g_active ? 1 : 0; }
extern "C" int gePortImguiConsoleOpen(void) { return geConsoleInputOpen(); }

#else /* !GE_WITH_IMGUI */

extern "C" void gePortImguiInit(void *window, void *glctx)
{
    GeImguiPolicy policy = geImguiPolicyResolve(0, getenv("GETV_IMGUI"));
    (void)window; (void)glctx;
    if (!policy.console_ui_enabled)
        printf("[getv][imgui] this binary was built without ImGui; console UI unavailable.\n");
    {
        const char *e = getenv("GETV_IMGUI");
        if (e != NULL && *e != '\0' && strcmp(e, "0") != 0) {
            printf("[getv][imgui] GETV_IMGUI was requested, so the developer overlay is also "
                   "unavailable.\n");
#if defined(__APPLE__)
            printf("[getv][imgui] run tools/fetch_imgui.sh, then ./getv/build_mac.sh all\n");
#elif defined(_WIN32)
            printf("[getv][imgui] run tools/fetch_imgui.sh, then .\\getv\\build_windows.ps1 all\n");
#else
            printf("[getv][imgui] run tools/fetch_imgui.sh, then ./getv/build_linux.sh all\n");
#endif
        }
    }
    fflush(stdout);
}

extern "C" void gePortImguiNewFrame(void) {}
extern "C" void gePortImguiRender(void) { ge_diagnostic_capture_opengl(); }
extern "C" int  gePortImguiEvent(void *sdl_event) { return ge_diagnostic_event(sdl_event); }
extern "C" void gePortImguiShutdown(void) {}
extern "C" int  gePortImguiActive(void) { return 0; }
extern "C" int  gePortImguiConsoleOpen(void) { return 0; }

#endif /* GE_WITH_IMGUI */
