#include "app/Application.h"
#include "notifications/ToastNotifier.h"
#include <combaseapi.h>

namespace mosaic::app {

int Application::Run(HINSTANCE hInstance, int nCmdShow) {
    // Per-Monitor-V2 DPI awareness must be set before any window is
    // created. This is what lets Window::OnDpiChanged receive real
    // suggested-rect data when the dashboard moves between monitors with
    // different scale factors (spec section 63).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // COM is needed for DirectComposition, WIC (Phase 5), and the WinRT
    // toast notification APIs (Phase 4) and later Windows Hello (Phase 8).
    // Single-threaded apartment is correct here: all UI and COM/WinRT calls
    // happen on this one thread.
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    // Registers Mosaic's App User Model ID so Windows attributes toast
    // notifications to "Mosaic" instead of a blank/generic sender — see
    // ToastNotifier.h for why an unpackaged .exe needs this explicit step.
    // Must happen before any ToastNotifier::Show call, and costs nothing
    // to call unconditionally on every launch.
    notifications::ToastNotifier::EnsureAppIdentity();

    m_window = std::make_unique<platform::Window>();
    hr = m_window->Create(hInstance, nCmdShow);
    if (FAILED(hr)) {
        CoUninitialize();
        return -1;
    }

    int exitCode = m_window->RunMessageLoop();

    m_window.reset();
    CoUninitialize();
    return exitCode;
}

} // namespace mosaic::app
