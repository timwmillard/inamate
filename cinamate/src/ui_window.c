// UI Window

#include <string.h>

#include "sokol_app.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "libgo.h"


static struct {
    // GUI
    bool show_canvas;
    bool show_properties;
    bool show_timeline;

    bool show_demo;
    bool dock_setup_done;

} window_state = {0};

// Pack RGBA into ImU32 (ImGui's ABGR byte order)
#define INAMATE_COL32(r,g,b,a) (((ImU32)(a)<<24) | ((ImU32)(b)<<16) | ((ImU32)(g)<<8) | ((ImU32)(r)))

// Parse "#RRGGBB" hex color to ImU32
static ImU32 hex_to_imu32(const char *hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) return INAMATE_COL32(255, 255, 255, 255);
    unsigned int r, g, b;
    sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    return INAMATE_COL32(r, g, b, 255);
}

// Apply affine transform [a,b,c,d,e,f] to a point, offset by canvas origin
static ImVec2 xform_pt(const float t[6], float x, float y, ImVec2 origin) {
    return (ImVec2){
        origin.x + t[0]*x + t[2]*y + t[4],
        origin.y + t[1]*x + t[3]*y + t[5],
    };
}

// Render a single path draw command to an ImDrawList
static void draw_path_cmd(ImDrawList *dl, InDrawCmd *cmd, ImVec2 origin) {
    float cx = 0, cy = 0; // current point

    ImDrawList_PathClear(dl);

    for (int j = 0; j < cmd->path_len; j++) {
        InPathCmd *pc = &cmd->path[j];
        switch (pc->type) {
        case 'M':
            // MoveTo starts a new sub-path
            ImDrawList_PathClear(dl);
            cx = pc->coords[0]; cy = pc->coords[1];
            ImDrawList_PathLineTo(dl, xform_pt(cmd->transform, cx, cy, origin));
            break;
        case 'L':
            cx = pc->coords[0]; cy = pc->coords[1];
            ImDrawList_PathLineTo(dl, xform_pt(cmd->transform, cx, cy, origin));
            break;
        case 'Q': {
            float x1 = pc->coords[0], y1 = pc->coords[1];
            float x2 = pc->coords[2], y2 = pc->coords[3];
            ImDrawList_PathBezierQuadraticCurveTo(dl,
                xform_pt(cmd->transform, x1, y1, origin),
                xform_pt(cmd->transform, x2, y2, origin), 0);
            cx = x2; cy = y2;
            break;
        }
        case 'C': {
            float x1 = pc->coords[0], y1 = pc->coords[1];
            float x2 = pc->coords[2], y2 = pc->coords[3];
            float x3 = pc->coords[4], y3 = pc->coords[5];
            ImDrawList_PathBezierCubicCurveTo(dl,
                xform_pt(cmd->transform, x1, y1, origin),
                xform_pt(cmd->transform, x2, y2, origin),
                xform_pt(cmd->transform, x3, y3, origin), 0);
            cx = x3; cy = y3;
            break;
        }
        case 'Z':
            // Close path — ImGui path is implicitly closed on fill/stroke
            break;
        }
    }

    // Fill
    if (cmd->fill[0] == '#') {
        ImU32 col = hex_to_imu32(cmd->fill);
        if (cmd->opacity > 0 && cmd->opacity < 1.0f) {
            col = (col & 0x00FFFFFF) | ((ImU32)(cmd->opacity * 255) << 24);
        }
        ImDrawList_PathFillConcave(dl, col);
    }

    // Stroke (need to re-walk path since fill consumed it)
    if (cmd->stroke[0] == '#') {
        ImDrawList_PathClear(dl);
        cx = 0; cy = 0;
        for (int j = 0; j < cmd->path_len; j++) {
            InPathCmd *pc = &cmd->path[j];
            switch (pc->type) {
            case 'M':
                ImDrawList_PathClear(dl);
                cx = pc->coords[0]; cy = pc->coords[1];
                ImDrawList_PathLineTo(dl, xform_pt(cmd->transform, cx, cy, origin));
                break;
            case 'L':
                cx = pc->coords[0]; cy = pc->coords[1];
                ImDrawList_PathLineTo(dl, xform_pt(cmd->transform, cx, cy, origin));
                break;
            case 'Q': {
                float x1 = pc->coords[0], y1 = pc->coords[1];
                float x2 = pc->coords[2], y2 = pc->coords[3];
                ImDrawList_PathBezierQuadraticCurveTo(dl,
                    xform_pt(cmd->transform, x1, y1, origin),
                    xform_pt(cmd->transform, x2, y2, origin), 0);
                cx = x2; cy = y2;
                break;
            }
            case 'C': {
                float x1 = pc->coords[0], y1 = pc->coords[1];
                float x2 = pc->coords[2], y2 = pc->coords[3];
                float x3 = pc->coords[4], y3 = pc->coords[5];
                ImDrawList_PathBezierCubicCurveTo(dl,
                    xform_pt(cmd->transform, x1, y1, origin),
                    xform_pt(cmd->transform, x2, y2, origin),
                    xform_pt(cmd->transform, x3, y3, origin), 0);
                cx = x3; cy = y3;
                break;
            }
            case 'Z': break;
            }
        }
        ImU32 scol = hex_to_imu32(cmd->stroke);
        float sw = cmd->stroke_width > 0 ? cmd->stroke_width : 1.0f;
        ImDrawList_PathStroke(dl, scol, ImDrawFlags_Closed, sw);
    }
}

#define ARRAY_COUNT(arr) (sizeof((arr))/sizeof((arr)[0]))

void ui_reset_layout(ImGuiID dockspace_id)
{
    window_state.show_canvas = true;

    igDockBuilderRemoveNode(dockspace_id);
    igDockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    igDockBuilderSetNodeSize(dockspace_id, igGetMainViewport()->Size);

    igDockBuilderDockWindow("Canvas", dockspace_id);
    igDockBuilderFinish(dockspace_id);
}

void ui_window(void)
{
    igGetStyle()->FrameRounding = 3.0f;
    igGetStyle()->FramePadding = (ImVec2){8.0f, 6.0f};

    ImGuiID dockspace_id = igGetID_Str("MainDockSpace");

    // Main Menu
    if (igBeginMainMenuBar()) {
        if (igBeginMenu("File", true)) {
            if (igMenuItem_Bool("New", "Ctrl+N", false, true)) {
                // Handle new file
            }
            igSeparator();
            if (igMenuItem_Bool("Export Frame as PNG", "", false, true)) {
            }
            if (igMenuItem_Bool("Export PNG Sequence", "", false, true)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Export MP4 Video", "", false, true)) {
            }
            if (igMenuItem_Bool("Export GIF", "", false, true)) {
            }
            if (igMenuItem_Bool("Export WebM Video", "", false, true)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Export HTML Animation", "", false, true)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Quit", "Ctrl+Q", false, true)) {
                sapp_quit();
            }
            igEndMenu();
        }
        if (igBeginMenu("Edit", true)) {
            if (igMenuItem_Bool("Undo", "Cmd+Z", false, true)) {
            }
            if (igMenuItem_Bool("Redo", "Cmd+Shift+Z", false, false)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Cut", "Cmd+X", false, true)) {
            }
            if (igMenuItem_Bool("Copy", "Cmd+C", false, true)) {
            }
            if (igMenuItem_Bool("Paste", "Cmd+V", false, false)) {
            }
            if (igMenuItem_Bool("Duplicate", "Cmd+D", false, true)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Delete", "Del", false, true)) {
            }
            if (igMenuItem_Bool("Delete All", "", false, true)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Select All", "Del", false, true)) {
            }
            if (igMenuItem_Bool("Deselect", "", false, true)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Group", "Cmd+G", false, false)) {
            }
            if (igMenuItem_Bool("Ungroup", "Cmd+Shift+G", false, false)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Bring to Front", "Cmd+Shift+]", false, true)) {
            }
            if (igMenuItem_Bool("Bring Forward", "Cmd+]", false, true)) {
            }
            if (igMenuItem_Bool("Send Backward", "Cmd+[", false, true)) {
            }
            if (igMenuItem_Bool("Send to Back", "Cmd+Shift+[", false, true)) {
            }
            igEndMenu();
        }
        if (igBeginMenu("View", true)) {
            if (igMenuItem_Bool("Reset Layout", "", false, true)) {
                ui_reset_layout(dockspace_id);
            }
            igSeparator();
            if (igMenuItem_Bool("Zoom In", "", false, true)) {
            }
            if (igMenuItem_Bool("Zoom Out", "", false, true)) {
            }
            if (igMenuItem_Bool("Reset Zoom", "", false, true)) {
            }
            if (igMenuItem_Bool("Fit to Screen", "", false, true)) {
            }
            igSeparator();
            if (igMenuItem_Bool("Toogle Canvas", "", false, true)) {
                window_state.show_canvas = !window_state.show_canvas;
            }
            if (igMenuItem_Bool("Toogle Timeline", "", false, true)) {
                window_state.show_timeline = !window_state.show_timeline;
            }
            if (igMenuItem_Bool("Toogle Properties", "", false, true)) {
                window_state.show_properties = !window_state.show_properties;
            }
            igEndMenu();
        }
        if (igBeginMenu("Debug", true)) {
            if (igMenuItem_Bool("Demo", "", false, true)) {
                window_state.show_demo = true;
            }
            igEndMenu();
        }

        igEndMainMenuBar();
    }

    // Dockspace
    igDockSpaceOverViewport(dockspace_id, igGetMainViewport(), ImGuiDockNodeFlags_None, NULL);

    // Setup docking layout on first frame
    if (!window_state.dock_setup_done) {
        ui_reset_layout(dockspace_id);

        window_state.dock_setup_done = true;
    }

    // Draw Window
    if (window_state.show_canvas) {
        if (igBegin("Canvas", &window_state.show_canvas, ImGuiWindowFlags_None)) {
            if (GoInamateIsDocLoaded()) {
                ImDrawList *dl = igGetWindowDrawList();
                ImVec2 origin;
                igGetCursorScreenPos(&origin);

                InDrawFrame frame = GoInamateEngineRenderFrame();

                // Draw scene background and clip to scene bounds
                if (frame.scene_width > 0 && frame.scene_height > 0) {
                    ImVec2 bg_min = origin;
                    ImVec2 bg_max = {origin.x + frame.scene_width, origin.y + frame.scene_height};

                    if (frame.background[0] == '#') {
                        ImDrawList_AddRectFilled(dl, bg_min, bg_max, hex_to_imu32(frame.background), 0, 0);
                    }

                    ImDrawList_PushClipRect(dl, bg_min, bg_max, true);
                }

                for (int i = 0; i < frame.count; i++) {
                    InDrawCmd *cmd = &frame.commands[i];
                    if (cmd->op == 0 && cmd->path_len > 0) {
                        draw_path_cmd(dl, cmd, origin);
                    }
                }

                if (frame.scene_width > 0 && frame.scene_height > 0) {
                    ImDrawList_PopClipRect(dl);
                }
                GoInamateDrawFrameFree(&frame);
            }
        }
        igEnd();
    }
    if (window_state.show_properties) {
        if (igBegin("Properties", &window_state.show_canvas, ImGuiWindowFlags_None)) {
        }
        igEnd();
    }
    if (window_state.show_timeline) {
        if (igBegin("Timeline", &window_state.show_canvas, ImGuiWindowFlags_None)) {
        }
        igEnd();
    }

    // Demo Window
    if (window_state.show_demo) {
        igShowDemoWindow(&window_state.show_demo);
    }
}

