cmake_minimum_required(VERSION 3.20)
project(Mosaic LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Mosaic is a native Windows application and must be built on Windows with the MSVC toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Unicode Win32 API surface everywhere.
add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

# Warnings-as-guidance, not warnings-as-errors (this project is still under
# active development phase by phase).
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
endif()

# --- SQLite (vendored amalgamation) ---------------------------------------
# Manual vendoring is preferred over always-download: it's deterministic,
# works offline once set up, and doesn't depend on a URL I can't verify
# from a non-Windows sandbox staying valid forever. See
# third_party/sqlite/README.txt for how to place the two files. If they're
# not there, we fall back to fetching them automatically so `cmake
# --build` still works out of the box on a machine with internet access.
set(MOSAIC_SQLITE_DIR "${CMAKE_SOURCE_DIR}/third_party/sqlite")

if(EXISTS "${MOSAIC_SQLITE_DIR}/sqlite3.c" AND EXISTS "${MOSAIC_SQLITE_DIR}/sqlite3.h")
    message(STATUS "Mosaic: using vendored SQLite amalgamation from ${MOSAIC_SQLITE_DIR}")
    set(SQLITE_C_FILE "${MOSAIC_SQLITE_DIR}/sqlite3.c")
    set(SQLITE_INCLUDE_DIR "${MOSAIC_SQLITE_DIR}")
else()
    message(STATUS "Mosaic: third_party/sqlite/sqlite3.{c,h} not found — attempting automatic download")
    include(FetchContent)
    FetchContent_Declare(
        sqlite_amalgamation
        URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
    )
    FetchContent_MakeAvailable(sqlite_amalgamation)
    set(SQLITE_C_FILE "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c")
    set(SQLITE_INCLUDE_DIR "${sqlite_amalgamation_SOURCE_DIR}")

    if(NOT EXISTS "${SQLITE_C_FILE}")
        message(FATAL_ERROR
            "Could not obtain SQLite automatically (no internet access, "
            "blocked download, or the pinned sqlite.org URL has moved). "
            "Please download the amalgamation manually from "
            "https://www.sqlite.org/download.html and place sqlite3.c + "
            "sqlite3.h into ${MOSAIC_SQLITE_DIR}/ (see "
            "third_party/sqlite/README.txt), then re-run CMake configure.")
    endif()
endif()

add_library(sqlite3 STATIC "${SQLITE_C_FILE}")
target_include_directories(sqlite3 PUBLIC "${SQLITE_INCLUDE_DIR}")
# SQLITE_DQS=0: disables treating double-quoted string literals as
# identifiers-or-strings, a decades-old MySQL-compatibility footgun with no
# reason to be on in a brand-new local app. SQLITE_THREADSAFE=1: Mosaic
# only ever touches the database from the UI thread today, but the safer
# default costs nothing measurable at this scale.
target_compile_definitions(sqlite3 PUBLIC SQLITE_DQS=0 SQLITE_THREADSAFE=1)
if(MSVC)
    # The amalgamation is C, generated code, and not ours to hold to /W4 —
    # silence its warnings specifically rather than the whole project's.
    target_compile_options(sqlite3 PRIVATE /W0)
endif()

# --- Mosaic itself ---------------------------------------------------------
add_executable(Mosaic WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/AppSettings.cpp
    src/platform/Window.cpp
    src/platform/TrayIcon.cpp
    src/ui/GraphicsDevice.cpp
    src/ui/Theme.cpp
    src/ui/Icons.cpp
    src/ui/DashboardView.cpp
    src/ui/SettingsPanel.cpp
    src/ui/components/GlassPanel.cpp
    src/ui/components/WidgetCard.cpp
    src/data/Database.cpp
    src/data/TodoRepository.cpp
    src/data/ActivityRepository.cpp
    src/data/PinnedRepository.cpp
    src/data/LayoutRepository.cpp
    src/data/SettingsRepository.cpp
    src/data/NotesRepository.cpp
    src/layout/LayoutEngine.cpp
    src/media/LocalFolderPhotoProvider.cpp
    src/media/ImagePipeline.cpp
    src/widgets/WidgetManager.cpp
    src/widgets/TodoWidget.cpp
    src/widgets/PhotoWidget.cpp
    src/widgets/ActivityWidget.cpp
    src/widgets/ActivityDateTime.cpp
    src/widgets/PinnedWidget.cpp
    src/widgets/QuickNotesWidget.cpp
    src/notifications/ToastNotifier.cpp
    src/security/NoteCrypto.cpp
    src/security/WindowsHello.cpp
)

target_include_directories(Mosaic PRIVATE src)

# Older Windows SDKs (pre-C++/WinRT 2.0) ship a version of the WinRT
# headers whose <winrt/base.h> still pulls in <experimental/coroutine>,
# which recent MSVC only compiles under the legacy /await flag. A current
# SDK doesn't need this at all — see README's "Dependencies" section — but
# scoping the flag to just this one WinRT-using file means the project
# still builds correctly on an older SDK without forcing everyone to
# update it first, and without affecting any other file's compile options.
if(MSVC)
    set_source_files_properties(
        src/notifications/ToastNotifier.cpp
        src/security/WindowsHello.cpp
        PROPERTIES COMPILE_OPTIONS "/await")
endif()

# Native Windows rendering/media/composition stack, plus SQLite for local
# persistence and the WinRT toast notification APIs. No browser runtime,
# no third-party UI framework.
target_link_libraries(Mosaic PRIVATE
    sqlite3
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
    shell32.lib
    ole32.lib
    runtimeobject.lib
    crypt32.lib
    bcrypt.lib
)

set_target_properties(Mosaic PROPERTIES
    OUTPUT_NAME "Mosaic"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
