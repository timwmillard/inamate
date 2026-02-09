# cinamate

Native desktop client for the Inamate animation editor. Built with C (Sokol + Dear ImGui) for the UI and rendering, with the core engine and collaboration logic provided by the shared Go backend compiled as a C-archive (`libgo`).

## What it does

- Connects to an Inamate server via WebSocket for real-time collaboration
- Receives and renders animation documents using the shared Go engine
- Displays vector paths on an ImGui canvas with pan and zoom
- Provides a dockable UI with canvas, properties, and timeline panels
- Cross-platform: macOS (Metal), Windows (D3D11), Linux (OpenGL)

## Dependencies

- **CMake** >= 3.15
- **Go** (for building `libgo`)
- A C/C++ compiler (Clang, GCC, or MSVC)

Vendored dependencies (in `deps/`):
- [Sokol](https://github.com/floooh/sokol) -- cross-platform app, gfx, and glue headers
- [cimgui](https://github.com/cimgui/cimgui) / [Dear ImGui](https://github.com/ocornut/imgui) -- immediate-mode GUI

## Building

```bash
cd cinamate
cmake -B build
cmake --build build
```

This will:

1. Build the `bin2c` tool and generate font headers from TTF files
2. Compile the Go code in `libgo/` into a static C-archive (`libgo.a`)
3. Build the `cinamate` executable, linking everything together

The resulting binary is at `build/cinamate`.

## Running

The app connects to a local Inamate server on startup:

```bash
./build/cinamate
```

Make sure the backend server is running first (`cd backend-go && go run ./cmd/server`).

## Project structure

```
cinamate/
  src/          C source (main.c, ui_window.c)
  libgo/        Go code compiled as c-archive (engine, collab, HTTP)
  deps/         Vendored C/C++ dependencies (sokol, cimgui)
  font/         TTF font files (converted to C headers at build time)
  tool/         Build utilities (bin2c)
  CMakeLists.txt
```
