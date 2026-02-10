// Properties panel — static layout with placeholder values

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

static void ui_scene_properties(void)
{
    igText("Artboard");
    igSeparator();

    if (igCollapsingHeader_TreeNodeFlags("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        igInputText("Name", props.scene_name, sizeof(props.scene_name), 0, NULL, NULL);
    }

    if (igCollapsingHeader_TreeNodeFlags("Dimensions", ImGuiTreeNodeFlags_DefaultOpen)) {
        igInputInt("Width", &props.scene_width, 1, 10, 0);
        igInputInt("Height", &props.scene_height, 1, 10, 0);
    }

    if (igCollapsingHeader_TreeNodeFlags("Background", ImGuiTreeNodeFlags_DefaultOpen)) {
        igColorEdit3("Color", props.scene_bg, 0);
    }
}

static void ui_object_properties(void)
{
    igText("Properties");
    igSeparator();

    igText("Type: Rectangle");
    igText("ID: obj_001");
    igSeparator();

    if (igCollapsingHeader_TreeNodeFlags("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        igDragFloat("X", &props.pos_x, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        igDragFloat("Y", &props.pos_y, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        igDragFloat("Scale X", &props.scale_x, 0.01f, 0.0f, 0.0f, "%.2f", 0);
        igDragFloat("Scale Y", &props.scale_y, 0.01f, 0.0f, 0.0f, "%.2f", 0);
        igDragFloat("Rotation", &props.rotation, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        igDragFloat("Skew X", &props.skew_x, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        igDragFloat("Skew Y", &props.skew_y, 1.0f, 0.0f, 0.0f, "%.1f", 0);
        igDragFloat("Anchor X", &props.anchor_x, 0.01f, 0.0f, 0.0f, "%.2f", 0);
        igDragFloat("Anchor Y", &props.anchor_y, 0.01f, 0.0f, 0.0f, "%.2f", 0);
    }

    if (igCollapsingHeader_TreeNodeFlags("Style", ImGuiTreeNodeFlags_DefaultOpen)) {
        igColorEdit3("Fill", props.fill, 0);
        igColorEdit3("Stroke", props.stroke, 0);
        igDragFloat("Stroke W", &props.stroke_width, 0.1f, 0.0f, 0.0f, "%.1f", 0);
        igDragFloat("Opacity", &props.opacity, 1.0f, 0.0f, 100.0f, "%.0f%%", 0);
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
