#include "ui/components/WidgetCard.h"
#include "ui/components/GlassPanel.h"

namespace mosaic::ui::components {

D2D1_RECT_F WidgetCard::DrawFrame(
    ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* titleFormat,
    const ThemeManager& theme, D2D1_RECT_F bounds, const std::wstring& title,
    icons::IconKind icon, D2D1_COLOR_F iconColor, bool hovered) {

    GlassPanel::Draw(ctx, brush, bounds, theme, hovered);

    const ThemeMetrics& metrics = theme.Metrics();
    const ThemeColors& colors = theme.Colors();
    float pad = metrics.cardPadding;
    float titleTop = bounds.top + pad;
    float titleHeight = 20.0f;
    float x = bounds.left + pad;

    if (icon != icons::IconKind::None) {
        float iconSize = 14.0f;
        D2D1_RECT_F iconRect = { x, titleTop + 1.0f, x + iconSize, titleTop + 1.0f + iconSize };
        icons::Draw(ctx, brush, icon, iconRect, iconColor);
        x += iconSize + 8.0f;
    }

    if (titleFormat && !title.empty()) {
        D2D1_RECT_F titleRect = { x, titleTop, bounds.right - pad, titleTop + titleHeight };
        brush->SetColor(colors.textPrimary);
        ctx->DrawText(title.c_str(), static_cast<UINT32>(title.size()), titleFormat, titleRect, brush,
                      D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // Content starts below the title row with one padding gap.
    return D2D1_RECT_F{
        bounds.left + pad, titleTop + titleHeight + 12.0f,
        bounds.right - pad, bounds.bottom - pad
    };
}

} // namespace mosaic::ui::components
