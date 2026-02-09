#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "sokol_imgui.h"
#include "libgo.h"

void ui_window(void);

// Clipboard callbacks for ImGui
static const char* imgui_get_clipboard(ImGuiContext* ctx) {
    (void)ctx;
    return sapp_get_clipboard_string();
}

static void imgui_set_clipboard(ImGuiContext* ctx, const char* text) {
    (void)ctx;
    sapp_set_clipboard_string(text);
}

static struct {
    sg_pass_action pass_action;
} state = {0};

void ui_draw()
{
    ui_window();
}

void frame(void)
{
    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });

    ui_draw();

    sg_begin_pass(&(sg_pass){
            .action = state.pass_action,
            .swapchain = sglue_swapchain(),
        });

    simgui_render();

    sg_end_pass();
    sg_commit();
}

void event(const sapp_event* ev)
{
    // Development only
    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_code == SAPP_KEYCODE_ESCAPE) {
        sapp_quit();
    }

    simgui_handle_event(ev);
}

// ImFont *main_font;

void cleanup(void)
{
    // ImGuiIO* io = igGetIO_Nil();
    // ImFontAtlas_RemoveFont(io->Fonts, main_font);

    simgui_shutdown();
    sg_shutdown();
}

// embed fonts
#include "font/Roboto-Regular_ttf.h"
#include "font/Roboto-Bold_ttf.h"

void init(void)
{
    sg_setup(&(sg_desc){
            .environment = sglue_environment(),
            .logger.func = slog_func,
            });

    simgui_setup(&(simgui_desc_t){.no_default_font = true});

    // Enable docking
    ImGuiIO* io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Set up clipboard callbacks
    ImGuiPlatformIO* pio = igGetPlatformIO_Nil();
    pio->Platform_GetClipboardTextFn = imgui_get_clipboard;
    pio->Platform_SetClipboardTextFn = imgui_set_clipboard;

    // Use cimgui constructor to get properly initialized config
    ImFontConfig* font_cfg = ImFontConfig_ImFontConfig();

    // Add font - sokol_imgui 1.92+ handles atlas texture automatically
    void *font_data = malloc(Roboto_Regular_ttf_len);
    memcpy(font_data, Roboto_Regular_ttf_data, Roboto_Regular_ttf_len);
    ImFontAtlas_AddFontFromMemoryTTF(io->Fonts, font_data, Roboto_Regular_ttf_len, 20.0f, font_cfg, NULL);

    ImFontConfig_destroy(font_cfg);

    state.pass_action = (sg_pass_action) {
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = {0.2, 0.2, 0.2, 1.0},
        },
    };
}


sapp_desc sokol_main(int argc, char *argv[])
{
    // GoFetchResponse resp = GoFetch("GET", "https://www.cgeek.dev/", NULL, NULL, 0);
    // if (resp.error)
    //     printf("ERROR: %s\n", resp.error);
    // else
    //     printf("BODY = %s\n", resp.body);
    // GoFetchResponseFree(&resp);


    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 1200,
        .height = 800,
        .window_title = "cinamate",
        .enable_clipboard = true,
        .clipboard_size = 8192,
    };
}
