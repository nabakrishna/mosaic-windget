#include "security/WindowsHello.h"

// <unknwn.h> before any C++/WinRT header — same requirement as
// ToastNotifier.cpp, for the same IUnknown-redefinition reason.
#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Credentials.UI.h>
#include <future>
#include <mutex>
#include <optional>

namespace mosaic::security {

namespace {

// The pending callback and its result, handed from the WinRT completion
// handler (which runs on an arbitrary thread pool thread) to the UI thread
// via PostMessage + PumpResult. Guarded because those are genuinely
// different threads.
std::mutex g_mutex;
std::function<void(HelloResult)> g_pendingCallback;
std::optional<HelloResult> g_pendingResult;

HelloResult TranslateResult(winrt::Windows::Security::Credentials::UI::UserConsentVerificationResult result) {
    using namespace winrt::Windows::Security::Credentials::UI;
    switch (result) {
    case UserConsentVerificationResult::Verified:
        return HelloResult::Verified;
    case UserConsentVerificationResult::Canceled:
        return HelloResult::Cancelled;
    case UserConsentVerificationResult::DeviceNotPresent:
    case UserConsentVerificationResult::NotConfiguredForUser:
    case UserConsentVerificationResult::DisabledByPolicy:
    case UserConsentVerificationResult::DeviceBusy:
        // All of these mean "we can't authenticate this way right now" —
        // the caller should fall back to the password rather than treat
        // it as a failed attempt.
        return HelloResult::Unavailable;
    case UserConsentVerificationResult::RetriesExhausted:
    default:
        return HelloResult::Failed;
    }
}

} // namespace

// HelloAvailability WindowsHello::CheckAvailability() {
//     using namespace winrt::Windows::Security::Credentials::UI;
//     try {
//         // CheckAvailabilityAsync is async but resolves essentially
//         // immediately (it's a capability query, not a prompt), so a
//         // blocking get() here is safe and keeps call sites simple —
//         // unlike RequestVerification, which genuinely waits on a human
//         // and must not block.
//         auto availability = UserConsentVerifier::CheckAvailabilityAsync().get();
//         switch (availability) {
//         case UserConsentVerifierAvailability::Available:
//             return HelloAvailability::Available;
//         case UserConsentVerifierAvailability::NotConfiguredForUser:
//             return HelloAvailability::NotConfigured;
//         case UserConsentVerifierAvailability::DeviceNotPresent:
//         case UserConsentVerifierAvailability::DisabledByPolicy:
//             return HelloAvailability::NoHardware;
//         default:
//             return HelloAvailability::Unknown;
//         }
//     } catch (...) {
//         // Older Windows, missing API, or a WinRT activation failure. Not
//         // an error worth surfacing — the caller just uses the password.
//         return HelloAvailability::Unknown;
//     }
// }
HelloAvailability WindowsHello::CheckAvailability() {
    using namespace winrt::Windows::Security::Credentials::UI;
    try {
        std::promise<UserConsentVerifierAvailability> promise;
        auto future = promise.get_future();

        auto operation = UserConsentVerifier::CheckAvailabilityAsync();
        
        // Let the WinRT thread pool resolve the promise in the background
        operation.Completed([&promise](auto const& async, winrt::Windows::Foundation::AsyncStatus status) {
            if (status == winrt::Windows::Foundation::AsyncStatus::Completed) {
                try {
                    promise.set_value(async.GetResults());
                } catch (...) {
                    promise.set_value(UserConsentVerifierAvailability::DeviceNotPresent);
                }
            } else {
                promise.set_value(UserConsentVerifierAvailability::DeviceNotPresent);
            }
        });

        // std::future::get() blocks until the background thread finishes, 
        // completely avoiding the WinRT STA debug assertion trap.
        auto availability = future.get();

        switch (availability) {
        case UserConsentVerifierAvailability::Available:
            return HelloAvailability::Available;
        case UserConsentVerifierAvailability::NotConfiguredForUser:
            return HelloAvailability::NotConfigured;
        case UserConsentVerifierAvailability::DeviceNotPresent:
        case UserConsentVerifierAvailability::DisabledByPolicy:
            return HelloAvailability::NoHardware;
        default:
            return HelloAvailability::Unknown;
        }
    } catch (...) {
        return HelloAvailability::Unknown;
    }
}








void WindowsHello::RequestVerification(HWND hwnd, const std::wstring& message,
                                        std::function<void(HelloResult)> onComplete) {
    using namespace winrt::Windows::Security::Credentials::UI;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pendingCallback = std::move(onComplete);
        g_pendingResult.reset();
    }

    try {
        auto operation = UserConsentVerifier::RequestVerificationAsync(winrt::hstring(message));

        // Completion runs on a thread pool thread — it must not touch UI
        // state directly, so it only stores the result and posts a message
        // for the UI thread to pick up in PumpResult.
        operation.Completed([hwnd](auto const& async, winrt::Windows::Foundation::AsyncStatus status) {
            HelloResult result = HelloResult::Failed;
            if (status == winrt::Windows::Foundation::AsyncStatus::Completed) {
                try {
                    result = TranslateResult(async.GetResults());
                } catch (...) {
                    result = HelloResult::Unavailable;
                }
            } else {
                result = HelloResult::Unavailable;
            }

            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_pendingResult = result;
            }
            PostMessage(hwnd, WM_MOSAIC_HELLO_RESULT, 0, 0);
        });
    } catch (...) {
        // Couldn't even start the prompt — report Unavailable through the
        // same path so the caller's fallback logic lives in one place.
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_pendingResult = HelloResult::Unavailable;
        }
        PostMessage(hwnd, WM_MOSAIC_HELLO_RESULT, 0, 0);
    }
}

void WindowsHello::PumpResult() {
    std::function<void(HelloResult)> callback;
    std::optional<HelloResult> result;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        callback = std::move(g_pendingCallback);
        result = g_pendingResult;
        g_pendingCallback = nullptr;
        g_pendingResult.reset();
    }
    if (callback && result) {
        callback(*result);
    }
}

} // namespace mosaic::security
