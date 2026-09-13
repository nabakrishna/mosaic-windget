cmake_minimum_required(VERSION 3.20)
project(Mosaic LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Mosaic is a native Windows application and must be built on Windows with the MSVC toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unicode Win32 API surface everywhere.
add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

# Warnings-as-guidance, not warnings-as-errors yet (Phase 1 is still stabilizing).
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
endif()

add_executable(Mosaic WIN32
    src/main.cpp
    src/app/Application.cpp
    src/platform/Window.cpp
    src/ui/GraphicsDevice.cpp
    src/ui/Theme.cpp
    src/ui/Icons.cpp
    src/ui/DashboardView.cpp
    src/ui/components/GlassPanel.cpp
    src/ui/components/WidgetCard.cpp
)

target_include_directories(Mosaic PRIVATE src)

# Native Windows rendering/media/composition stack. No browser runtime, no
# third-party UI framework — this is the whole dependency list for Phase 1.
target_link_libraries(Mosaic PRIVATE
    d3d11.lib
    dxgi.lib
    d2d1.lib
    dwrite.lib
    dcomp.lib
    dwmapi.lib
    windowscodecs.lib
    user32.lib
    gdi32.lib
    shcore.lib
)

set_target_properties(Mosaic PROPERTIES
    OUTPUT_NAME "Mosaic"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
