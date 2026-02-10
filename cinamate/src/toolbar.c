#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "toolbar.h"
#include "font/IconsFontAwesome6.h"

static ToolType active_tool = TOOL_SELECT;

ToolType ui_toolbar_get_active_tool(void)
{
    return active_tool;
}

void ui_toolbar_set_active_tool(ToolType tool)
{
    active_tool = tool;
}

typedef struct {
    const char *icon;
    const char *name;
    const char *shortcut;
} ToolDef;

static const ToolDef tool_defs[TOOL_COUNT] = {
    [TOOL_SELECT]    = { ICON_FA_ARROW_POINTER,    "Select",    "V" },
    [TOOL_SUBSELECT] = { ICON_FA_VECTOR_SQUARE,    "Subselect", "A" },
    [TOOL_RECT]      = { ICON_FA_SQUARE,           "Rectangle", "R" },
    [TOOL_ELLIPSE]   = { ICON_FA_CIRCLE,           "Ellipse",   "O" },
    [TOOL_PEN]       = { ICON_FA_PEN_NIB,          "Pen",       "P" },
    [TOOL_LINE]      = { ICON_FA_SLASH,            "Line",      "L" },
    [TOOL_TEXT]      = { ICON_FA_FONT,             "Text",      "T" },
    [TOOL_SHEAR]     = { ICON_FA_ITALIC,           "Shear",     "S" },
    [TOOL_ZOOM]      = { ICON_FA_MAGNIFYING_GLASS, "Zoom",      "Z" },
    [TOOL_HAND]      = { ICON_FA_HAND,             "Hand",      "H" },
};

void ui_toolbar(bool *open)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoResize;

    if (!igBegin("Toolbar", open, flags)) {
        igEnd();
        return;
    }

    ImVec2 btn_size = {28, 28};

    for (int i = 0; i < TOOL_COUNT; i++) {
        bool is_active = (active_tool == (ToolType)i);

        if (is_active) {
            igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.2f, 0.4f, 0.8f, 1.0f});
            igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0.3f, 0.5f, 0.9f, 1.0f});
            igPushStyleColor_Vec4(ImGuiCol_ButtonActive, (ImVec4){0.15f, 0.35f, 0.75f, 1.0f});
        } else {
            igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.0f, 0.0f, 0.0f, 0.0f});
            igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0.3f, 0.3f, 0.3f, 0.5f});
            igPushStyleColor_Vec4(ImGuiCol_ButtonActive, (ImVec4){0.2f, 0.2f, 0.2f, 0.5f});
        }

        igPushID_Int(i);
        if (igButton(tool_defs[i].icon, btn_size)) {
            active_tool = (ToolType)i;
        }
        igPopID();

        if (igIsItemHovered(0)) {
            igSetTooltip("%s (%s)", tool_defs[i].name, tool_defs[i].shortcut);
        }

        igPopStyleColor(3);
    }

    igEnd();
}
