#include "platform/TrayIcon.h"

#pragma comment(lib, "shell32.lib")

namespace mosaic::platform {

TrayIcon::~TrayIcon() {
    Destroy();
}

bool TrayIcon::Create(HWND owner, const wchar_t* tooltip) {
    if (m_created) return true;

    m_data = {};
    m_data.cbSize = sizeof(NOTIFYICONDATAW);
    m_data.hWnd = owner;
    m_data.uID = 1;
    m_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_data.uCallbackMessage = WM_MOSAIC_TRAY;

    // No custom .ico is shipped with the project, so this uses the
    // standard application icon rather than referencing a resource that
    // doesn't exist. Dropping a real icon into resources/icons/ and
    // loading it here is a one-line change later.
    m_data.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    wcsncpy_s(m_data.szTip, tooltip, _TRUNCATE);

    m_created = Shell_NotifyIconW(NIM_ADD, &m_data) != FALSE;
    return m_created;
}

void TrayIcon::Destroy() {
    if (!m_created) return;
    Shell_NotifyIconW(NIM_DELETE, &m_data);
    m_created = false;
}

void TrayIcon::ShowContextMenu(HWND owner, bool dashboardVisible, bool photosPaused) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    // Show/Hide is one item that reflects current state rather than two
    // items where one is always meaningless.
    if (dashboardVisible) {
        AppendMenuW(menu, MF_STRING, TrayCmd_Hide, L"Hide Dashboard");
    } else {
        AppendMenuW(menu, MF_STRING, TrayCmd_Show, L"Show Dashboard");
    }
    AppendMenuW(menu, MF_STRING, TrayCmd_Settings, L"Settings\u2026");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (photosPaused ? MF_CHECKED : MF_UNCHECKED),
                TrayCmd_PausePhotos, L"Pause Photo Rotation");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, TrayCmd_Quit, L"Quit Mosaic");

    POINT cursor{};
    GetCursorPos(&cursor);

    // SetForegroundWindow before TrackPopupMenu is the documented
    // workaround for a long-standing Win32 quirk: without it, the menu
    // doesn't dismiss when you click elsewhere and can linger on screen.
    SetForegroundWindow(owner);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, owner, nullptr);
    PostMessage(owner, WM_NULL, 0, 0); // second half of the same workaround

    DestroyMenu(menu);
}

} // namespace mosaic::platform
