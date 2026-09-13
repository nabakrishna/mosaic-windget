#pragma once
#include <d2d1_1.h>
#include "ui/Theme.h"
#include "ui/Icons.h"

namespace mosaic::ui::components {

// A small circular button (used for the header's settings gear today; the
// same class will back the Settings panel's close button and any future
// per-widget icon actions). Unlike Phase 1's placeholder circle, this one
// actually tracks hover/pressed state and can be hit-tested against real
// mouse coordinates — Window forwards WM_MOUSEMOVE/WM_LBUTTONDOWN into
// whichever IconButtons the current view owns.
class IconButton {
public:
    void SetBounds(D2D1_POINT_2F center, float radius) { m_center = center; m_radius = radius; }

    bool HitTest(D2D1_POINT_2F point) const {
        float dx = point.x - m_center.x;
        float dy = point.y - m_center.y;
        return (dx * dx + dy * dy) <= (m_radius * m_radius);
    }

    void SetHovered(bool hovered) { m_hovered = hovered; }
    void SetPressed(bool pressed) { m_pressed = pressed; }
    bool IsHovered() const { return m_hovered; }
    bool IsPressed() const { return m_pressed; }

    void Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush,
              icons::IconKind icon, const ThemeManager& theme) const {
        const ThemeColors& colors = theme.Colors();

        D2D1_ELLIPSE circle = D2D1::Ellipse(m_center, m_radius, m_radius);
        brush->SetColor(m_pressed ? colors.cardFill : (m_hovered ? colors.cardFillHover : colors.cardFill));
        ctx->FillEllipse(circle, brush);
        brush->SetColor(colors.cardBorder);
        ctx->DrawEllipse(circle, brush, 1.0f);

        float iconInset = m_radius * 0.55f;
        D2D1_RECT_F iconRect = {
            m_center.x - iconInset, m_center.y - iconInset,
            m_center.x + iconInset, m_center.y + iconInset
        };
        icons::Draw(ctx, brush, icon, iconRect, m_hovered ? colors.textPrimary : colors.textSecondary);
    }

private:
    D2D1_POINT_2F m_center{ 0.0f, 0.0f };
    float m_radius = 0.0f;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace mosaic::ui::components
