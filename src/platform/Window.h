#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include "ui/GraphicsDevice.h"
#include "ui/DashboardView.h"
#include "ui/Theme.h"
#include "data/Database.h"
#include "data/TodoRepository.h"
#include "widgets/WidgetManager.h"

namespace mosaic::platform {

// Wraps a single top-level Win32 window configured the way Mosaic needs:
//   - WS_EX_NOREDIRECTIONBITMAP so DWM doesn't allocate a redirection
//     surface behind our own DirectComposition-presented swap chain
//   - DWMWA_SYSTEMBACKDROP_TYPE for a real, OS-native acrylic/mica blur of
//     the desktop behind the window (Windows 11 22H2+; falls back to a
//     solid translucent fill on older builds — see GraphicsDevice/Theme)
//   - per-monitor-v2 DPI awareness so the dashboard is crisp at any scale
//
// As of Phase 3, Window also owns the local database, the TodoRepository,
// and the WidgetManager — the actual data/widget layer, not just rendering
// plumbing. This is still the right home for them: Window is the object
// that's guaranteed to exist for the whole process lifetime and to be
// destroyed in a well-defined order (repository/database outlive the
// widgets that reference them, since they're declared first and C++
// destroys members in reverse declaration order).
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Creates the HWND, opens the database, initializes graphics/widgets, and shows the window.
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
    void OnLButtonDblClk(int pixelX, int pixelY);
    void OnChar(wchar_t ch);
    void OnKeyDown(UINT virtualKey);

    // Enables the Windows 11 system backdrop (acrylic-style blur of
    // whatever sits behind the window on the desktop). No-op with a
    // logged fallback path on Windows versions that don't support it.
    void EnableAcrylicBackdrop();

    void UpdateHeaderData(); // refreshes greeting/date only — real widget data lives in the widgets themselves

    D2D1_POINT_2F PixelToDip(int pixelX, int pixelY) const;

    HWND m_hwnd = nullptr;
    UINT m_dpi = 96;
    bool m_trackingMouseLeave = false;

    ui::ThemeManager m_theme = ui::ThemeManager::CreateDark();

    // Declaration order matters here: members are destroyed in reverse
    // order, so m_widgetManager (which holds a raw pointer into
    // m_todoRepository, which holds one into m_database) must be declared
    // — and therefore destroyed — before them.
    data::Database m_database;
    data::TodoRepository m_todoRepository{ &m_database };
    widgets::WidgetManager m_widgetManager{ &m_todoRepository };

    std::unique_ptr<ui::GraphicsDevice> m_graphics;
    std::unique_ptr<ui::DashboardView>  m_dashboardView;
    ui::DashboardHeaderData m_headerData;

    bool m_deviceResourcesValid = false;
};

} // namespace mosaic::platform
