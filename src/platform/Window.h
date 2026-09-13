#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include "ui/GraphicsDevice.h"
#include "ui/DashboardView.h"
#include "ui/Theme.h"

namespace mosaic::platform {

// Wraps a single top-level Win32 window configured the way Mosaic needs:
//   - WS_EX_NOREDIRECTIONBITMAP so DWM doesn't allocate a redirection
//     surface behind our own DirectComposition-presented swap chain
//   - DWMWA_SYSTEMBACKDROP_TYPE for a real, OS-native acrylic/mica blur of
//     the desktop behind the window (Windows 11 22H2+; falls back to a
//     solid translucent fill on older builds — see GraphicsDevice/Theme)
//   - per-monitor-v2 DPI awareness so the dashboard is crisp at any scale
//
// This class owns the render loop for its window: it drives GraphicsDevice
// and DashboardView, but knows nothing about widgets, layout, or data — Draw()
// calls DashboardView::Draw() with placeholder data pending Phase 3.
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Creates the HWND, initializes graphics, and shows the window.
    HRESULT Create(HINSTANCE hInstance, int nCmdShow);

    // Standard Win32 message loop. Returns the WM_QUIT exit code.
    int RunMessageLoop();

    HWND Handle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnResize(UINT width, UINT height);
    void OnDpiChanged(UINT newDpi, const RECT* suggestedRect);
    void OnMouseMove(int pixelX, int pixelY);
    void OnMouseLeave();
    void OnLButtonDown(int pixelX, int pixelY);

    // Enables the Windows 11 system backdrop (acrylic-style blur of
    // whatever sits behind the window on the desktop). No-op with a
    // logged fallback path on Windows versions that don't support it.
    void EnableAcrylicBackdrop();

    void UpdateSampleData(); // Phase 1 placeholder: refreshes greeting/date only

    D2D1_POINT_2F PixelToDip(int pixelX, int pixelY) const;

    HWND m_hwnd = nullptr;
    UINT m_dpi = 96;
    bool m_trackingMouseLeave = false;

    ui::ThemeManager m_theme = ui::ThemeManager::CreateDark();
    std::unique_ptr<ui::GraphicsDevice> m_graphics;
    std::unique_ptr<ui::DashboardView>  m_dashboardView;
    ui::DashboardSampleData m_sampleData;

    bool m_deviceResourcesValid = false;
};

} // namespace mosaic::platform
