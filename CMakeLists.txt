cmake_minimum_required(VERSION 3.20)
project(hotd3-re
    VERSION 0.1.0
    DESCRIPTION "The House of the Dead III - Reverse Engineered"
    LANGUAGES CXX C
)

# ── C++ Standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build type ────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# ── Options ───────────────────────────────────────────────────────────────────
option(HOTD3_BUILD_DX11    "Build DirectX 11 rendering backend"   ON)
option(HOTD3_BUILD_VULKAN  "Build Vulkan rendering backend"       OFF)
option(HOTD3_BUILD_TOOLS   "Build asset extraction tools"         ON)
option(HOTD3_USE_IMGUI     "Enable Dear ImGui debug overlay"      ON)

# ── Platform detection ────────────────────────────────────────────────────────
if(WIN32)
    add_definitions(-DHOTD3_PLATFORM_WINDOWS)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DHOTD3_PLATFORM_LINUX)
elseif(APPLE)
    add_definitions(-DHOTD3_PLATFORM_MACOS)
endif()

# ── Find packages ─────────────────────────────────────────────────────────────
# SDL2/SDL3 - windowing, input, audio device
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not found via find_package, trying pkg-config...")
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 sdl2)
    endif()
endif()

if(NOT SDL2_FOUND)
    message(WARNING
        "SDL2 not found. Please install SDL2 development libraries.\n"
        "  Windows: vcpkg install sdl2:x64-windows\n"
        "  Linux:   sudo apt install libsdl2-dev\n"
        "  macOS:   brew install sdl2"
    )
endif()

# ── Source files ──────────────────────────────────────────────────────────────
# Core engine
set(CORE_SOURCES
    src/core/game.cpp
    src/core/state_machine.cpp
    src/core/timer.cpp
    src/core/config.cpp
)

set(CORE_HEADERS
    src/core/game.h
    src/core/state_machine.h
    src/core/timer.h
    src/core/config.h
    src/core/types.h
)

# Rendering - abstract interface
set(RENDER_COMMON_SOURCES
    src/render/common/vertex.cpp
)

set(RENDER_COMMON_HEADERS
    src/render/renderer.h
    src/render/common/vertex.h
    src/render/common/shader_defs.h
)

# Input
set(INPUT_SOURCES
    src/input/input_manager.cpp
)

set(INPUT_HEADERS
    src/input/input_manager.h
    src/input/input_device.h
)

# Audio
set(AUDIO_SOURCES
    src/audio/audio_manager.cpp
)

set(AUDIO_HEADERS
    src/audio/audio_manager.h
)

# Scene
set(SCENE_SOURCES
    src/scene/scene_manager.cpp
)

set(SCENE_HEADERS
    src/scene/scene_manager.h
)

# UI / HUD
set(UI_SOURCES
    src/ui/hud.cpp
)

set(UI_HEADERS
    src/ui/hud.h
)

# File format parsers
set(FORMAT_SOURCES
    src/formats/format_registry.cpp
)

set(FORMAT_HEADERS
    src/formats/format_registry.h
)

# Video / FMV playback
set(VIDEO_SOURCES
    src/video/video_player.cpp
)

set(VIDEO_HEADERS
    src/video/video_player.h
)

# Physics / hit detection
set(PHYSICS_SOURCES
    src/physics/hit_detection.cpp
)

set(PHYSICS_HEADERS
    src/physics/hit_detection.h
)

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(hotd3-re
    src/main.cpp
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    ${RENDER_COMMON_SOURCES}
    ${RENDER_COMMON_HEADERS}
    ${INPUT_SOURCES}
    ${INPUT_HEADERS}
    ${AUDIO_SOURCES}
    ${AUDIO_HEADERS}
    ${SCENE_SOURCES}
    ${SCENE_HEADERS}
    ${UI_SOURCES}
    ${UI_HEADERS}
    ${FORMAT_SOURCES}
    ${FORMAT_HEADERS}
    ${VIDEO_SOURCES}
    ${VIDEO_HEADERS}
    ${PHYSICS_SOURCES}
    ${PHYSICS_HEADERS}
)

target_include_directories(hotd3-re PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib
)

# ── DX11 backend ─────────────────────────────────────────────────────────────
if(HOTD3_BUILD_DX11 AND WIN32)
    target_sources(hotd3-re PRIVATE
        src/render/dx11/dx11_renderer.cpp
        src/render/dx11/dx11_renderer.h
    )
    target_link_libraries(hotd3-re PRIVATE
        d3d11
        dxgi
        d3dcompiler
    )
    target_compile_definitions(hotd3-re PRIVATE HOTD3_HAS_DX11)
    message(STATUS "DirectX 11 backend: ENABLED")
endif()

# ── Vulkan backend ───────────────────────────────────────────────────────────
if(HOTD3_BUILD_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_sources(hotd3-re PRIVATE
            src/render/vulkan/vk_renderer.cpp
            src/render/vulkan/vk_renderer.h
        )
        target_link_libraries(hotd3-re PRIVATE Vulkan::Vulkan)
        target_compile_definitions(hotd3-re PRIVATE HOTD3_HAS_VULKAN)
        message(STATUS "Vulkan backend: ENABLED")
    else()
        message(WARNING "Vulkan SDK not found. Vulkan backend disabled.")
    endif()
endif()

# ── Link SDL2 ────────────────────────────────────────────────────────────────
if(SDL2_FOUND)
    target_include_directories(hotd3-re PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(hotd3-re PRIVATE ${SDL2_LIBRARIES})
    target_compile_definitions(hotd3-re PRIVATE HOTD3_HAS_SDL2)
endif()

# ── Compiler warnings ────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(hotd3-re PRIVATE /W4 /WX-)
else()
    target_compile_options(hotd3-re PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Asset extraction tools ───────────────────────────────────────────────────
if(HOTD3_BUILD_TOOLS)
    add_executable(hotd3-asset-extractor
        tools/asset-extractor/main.cpp
    )
    target_include_directories(hotd3-asset-extractor PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    message(STATUS "Asset extraction tools: ENABLED")
endif()

# ── Install ──────────────────────────────────────────────────────────────────
install(TARGETS hotd3-re RUNTIME DESTINATION bin)

message(STATUS "")
message(STATUS "=== HOTD3-RE Configuration ===")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform:       ${CMAKE_SYSTEM_NAME}")
message(STATUS "  DX11 backend:   ${HOTD3_BUILD_DX11}")
message(STATUS "  Vulkan backend: ${HOTD3_BUILD_VULKAN}")
message(STATUS "  Build tools:    ${HOTD3_BUILD_TOOLS}")
message(STATUS "  ImGui overlay:  ${HOTD3_USE_IMGUI}")
message(STATUS "==============================")
