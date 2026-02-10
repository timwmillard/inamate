// Canvas rendering — extracted from ui_window.c

#ifndef CANVAS_H
#define CANVAS_H

#include <stdbool.h>

typedef struct {
    char scene_id[64];
    char scene_name[128];
    char background[16];
    int scene_width;
    int scene_height;
} CanvasSceneInfo;

void ui_canvas(bool *open);
void ui_canvas_zoom_in(void);
void ui_canvas_zoom_out(void);
void ui_canvas_zoom_reset(void);
const CanvasSceneInfo *ui_canvas_get_scene_info(void);
const char *ui_canvas_get_selected_id(void);
void ui_canvas_clear_selection(void);

#endif
