cmake_minimum_required(VERSION 3.15)

project(cinamate C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build bin2c tool for schema generation
add_executable(bin2c tool/bin2c.c)
set_target_properties(bin2c PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tool
)

# Generate font headers from TTF files using bin2c
set(FONT_SRC_DIR ${CMAKE_SOURCE_DIR}/font)
set(FONT_DST_DIR ${CMAKE_SOURCE_DIR}/src/font)
file(MAKE_DIRECTORY ${FONT_DST_DIR})
file(GLOB FONT_TTF_FILES ${FONT_SRC_DIR}/*.ttf)
set(FONT_HEADERS "")
foreach(TTF_FILE ${FONT_TTF_FILES})
    get_filename_component(TTF_NAME ${TTF_FILE} NAME)
    string(REPLACE "." "_" HEADER_BASE ${TTF_NAME})
    set(HEADER_FILE ${FONT_DST_DIR}/${HEADER_BASE}.h)
    string(REPLACE "-" "_" VAR_NAME ${HEADER_BASE})
    add_custom_command(
        OUTPUT ${HEADER_FILE}
        COMMAND $<TARGET_FILE:bin2c> ${TTF_FILE} ${HEADER_FILE} ${VAR_NAME}
        DEPENDS ${TTF_FILE} bin2c
        COMMENT "Generating ${HEADER_BASE}.h from ${TTF_NAME} using bin2c"
    )
    list(APPEND FONT_HEADERS ${HEADER_FILE})
endforeach()

# Copy icon header files into src/font
set(ICON_HEADERS IconsFontAwesome6.h IconsMaterialDesign.h)
foreach(ICON_HEADER ${ICON_HEADERS})
    add_custom_command(
        OUTPUT ${FONT_DST_DIR}/${ICON_HEADER}
        COMMAND ${CMAKE_COMMAND} -E copy ${FONT_SRC_DIR}/${ICON_HEADER} ${FONT_DST_DIR}/${ICON_HEADER}
        DEPENDS ${FONT_SRC_DIR}/${ICON_HEADER}
        COMMENT "Copying ${ICON_HEADER} to src/font"
    )
    list(APPEND FONT_HEADERS ${FONT_DST_DIR}/${ICON_HEADER})
endforeach()

add_custom_target(generate_fonts DEPENDS ${FONT_HEADERS})

# Build libgo from Go source
find_program(GOLANG_BINARY "go")
if(NOT GOLANG_BINARY)
    message(FATAL_ERROR "Cannot find the \"go\" binary, please install it and add it to PATH")
endif()
file(GLOB LIBGO_SOURCES "${PROJECT_SOURCE_DIR}/libgo/*.go")
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/libgo/libgo.a" "${PROJECT_BINARY_DIR}/libgo/libgo.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/libgo"
    COMMAND ${GOLANG_BINARY} build -buildmode=c-archive -o "${PROJECT_BINARY_DIR}/libgo/libgo.a" .
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/libgo"
    DEPENDS ${LIBGO_SOURCES}
    COMMENT "Building libgo from Go source"
)
add_custom_target(build_libgo DEPENDS "${PROJECT_BINARY_DIR}/libgo/libgo.a")

####### BUILD APP ###############
include_directories(.)
include_directories(deps)
include_directories(deps/cimgui)
include_directories(${PROJECT_BINARY_DIR}/libgo)

# Platform-specific configuration
if(APPLE)
    set(BACKEND -DSOKOL_METAL)
    set(PLATFORM_LIBS "-framework Cocoa" "-framework QuartzCore" "-framework Metal" "-framework MetalKit" "-framework Security" "-framework CoreFoundation")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ObjC")
elseif(WIN32)
    set(BACKEND -DSOKOL_D3D11)
    set(PLATFORM_LIBS gdi32 ole32 d3d11 dxgi ws2_32 crypt32 bcrypt ncrypt winmm)
    add_definitions(-D_WIN32_WINNT=0x0601)
else()
    # Linux
    set(BACKEND -DSOKOL_GLCORE)
    set(PLATFORM_LIBS GL m pthread dl resolv)
    add_definitions(-D_GNU_SOURCE)
endif()

add_definitions(${BACKEND})


add_executable(${PROJECT_NAME}
    src/main.c
    src/window.c
    src/canvas.c
    src/properties.c
    src/timeline.c
    src/toolbar.c
    deps/deps.c
    deps/deps.cpp
    deps/cimgui/cimgui.cpp
    deps/cimgui/imgui/imgui.cpp
    deps/cimgui/imgui/imgui_draw.cpp
    deps/cimgui/imgui/imgui_tables.cpp
    deps/cimgui/imgui/imgui_widgets.cpp
    deps/cimgui/imgui/imgui_demo.cpp
)

# macOS app bundle
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Cinamate"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.millardsoftware.cinamate"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    )
endif()

# Ensure generated files are created before building the executable
add_dependencies(${PROJECT_NAME}
    generate_fonts
    build_libgo
)

target_link_libraries(${PROJECT_NAME}
    ${PLATFORM_LIBS}
    ${PROJECT_BINARY_DIR}/libgo/libgo.a
)

