#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <stdbool.h>

typedef enum {
    TOOL_SELECT,
    TOOL_SUBSELECT,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_PEN,
    TOOL_LINE,
    TOOL_TEXT,
    TOOL_SHEAR,
    TOOL_ZOOM,
    TOOL_HAND,
    TOOL_COUNT
} ToolType;

ToolType ui_toolbar_get_active_tool(void);
void ui_toolbar_set_active_tool(ToolType tool);
#define TOOLBAR_WIDTH 40.0f

void ui_toolbar(bool *open);

#endif
