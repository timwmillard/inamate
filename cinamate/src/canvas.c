// Canvas rendering — extracted from ui_window.c

#include <math.h>
#include <string.h>

#include "arena.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#define ARENA_FWD_DECL_ // suppress redeclaration in libgo.h
#include "libgo.h"

#include "canvas.h"
#include "toolbar.h"

static Arena frame_arena = {0};

static float canvas_zoom = 1.0f;
static ImVec2 canvas_pan = {0};

static CanvasSceneInfo last_scene_info = {0};

static bool is_drawing_rect = false;
static ImVec2 rect_draw_start = {0};

static char selected_object_id[64] = {0};

static bool is_dragging_selection = false;
static ImVec2 drag_start_scene = {0};
static float drag_initial_x = 0, drag_initial_y = 0;

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

void ui_canvas_zoom_in(void) {
    canvas_zoom *= 1.25f;
    if (canvas_zoom > 20.0f) canvas_zoom = 20.0f;
}

void ui_canvas_zoom_out(void) {
    canvas_zoom *= 0.8f;
    if (canvas_zoom < 0.05f) canvas_zoom = 0.05f;
}

void ui_canvas_zoom_reset(void) {
    canvas_zoom = 1.0f;
    canvas_pan = (ImVec2){0, 0};
}

const CanvasSceneInfo *ui_canvas_get_scene_info(void) {
    return &last_scene_info;
}

const char *ui_canvas_get_selected_id(void) {
    return selected_object_id;
}

void ui_canvas_clear_selection(void) {
    selected_object_id[0] = '\0';
}

void ui_canvas(bool *open) {
    igPushStyleVar_Vec2(ImGuiStyleVar_WindowPadding, (ImVec2){0, 0});
    if (igBegin("Canvas", open, ImGuiWindowFlags_None)) {
        ImVec2 cursor_origin;
        igGetCursorScreenPos(&cursor_origin);

        // Compute scene-space mouse position (used for tools and cursor sending)
        ImGuiIO *io = igGetIO_Nil();
        float scene_x = (io->MousePos.x - cursor_origin.x - canvas_pan.x) / canvas_zoom;
        float scene_y = (io->MousePos.y - cursor_origin.y - canvas_pan.y) / canvas_zoom;

        // Input: zoom and pan
        if (igIsWindowHovered(ImGuiHoveredFlags_None)) {
            // Mouse wheel → zoom toward cursor
            if (io->MouseWheel != 0) {
                float old_zoom = canvas_zoom;
                float factor = (io->MouseWheel > 0) ? 1.1f : 1.0f / 1.1f;
                canvas_zoom *= factor;
                if (canvas_zoom < 0.05f) canvas_zoom = 0.05f;
                if (canvas_zoom > 20.0f) canvas_zoom = 20.0f;

                // Zoom toward mouse position
                float ratio = 1.0f - canvas_zoom / old_zoom;
                canvas_pan.x += (io->MousePos.x - cursor_origin.x - canvas_pan.x) * ratio;
                canvas_pan.y += (io->MousePos.y - cursor_origin.y - canvas_pan.y) * ratio;
            }

            // Middle-mouse drag → pan
            if (igIsMouseDragging(ImGuiMouseButton_Middle, 0)) {
                ImVec2 delta;
                igGetMouseDragDelta(&delta, ImGuiMouseButton_Middle, 0);
                igResetMouseDragDelta(ImGuiMouseButton_Middle);
                canvas_pan.x += delta.x;
                canvas_pan.y += delta.y;
            }

            // Send cursor position to server (throttled ~60ms)
            {
                static double last_cursor_send = 0;
                double now = igGetTime();
                if (now - last_cursor_send > 0.060) {
                    GoInamateSendCursor(scene_x, scene_y);
                    last_cursor_send = now;
                }
            }

            ToolType tool = ui_toolbar_get_active_tool();

            // Select tool: click to select/deselect, initiate drag
            if (tool == TOOL_SELECT && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false)) {
                char hit_id[64] = {0};
                int hit = GoInamateHitTest(scene_x, scene_y, hit_id, 64);
                if (hit) {
                    memcpy(selected_object_id, hit_id, 64);
                    is_dragging_selection = true;
                    drag_start_scene = (ImVec2){scene_x, scene_y};
                    InObjectInfo info = GoInamateGetSelectedObject();
                    drag_initial_x = info.x;
                    drag_initial_y = info.y;
                } else {
                    selected_object_id[0] = '\0';
                    GoInamateClearSelection();
                }
            }

            // Rect tool: start drawing
            if (tool == TOOL_RECT && igIsMouseClicked_Bool(ImGuiMouseButton_Left, false)) {
                is_drawing_rect = true;
                rect_draw_start.x = scene_x;
                rect_draw_start.y = scene_y;
            }
        }

        // Rect tool: finalize on mouse release (works even if mouse drifts outside window)
        if (is_drawing_rect && igIsMouseReleased_Nil(ImGuiMouseButton_Left)) {
            is_drawing_rect = false;
            float rx = fminf(rect_draw_start.x, scene_x);
            float ry = fminf(rect_draw_start.y, scene_y);
            float rw = fabsf(scene_x - rect_draw_start.x);
            float rh = fabsf(scene_y - rect_draw_start.y);
            if (rw < 3 && rh < 3) {
                rw = 100; rh = 100;
                rx = rect_draw_start.x - 50;
                ry = rect_draw_start.y - 50;
            }
            GoInamateCreateRect(rx, ry, rw, rh);
        }

        // Select tool: drag to move selected object
        if (is_dragging_selection) {
            if (igIsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
                float new_x = drag_initial_x + (scene_x - drag_start_scene.x);
                float new_y = drag_initial_y + (scene_y - drag_start_scene.y);
                char json[128];
                snprintf(json, sizeof(json), "{\"x\":%g,\"y\":%g}", (double)new_x, (double)new_y);
                GoInamateObjectTransform(selected_object_id, json);
            }
            if (igIsMouseReleased_Nil(ImGuiMouseButton_Left)) {
                is_dragging_selection = false;
            }
        }

        float zoom = canvas_zoom;
        ImVec2 view_origin = {
            cursor_origin.x + canvas_pan.x,
            cursor_origin.y + canvas_pan.y,
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

            // Cache scene info for properties panel
            memcpy(last_scene_info.scene_id, frame.scene_id, 64);
            memcpy(last_scene_info.scene_name, frame.scene_name, 128);
            memcpy(last_scene_info.background, frame.background, 16);
            last_scene_info.scene_width = frame.scene_width;
            last_scene_info.scene_height = frame.scene_height;

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

            // Draw rect tool preview overlay
            if (is_drawing_rect) {
                float sx = scene_x, sy = scene_y;
                float min_sx = fminf(rect_draw_start.x, sx);
                float min_sy = fminf(rect_draw_start.y, sy);
                float max_sx = fmaxf(rect_draw_start.x, sx);
                float max_sy = fmaxf(rect_draw_start.y, sy);
                ImVec2 min_pt = {
                    view_origin.x + min_sx * zoom,
                    view_origin.y + min_sy * zoom,
                };
                ImVec2 max_pt = {
                    view_origin.x + max_sx * zoom,
                    view_origin.y + max_sy * zoom,
                };
                ImU32 preview_fill = INAMATE_COL32(74, 144, 217, 40);
                ImU32 preview_stroke = INAMATE_COL32(74, 144, 217, 200);
                ImDrawList_AddRectFilled(dl, min_pt, max_pt, preview_fill, 0, 0);
                ImDrawList_AddRect(dl, min_pt, max_pt, preview_stroke, 0, 0, 1.5f);
            }

            // Selection indicator: outline + corner/edge handles
            if (selected_object_id[0]) {
                float sel_x, sel_y, sel_w, sel_h;
                if (GoInamateGetSelectionBounds(&sel_x, &sel_y, &sel_w, &sel_h)) {
                    ImVec2 smin = {
                        view_origin.x + sel_x * zoom,
                        view_origin.y + sel_y * zoom,
                    };
                    ImVec2 smax = {
                        view_origin.x + (sel_x + sel_w) * zoom,
                        view_origin.y + (sel_y + sel_h) * zoom,
                    };

                    ImU32 blue = INAMATE_COL32(74, 144, 217, 255);
                    ImU32 white = INAMATE_COL32(255, 255, 255, 255);

                    // Outer contrast border (dark) then inner selection border (blue)
                    ImDrawList_AddRect(dl, smin, smax, INAMATE_COL32(0, 0, 0, 80), 0, 0, 3.0f);
                    ImDrawList_AddRect(dl, smin, smax, blue, 0, 0, 1.5f);

                    // Handle size in screen pixels
                    float hs = 4.0f;
                    float mx = (smin.x + smax.x) * 0.5f;
                    float my = (smin.y + smax.y) * 0.5f;

                    // Corner + edge midpoint positions (8 handles)
                    ImVec2 handles[8] = {
                        {smin.x, smin.y}, {smax.x, smin.y},  // TL, TR
                        {smin.x, smax.y}, {smax.x, smax.y},  // BL, BR
                        {mx,     smin.y}, {mx,     smax.y},   // T,  B
                        {smin.x, my},     {smax.x, my},       // L,  R
                    };

                    for (int h = 0; h < 8; h++) {
                        ImVec2 hmin = {handles[h].x - hs, handles[h].y - hs};
                        ImVec2 hmax = {handles[h].x + hs, handles[h].y + hs};
                        ImDrawList_AddRectFilled(dl, hmin, hmax, white, 0, 0);
                        ImDrawList_AddRect(dl, hmin, hmax, blue, 0, 0, 1.0f);
                    }
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

    arena_reset(&frame_arena);
}
