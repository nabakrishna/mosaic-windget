#pragma once
#include <windows.h>
#include <functional>
#include <string>

namespace mosaic::security {

// Wraps Windows Hello via WinRT's UserConsentVerifier — the real OS
// biometric/PIN prompt (fingerprint, face, or PIN, whichever the machine
// actually has enrolled). Spec section 14 is explicit: "Do NOT invent a
// fake fingerprint system. Use Windows-supported authentication
// mechanisms." So there is no simulated fingerprint reader anywhere in
// this codebase — either the OS prompt appears and the OS decides, or we
// report honestly that Hello isn't available and fall back to the
// password.
enum class HelloAvailability {
    Available,          // enrolled and ready
    NotConfigured,      // hardware present but nothing enrolled — user can set it up in Windows Settings
    NoHardware,         // no biometric device and no PIN configured
    Unknown,            // the check itself failed (old OS, API unavailable)
};

enum class HelloResult {
    Verified,
    Cancelled,          // user dismissed the prompt
    Failed,             // wrong biometric / too many retries
    Unavailable,        // couldn't even show the prompt — caller should fall back to password
};

class WindowsHello {
public:
    // Synchronous availability check. Safe to call on the UI thread; this
    // is a quick capability query, not a prompt.
    static HelloAvailability CheckAvailability();

    // Shows the real Windows Hello consent prompt and invokes `onComplete`
    // on the UI thread when the user responds.
    //
    // The underlying API is asynchronous (it must be — the prompt waits on
    // a human). Rather than blocking the UI thread on the async operation
    // (which would freeze the dashboard while the prompt is up, and can
    // deadlock an STA thread), this posts the result back as a window
    // message. `hwnd` must be the window whose message loop will deliver
    // it, and the caller must forward WM_MOSAIC_HELLO_RESULT to
    // PumpResult().
    static void RequestVerification(HWND hwnd, const std::wstring& message,
                                     std::function<void(HelloResult)> onComplete);

    // Call from the WM_MOSAIC_HELLO_RESULT handler; invokes the pending
    // callback from RequestVerification.
    static void PumpResult();
};

// Posted by WindowsHello when a verification completes. WM_APP+1 is
// already taken by the photo pipeline (see media/ImagePipeline.h), so
// this uses WM_APP+2.
constexpr UINT WM_MOSAIC_HELLO_RESULT = WM_APP + 2;

} // namespace mosaic::security
