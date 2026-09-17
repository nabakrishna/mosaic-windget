#include "platform/Window.h"
#include "notifications/ToastNotifier.h"
#include "widgets/ActivityDateTime.h"
#include "security/WindowsHello.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <ctime>
#include <string>
#include <sstream>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shcore.lib")

// DWMWA_SYSTEMBACKDROP_TYPE and DWM_SYSTEMBACKDROP_TYPE were added in the
// Windows 11 22H2 SDK. Guard them so this still compiles against slightly
// older Windows SDKs; the feature simply becomes a no-op there.
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
// typedef enum {
//     DWMSBT_AUTO = 0,
//     DWMSBT_NONE = 1,
//     DWMSBT_MAINWINDOW = 2,     // Mica
//     DWMSBT_TRANSIENTWINDOW = 3, // Acrylic
//     DWMSBT_TABBEDWINDOW = 4,
// } DWM_SYSTEMBACKDROP_TYPE;
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace mosaic::platform {

namespace {
constexpr wchar_t kWindowClassName[] = L"MosaicDashboardWindow";
constexpr int kDefaultWidthDip = 940;
constexpr int kDefaultHeightDip = 520;

//-------------------------new for pin to desktop workerw---------------------------------------
struct EnumWorkerWContext { HWND result = nullptr; };
BOOL CALLBACK FindWorkerW(HWND hwnd, LPARAM lParam) {
    HWND shellView = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
    if (shellView) {
        HWND* out = reinterpret_cast<HWND*>(lParam);
        *out = FindWindowExW(nullptr, hwnd, L"WorkerW", nullptr);
        return FALSE;
    }
    return TRUE;
}
//----------------------------------------------------------------------------------
} // namespace

//----------------------------------------- implemnt PinToDesktopWorkerW----------------------------
void Window::PinToDesktopWorkerW() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) return;
    
    SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
    
    HWND targetWorkerW = nullptr;
    EnumWindows(FindWorkerW, reinterpret_cast<LPARAM>(&targetWorkerW));
    
    if (!targetWorkerW) return;
    SetParent(m_hwnd, targetWorkerW);
}
//-----------------------------------------------------------------------------------

Window::~Window() {
    if (m_dashboardView) m_dashboardView->ReleaseDeviceResources();
    if (m_hwnd) DestroyWindow(m_hwnd);
}

LRESULT CALLBACK Window::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        OnPaint();
        ValidateRect(m_hwnd, nullptr); // we drew via D2D, not GDI BeginPaint/EndPaint
        return 0;

    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_DPICHANGED:
        OnDpiChanged(HIWORD(wParam), reinterpret_cast<RECT*>(lParam));
        return 0;

    case WM_TIMER:
        // id 1: once a minute is plenty for a greeting/date string and an
        // activity-reminder check — this is the event-driven philosophy
        // from spec section 36 applied literally: we redraw because
        // something *could* have changed, not on a tight render loop. It
        // also doubles as the photo rotation clock (see
        // m_settings.photoRotationMinutes) rather than running a third OS timer for
        // something that's naturally expressible in whole minutes.
        // id 2: the short-lived crossfade animation timer — see
        // StartTransitionTimer's comment in Window.h.
        if (wParam == 1) {
            UpdateHeaderData();
            CheckActivityReminders();

            if (m_quickNotesWidget && m_quickNotesWidget->TickAutoLock()) {
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }

            if (!m_photosPaused && ++m_minutesSinceLastPhoto >= m_settings.photoRotationMinutes) {
                m_minutesSinceLastPhoto = 0;
                if (m_photoWidget && m_graphics) {
                    m_photoWidget->RequestNextPhoto(m_graphics->DeviceContext());
                }
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        } else if (wParam == 2) {
            InvalidateRect(m_hwnd, nullptr, FALSE);
            bool photoDone = !m_photoWidget || !m_photoWidget->IsTransitioning();
            bool layoutDone = !m_dashboardView || !m_dashboardView->IsAnimating();
            if (photoDone && layoutDone) {
                StopTransitionTimer();
            }
        }
        return 0;

    case security::WM_MOSAIC_HELLO_RESULT:
        // The Windows Hello prompt finished on a background thread; this
        // is the UI thread picking up the result (see WindowsHello.h).
        security::WindowsHello::PumpResult();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_MOSAIC_TRAY:
        // Left-click toggles visibility, right-click opens the menu —
        // the conventions every Windows tray app follows.
        if (LOWORD(lParam) == WM_LBUTTONUP) {
            if (m_dashboardVisible) HideDashboard(); else ShowDashboard();
        } else if (LOWORD(lParam) == WM_RBUTTONUP) {
            m_trayIcon.ShowContextMenu(m_hwnd, m_dashboardVisible, m_photosPaused);
        }
        return 0;

    case WM_COMMAND:
        OnTrayCommand(LOWORD(wParam));
        return 0;

    case WM_KILLFOCUS:
        // Settings > Quick Notes > "Lock when window loses focus".
        if (m_settings.lockNotesOnFocusLoss && m_quickNotesWidget && m_quickNotesWidget->IsUnlocked()) {
            m_quickNotesWidget->Lock();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case media::WM_MOSAIC_PHOTO_READY:
        m_imagePipeline.PumpResult();
        StartTransitionTimer(); // the just-delivered photo may have started a crossfade
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSELEAVE:
        OnMouseLeave();
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONDBLCLK:
        OnLButtonDblClk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_CHAR:
        OnChar(static_cast<wchar_t>(wParam));
        return 0;

    case WM_KEYDOWN:
        OnKeyDown(static_cast<UINT>(wParam));
        return 0;

    case WM_ERASEBKGND:
        // Prevent GDI from painting the background — we own every pixel via
        // the D2D/DirectComposition swap chain.
        return 1;

    case WM_DESTROY:
        RECT rect;
        if (GetWindowRect(m_hwnd, &rect)) {
            m_settingsRepository.SetInt(L"WindowX", rect.left);
            m_settingsRepository.SetInt(L"WindowY", rect.top);
        }
        PostQuitMessage(0);
        return 0;

    //new code for the WM_DESTROY message to save the window position in the settings repository--------------------------------------
    // case WM_DESTROY: {
    //     // Save the exact window position before shutting down
    //     RECT rect;
    //     if (GetWindowRect(m_hwnd, &rect)) {
    //         m_settingsRepository.SetInt(L"WindowX", rect.left);
    //         m_settingsRepository.SetInt(L"WindowY", rect.top);
    //     }
    //     PostQuitMessage(0);
    //     return 0;
    // }
    //-----------------------------------------------------------------------------------------------------------------------------------

    //------------------- new ----------------------------------------------------
    case WM_WINDOWPOSCHANGING: {
        auto* wp = reinterpret_cast<WINDOWPOS*>(lParam);
        if (!(wp->flags & SWP_NOZORDER)) {
            wp->hwndInsertAfter = HWND_BOTTOM;
        }
        return 0;
    }
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        if (wParam != WA_INACTIVE) {
            SetWindowPos(m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    //-------------------------------------------------------------------------------
    default:
        return DefWindowProc(m_hwnd, msg, wParam, lParam);
    }
}

void Window::EnableAcrylicBackdrop() {
    // Real, native OS-level blur-behind. This is the "macOS-widget-style
    // glass" the design calls for, implemented the cheap way: ask DWM to do
    // it, rather than us sampling and blurring the desktop ourselves (which
    // would cost real CPU/GPU every frame and violate the low-resource
    // requirement). Requires Windows 11 22H2+; harmless no-op otherwise.
    //
    // Settings > Appearance > Background Blur toggles this: DWMSBT_NONE
    // turns the OS backdrop off entirely, leaving the dashboard's own
    // translucent card fills over a plain transparent window.
    DWM_SYSTEMBACKDROP_TYPE backdrop = m_settings.blurEnabled ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    // Extending the frame into the client area with negative margins tells
    // DWM the entire client area participates in glass composition.
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);
}

HRESULT Window::Create(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    // CS_DBLCLKS: without this, Windows never sends WM_LBUTTONDBLCLK — the
    // To Do widget's "double-click a row to rename" relies on it.
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProcThunk;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // we own painting entirely
    wc.lpszClassName = kWindowClassName;
    RegisterClassEx(&wc);

    m_dpi = GetDpiForSystem();
    int widthPx = MulDiv(kDefaultWidthDip, m_dpi, 96);
    int heightPx = MulDiv(kDefaultHeightDip, m_dpi, 96);

    // WS_EX_NOREDIRECTIONBITMAP: required so DWM does not allocate its own
    // redirection surface, which would sit *behind* our DirectComposition
    // visual and defeat the whole point of presenting through DComp.
    // WS_POPUP (no title bar/border) matches the borderless widget look;
    // window chrome (drag-to-move, resize handles) is added in a later
    // pass via custom hit-testing.

    //----------------------------------------------------
    // HWND hwnd = CreateWindowEx(
    //     WS_EX_NOREDIRECTIONBITMAP,
    //     kWindowClassName, L"Mosaic",
    //     WS_POPUP | WS_VISIBLE,
    //     CW_USEDEFAULT, CW_USEDEFAULT, widthPx, heightPx,
    //     nullptr, nullptr, hInstance, this);
    //----------------------------------------------------------    
    //new for the above block----------------------------------------------
    // int startX = m_settingsRepository.GetInt(L"WindowX", CW_USEDEFAULT);
    // int startY = m_settingsRepository.GetInt(L"WindowY", CW_USEDEFAULT);
    // HWND hwnd = CreateWindowEx(
    //     WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
    //     kWindowClassName, L"Mosaic",
    //     WS_POPUP | WS_VISIBLE,
    //     startX, startY, widthPx, heightPx,
    //     nullptr, nullptr, hInstance, this);
    // Default to coordinate (100, 100) if no saved position exists
    // Temporarily hardcode this to flush out the bad database values
    int startX = 100; // m_settingsRepository.GetInt(L"WindowX", 100);
    int startY = 100; // m_settingsRepository.GetInt(L"WindowY", 100);

    HWND hwnd = CreateWindowEx(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClassName, L"Mosaic",
        WS_POPUP | WS_VISIBLE,
        startX, startY, widthPx, heightPx,
        nullptr, nullptr, hInstance, this);
        
    if (!hwnd) return HRESULT_FROM_WIN32(GetLastError());
    m_hwnd = hwnd;

    // Apply baseline HWND_BOTTOM enforcement
    SetWindowPos(m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    //--------------------------------------------------------------------

    //new code for the createwindowex function to get the window position from the settings repository--------------------------------------
    // Load saved coordinates from the SQLite settings repository
    // int startX = m_settingsRepository.GetInt(L"WindowX", CW_USEDEFAULT);
    // int startY = m_settingsRepository.GetInt(L"WindowY", CW_USEDEFAULT);

    // HWND hwnd = CreateWindowEx(
    //     WS_EX_NOREDIRECTIONBITMAP,
    //     kWindowClassName, L"Mosaic",
    //     WS_POPUP | WS_VISIBLE,
    //     startX, startY, widthPx, heightPx,
    //     nullptr, nullptr, hInstance, this);
        //-----------------------------------------------------------------------------------------------------------------------------------
    if (!hwnd) return HRESULT_FROM_WIN32(GetLastError());
    m_hwnd = hwnd;

    // --- Data layer: open the database before anything tries to use it ---
    // A failed Open() (permissions, disk full, corrupt file) must not crash
    // the dashboard (spec section 60) — TodoRepository's methods already
    // fail safe (return empty/0 on error) if m_database's handle is null,
    // so we deliberately continue past a failed Open() rather than
    // aborting Create() entirely. The user gets a dashboard with a To Do
    // widget that just can't save anything this session, not a crash.
    HRESULT dbHr = m_database.Open();
    if (SUCCEEDED(dbHr)) {
        m_todoRepository.SeedDefaultsIfEmpty();
        m_activityRepository.SeedDefaultIfEmpty();
        m_pinnedRepository.SeedDefaultsIfEmpty();
    }

    // Settings must load before anything that reads them — the theme, the
    // backdrop, z-order, and which widgets the layout includes all depend
    // on these values. Falls back to AppSettings' own defaults for a fresh
    // install where nothing has been saved yet.
    m_settings.LoadFrom(m_settingsRepository);
    app::ApplyThemeSettings(m_settings, m_theme);
    EnableAcrylicBackdrop();

    // --- Photo source ---------------------------------------------------
    // Uses whatever folder Settings > Photo & Media last picked, falling
    // back to the user's Pictures folder if none has been chosen. A
    // missing or empty folder isn't an error — PhotoWidget shows "No
    // photos found" rather than anything crashing (spec section 60).
    std::wstring photoFolder = m_settingsRepository.GetString(L"photo.folder", L"");
    if (photoFolder.empty()) {
        PWSTR picturesPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &picturesPath))) {
            photoFolder = picturesPath;
            CoTaskMemFree(picturesPath);
        }
    }
    if (!photoFolder.empty()) {
        m_photoProvider.SetFolder(photoFolder);
    }
    m_imagePipeline.Start(m_hwnd);

    // Windows Hello runs asynchronously and posts its result back to this
    // window, so the widget can't call it directly — Window supplies this
    // adapter, which is also the single place that decides Hello is
    // unavailable and lets the widget fall back to a password.
    auto requestHello = [this](const std::wstring& message,
                                std::function<void(security::HelloResult)> onComplete) {
        security::WindowsHello::RequestVerification(m_hwnd, message, std::move(onComplete));
    };

    m_widgetManager = std::make_unique<widgets::WidgetManager>(
        &m_todoRepository, &m_activityRepository, &m_pinnedRepository, &m_notesRepository,
        &m_photoProvider, &m_imagePipeline, requestHello);
    m_widgetManager->Initialize();
    m_photoWidget = static_cast<widgets::PhotoWidget*>(m_widgetManager->Get(widgets::WidgetId::Photo));
    m_quickNotesWidget = static_cast<widgets::QuickNotesWidget*>(m_widgetManager->Get(widgets::WidgetId::QuickNotes));
    if (m_quickNotesWidget) {
        m_quickNotesWidget->SetAutoLockSeconds(m_settings.noteAutoLockSeconds);
    }

    m_trayIcon.Create(m_hwnd, L"Mosaic");

    m_graphics = std::make_unique<ui::GraphicsDevice>();
    HRESULT hr = m_graphics->Initialize(m_hwnd, static_cast<UINT>(widthPx), static_cast<UINT>(heightPx));
    if (FAILED(hr)) return hr;

    ui::SettingsCallbacks settingsCallbacks;
    settingsCallbacks.onSettingsChanged = [this] {
        // Persist immediately, then apply. There's no "Save" button by
        // design — every other control in Mosaic (To Do, Activity, widget
        // position) already commits on change, and Settings matching that
        // is less surprising than introducing a second convention.
        m_settings.SaveTo(m_settingsRepository);
        app::ApplyThemeSettings(m_settings, m_theme);
        ApplyNonThemeSettings();
        if (m_quickNotesWidget) m_quickNotesWidget->SetAutoLockSeconds(m_settings.noteAutoLockSeconds);
        if (m_dashboardView) m_dashboardView->RebuildLayout(); // widget enable/disable may have changed
        InvalidateRect(m_hwnd, nullptr, FALSE);
    };
    settingsCallbacks.onResetLayout = [this] {
        m_layoutRepository.ClearAll();
        if (m_dashboardView) m_dashboardView->RebuildLayout();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    };
    settingsCallbacks.onPickPhotoFolder = [this] { PickPhotoFolder(); };

    m_dashboardView = std::make_unique<ui::DashboardView>(
        m_graphics->DWriteFactory(), &m_theme, m_widgetManager.get(), &m_layoutRepository,
        &m_settings, std::move(settingsCallbacks));
    hr = m_dashboardView->CreateDeviceResources(m_graphics->DeviceContext());
    if (FAILED(hr)) return hr;
    m_deviceResourcesValid = true;

    if (m_photoWidget) {
        m_photoWidget->RequestNextPhoto(m_graphics->DeviceContext());
    }

    ApplyNonThemeSettings();

    UpdateHeaderData();
    CheckActivityReminders();
    SetTimer(hwnd, /*id*/ 1, 60000, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    //----------------------- foe call the PinToDesktopWorkerW func-------------------
    // PinToDesktopWorkerW();
    //---------------------------------
    return S_OK;
}

void Window::UpdateHeaderData() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    const wchar_t* greeting =
        (st.wHour < 12)  ? L"Good Morning," :
        (st.wHour < 17)  ? L"Good Afternoon," :
        (st.wHour < 21)  ? L"Good Evening," :
                            L"Good Night,";
    m_headerData.greetingLine = greeting;
    m_headerData.userName = L"Naba"; // becomes a stored profile setting in Phase 7
    m_headerData.motivation = L"Keep going, great things take time.";

    static const wchar_t* kWeekday[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    static const wchar_t* kMonth[] = {
        L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
    };
    m_headerData.weekday = kWeekday[st.wDayOfWeek];

    std::wstringstream dateStream;
    dateStream << st.wDay << L" " << kMonth[st.wMonth - 1] << L" " << st.wYear;
    m_headerData.fullDate = dateStream.str();
}

void Window::CheckActivityReminders() {
    int64_t now = static_cast<int64_t>(std::time(nullptr));
    auto due = m_activityRepository.GetDueForNotification(now);
    for (const auto& activity : due) {
        std::wstring when = widgets::activity_datetime::FormatForDisplay(activity.dueAt);
        notifications::ToastNotifier::Show(activity.title, when);
        m_activityRepository.MarkNotified(activity.id);
    }
}

void Window::ApplyNonThemeSettings() {
    // Always-on-top: a real z-order change, not a cosmetic flag.
    SetWindowPos(m_hwnd, m_settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    EnableAcrylicBackdrop(); // picks up m_settings.blurEnabled
    ApplyStartWithWindows(m_settings.startWithWindows);
}

void Window::ApplyStartWithWindows(bool enabled) {
    // The standard unprivileged run-at-login mechanism: a per-user value
    // under HKCU\...\Run. No admin rights, no scheduled task, no service —
    // and trivially inspectable/removable by the user, which matters for
    // something that modifies startup behavior.
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return; // registry unavailable — fail silently rather than crash (spec section 60)
    }

    if (enabled) {
        wchar_t exePath[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            // Quoted so a path containing spaces (very common under
            // C:\Users\First Last\...) parses as one argument.
            std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(key, L"Mosaic", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(quoted.c_str()),
                           static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
        }
    } else {
        RegDeleteValueW(key, L"Mosaic"); // absent value is fine; error ignored deliberately
    }
    RegCloseKey(key);
}

void Window::PickPhotoFolder() {
    // IFileDialog with FOS_PICKFOLDERS is the modern folder picker —
    // SHBrowseForFolder still works but looks like Windows XP, which would
    // undercut the whole point of the design.
    Microsoft::WRL::ComPtr<IFileDialog> dialog;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return;

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }

    if (FAILED(dialog->Show(m_hwnd))) return; // user cancelled — not an error

    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) return;

    PWSTR path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
        m_settingsRepository.SetString(L"photo.folder", path);
        m_photoProvider.SetFolder(path);
        CoTaskMemFree(path);

        // Show something from the new folder straight away rather than
        // waiting out the rest of the current rotation interval.
        m_minutesSinceLastPhoto = 0;
        if (m_photoWidget && m_graphics) {
            m_photoWidget->RequestNextPhoto(m_graphics->DeviceContext());
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::ShowDashboard() {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    m_dashboardVisible = true;
}

void Window::HideDashboard() {
    // Lock notes before hiding — leaving decrypted content in memory
    // behind a hidden window would quietly defeat the lock.
    if (m_quickNotesWidget && m_quickNotesWidget->IsUnlocked()) {
        m_quickNotesWidget->Lock();
    }
    ShowWindow(m_hwnd, SW_HIDE);
    m_dashboardVisible = false;
}

void Window::OnTrayCommand(UINT commandId) {
    switch (commandId) {
    case TrayCmd_Show:
        ShowDashboard();
        break;
    case TrayCmd_Hide:
        HideDashboard();
        break;
    case TrayCmd_Settings:
        ShowDashboard();
        if (m_dashboardView) {
            m_dashboardView->OpenSettings();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        break;
    case TrayCmd_PausePhotos:
        m_photosPaused = !m_photosPaused;
        break;
    case TrayCmd_Quit:
        // Lock (and therefore save) notes before tearing down, so a quit
        // from the tray doesn't silently lose an unsaved edit.
        if (m_quickNotesWidget) m_quickNotesWidget->Lock();
        m_trayIcon.Destroy();
        DestroyWindow(m_hwnd);
        break;
    default:
        break;
    }
}

void Window::StartTransitionTimer() {
    if (m_transitionTimerRunning) return;
    // Settings > Performance > Animations off means the timer never runs:
    // photo changes cut straight to the new image and dropped widgets
    // appear in their final slot immediately. Both code paths already
    // handle "no animation frames arrive" correctly — they just render
    // their end state — so this needs no special-casing elsewhere.
    if (!m_settings.animationsEnabled) return;
    bool photoAnimating = m_photoWidget && m_photoWidget->IsTransitioning();
    bool layoutAnimating = m_dashboardView && m_dashboardView->IsAnimating();
    if (!photoAnimating && !layoutAnimating) return;
    // ~30fps is plenty smooth for a simple opacity crossfade or rect
    // interpolation, and cheap enough to run for a couple hundred
    // milliseconds without it reading as "the app just started animating
    // things continuously" — it stops itself (see the WM_TIMER id==2
    // handler) the moment both report done.
    SetTimer(m_hwnd, /*id*/ 2, 33, nullptr);
    m_transitionTimerRunning = true;
}

void Window::StopTransitionTimer() {
    if (!m_transitionTimerRunning) return;
    KillTimer(m_hwnd, /*id*/ 2);
    m_transitionTimerRunning = false;
}

void Window::OnPaint() {
    if (!m_deviceResourcesValid) return;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float dipScale = 96.0f / static_cast<float>(m_dpi);
    D2D1_RECT_F bounds = {
        0.0f, 0.0f,
        static_cast<float>(rc.right - rc.left) * dipScale,
        static_cast<float>(rc.bottom - rc.top) * dipScale
    };

    m_graphics->DeviceContext()->SetDpi(static_cast<float>(m_dpi), static_cast<float>(m_dpi));
    m_graphics->BeginDraw();
    m_graphics->DeviceContext()->Clear(D2D1::ColorF(0, 0, 0, 0)); // fully transparent; DWM backdrop shows through
    m_dashboardView->Draw(m_graphics->DeviceContext(), bounds, m_headerData);
    HRESULT hr = m_graphics->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // Device-lost recovery: drop everything and rebuild. Rare in
        // practice (driver reset, remote-desktop reconnect) but must never
        // crash the dashboard (see spec section 60, error handling).
        m_dashboardView->ReleaseDeviceResources();
        m_deviceResourcesValid = false;

        // PhotoWidget's texture belongs to the device we're about to
        // destroy — every other widget's OnDeviceLost is a no-op today,
        // but calling it on all of them costs nothing and means a future
        // widget with its own GPU resource doesn't need this call site
        // updated to remember it exists.
        for (const auto& [id, widget] : m_widgetManager->Widgets()) {
            widget->OnDeviceLost();
        }

        RECT client;
        GetClientRect(m_hwnd, &client);
        m_graphics = std::make_unique<ui::GraphicsDevice>();
        if (SUCCEEDED(m_graphics->Initialize(m_hwnd, client.right - client.left, client.bottom - client.top)) &&
            SUCCEEDED(m_dashboardView->CreateDeviceResources(m_graphics->DeviceContext()))) {
            m_deviceResourcesValid = true;
            if (m_photoWidget) {
                m_photoWidget->RequestNextPhoto(m_graphics->DeviceContext());
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }
}

void Window::OnResize(UINT width, UINT height) {
    if (!m_graphics) return;
    m_graphics->Resize(width, height);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Window::OnDpiChanged(UINT newDpi, const RECT* suggestedRect) {
    m_dpi = newDpi;
    if (suggestedRect) {
        SetWindowPos(m_hwnd, nullptr,
                     suggestedRect->left, suggestedRect->top,
                     suggestedRect->right - suggestedRect->left,
                     suggestedRect->bottom - suggestedRect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

D2D1_POINT_2F Window::PixelToDip(int pixelX, int pixelY) const {
    float scale = 96.0f / static_cast<float>(m_dpi);
    return { static_cast<float>(pixelX) * scale, static_cast<float>(pixelY) * scale };
}

void Window::OnMouseMove(int pixelX, int pixelY) {
    if (!m_trackingMouseLeave) {
        // Ask Windows to send us exactly one WM_MOUSELEAVE when the cursor
        // exits the client area — the standard pattern for hover state,
        // since WM_MOUSEMOVE alone never fires once the pointer leaves.
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEvent(&tme);
        m_trackingMouseLeave = true;
    }

    if (!m_dashboardView) return;
    D2D1_POINT_2F dip = PixelToDip(pixelX, pixelY);
    if (m_dashboardView->OnMouseMove(dip)) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::OnMouseLeave() {
    m_trackingMouseLeave = false;
    if (!m_dashboardView) return;
    if (m_dashboardView->OnMouseLeave()) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::OnLButtonDown(int pixelX, int pixelY) {
    if (!m_dashboardView) return;
    // Windows sends focus to whatever window was clicked, but WS_POPUP
    // windows don't automatically take keyboard focus the way a normal
    // top-level window does — without this, WM_CHAR/WM_KEYDOWN never
    // arrive after clicking into the dashboard.
    SetFocus(m_hwnd);

    // Captures the mouse for the duration of a potential drag: without
    // this, moving the cursor fast enough during a drag can leave the
    // window's client area, which would stop delivering WM_MOUSEMOVE (and
    // worse, the eventual WM_LBUTTONUP) entirely. Released unconditionally
    // in OnLButtonUp, whether or not a drag actually happened.
    SetCapture(m_hwnd);

    D2D1_POINT_2F dip = PixelToDip(pixelX, pixelY);
    bool changed = m_dashboardView->OnLButtonDown(dip);

    if (changed) InvalidateRect(m_hwnd, nullptr, FALSE);
}

//new code for the OnLButtonDown function to allow dragging the window when clicking on the empty background of the dashboard--------------------------------------
// void Window::OnLButtonDown(int pixelX, int pixelY) {
//     if (!m_dashboardView) return;
//     SetFocus(m_hwnd);

//     D2D1_POINT_2F dip = PixelToDip(pixelX, pixelY);
//     bool changed = m_dashboardView->OnLButtonDown(dip);

//     // If DashboardView didn't flag a change, assume we clicked the empty 
//     // background and drag the whole OS window.
//     if (!changed) {
//         ReleaseCapture();
//         SendMessage(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
//     } else {
//         SetCapture(m_hwnd);
//         InvalidateRect(m_hwnd, nullptr, FALSE);
//     }
// }
//-----------------------------------------------------------------------------------------------------------------------------------

void Window::OnLButtonUp(int pixelX, int pixelY) {
    ReleaseCapture();
    if (!m_dashboardView) return;

    D2D1_POINT_2F dip = PixelToDip(pixelX, pixelY);
    bool changed = m_dashboardView->OnLButtonUp(dip);

    // A drop may have just started a settle animation (or a drag that
    // moved but didn't cross the threshold may have just resolved into an
    // ordinary click) — either way, make sure the shared animation timer
    // is running if DashboardView now needs it.
    StartTransitionTimer();

    if (changed) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Window::OnLButtonDblClk(int pixelX, int pixelY) {
    if (!m_dashboardView) return;
    D2D1_POINT_2F dip = PixelToDip(pixelX, pixelY);
    if (m_dashboardView->OnDoubleClick(dip)) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::OnChar(wchar_t ch) {
    if (!m_dashboardView) return;
    if (m_dashboardView->OnChar(ch)) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::OnKeyDown(UINT virtualKey) {
    if (!m_dashboardView) return;
    if (m_dashboardView->OnKeyDown(virtualKey)) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

int Window::RunMessageLoop() {
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

} // namespace mosaic::platform
