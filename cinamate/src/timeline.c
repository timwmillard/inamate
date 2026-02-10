// Timeline panel

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "timeline.h"

void ui_timeline(bool *open)
{
    if (!igBegin("Timeline", open, ImGuiWindowFlags_None)) {
        igEnd();
        return;
    }

    igEnd();
}
