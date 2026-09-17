#pragma once
#include <string>
#include <functional>
#include "widgets/IWidget.h"
#include "data/NotesRepository.h"
#include "security/WindowsHello.h"

namespace mosaic::widgets {

// The real Quick Notes widget (Phase 8): a private scratchpad whose
// contents are DPAPI-encrypted at rest and hidden behind a lock.
//
// State machine:
//
//   Locked ──(click "Unlock")──> prompting for password / Hello
//     │                                    │
//     │                          (success) ▼
//     └──(auto-lock timeout)────────── Unlocked ──(type)──> edits, saved on lock/blur
//
// On first use there's no password set, so the widget offers "Set a
// password" instead of "Unlock" — there's nothing to protect yet and
// pretending otherwise would be theatre.
//
// Windows Hello is offered only when the OS reports it's actually
// available and enrolled (see security::WindowsHello). When it isn't, the
// widget says so plainly and uses the password. There is no simulated
// fingerprint anywhere — spec section 14 is explicit about that.
class QuickNotesWidget : public IWidget {
public:
    // `requestHello` is supplied by Window (which owns the HWND the async
    // Hello result is posted to). Null means Hello is unavailable in this
    // build/session and only the password path is offered.
    using HelloRequestFn = std::function<void(const std::wstring& message,
                                               std::function<void(security::HelloResult)>)>;

    QuickNotesWidget(data::NotesRepository* repository, HelloRequestFn requestHello);

    WidgetId Id() const override { return WidgetId::QuickNotes; }
    WidgetMetadata Metadata() const override {
        return WidgetMetadata{ WidgetId::QuickNotes, L"Quick Notes", { 2, 2 }, { 1, 1 }, { 2, 3 } };
    }

    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;
    bool OnMouseMove(D2D1_POINT_2F pointDip) override;
    bool OnMouseLeave() override;
    bool OnLButtonDown(D2D1_POINT_2F pointDip) override;
    bool OnChar(wchar_t ch) override;
    bool OnKeyDown(unsigned int virtualKey) override;

    // Saves any pending edit and clears plaintext from memory. Called by
    // the auto-lock timer, on app exit, and when Settings changes the
    // lock configuration.
    void Lock();

    bool IsUnlocked() const { return m_state == State::Unlocked; }

    // Seconds of inactivity before auto-lock; 0 disables it. Driven by
    // Settings > Quick Notes.
    void SetAutoLockSeconds(int seconds) { m_autoLockSeconds = seconds; }

    // Called once a minute by Window's existing timer. Returns true if the
    // widget locked itself and a repaint is needed.
    bool TickAutoLock();

private:
    enum class State {
        Locked,          // showing the lock prompt
        AwaitingHello,   // OS prompt is up; waiting on the async result
        EnteringPassword,
        SettingPassword, // first-run: choosing a password
        Unlocked,
    };

    void EnterUnlockedState(const std::wstring& noteText);
    void BeginHelloVerification();
    void SubmitPassword();

    void DrawLockedState(ID2D1DeviceContext* ctx, D2D1_RECT_F content, const WidgetRenderResources& res);
    void DrawPasswordEntry(ID2D1DeviceContext* ctx, D2D1_RECT_F content, const WidgetRenderResources& res);
    void DrawNoteEditor(ID2D1DeviceContext* ctx, D2D1_RECT_F content, const WidgetRenderResources& res);

    data::NotesRepository* m_repository;
    HelloRequestFn m_requestHello;

    State m_state = State::Locked;
    std::wstring m_noteText;       // plaintext — only populated while unlocked
    std::wstring m_passwordInput;  // plaintext — wiped immediately after use
    std::wstring m_statusMessage;  // "Incorrect password", "Hello unavailable", etc.
    bool m_noteDirty = false;

    bool m_helloAvailable = false;
    int m_autoLockSeconds = 300; // 5 minutes default
    int m_idleSeconds = 0;

    // Cached from the last Render for hit-testing, same pattern the other
    // widgets use.
    D2D1_RECT_F m_primaryButtonRect{};
    D2D1_RECT_F m_helloButtonRect{};
    bool m_primaryButtonHovered = false;
    bool m_helloButtonHovered = false;
};

} // namespace mosaic::widgets
