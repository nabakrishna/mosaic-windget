#pragma once
#include <windows.h>
#include <memory>
#include "platform/Window.h"

namespace mosaic::app {

// Owns process-wide setup (COM, DPI awareness) and the main window.
// Deliberately thin in Phase 1 — this is the seam where later phases plug
// in the database, widget manager, tray icon, and settings service without
// touching Window or main.cpp.
class Application {
public:
    int Run(HINSTANCE hInstance, int nCmdShow);

private:
    std::unique_ptr<platform::Window> m_window;
};

} // namespace mosaic::app
