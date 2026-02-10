// UI Window

#include <math.h>
#include <string.h>

#include "sokol_app.h"
#include "arena.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#define ARENA_FWD_DECL_ // suppress redeclaration in libgo.h
#include "libgo.h"

static Arena frame_arena = {0};

static struct {
    // GUI
    bool show_canvas;
    bool show_properties;
    bool show_timeline;

    bool show_demo;
    bool dock_setup_done;

    // Canvas view
    float canvas_zoom;
    ImVec2 canvas_pan;

} window_state = {
    .canvas_zoom = 1.0f,
};

// Pack RGBA into ImU32 (ImGui's ABGR byte order)
#define INAMATE_COL32(r,g,b,a) (((ImU32)(a)<<24) | ((ImU32)(b)<<16) | ((ImU32)(g)<<8) | ((ImU32)(r)))

// Parse "#RRGGBB" hex color to ImU32
static ImU32 hex_to_imu32(const char *hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) return INAMATE_COL32(255, 255, 255, 255);
    unsigned int r, g, b;
    sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    return INAMATE_COL32(r, g, b, 255);
}

// Apply affine transform [a,b,c,d,e,f] to a point, with view zoom and origin offset
static ImVec2 xform_pt(const float t[6], float x, float y, ImVec2 origin, float zoom) {
    return (ImVec2){
        origin.x + (t[0]*x + t[2]*y + t[4]) * zoom,
        origin.y + (t[1]*x + t[3]*y + t[5]) * zoom,
    };
}

// Walk path commands into the ImDrawList path buffer
static void walk_path(ImDrawList *dl, InDrawCmd *cmd, ImVec2 origin, float zoom) {
    float cx = 0, cy = 0;
    for (int j = 0; j < cmd->path_len; j++) {
        InPathCmd *pc = &cmd->path[j];
        switch (pc->type) {
        case 'M':
            ImDrawList_PathClear(dl);
            cx = pc->coords[0]; cy = pc->coords[1];
            ImDrawList_PathLineTo(dl, xform_pt(cmd->transform, cx, cy, origin, zoom));
            break;
        case 'L':
            cx = pc->coords[0]; cy = pc->coords[1];
            ImDrawList_PathLineTo(dl, xform_pt(cmd->transform, cx, cy, origin, zoom));
            break;
        case 'Q': {
            float x1 = pc->coords[0], y1 = pc->coords[1];
            float x2 = pc->coords[2], y2 = pc->coords[3];
            ImDrawList_PathBezierQuadraticCurveTo(dl,
                xform_pt(cmd->transform, x1, y1, origin, zoom),
                xform_pt(cmd->transform, x2, y2, origin, zoom), 0);
            cx = x2; cy = y2;
            break;
        }
        case 'C': {
            float x1 = pc->coords[0], y1 = pc->coords[1];
            float x2 = pc->coords[2], y2 = pc->coords[3];
            float x3 = pc->coords[4], y3 = pc->coords[5];
            ImDrawList_PathBezierCubicCurveTo(dl,
                xform_pt(cmd->transform, x1, y1, origin, zoom),
                xform_pt(cmd->transform, x2, y2, origin, zoom),
                xform_pt(cmd->transform, x3, y3, origin, zoom), 0);
            cx = x3; cy = y3;
            break;
        }
        case 'Z':
            break;
        }
    }
}

// If the last point in the path buffer is very close to the first, remove it
// so that ImDrawFlags_Closed creates a proper non-degenerate closing segment.
// Without this, self-closing paths (e.g. ellipses whose last bezier returns
// to the moveTo point) produce a zero-length closing segment that causes a
// diamond-shaped join artifact when stroked.
static void dedup_closing_point(ImDrawList *dl) {
    if (dl->_Path.Size < 3) return;
    ImVec2 first = dl->_Path.Data[0];
    ImVec2 last  = dl->_Path.Data[dl->_Path.Size - 1];
    float dx = first.x - last.x;
    float dy = first.y - last.y;
    if (dx*dx + dy*dy < 1.0f) {
        dl->_Path.Size--;
    }
}

// Render a single path draw command to an ImDrawList
static void draw_path_cmd(ImDrawList *dl, InDrawCmd *cmd, ImVec2 origin, float zoom) {
    ImDrawList_PathClear(dl);

    // Fill
    if (cmd->fill[0] == '#') {
        walk_path(dl, cmd, origin, zoom);
        ImU32 col = hex_to_imu32(cmd->fill);
        if (cmd->opacity > 0 && cmd->opacity < 1.0f) {
            col = (col & 0x00FFFFFF) | ((ImU32)(cmd->opacity * 255) << 24);
        }
        ImDrawList_PathFillConcave(dl, col);
    }

    // Stroke (re-walk path since fill consumed it)
    if (cmd->stroke[0] == '#') {
        ImDrawList_PathClear(dl);
        walk_path(dl, cmd, origin, zoom);
        dedup_closing_point(dl);
        ImU32 scol = hex_to_imu32(cmd->stroke);
        float sw = (cmd->stroke_width > 0 ? cmd->stroke_width : 1.0f) * zoom;
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
            if (igMenuItem_Bool("Zoom In", "Cmd+=", false, true)) {
                window_state.canvas_zoom *= 1.25f;
                if (window_state.canvas_zoom > 20.0f) window_state.canvas_zoom = 20.0f;
            }
            if (igMenuItem_Bool("Zoom Out", "Cmd+-", false, true)) {
                window_state.canvas_zoom *= 0.8f;
                if (window_state.canvas_zoom < 0.05f) window_state.canvas_zoom = 0.05f;
            }
            if (igMenuItem_Bool("Reset Zoom", "Cmd+0", false, true)) {
                window_state.canvas_zoom = 1.0f;
                window_state.canvas_pan = (ImVec2){0, 0};
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

    // Canvas Window
    if (window_state.show_canvas) {
        igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){0, 0});
        if (igBegin("Canvas", &window_state.show_canvas, ImGuiWindowFlags_None)) {
            ImVec2 cursor_origin;
            igGetCursorScreenPos(&cursor_origin);

            // Input: zoom and pan
            if (igIsWindowHovered(ImGuiHoveredFlags_None)) {
                ImGuiIO *io = igGetIO_Nil();

                // Mouse wheel → zoom toward cursor
                if (io->MouseWheel != 0) {
                    float old_zoom = window_state.canvas_zoom;
                    float factor = (io->MouseWheel > 0) ? 1.1f : 1.0f / 1.1f;
                    window_state.canvas_zoom *= factor;
                    if (window_state.canvas_zoom < 0.05f) window_state.canvas_zoom = 0.05f;
                    if (window_state.canvas_zoom > 20.0f) window_state.canvas_zoom = 20.0f;

                    // Zoom toward mouse position
                    float ratio = 1.0f - window_state.canvas_zoom / old_zoom;
                    window_state.canvas_pan.x += (io->MousePos.x - cursor_origin.x - window_state.canvas_pan.x) * ratio;
                    window_state.canvas_pan.y += (io->MousePos.y - cursor_origin.y - window_state.canvas_pan.y) * ratio;
                }

                // Middle-mouse drag → pan
                if (igIsMouseDragging(ImGuiMouseButton_Middle, 0)) {
                    ImVec2 delta;
                    igGetMouseDragDelta(&delta, ImGuiMouseButton_Middle, 0);
                    igResetMouseDragDelta(ImGuiMouseButton_Middle);
                    window_state.canvas_pan.x += delta.x;
                    window_state.canvas_pan.y += delta.y;
                }

                // Send cursor position to server (throttled ~60ms)
                {
                    static double last_cursor_send = 0;
                    double now = igGetTime();
                    if (now - last_cursor_send > 0.060) {
                        float scene_x = (io->MousePos.x - cursor_origin.x - window_state.canvas_pan.x) / window_state.canvas_zoom;
                        float scene_y = (io->MousePos.y - cursor_origin.y - window_state.canvas_pan.y) / window_state.canvas_zoom;
                        GoInamateSendCursor(scene_x, scene_y);
                        last_cursor_send = now;
                    }
                }
            }

            float zoom = window_state.canvas_zoom;
            ImVec2 view_origin = {
                cursor_origin.x + window_state.canvas_pan.x,
                cursor_origin.y + window_state.canvas_pan.y,
            };

            // Draw checkerboard background (matches frontend viewport pattern)
            {
                ImDrawList *dl = igGetWindowDrawList();
                ImVec2 wsize;
                igGetContentRegionAvail(&wsize);
                float x0 = cursor_origin.x;
                float y0 = cursor_origin.y;
                float x1 = x0 + wsize.x;
                float y1 = y0 + wsize.y;

                ImU32 col_a = INAMATE_COL32(26, 26, 26, 255);   // #1a1a1a
                ImU32 col_b = INAMATE_COL32(34, 34, 34, 255);   // #222222
                float tile = 20.0f;

                // Fill base color
                ImDrawList_AddRectFilled(dl, (ImVec2){x0, y0}, (ImVec2){x1, y1}, col_a, 0, 0);

                // Draw alternating squares
                ImDrawList_PushClipRect(dl, (ImVec2){x0, y0}, (ImVec2){x1, y1}, true);
                int col_start = (int)floorf(x0 / tile);
                int col_end   = (int)ceilf(x1 / tile);
                int row_start = (int)floorf(y0 / tile);
                int row_end   = (int)ceilf(y1 / tile);
                for (int row = row_start; row < row_end; row++) {
                    for (int col = col_start; col < col_end; col++) {
                        if ((row + col) % 2 == 0) continue;
                        float rx = col * tile;
                        float ry = row * tile;
                        ImDrawList_AddRectFilled(dl,
                            (ImVec2){rx, ry},
                            (ImVec2){rx + tile, ry + tile},
                            col_b, 0, 0);
                    }
                }
                ImDrawList_PopClipRect(dl);
            }

            if (GoInamateIsDocLoaded()) {
                ImDrawList *dl = igGetWindowDrawList();

                InDrawFrame frame = GoInamateEngineRenderFrame(&frame_arena);

                // Draw scene background and clip to scene bounds
                if (frame.scene_width > 0 && frame.scene_height > 0) {
                    ImVec2 bg_min = view_origin;
                    ImVec2 bg_max = {
                        view_origin.x + frame.scene_width * zoom,
                        view_origin.y + frame.scene_height * zoom,
                    };

                    if (frame.background[0] == '#') {
                        ImDrawList_AddRectFilled(dl, bg_min, bg_max, hex_to_imu32(frame.background), 0, 0);
                    }

                    ImDrawList_PushClipRect(dl, bg_min, bg_max, true);
                }

                for (int i = 0; i < frame.count; i++) {
                    InDrawCmd *cmd = &frame.commands[i];
                    if (cmd->op == 0 && cmd->path_len > 0) {
                        draw_path_cmd(dl, cmd, view_origin, zoom);
                    }
                }

                if (frame.scene_width > 0 && frame.scene_height > 0) {
                    ImDrawList_PopClipRect(dl);
                }
            }

            // Draw remote user cursors
            {
                InPresenceList presences = GoInamateGetPresences(&frame_arena);
                if (presences.count > 0) {
                    ImDrawList *dl = igGetWindowDrawList();

                    for (int i = 0; i < presences.count; i++) {
                        InPresence *p = &presences.entries[i];
                        if (!p->has_cursor) continue;

                        // Scene to screen coordinates
                        float sx = view_origin.x + p->cursor_x * zoom;
                        float sy = view_origin.y + p->cursor_y * zoom;

                        ImU32 col = hex_to_imu32(p->color);
                        ImU32 white = INAMATE_COL32(255, 255, 255, 255);

                        // Cursor arrow shape (matches frontend SVG path)
                        ImVec2 pts[4] = {
                            {sx,        sy},
                            {sx + 14.f, sy + 9.f},
                            {sx + 7.f,  sy + 9.5f},
                            {sx + 4.f,  sy + 16.f},
                        };

                        // Fill
                        ImDrawList_PathClear(dl);
                        for (int j = 0; j < 4; j++) ImDrawList_PathLineTo(dl, pts[j]);
                        ImDrawList_PathFillConcave(dl, col);

                        // Outline
                        ImDrawList_PathClear(dl);
                        for (int j = 0; j < 4; j++) ImDrawList_PathLineTo(dl, pts[j]);
                        ImDrawList_PathStroke(dl, white, ImDrawFlags_Closed, 1.0f);

                        // Name label
                        if (p->display_name[0]) {
                            ImVec2 text_size;
                            igCalcTextSize(&text_size, p->display_name, NULL, false, 0);
                            float lx = sx + 12.f;
                            float ly = sy + 14.f;
                            float px = 4.f, py = 2.f;

                            ImDrawList_AddRectFilled(dl,
                                (ImVec2){lx - px, ly - py},
                                (ImVec2){lx + text_size.x + px, ly + text_size.y + py},
                                col, 3.0f, 0);
                            ImDrawList_AddText_Vec2(dl,
                                (ImVec2){lx, ly}, white,
                                p->display_name, NULL);
                        }
                    }
                }
            }

        }
        igEnd();
        igPopStyleVar(1);
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

    arena_reset(&frame_arena);
}

