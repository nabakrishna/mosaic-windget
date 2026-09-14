#pragma once
#include <windows.h>
#include <memory>
#include "platform/Window.h"

namespace mosaic::app {

// Owns process-wide setup (COM, DPI awareness) and the main window. The
// database/widget-manager/repository layer lives inside Window as of
// Phase 3 (see Window.h's comment on why) rather than here — Application
// stays the thin process-lifetime shell it was always meant to be.
class Application {
public:
    int Run(HINSTANCE hInstance, int nCmdShow);

private:
    std::unique_ptr<platform::Window> m_window;
};

} // namespace mosaic::app
