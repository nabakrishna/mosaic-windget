#include "platform/Window.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <ctime>
#include <sstream>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shcore.lib")

// DWMWA_SYSTEMBACKDROP_TYPE and DWM_SYSTEMBACKDROP_TYPE were added in the
// Windows 11 22H2 SDK. Guard them so this still compiles against slightly
// older Windows SDKs; the feature simply becomes a no-op there.
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
typedef enum {
    DWMSBT_AUTO = 0,
    DWMSBT_NONE = 1,
    DWMSBT_MAINWINDOW = 2,     // Mica
    DWMSBT_TRANSIENTWINDOW = 3, // Acrylic
    DWMSBT_TABBEDWINDOW = 4,
} DWM_SYSTEMBACKDROP_TYPE;
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace mosaic::platform {

namespace {
constexpr wchar_t kWindowClassName[] = L"MosaicDashboardWindow";
constexpr int kDefaultWidthDip = 940;
constexpr int kDefaultHeightDip = 520;
} // namespace

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
        // Once a minute is plenty for a greeting/date string — this is the
        // event-driven philosophy from spec section 36 applied literally:
        // we redraw because something *could* have changed, not on a tight
        // render loop. No network, no polling of anything else here.
        if (wParam == 1) {
            UpdateSampleData();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
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

    case WM_ERASEBKGND:
        // Prevent GDI from painting the background — we own every pixel via
        // the D2D/DirectComposition swap chain.
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

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
    DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_TRANSIENTWINDOW; // acrylic
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
    wc.style = CS_HREDRAW | CS_VREDRAW;
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
    // window chrome (drag-to-move, resize handles) is added in Phase 2's
    // design-system pass via custom hit-testing.
    HWND hwnd = CreateWindowEx(
        WS_EX_NOREDIRECTIONBITMAP,
        kWindowClassName, L"Mosaic",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, widthPx, heightPx,
        nullptr, nullptr, hInstance, this);

    if (!hwnd) return HRESULT_FROM_WIN32(GetLastError());
    m_hwnd = hwnd;

    EnableAcrylicBackdrop();

    m_graphics = std::make_unique<ui::GraphicsDevice>();
    HRESULT hr = m_graphics->Initialize(m_hwnd, static_cast<UINT>(widthPx), static_cast<UINT>(heightPx));
    if (FAILED(hr)) return hr;

    m_dashboardView = std::make_unique<ui::DashboardView>(m_graphics->DWriteFactory(), &m_theme);
    hr = m_dashboardView->CreateDeviceResources(m_graphics->DeviceContext());
    if (FAILED(hr)) return hr;
    m_deviceResourcesValid = true;

    UpdateSampleData();
    SetTimer(hwnd, /*id*/ 1, 60000, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return S_OK;
}

void Window::UpdateSampleData() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    const wchar_t* greeting =
        (st.wHour < 12)  ? L"Good Morning," :
        (st.wHour < 17)  ? L"Good Afternoon," :
        (st.wHour < 21)  ? L"Good Evening," :
                            L"Good Night,";
    m_sampleData.greetingLine = greeting;
    m_sampleData.userName = L"Naba"; // Phase 3 replaces this with the stored profile name
    m_sampleData.motivation = L"Keep going, great things take time.";

    static const wchar_t* kWeekday[] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
    static const wchar_t* kMonth[] = {
        L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
    };
    m_sampleData.weekday = kWeekday[st.wDayOfWeek];

    std::wstringstream dateStream;
    dateStream << st.wDay << L" " << kMonth[st.wMonth - 1] << L" " << st.wYear;
    m_sampleData.fullDate = dateStream.str();

    // Placeholder content lifted straight from the reference design; real
    // persistence arrives in Phase 3 (SQLite-backed TodoRepository etc.).
    m_sampleData.todos = {
        { L"Complete DSA revision", true },
        { L"Finish Jarvis project", true },
        { L"Read 20 pages of a book", false },
        { L"Workout (30 mins)", false },
        { L"Prepare for next week", false },
        { L"Plan trip (Chennai to Pondicherry)", false },
    };
    m_sampleData.activityTitle = L"Gym Session";
    m_sampleData.activityWhen = L"Tomorrow, 6:00 PM";
    m_sampleData.activityNote = L"Small steps every day lead to big results.";
    m_sampleData.pinnedItems = { L"Study Plan", L"Project Ideas" };
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
    m_dashboardView->Draw(m_graphics->DeviceContext(), bounds, m_sampleData);
    HRESULT hr = m_graphics->EndDraw();

    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // Device-lost recovery: drop everything and rebuild. Rare in
        // practice (driver reset, remote-desktop reconnect) but must never
        // crash the dashboard (see spec section 60, error handling).
        m_dashboardView->ReleaseDeviceResources();
        m_deviceResourcesValid = false;

        RECT client;
        GetClientRect(m_hwnd, &client);
        m_graphics = std::make_unique<ui::GraphicsDevice>();
        if (SUCCEEDED(m_graphics->Initialize(m_hwnd, client.right - client.left, client.bottom - client.top)) &&
            SUCCEEDED(m_dashboardView->CreateDeviceResources(m_graphics->DeviceContext()))) {
            m_deviceResourcesValid = true;
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
    if (m_dashboardView->UpdateHover(dip)) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::OnMouseLeave() {
    m_trackingMouseLeave = false;
    if (!m_dashboardView) return;
    // A point guaranteed to be outside every hoverable element's bounds.
    if (m_dashboardView->UpdateHover({ -10000.0f, -10000.0f })) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Window::OnLButtonDown(int pixelX, int pixelY) {
    if (!m_dashboardView) return;
    D2D1_POINT_2F dip = PixelToDip(pixelX, pixelY);
    m_dashboardView->UpdateHover(dip); // ensure hover state matches the click position
    if (m_dashboardView->IsPointerOverSettingsButton()) {
        // Phase 7 hooks the actual Settings panel here. For now this is a
        // deliberate no-op rather than a fabricated dialog — the button is
        // real and clickable, but there is nothing behind it yet.
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
