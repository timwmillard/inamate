package main

/*
#include <stdlib.h>
#include <string.h>

#ifndef ARENA_FWD_DECL_
#define ARENA_FWD_DECL_
typedef struct Arena Arena;
extern void *arena_alloc(Arena *a, size_t size_bytes);
#endif

typedef struct {
    char type;        // 'M', 'L', 'C', 'Q', 'Z'
    int num_coords;
    float coords[6];  // M/L: x,y (2); Q: x1,y1,x,y (4); C: x1,y1,x2,y2,x,y (6)
} InPathCmd;

typedef struct {
    int op;              // enum: 0=path, 1=image, 2=save, 3=restore, 4=clip
    char object_id[64];
    float transform[6];  // affine [a,b,c,d,e,f]
    InPathCmd *path;
    int path_len;
    char fill[16];       // hex color e.g. "#e94560"
    char stroke[16];
    float stroke_width;
    float opacity;
    char image_asset_id[64];
    float image_width;
    float image_height;
} InDrawCmd;

typedef struct {
    InDrawCmd *commands;
    int count;
    char scene_id[64];
    char scene_name[128];
    char background[16];  // scene background hex color
    int scene_width;
    int scene_height;
} InDrawFrame;
*/
import "C"
import (
	"unsafe"

	"github.com/inamate/inamate/backend-go/engine"
)

// opIndex maps draw command op strings to integer enum values.
var opIndex = map[string]C.int{
	"path":    0,
	"image":   1,
	"save":    2,
	"restore": 3,
	"clip":    4,
}

// copyToCharArray copies a Go string into a fixed-size C char array.
func copyToCharArray(dst *C.char, size int, src string) {
	if len(src) >= size {
		src = src[:size-1]
	}
	buf := unsafe.Slice((*byte)(unsafe.Pointer(dst)), size)
	copy(buf, src)
	buf[len(src)] = 0
}

// packPathCommands converts a Go []PathCommand into an arena-allocated InPathCmd array.
func packPathCommands(a *C.Arena, path []engine.PathCommand) (*C.InPathCmd, C.int) {
	n := len(path)
	if n == 0 {
		return nil, 0
	}

	arr := (*C.InPathCmd)(C.arena_alloc(a, C.size_t(n)*C.size_t(unsafe.Sizeof(C.InPathCmd{}))))
	slice := unsafe.Slice(arr, n)

	for i, pc := range path {
		cmd := &slice[i]
		// Zero out
		C.memset(unsafe.Pointer(cmd), 0, C.size_t(unsafe.Sizeof(C.InPathCmd{})))

		if len(pc) == 0 {
			continue
		}

		// First element is the type string
		if t, ok := pc[0].(string); ok && len(t) > 0 {
			cmd._type = C.char(t[0])
		}

		// Remaining elements are float coordinates
		coordCount := 0
		for j := 1; j < len(pc) && coordCount < 6; j++ {
			if v, ok := toFloat64(pc[j]); ok {
				cmd.coords[coordCount] = C.float(v)
				coordCount++
			}
		}
		cmd.num_coords = C.int(coordCount)
	}

	return arr, C.int(n)
}

// toFloat64 extracts a float64 from an interface{} that may be float64 or int.
func toFloat64(v interface{}) (float64, bool) {
	switch n := v.(type) {
	case float64:
		return n, true
	case float32:
		return float64(n), true
	case int:
		return float64(n), true
	case int64:
		return float64(n), true
	default:
		return 0, false
	}
}

// GoInamateEngineRenderFrame returns the current frame's draw commands as C structs.
// Memory is allocated from the provided arena; caller resets the arena when done.
//
//export GoInamateEngineRenderFrame
func GoInamateEngineRenderFrame(a *C.Arena) C.InDrawFrame {
	engMu.Lock()
	scene := eng.GetSceneInfo()
	commands := eng.RenderCommands()
	engMu.Unlock()
	n := len(commands)
	if n == 0 {
		var frame C.InDrawFrame
		copyToCharArray(&frame.scene_id[0], 64, scene.ID)
		copyToCharArray(&frame.scene_name[0], 128, scene.Name)
		copyToCharArray(&frame.background[0], 16, scene.Background)
		frame.scene_width = C.int(scene.Width)
		frame.scene_height = C.int(scene.Height)
		return frame
	}

	arr := (*C.InDrawCmd)(C.arena_alloc(a, C.size_t(n)*C.size_t(unsafe.Sizeof(C.InDrawCmd{}))))
	slice := unsafe.Slice(arr, n)

	for i, dc := range commands {
		cmd := &slice[i]
		C.memset(unsafe.Pointer(cmd), 0, C.size_t(unsafe.Sizeof(C.InDrawCmd{})))

		cmd.op = opIndex[dc.Op]
		copyToCharArray(&cmd.object_id[0], 64, dc.ObjectID)
		copyToCharArray(&cmd.fill[0], 16, dc.Fill)
		copyToCharArray(&cmd.stroke[0], 16, dc.Stroke)
		cmd.stroke_width = C.float(dc.StrokeWidth)
		cmd.opacity = C.float(dc.Opacity)
		copyToCharArray(&cmd.image_asset_id[0], 64, dc.ImageAssetID)
		cmd.image_width = C.float(dc.ImageWidth)
		cmd.image_height = C.float(dc.ImageHeight)

		// Pack transform
		for j := 0; j < len(dc.Transform) && j < 6; j++ {
			cmd.transform[j] = C.float(dc.Transform[j])
		}

		// Pack path commands
		cmd.path, cmd.path_len = packPathCommands(a, dc.Path)
	}

	var frame C.InDrawFrame
	frame.commands = arr
	frame.count = C.int(n)
	copyToCharArray(&frame.scene_id[0], 64, scene.ID)
	copyToCharArray(&frame.scene_name[0], 128, scene.Name)
	copyToCharArray(&frame.background[0], 16, scene.Background)
	frame.scene_width = C.int(scene.Width)
	frame.scene_height = C.int(scene.Height)
	return frame
}

