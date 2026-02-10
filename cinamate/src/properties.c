// Properties panel — static layout with placeholder values

#include <float.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

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

static void ui_scene_properties(void)
{
    igText("Artboard");
    igSeparator();

    if (igCollapsingHeader_TreeNodeFlags("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (begin_prop_table()) {
            PROP_ROW_BEGIN("Name");
            igInputText("##name", props.scene_name, sizeof(props.scene_name), 0, NULL, NULL);
            end_prop_table();
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Dimensions", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (begin_prop_table()) {
            PROP_ROW_BEGIN("Width");
            igInputInt("##width", &props.scene_width, 1, 10, 0);
            PROP_ROW_BEGIN("Height");
            igInputInt("##height", &props.scene_height, 1, 10, 0);
            end_prop_table();
        }
    }

    if (igCollapsingHeader_TreeNodeFlags("Background", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (begin_prop_table()) {
            PROP_ROW_BEGIN("Color");
            igColorEdit3("##bg_color", props.scene_bg, ImGuiColorEditFlags_DisplayHex);
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
