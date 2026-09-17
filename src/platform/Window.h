#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <vector>
#include "ui/GraphicsDevice.h"
#include "ui/DashboardView.h"
#include "ui/Theme.h"
#include "data/Database.h"
#include "data/TodoRepository.h"
#include "data/ActivityRepository.h"
#include "data/PinnedRepository.h"
#include "data/LayoutRepository.h"
#include "data/SettingsRepository.h"
#include "data/NotesRepository.h"
#include "platform/TrayIcon.h"
#include "widgets/QuickNotesWidget.h"
#include "app/AppSettings.h"
#include "media/LocalFolderPhotoProvider.h"
#include "media/ImagePipeline.h"
#include "widgets/WidgetManager.h"
#include "widgets/PhotoWidget.h"

namespace mosaic::platform {

// Wraps a single top-level Win32 window configured the way Mosaic needs:
//   - WS_EX_NOREDIRECTIONBITMAP so DWM doesn't allocate a redirection
//     surface behind our own DirectComposition-presented swap chain
//   - DWMWA_SYSTEMBACKDROP_TYPE for a real, OS-native acrylic/mica blur of
//     the desktop behind the window (Windows 11 22H2+; falls back to a
//     solid translucent fill on older builds — see GraphicsDevice/Theme)
//   - per-monitor-v2 DPI awareness so the dashboard is crisp at any scale
//
// As of Phase 3, Window also owns the local database and the repository/
// widget-manager layer — the actual data/widget layer, not just rendering
// plumbing. This is still the right home for them: Window is the object
// that's guaranteed to exist for the whole process lifetime and to be
// destroyed in a well-defined order (repositories/database/media layer
// outlive the widgets that reference them, since they're declared first
// and C++ destroys members in reverse declaration order).
//
// As of Phase 4, Window's once-a-minute timer does double duty: it still
// refreshes the header's greeting/date, and now also asks ActivityRepository
// which reminders have come due and fires a real Windows toast for each —
// see CheckActivityReminders(). A minute of latency on a reminder is an
// acceptable trade for not running a second timer or any sub-minute polling.
//
// As of Phase 5/6, there's a second, deliberately short-lived timer shared
// by two unrelated animations: the photo crossfade (Phase 5) and the
// layout drag-drop settle animation (Phase 6). Running a ~30fps timer
// permanently would violate the near-0%-idle-CPU requirement, but both
// animations only need to run for a couple hundred milliseconds at a
// time. StartTransitionTimer/StopTransitionTimer turn it on the moment
// either PhotoWidget or DashboardView reports it's actually animating,
// and off again the instant *both* report they're done — see the
// WM_TIMER id==2 handler for the exact condition.
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
    void OnLButtonUp(int pixelX, int pixelY);
    void OnLButtonDblClk(int pixelX, int pixelY);
    void OnChar(wchar_t ch);
    void OnKeyDown(UINT virtualKey);
    // added the OnTrayCommand function to handle the tray icon commands
    void PinToDesktopWorkerW();


    // Enables the Windows 11 system backdrop (acrylic-style blur of
    // whatever sits behind the window on the desktop). No-op with a
    // logged fallback path on Windows versions that don't support it.
    void EnableAcrylicBackdrop();

    void UpdateHeaderData(); // refreshes greeting/date only — real widget data lives in the widgets themselves

    // Applies every AppSettings value that isn't pure theme: always-on-top
    // z-order, the DWM backdrop on/off, run-at-startup registry entry, and
    // the photo rotation interval. Theme values go through
    // app::ApplyThemeSettings instead. Called at startup and after every
    // Settings change.
    void ApplyNonThemeSettings();
    void ApplyStartWithWindows(bool enabled);
    void PickPhotoFolder(); // opens the Win32 folder browser, rescans the provider

    void OnTrayCommand(UINT commandId);
    void ShowDashboard();
    void HideDashboard();
    void CheckActivityReminders(); // fires toasts for any activity whose reminder time has passed

    void StartTransitionTimer();
    void StopTransitionTimer();

    D2D1_POINT_2F PixelToDip(int pixelX, int pixelY) const;

    HWND m_hwnd = nullptr;
    UINT m_dpi = 96;
    bool m_trackingMouseLeave = false;
    bool m_transitionTimerRunning = false;
    int m_minutesSinceLastPhoto = 0;
    // Rotation interval now comes from m_settings.photoRotationMinutes
    // (spec section 8's 5-minute default lives in AppSettings), changeable
    // live from Settings > Photo & Media.

    ui::ThemeManager m_theme = ui::ThemeManager::CreateDark();

    // Declaration order matters here: members are destroyed in reverse
    // order, so m_widgetManager (which holds raw pointers into everything
    // below it) must be declared — and therefore destroyed — before them.
    data::Database m_database;
    data::TodoRepository m_todoRepository{ &m_database };
    data::ActivityRepository m_activityRepository{ &m_database };
    data::PinnedRepository m_pinnedRepository{ &m_database };
    data::LayoutRepository m_layoutRepository{ &m_database };
    data::SettingsRepository m_settingsRepository{ &m_database };
    data::NotesRepository m_notesRepository{ &m_database };
    app::AppSettings m_settings;
    media::LocalFolderPhotoProvider m_photoProvider;
    media::ImagePipeline m_imagePipeline;
    // Constructed in Create() rather than here, because QuickNotesWidget's
    // Windows Hello callback needs m_hwnd, which doesn't exist until the
    // window is created. unique_ptr keeps the "declared before the things
    // that outlive it" ordering intact — it's still destroyed before every
    // repository it points into.
    std::unique_ptr<widgets::WidgetManager> m_widgetManager;

    // Non-owning — WidgetManager owns the actual PhotoWidget instance.
    // Photo is the one widget Window talks to directly (to drive rotation
    // and the transition timer); every other widget is only ever reached
    // generically through DashboardView/WidgetManager. A deliberate,
    // documented special case rather than a generalized per-widget
    // animation/scheduling system this single use doesn't justify yet.
    widgets::PhotoWidget* m_photoWidget = nullptr;

    // Also non-owning (WidgetManager owns it) — Window needs direct access
    // to drive auto-lock from the minute timer and to lock on focus loss.
    widgets::QuickNotesWidget* m_quickNotesWidget = nullptr;

    TrayIcon m_trayIcon;
    bool m_dashboardVisible = true;
    bool m_photosPaused = false;

    std::unique_ptr<ui::GraphicsDevice> m_graphics;
    std::unique_ptr<ui::DashboardView>  m_dashboardView;
    ui::DashboardHeaderData m_headerData;

    bool m_deviceResourcesValid = false;
};

} // namespace mosaic::platform
