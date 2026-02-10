// Canvas rendering — extracted from ui_window.c

#ifndef CANVAS_H
#define CANVAS_H

#include <stdbool.h>

void ui_canvas(bool *open);
void ui_canvas_zoom_in(void);
void ui_canvas_zoom_out(void);
void ui_canvas_zoom_reset(void);

#endif
