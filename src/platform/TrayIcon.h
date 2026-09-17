#pragma once
#include <windows.h>
#include <functional>
#include <shellapi.h>

namespace mosaic::platform {

// Posted when the tray icon is clicked or its menu is used. WM_APP+1 and
// WM_APP+2 are taken (photo pipeline, Windows Hello), so this uses
// WM_APP+3.
constexpr UINT WM_MOSAIC_TRAY = WM_APP + 3;

// Menu command IDs, dispatched via WM_COMMAND.
enum TrayCommand : UINT {
    TrayCmd_Show = 5001,
    TrayCmd_Hide = 5002,
    TrayCmd_Settings = 5003,
    TrayCmd_PausePhotos = 5004,
    TrayCmd_Quit = 5005,
};

// A minimal Shell_NotifyIcon wrapper (spec section 43). Deliberately thin:
// no background thread, no polling, no separate process — the icon is just
// a registration against the owning window, and every interaction arrives
// as an ordinary message on the existing message loop, so it costs
// effectively nothing while idle.
class TrayIcon {
public:
    ~TrayIcon();

    bool Create(HWND owner, const wchar_t* tooltip);
    void Destroy();

    // Shows the context menu at the current cursor position. Commands
    // arrive as WM_COMMAND with one of the TrayCommand IDs above.
    // `photosPaused` controls the check mark on the pause item.
    void ShowContextMenu(HWND owner, bool dashboardVisible, bool photosPaused);

private:
    NOTIFYICONDATAW m_data{};
    bool m_created = false;
};

} // namespace mosaic::platform
