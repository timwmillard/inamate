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

// Placeholder state (will be replaced with real data binding later)
static struct {
    bool has_selection;

    // Scene properties
    char scene_name[128];
    int scene_width;
    int scene_height;
    float scene_bg[3];

    // Object properties
    float pos_x, pos_y;
    float scale_x, scale_y;
    float rotation;
    float skew_x, skew_y;
    float anchor_x, anchor_y;
    float fill[3];
    float stroke[3];
    float stroke_width;
    float opacity;
} props = {
    .has_selection = false,
    .scene_name = "My Animation",
    .scene_width = 800,
    .scene_height = 600,
    .scene_bg = {0.18f, 0.18f, 0.22f},
    .pos_x = 100.0f, .pos_y = 150.0f,
    .scale_x = 1.0f, .scale_y = 1.0f,
    .rotation = 0.0f,
    .skew_x = 0.0f, .skew_y = 0.0f,
    .anchor_x = 0.5f, .anchor_y = 0.5f,
    .fill = {0.35f, 0.55f, 0.90f},
    .stroke = {0.0f, 0.0f, 0.0f},
    .stroke_width = 1.0f,
    .opacity = 100.0f,
};

// Left-aligned label row helper: label on left, widget fills right column.
// Call before the input widget, then igTableNextColumn() + widget + igTableNextRow().
#define PROP_ROW_BEGIN(label) \
    igTableNextRow(0, 0);    \
    igTableNextColumn();     \
    igAlignTextToFramePadding(); \
    igTextUnformatted(label, NULL); \
    igTableNextColumn();     \
    igSetNextItemWidth(-FLT_MIN)

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

    igText("Artboard");
    igSeparator();

    // Sync scene info from engine each frame (when not actively editing)
    if (!igIsAnyItemActive()) {
        if (si->scene_name[0]) {
            strncpy(props.scene_name, si->scene_name, sizeof(props.scene_name) - 1);
            props.scene_name[sizeof(props.scene_name) - 1] = '\0';
        }
        props.scene_width = si->scene_width;
        props.scene_height = si->scene_height;
    }

    if (igCollapsingHeader_TreeNodeFlags("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    }

    if (igCollapsingHeader_TreeNodeFlags("Dimensions", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    }

    if (igCollapsingHeader_TreeNodeFlags("Background", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    }
}

static void ui_object_properties(void)
{
    igText("Properties");
    igSeparator();

    if (begin_prop_table()) {
        PROP_ROW_BEGIN("Type");
        igTextUnformatted("Rectangle", NULL);
        PROP_ROW_BEGIN("ID");
        igTextUnformatted("obj_001", NULL);
        end_prop_table();
    }
    igSeparator();

    if (igCollapsingHeader_TreeNodeFlags("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (begin_prop_table()) {
            PROP_ROW_BEGIN("X");
            igDragFloat("##x", &props.pos_x, 1.0f, 0.0f, 0.0f, "%.1f", 0);
            PROP_ROW_BEGIN("Y");
            igDragFloat("##y", &props.pos_y, 1.0f, 0.0f, 0.0f, "%.1f", 0);
            PROP_ROW_BEGIN("Scale X");
            igDragFloat("##sx", &props.scale_x, 0.01f, 0.0f, 0.0f, "%.2f", 0);
            PROP_ROW_BEGIN("Scale Y");
            igDragFloat("##sy", &props.scale_y, 0.01f, 0.0f, 0.0f, "%.2f", 0);
            PROP_ROW_BEGIN("Rotation");
            igDragFloat("##rot", &props.rotation, 1.0f, 0.0f, 0.0f, "%.1f", 0);
            PROP_ROW_BEGIN("Skew X");
            igDragFloat("##skx", &props.skew_x, 1.0f, 0.0f, 0.0f, "%.1f", 0);
            PROP_ROW_BEGIN("Skew Y");
            igDragFloat("##sky", &props.skew_y, 1.0f, 0.0f, 0.0f, "%.1f", 0);
            PROP_ROW_BEGIN("Anchor X");
            igDragFloat("##ax", &props.anchor_x, 0.01f, 0.0f, 0.0f, "%.2f", 0);
            PROP_ROW_BEGIN("Anchor Y");
            igDragFloat("##ay", &props.anchor_y, 0.01f, 0.0f, 0.0f, "%.2f", 0);
            end_prop_table();
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Style", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (begin_prop_table()) {
            PROP_ROW_BEGIN("Fill");
            igColorEdit3("##fill", props.fill, ImGuiColorEditFlags_DisplayHex);
            PROP_ROW_BEGIN("Stroke");
            igColorEdit3("##stroke", props.stroke, ImGuiColorEditFlags_DisplayHex);
            PROP_ROW_BEGIN("Stroke W");
            igDragFloat("##strokew", &props.stroke_width, 0.1f, 0.0f, 0.0f, "%.1f", 0);
            PROP_ROW_BEGIN("Opacity");
            igDragFloat("##opacity", &props.opacity, 1.0f, 0.0f, 100.0f, "%.0f%%", 0);
            end_prop_table();
        }
    }
}

void ui_properties(bool *open)
{
    if (!igBegin("Properties", open, ImGuiWindowFlags_None)) {
        igEnd();
        return;
    }

    if (props.has_selection) {
        ui_object_properties();
    } else {
        ui_scene_properties();
    }

    igEnd();
}
