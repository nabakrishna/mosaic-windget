#pragma once
#include <string>

namespace mosaic::notifications {

// Real Windows Action Center toast notifications, using WinRT's
// Windows.UI.Notifications API (spec section 29's recommendation) rather
// than the legacy Shell_NotifyIcon balloon-tip API — the latter needs a
// permanent tray icon to exist (Phase 9's job) and renders as a
// deprecated-looking balloon on modern Windows, not a real Action Center
// toast.
//
// Mosaic is an unpackaged Win32 .exe, not an MSIX app, which is the
// scenario Microsoft's own guidance calls out as needing two extra steps
// beyond just calling the notification API: an explicit App User Model ID,
// and a registry entry so Windows attributes the toast to "Mosaic" rather
// than a generic or missing name. EnsureAppIdentity() does both once at
// startup; Show() is the only thing called after that.
class ToastNotifier {
public:
    // Call exactly once, early in Application::Run, before any window is
    // created. Cheap (a registry write only happens if the key doesn't
    // already have the right value) and safe to call on every launch.
    static void EnsureAppIdentity();

    // Shows a two-line toast (title + body). Swallows every failure
    // internally — notifications being disabled in Windows Settings, no
    // Action Center service, a malformed title with stray XML-special
    // characters, etc. must never crash or otherwise disrupt the
    // dashboard (spec section 60). There is no return value to check
    // because there is nothing a caller could usefully do differently.
    static void Show(const std::wstring& title, const std::wstring& body);

private:
    static const wchar_t* AppUserModelId();
};

} // namespace mosaic::notifications
