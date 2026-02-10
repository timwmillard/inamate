// Properties panel — static layout with placeholder values

#include <float.h>
#include <stdio.h>
#include <string.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "arena.h"
#define ARENA_FWD_DECL_ // suppress redeclaration in libgo.h
#include "libgo.h"

#include "canvas.h"
#include "properties.h"

static struct {
    // Scene properties
    char scene_name[128];
    int scene_width;
    int scene_height;
    float scene_bg[3];

    // Object properties (synced from Go each frame when not editing)
    float pos_x, pos_y;
    float scale_x, scale_y;
    float rotation;
    float skew_x, skew_y;
    float anchor_x, anchor_y;
    float fill[3];
    float stroke[3];
    float stroke_width;
    float opacity;
} props = {0};

// Parse "#RRGGBB" hex to float[3] (0-1 range)
static void hex_to_float3(const char *hex, float out[3]) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    unsigned int r, g, b;
    sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    out[0] = r / 255.0f;
    out[1] = g / 255.0f;
    out[2] = b / 255.0f;
}

// Convert float[3] (0-1 range) to "#RRGGBB" hex string
static void float3_to_hex(const float c[3], char out[16]) {
    int r = (int)(c[0] * 255.0f + 0.5f);
    int g = (int)(c[1] * 255.0f + 0.5f);
    int b = (int)(c[2] * 255.0f + 0.5f);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    snprintf(out, 16, "#%02x%02x%02x", r, g, b);
}

// Left-aligned label row helper: label on left, widget fills right column.
// Call before the input widget, then igTableNextColumn() + widget + igTableNextRow().
#define PROP_ROW_BEGIN(label) \
    igTableNextRow(0, 0);    \
    igTableNextColumn();     \
    igAlignTextToFramePadding(); \
    igTextUnformatted(label, NULL); \
    igTableNextColumn();     \
    igSetNextItemWidth(-FLT_MIN)

// Matches frontend Section component: heading then content then bottom border
static void section_heading(const char *title)
{
    igSpacing();
    igTextDisabled(title);
}

static void section_end(void)
{
    igSpacing();
    igSeparator();
}

// Matches frontend: text-xs font-semibold uppercase tracking-wider
static void panel_title(const char *title)
{
    igTextDisabled(title);
    igSpacing();
}

static bool begin_prop_table(void)
{
    return igBeginTable("##prop", 2, ImGuiTableFlags_None, (ImVec2){0,0}, 0);
}

static void end_prop_table(void)
{
    igEndTable();
}

// Helper to send a scene.update JSON change to the engine + collab
static void send_scene_update(const char *scene_id, const char *changes_json) {
    GoInamateSceneUpdate((char *)scene_id, (char *)changes_json);
}

static void ui_scene_properties(void)
{
    const CanvasSceneInfo *si = ui_canvas_get_scene_info();

    panel_title("ARTBOARD");

    // Sync scene info from engine each frame (when not actively editing)
    if (!igIsAnyItemActive()) {
        if (si->scene_name[0]) {
            strncpy(props.scene_name, si->scene_name, sizeof(props.scene_name) - 1);
            props.scene_name[sizeof(props.scene_name) - 1] = '\0';
        }
        props.scene_width = si->scene_width;
        props.scene_height = si->scene_height;
    }

    section_heading("Scene");
    if (begin_prop_table()) {
        PROP_ROW_BEGIN("Name");
        igInputText("##name", props.scene_name, sizeof(props.scene_name), 0, NULL, NULL);
        if (igIsItemDeactivatedAfterEdit()) {
            char changes[256];
            snprintf(changes, sizeof(changes), "{\"name\":\"%s\"}", props.scene_name);
            send_scene_update(si->scene_id, changes);
        }
        end_prop_table();
    }
    section_end();

    section_heading("Dimensions");
    if (begin_prop_table()) {
        PROP_ROW_BEGIN("Width");
        igInputInt("##width", &props.scene_width, 1, 10, 0);
        if (igIsItemDeactivatedAfterEdit()) {
            if (props.scene_width < 1) props.scene_width = 1;
            char changes[64];
            snprintf(changes, sizeof(changes), "{\"width\":%d}", props.scene_width);
            send_scene_update(si->scene_id, changes);
        }
        PROP_ROW_BEGIN("Height");
        igInputInt("##height", &props.scene_height, 1, 10, 0);
        if (igIsItemDeactivatedAfterEdit()) {
            if (props.scene_height < 1) props.scene_height = 1;
            char changes[64];
            snprintf(changes, sizeof(changes), "{\"height\":%d}", props.scene_height);
            send_scene_update(si->scene_id, changes);
        }
        end_prop_table();
    }
    section_end();

    section_heading("Background");
    if (begin_prop_table()) {
        if (si->background[0] == '#' && strlen(si->background) >= 7) {
            unsigned int r, g, b;
            sscanf(si->background + 1, "%02x%02x%02x", &r, &g, &b);
            props.scene_bg[0] = r / 255.0f;
            props.scene_bg[1] = g / 255.0f;
            props.scene_bg[2] = b / 255.0f;
        }

        PROP_ROW_BEGIN("Color");
        if (igColorEdit3("##bg_color", props.scene_bg, ImGuiColorEditFlags_DisplayHex)) {
            int cr = (int)(props.scene_bg[0] * 255.0f + 0.5f);
            int cg = (int)(props.scene_bg[1] * 255.0f + 0.5f);
            int cb = (int)(props.scene_bg[2] * 255.0f + 0.5f);
            if (cr > 255) cr = 255; if (cg > 255) cg = 255; if (cb > 255) cb = 255;

            char hex[16];
            snprintf(hex, sizeof(hex), "#%02x%02x%02x", cr, cg, cb);

            char changes[64];
            snprintf(changes, sizeof(changes), "{\"background\":\"%s\"}", hex);

            send_scene_update(si->scene_id, changes);
        }
        end_prop_table();
    }
    section_end();
}

// Helper to send transform change for the selected object
static void send_transform(const char *obj_id, const char *key, float value) {
    char json[128];
    snprintf(json, sizeof(json), "{\"%s\":%g}", key, (double)value);
    GoInamateObjectTransform((char *)obj_id, json);
}

// Helper to send style change for the selected object
static void send_style_str(const char *obj_id, const char *key, const char *value) {
    char json[128];
    snprintf(json, sizeof(json), "{\"%s\":\"%s\"}", key, value);
    GoInamateObjectStyle((char *)obj_id, json);
}

static void send_style_float(const char *obj_id, const char *key, float value) {
    char json[128];
    snprintf(json, sizeof(json), "{\"%s\":%g}", key, (double)value);
    GoInamateObjectStyle((char *)obj_id, json);
}

static void ui_object_properties(const char *sel_id)
{
    // Sync object data from Go each frame (when not actively editing a field)
    if (!igIsAnyItemActive()) {
        InObjectInfo info = GoInamateGetSelectedObject();
        props.pos_x = info.x;
        props.pos_y = info.y;
        props.scale_x = info.sx;
        props.scale_y = info.sy;
        props.rotation = info.r;
        props.skew_x = info.skew_x;
        props.skew_y = info.skew_y;
        props.anchor_x = info.ax;
        props.anchor_y = info.ay;
        hex_to_float3(info.fill, props.fill);
        hex_to_float3(info.stroke, props.stroke);
        props.stroke_width = info.stroke_width;
        props.opacity = info.opacity * 100.0f; // 0-1 → 0-100 for display
    }

    InObjectInfo info = GoInamateGetSelectedObject();

    panel_title("OBJECT");

    if (begin_prop_table()) {
        PROP_ROW_BEGIN("Type");
        igTextUnformatted(info.type, NULL);
        PROP_ROW_BEGIN("ID");
        // Show truncated ID
        char short_id[16];
        strncpy(short_id, info.id, 8);
        short_id[8] = '\0';
        igTextUnformatted(short_id, NULL);
        end_prop_table();
    }
    section_end();

    section_heading("Transform");
    if (begin_prop_table()) {
        PROP_ROW_BEGIN("X");
        igDragFloat("##x", &props.pos_x, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "x", props.pos_x);

        PROP_ROW_BEGIN("Y");
        igDragFloat("##y", &props.pos_y, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "y", props.pos_y);

        PROP_ROW_BEGIN("Scale X");
        igDragFloat("##sx", &props.scale_x, 0.01f, 0.0f, 0.0f, "%.2f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "sx", props.scale_x);

        PROP_ROW_BEGIN("Scale Y");
        igDragFloat("##sy", &props.scale_y, 0.01f, 0.0f, 0.0f, "%.2f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "sy", props.scale_y);

        PROP_ROW_BEGIN("Rotation");
        igDragFloat("##rot", &props.rotation, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "r", props.rotation);

        PROP_ROW_BEGIN("Skew X");
        igDragFloat("##skx", &props.skew_x, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "skewX", props.skew_x);

        PROP_ROW_BEGIN("Skew Y");
        igDragFloat("##sky", &props.skew_y, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "skewY", props.skew_y);

        PROP_ROW_BEGIN("Anchor X");
        igDragFloat("##ax", &props.anchor_x, 0.01f, 0.0f, 0.0f, "%.2f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "ax", props.anchor_x);

        PROP_ROW_BEGIN("Anchor Y");
        igDragFloat("##ay", &props.anchor_y, 0.01f, 0.0f, 0.0f, "%.2f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_transform(sel_id, "ay", props.anchor_y);

        end_prop_table();
    }
    section_end();

    section_heading("Style");
    if (begin_prop_table()) {
        PROP_ROW_BEGIN("Fill");
        if (igColorEdit3("##fill", props.fill, ImGuiColorEditFlags_DisplayHex)) {
            char hex[16];
            float3_to_hex(props.fill, hex);
            send_style_str(sel_id, "fill", hex);
        }

        PROP_ROW_BEGIN("Stroke");
        if (igColorEdit3("##stroke", props.stroke, ImGuiColorEditFlags_DisplayHex)) {
            char hex[16];
            float3_to_hex(props.stroke, hex);
            send_style_str(sel_id, "stroke", hex);
        }

        PROP_ROW_BEGIN("Stroke W");
        igDragFloat("##strokew", &props.stroke_width, 0.1f, 0.0f, 0.0f, "%.1f", 0);
        if (igIsItemDeactivatedAfterEdit()) send_style_float(sel_id, "strokeWidth", props.stroke_width);

        PROP_ROW_BEGIN("Opacity");
        igDragFloat("##opacity", &props.opacity, 1.0f, 0.0f, 100.0f, "%.0f%%", 0);
        if (igIsItemDeactivatedAfterEdit()) send_style_float(sel_id, "opacity", props.opacity / 100.0f);

        end_prop_table();
    }
    section_end();
}

void ui_properties(bool *open)
{
    if (!igBegin("Properties", open, ImGuiWindowFlags_None)) {
        igEnd();
        return;
    }

    const char *sel_id = ui_canvas_get_selected_id();
    bool has_selection = (sel_id && sel_id[0]);

    if (has_selection) {
        ui_object_properties(sel_id);
    } else {
        ui_scene_properties();
    }

    igEnd();
}
