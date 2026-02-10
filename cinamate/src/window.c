// UI Window

#include "sokol_app.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "canvas.h"
#include "properties.h"
#include "timeline.h"
#include "toolbar.h"

#include "arena.h"
#define ARENA_FWD_DECL_
#include "libgo.h"

static struct {
    // GUI
    bool show_canvas;
    bool show_properties;
    bool show_timeline;
    bool show_toolbar;

    bool show_demo;
    bool dock_setup_done;

} window_state = {0};

#define ARRAY_COUNT(arr) (sizeof((arr))/sizeof((arr)[0]))

void ui_reset_layout(ImGuiID dockspace_id)
{
    window_state.show_canvas = true;
    window_state.show_properties = true;
    window_state.show_timeline = true;
    window_state.show_toolbar = true;

    igDockBuilderRemoveNode(dockspace_id);
    igDockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    igDockBuilderSetNodeSize(dockspace_id, igGetMainViewport()->Size);

    ImGuiID dock_top = 0;
    ImGuiID dock_timeline = 0;
    igDockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.7f, &dock_top, &dock_timeline);

    ImGuiID dock_right = 0;
    ImGuiID dock_canvas_area = igDockBuilderSplitNode(dock_top, ImGuiDir_Left, 0.75f, NULL, &dock_right);

    // Split canvas area: toolbar (left) | canvas (right)
    ImGuiID dock_toolbar = 0;
    ImGuiID dock_canvas = 0;
    igDockBuilderSplitNode(dock_canvas_area, ImGuiDir_Left, 0.05f, &dock_toolbar, &dock_canvas);

    igDockBuilderDockWindow("Toolbar", dock_toolbar);
    igDockBuilderDockWindow("Canvas", dock_canvas);
    igDockBuilderDockWindow("Properties", dock_right);
    igDockBuilderDockWindow("Timeline", dock_timeline);

    igDockBuilderGetNode(dock_toolbar)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResize;
    igDockBuilderGetNode(dock_canvas)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
    igDockBuilderGetNode(dock_right)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
    igDockBuilderGetNode(dock_timeline)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

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
                GoInamateDeleteAll();
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
                ui_canvas_zoom_in();
            }
            if (igMenuItem_Bool("Zoom Out", "Cmd+-", false, true)) {
                ui_canvas_zoom_out();
            }
            if (igMenuItem_Bool("Reset Zoom", "Cmd+0", false, true)) {
                ui_canvas_zoom_reset();
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

    // Toolbar
    if (window_state.show_toolbar) {
        ui_toolbar(&window_state.show_toolbar);
    }

    // Canvas Window
    if (window_state.show_canvas) {
        ui_canvas(&window_state.show_canvas);
    }
    if (window_state.show_properties) {
        ui_properties(&window_state.show_properties);
    }
    if (window_state.show_timeline) {
        ui_timeline(&window_state.show_timeline);
    }

    // Demo Window
    if (window_state.show_demo) {
        igShowDemoWindow(&window_state.show_demo);
    }
}
