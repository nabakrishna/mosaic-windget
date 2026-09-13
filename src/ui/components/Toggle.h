#pragma once
#include <d2d1_1.h>
#include "ui/Theme.h"

namespace mosaic::ui::components {

// A pill-shaped on/off switch, the standard control for every boolean
// Settings row (spec sections 19–27 are full of "[✓] ..." checkboxes —
// this is what renders those). Not exercised by the dashboard itself yet;
// it's built now, correctly and completely, because Settings (Phase 7)
// needs a working control library on day one rather than a rewrite.
class Toggle {
public:
    void SetBounds(D2D1_RECT_F bounds) { m_bounds = bounds; }
    void SetValue(bool on) { m_value = on; }
    bool Value() const { return m_value; }

    bool HitTest(D2D1_POINT_2F point) const {
        return point.x >= m_bounds.left && point.x <= m_bounds.right &&
               point.y >= m_bounds.top && point.y <= m_bounds.bottom;
    }

    // Flips state and returns the new value; callers wire this to a click
    // (WM_LBUTTONDOWN + HitTest) and persist the result via a settings
    // repository in Phase 7.
    bool Flip() { m_value = !m_value; return m_value; }

    void SetHovered(bool hovered) { m_hovered = hovered; }

    void Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, const ThemeManager& theme) const {
        const ThemeColors& colors = theme.Colors();
        float h = m_bounds.bottom - m_bounds.top;
        float radius = h * 0.5f;

        D2D1_COLOR_F trackOff = colors.cardFill;
        D2D1_COLOR_F trackOn = colors.accentGreen;
        D2D1_COLOR_F track = m_value ? trackOn : trackOff;
        if (m_hovered) track.a = (track.a + 1.0f) * 0.5f; // subtle brighten on hover

        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(m_bounds, radius, radius);
        brush->SetColor(track);
        ctx->FillRoundedRectangle(rr, brush);
        brush->SetColor(colors.cardBorder);
        ctx->DrawRoundedRectangle(rr, brush, 1.0f);

        // Thumb: a filled circle that sits left when off, right when on.
        float thumbRadius = radius - 3.0f;
        float thumbX = m_value ? (m_bounds.right - radius) : (m_bounds.left + radius);
        D2D1_ELLIPSE thumb = D2D1::Ellipse({ thumbX, m_bounds.top + radius }, thumbRadius, thumbRadius);
        brush->SetColor(colors.textPrimary);
        ctx->FillEllipse(thumb, brush);
    }

private:
    D2D1_RECT_F m_bounds{};
    bool m_value = false;
    bool m_hovered = false;
};

} // namespace mosaic::ui::components
