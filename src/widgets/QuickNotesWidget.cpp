#include "widgets/QuickNotesWidget.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"
#include "security/NoteCrypto.h"
#include <windows.h>
#include <algorithm>

using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

namespace {
constexpr float kButtonHeight = 32.0f;
constexpr float kLineHeight = 18.0f;

void DrawTextLine(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* fmt,
                   const std::wstring& text, D2D1_RECT_F rect, D2D1_COLOR_F color) {
    if (text.empty() || !fmt) return;
    brush->SetColor(color);
    ctx->DrawText(text.c_str(), static_cast<UINT32>(text.size()), fmt, rect, brush,
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

// Draws a rounded button and returns its rect for hit-test caching.
D2D1_RECT_F DrawButton(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* fmt,
                        D2D1_RECT_F rect, const std::wstring& label, bool hovered,
                        const ThemeManager& theme, bool accent) {
    const ThemeColors& colors = theme.Colors();
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, 10.0f, 10.0f);

    D2D1_COLOR_F fill = accent ? colors.accentViolet : colors.cardFill;
    if (hovered) fill.a = (std::min)(1.0f, fill.a + 0.15f);
    brush->SetColor(fill);
    ctx->FillRoundedRectangle(rr, brush);
    brush->SetColor(colors.cardBorder);
    ctx->DrawRoundedRectangle(rr, brush, 1.0f);

    DrawTextLine(ctx, brush, fmt, label, rect, colors.textPrimary);
    return rect;
}

bool PointInRect(D2D1_POINT_2F pt, D2D1_RECT_F r) {
    return pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom;
}
} // namespace

QuickNotesWidget::QuickNotesWidget(data::NotesRepository* repository, HelloRequestFn requestHello)
    : m_repository(repository), m_requestHello(std::move(requestHello)) {
    m_helloAvailable = (security::WindowsHello::CheckAvailability() == security::HelloAvailability::Available);
}

// --- state transitions ------------------------------------------------

void QuickNotesWidget::EnterUnlockedState(const std::wstring& noteText) {
    m_state = State::Unlocked;
    m_noteText = noteText;
    m_noteDirty = false;
    m_idleSeconds = 0;
    m_statusMessage.clear();
    security::NoteCrypto::SecureWipe(m_passwordInput);
}

void QuickNotesWidget::Lock() {
    if (m_state == State::Unlocked && m_noteDirty && m_repository) {
        m_repository->SaveNote(m_noteText);
    }
    // Clear plaintext from memory on lock — the whole point of locking is
    // that the content isn't sitting around readable afterwards.
    security::NoteCrypto::SecureWipe(m_noteText);
    security::NoteCrypto::SecureWipe(m_passwordInput);
    m_noteDirty = false;
    m_state = State::Locked;
    m_statusMessage.clear();
}

bool QuickNotesWidget::TickAutoLock() {
    if (m_state != State::Unlocked) return false;
    if (m_autoLockSeconds <= 0) return false; // "Never"

    m_idleSeconds += 60; // called once a minute by Window's existing timer
    if (m_idleSeconds >= m_autoLockSeconds) {
        Lock();
        return true;
    }
    return false;
}

void QuickNotesWidget::BeginHelloVerification() {
    if (!m_requestHello) {
        m_statusMessage = L"Windows Hello isn't available.";
        return;
    }
    m_state = State::AwaitingHello;
    m_statusMessage = L"Waiting for Windows Hello\u2026";

    m_requestHello(L"Unlock your Quick Notes", [this](security::HelloResult result) {
        switch (result) {
        case security::HelloResult::Verified: {
            std::wstring text;
            if (m_repository) m_repository->LoadNote(text);
            EnterUnlockedState(text);
            break;
        }
        case security::HelloResult::Cancelled:
            m_state = State::Locked;
            m_statusMessage.clear();
            break;
        case security::HelloResult::Failed:
            m_state = State::Locked;
            m_statusMessage = L"Windows Hello didn't recognize you.";
            break;
        case security::HelloResult::Unavailable:
        default:
            // Fall back to the password rather than dead-ending.
            m_helloAvailable = false;
            m_state = m_repository && m_repository->HasPassword() ? State::EnteringPassword : State::SettingPassword;
            m_statusMessage = L"Hello unavailable \u2014 use your password.";
            break;
        }
    });
}

void QuickNotesWidget::SubmitPassword() {
    if (!m_repository) return;

    if (m_state == State::SettingPassword) {
        if (m_passwordInput.size() < 4) {
            m_statusMessage = L"Use at least 4 characters.";
            return;
        }
        if (m_repository->SetPassword(m_passwordInput)) {
            std::wstring text;
            m_repository->LoadNote(text); // may be empty on first run — that's fine
            EnterUnlockedState(text);
        } else {
            m_statusMessage = L"Couldn't save that password.";
        }
        return;
    }

    if (m_repository->VerifyPassword(m_passwordInput)) {
        std::wstring text;
        m_repository->LoadNote(text);
        EnterUnlockedState(text);
    } else {
        m_statusMessage = L"Incorrect password.";
        security::NoteCrypto::SecureWipe(m_passwordInput);
    }
}

// --- rendering -------------------------------------------------------

void QuickNotesWidget::DrawLockedState(ID2D1DeviceContext* ctx, D2D1_RECT_F content, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();
    float y = content.top;

    D2D1_RECT_F iconRect = { content.left, y, content.left + 18.0f, y + 18.0f };
    icons::Draw(ctx, res.brush, icons::IconKind::Lock, iconRect, colors.textSecondary);

    bool firstRun = m_repository && !m_repository->HasPassword();
    std::wstring headline = firstRun ? L"Set a password to start" : L"Your notes are locked";
    DrawTextLine(ctx, res.brush, res.bodyFormat, headline,
                 { content.left + 26.0f, y, content.right, y + 20.0f }, colors.textPrimary);
    y += 30.0f;

    if (!m_statusMessage.empty()) {
        DrawTextLine(ctx, res.brush, res.smallFormat, m_statusMessage,
                     { content.left, y, content.right, y + 18.0f }, colors.textMuted);
        y += 22.0f;
    }

    std::wstring primaryLabel = firstRun ? L"Set Password" : L"Enter Password";
    m_primaryButtonRect = DrawButton(ctx, res.brush, res.bodyFormat,
                                      { content.left, y, content.left + 150.0f, y + kButtonHeight },
                                      primaryLabel, m_primaryButtonHovered, *res.theme, true);
    y += kButtonHeight + 10.0f;

    // Only offer Hello when the OS says it's genuinely available and a
    // password already exists (Hello unlocks; it doesn't set the initial
    // password, since we still need a password fallback to exist).
    if (m_helloAvailable && !firstRun) {
        m_helloButtonRect = DrawButton(ctx, res.brush, res.bodyFormat,
                                        { content.left, y, content.left + 180.0f, y + kButtonHeight },
                                        L"Use Windows Hello", m_helloButtonHovered, *res.theme, false);
    } else {
        m_helloButtonRect = {};
        if (!firstRun) {
            DrawTextLine(ctx, res.brush, res.smallFormat, L"Windows Hello isn't set up on this PC.",
                         { content.left, y + 6.0f, content.right, y + 24.0f }, colors.textMuted);
        }
    }
}

void QuickNotesWidget::DrawPasswordEntry(ID2D1DeviceContext* ctx, D2D1_RECT_F content, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();
    float y = content.top;

    std::wstring prompt = (m_state == State::SettingPassword) ? L"Choose a password" : L"Enter your password";
    DrawTextLine(ctx, res.brush, res.bodyFormat, prompt,
                 { content.left, y, content.right, y + 20.0f }, colors.textPrimary);
    y += 26.0f;

    // Masked field. The actual characters never appear on screen.
    D2D1_RECT_F fieldRect = { content.left, y, content.right, y + 30.0f };
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(fieldRect, 8.0f, 8.0f);
    res.brush->SetColor(colors.cardFill);
    ctx->FillRoundedRectangle(rr, res.brush);
    res.brush->SetColor(colors.accentViolet);
    ctx->DrawRoundedRectangle(rr, res.brush, 1.5f);

    std::wstring masked(m_passwordInput.size(), L'\u2022');
    masked += L"|"; // caret
    DrawTextLine(ctx, res.brush, res.bodyFormat, masked,
                 { fieldRect.left + 10.0f, fieldRect.top + 5.0f, fieldRect.right - 10.0f, fieldRect.bottom },
                 colors.textPrimary);
    y += 38.0f;

    if (!m_statusMessage.empty()) {
        DrawTextLine(ctx, res.brush, res.smallFormat, m_statusMessage,
                     { content.left, y, content.right, y + 18.0f }, colors.accentAmber);
        y += 22.0f;
    }

    DrawTextLine(ctx, res.brush, res.smallFormat, L"Enter to confirm \u00b7 Esc to cancel",
                 { content.left, y, content.right, y + 18.0f }, colors.textMuted);
    m_primaryButtonRect = {};
    m_helloButtonRect = {};
}

void QuickNotesWidget::DrawNoteEditor(ID2D1DeviceContext* ctx, D2D1_RECT_F content, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();

    if (m_noteText.empty()) {
        DrawTextLine(ctx, res.brush, res.bodyFormat, L"Write something\u2026",
                     { content.left, content.top, content.right, content.top + 20.0f }, colors.textMuted);
    } else {
        // Simple greedy wrap at the card width. DirectWrite could lay this
        // out properly with word wrapping enabled on the format, but the
        // shared bodyFormat is configured NO_WRAP for every other widget's
        // single-line use — so this wraps manually rather than mutating
        // shared state other widgets depend on.
        float y = content.top;
        size_t pos = 0;
        const size_t approxCharsPerLine = static_cast<size_t>(
            (std::max)(10.0f, (content.right - content.left) / 7.0f));

        while (pos < m_noteText.size() && y + kLineHeight <= content.bottom - 22.0f) {
            size_t take = (std::min)(approxCharsPerLine, m_noteText.size() - pos);

            // Prefer breaking at an explicit newline, then at whitespace.
            size_t newline = m_noteText.find(L'\n', pos);
            if (newline != std::wstring::npos && newline < pos + take) {
                take = newline - pos;
            } else if (pos + take < m_noteText.size()) {
                size_t space = m_noteText.rfind(L' ', pos + take);
                if (space != std::wstring::npos && space > pos) take = space - pos;
            }

            DrawTextLine(ctx, res.brush, res.bodyFormat, m_noteText.substr(pos, take),
                         { content.left, y, content.right, y + kLineHeight }, colors.textPrimary);
            pos += take;
            while (pos < m_noteText.size() && (m_noteText[pos] == L' ' || m_noteText[pos] == L'\n')) ++pos;
            y += kLineHeight;
        }
    }

    // Footer: lock affordance + save state.
    float footerY = content.bottom - 18.0f;
    D2D1_RECT_F iconRect = { content.left, footerY, content.left + 14.0f, footerY + 14.0f };
    icons::Draw(ctx, res.brush, icons::IconKind::Lock, iconRect, colors.textSecondary);

    m_primaryButtonRect = { content.left, footerY, content.left + 70.0f, footerY + 18.0f };
    DrawTextLine(ctx, res.brush, res.smallFormat, L"Lock now",
                 { content.left + 20.0f, footerY, content.left + 90.0f, footerY + 18.0f },
                 m_primaryButtonHovered ? colors.textPrimary : colors.textSecondary);
    m_helloButtonRect = {};
}

void QuickNotesWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, res.brush, res.titleFormat, *res.theme, bounds, L"Quick Notes");

    switch (m_state) {
    case State::Unlocked:
        DrawNoteEditor(ctx, content, res);
        break;
    case State::EnteringPassword:
    case State::SettingPassword:
        DrawPasswordEntry(ctx, content, res);
        break;
    case State::AwaitingHello:
    case State::Locked:
    default:
        DrawLockedState(ctx, content, res);
        break;
    }
}

// --- input ----------------------------------------------------------------

bool QuickNotesWidget::OnMouseMove(D2D1_POINT_2F pt) {
    bool primary = PointInRect(pt, m_primaryButtonRect);
    bool hello = PointInRect(pt, m_helloButtonRect);
    bool changed = (primary != m_primaryButtonHovered) || (hello != m_helloButtonHovered);
    m_primaryButtonHovered = primary;
    m_helloButtonHovered = hello;
    return changed;
}

bool QuickNotesWidget::OnMouseLeave() {
    bool changed = m_primaryButtonHovered || m_helloButtonHovered;
    m_primaryButtonHovered = false;
    m_helloButtonHovered = false;
    return changed;
}

bool QuickNotesWidget::OnLButtonDown(D2D1_POINT_2F pt) {
    m_idleSeconds = 0; // any interaction resets the auto-lock countdown

    if (m_state == State::Unlocked) {
        if (PointInRect(pt, m_primaryButtonRect)) {
            Lock();
            return true;
        }
        return false;
    }

    if (m_state == State::Locked) {
        if (PointInRect(pt, m_helloButtonRect)) {
            BeginHelloVerification();
            return true;
        }
        if (PointInRect(pt, m_primaryButtonRect)) {
            bool firstRun = m_repository && !m_repository->HasPassword();
            m_state = firstRun ? State::SettingPassword : State::EnteringPassword;
            security::NoteCrypto::SecureWipe(m_passwordInput);
            m_statusMessage.clear();
            return true;
        }
    }
    return false;
}

bool QuickNotesWidget::OnChar(wchar_t ch) {
    m_idleSeconds = 0;

    if (m_state == State::EnteringPassword || m_state == State::SettingPassword) {
        if (ch == L'\r' || ch == L'\n' || ch == L'\t' || ch == 27) return false; // handled in OnKeyDown
        if (ch == L'\b') {
            if (!m_passwordInput.empty()) m_passwordInput.pop_back();
            return true;
        }
        if (ch >= 32) {
            m_passwordInput.push_back(ch);
            return true;
        }
        return false;
    }

    if (m_state == State::Unlocked) {
        if (ch == L'\b') {
            if (!m_noteText.empty()) {
                m_noteText.pop_back();
                m_noteDirty = true;
                return true;
            }
            return false;
        }
        if (ch == L'\r' || ch == L'\n') {
            m_noteText.push_back(L'\n');
            m_noteDirty = true;
            return true;
        }
        if (ch >= 32) {
            m_noteText.push_back(ch);
            m_noteDirty = true;
            return true;
        }
    }
    return false;
}

bool QuickNotesWidget::OnKeyDown(unsigned int virtualKey) {
    m_idleSeconds = 0;

    if (m_state == State::EnteringPassword || m_state == State::SettingPassword) {
        if (virtualKey == VK_RETURN) {
            SubmitPassword();
            return true;
        }
        if (virtualKey == VK_ESCAPE) {
            security::NoteCrypto::SecureWipe(m_passwordInput);
            m_state = State::Locked;
            m_statusMessage.clear();
            return true;
        }
        return false;
    }

    if (m_state == State::Unlocked && virtualKey == VK_ESCAPE) {
        Lock();
        return true;
    }
    return false;
}

} // namespace mosaic::widgets
