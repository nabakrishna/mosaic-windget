#pragma once
#include <d2d1_1.h>
#include <algorithm>
#include "ui/Theme.h"

namespace mosaic::ui::components {

// Horizontal slider backing Settings rows like the transparency 0–100%
// control from spec section 19. Value is normalized to [0, 1]; callers
// (Phase 7's Settings panel) map that to whatever real range they need
// (percentage, seconds, degrees) themselves — keeping this class free of
// per-setting knowledge.
class Slider {
public:
    void SetBounds(D2D1_RECT_F bounds) { m_bounds = bounds; }
    void SetValue(float v) { m_value = std::clamp(v, 0.0f, 1.0f); }
    float Value() const { return m_value; }

    bool HitTest(D2D1_POINT_2F point) const {
        // Generous vertical hit area (full track height + a little) so the
        // thumb is easy to grab without needing pixel-perfect precision.
        return point.x >= m_bounds.left && point.x <= m_bounds.right &&
               point.y >= m_bounds.top - 6.0f && point.y <= m_bounds.bottom + 6.0f;
    }

    // Converts an x coordinate (already known to be within/near the track,
    // typically because HitTest passed) into a value and stores it. Called
    // continuously during a drag from WM_MOUSEMOVE while the button is down.
    void SetValueFromPointerX(float x) {
        float width = m_bounds.right - m_bounds.left;
        if (width <= 0.0f) return;
        m_value = std::clamp((x - m_bounds.left) / width, 0.0f, 1.0f);
    }

    void SetHovered(bool hovered) { m_hovered = hovered; }
    void SetDragging(bool dragging) { m_dragging = dragging; }

    void Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, const ThemeManager& theme) const {
        const ThemeColors& colors = theme.Colors();
        float trackY = (m_bounds.top + m_bounds.bottom) * 0.5f;
        float trackHeight = 4.0f;

        D2D1_RECT_F fullTrack = { m_bounds.left, trackY - trackHeight * 0.5f, m_bounds.right, trackY + trackHeight * 0.5f };
        D2D1_ROUNDED_RECT fullRR = D2D1::RoundedRect(fullTrack, trackHeight * 0.5f, trackHeight * 0.5f);
        brush->SetColor(colors.cardFill);
        ctx->FillRoundedRectangle(fullRR, brush);

        float fillX = m_bounds.left + (m_bounds.right - m_bounds.left) * m_value;
        D2D1_RECT_F filledTrack = { m_bounds.left, trackY - trackHeight * 0.5f, fillX, trackY + trackHeight * 0.5f };
        D2D1_ROUNDED_RECT filledRR = D2D1::RoundedRect(filledTrack, trackHeight * 0.5f, trackHeight * 0.5f);
        brush->SetColor(colors.accentViolet);
        ctx->FillRoundedRectangle(filledRR, brush);

        float thumbRadius = (m_dragging || m_hovered) ? 8.0f : 7.0f;
        D2D1_ELLIPSE thumb = D2D1::Ellipse({ fillX, trackY }, thumbRadius, thumbRadius);
        brush->SetColor(colors.textPrimary);
        ctx->FillEllipse(thumb, brush);
    }

private:
    D2D1_RECT_F m_bounds{};
    float m_value = 0.0f;
    bool m_hovered = false;
    bool m_dragging = false;
};

} // namespace mosaic::ui::components
