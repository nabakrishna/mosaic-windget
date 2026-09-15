#include "notifications/ToastNotifier.h"

// <unknwn.h> must be included before any C++/WinRT header — a well-known
// requirement to avoid IUnknown redefinition conflicts when a project
// isn't using the /await-style Visual-Studio-generated precompiled setup.
#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

#include <windows.h>
#include <shobjidl.h> // SetCurrentProcessExplicitAppUserModelID

namespace mosaic::notifications {

namespace {

// Escapes the handful of characters that are meaningful inside XML element
// text. Activity titles are free-typed user text (see ActivityWidget) and
// could easily contain '&' or '<' — an unescaped one would make the toast
// XML fail to parse, which Show() would otherwise swallow silently and the
// user would just never see their reminder. Escaping properly means a
// title like "Gym & Swim" shows up exactly as typed instead of either
// breaking the toast or silently mangling the text.
std::wstring EscapeXml(const std::wstring& text) {
    std::wstring out;
    out.reserve(text.size());
    for (wchar_t c : text) {
        switch (c) {
        case L'&':  out += L"&amp;"; break;
        case L'<':  out += L"&lt;"; break;
        case L'>':  out += L"&gt;"; break;
        case L'"':  out += L"&quot;"; break;
        case L'\'': out += L"&apos;"; break;
        default:    out += c; break;
        }
    }
    return out;
}

} // namespace

const wchar_t* ToastNotifier::AppUserModelId() {
    return L"Mosaic.Dashboard.App";
}

void ToastNotifier::EnsureAppIdentity() {
    SetCurrentProcessExplicitAppUserModelID(AppUserModelId());

    // HKCU\Software\Classes\AppUserModelId\<AUMID>\DisplayName is what
    // makes an unpackaged Win32 app's toast show "Mosaic" as the sender
    // instead of a generic or blank name — this is Microsoft's documented
    // workaround for apps without an MSIX identity. Failure here (e.g. no
    // registry write permission in some locked-down environment) is not
    // fatal: the toast can still fire, it'll just show a less friendly
    // name, so we don't check/report the result any further than logging
    // would in a debug build.
    std::wstring keyPath = std::wstring(L"Software\\Classes\\AppUserModelId\\") + AppUserModelId();
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, 0,
                         KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        const wchar_t* displayName = L"Mosaic";
        RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(displayName),
                        static_cast<DWORD>((wcslen(displayName) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void ToastNotifier::Show(const std::wstring& title, const std::wstring& body) {
    try {
        using namespace winrt::Windows::Data::Xml::Dom;
        using namespace winrt::Windows::UI::Notifications;

        // The smallest valid ToastGeneric payload: two text lines. Built
        // as a string and parsed, rather than the older enum-based
        // ToastTemplateType helpers, which is the currently-recommended
        // approach for anything beyond the most basic toasts.
        std::wstring xml =
            L"<toast><visual><binding template=\"ToastGeneric\">"
            L"<text>" + EscapeXml(title) + L"</text>"
            L"<text>" + EscapeXml(body) + L"</text>"
            L"</binding></visual></toast>";

        XmlDocument doc;
        doc.LoadXml(xml);

        ToastNotification notification{ doc };
        ToastNotificationManager::CreateToastNotifier(AppUserModelId()).Show(notification);
    } catch (...) {
        // See header comment: every failure mode here is something the
        // dashboard should simply continue past, not propagate.
    }
}

} // namespace mosaic::notifications
