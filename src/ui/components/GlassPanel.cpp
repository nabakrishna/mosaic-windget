#include "ui/components/GlassPanel.h"

namespace mosaic::ui::components {

void GlassPanel::Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush,
                       D2D1_RECT_F rect, const ThemeManager& theme, bool hovered) {
    const ThemeColors& colors = theme.Colors();
    const ThemeMetrics& metrics = theme.Metrics();

    // Poor-man's shadow: three descending rounded rects, expanding outward
    // and fading out, drawn before the card itself.
    const int layers = 3;
    for (int i = layers; i >= 1; --i) {
        float t = static_cast<float>(i) / layers;
        float expand = metrics.shadowSpread * t;
        float alpha = metrics.shadowMaxAlpha * (1.0f - t) * colors.shadowColor.a / 0.35f;
        // (dividing by the reference alpha keeps this proportionate if a
        // theme ever ships a different base shadowColor.a)

        D2D1_RECT_F shadowRect = {
            rect.left - expand * 0.5f, rect.top - expand * 0.25f + metrics.shadowSpread * 0.3f,
            rect.right + expand * 0.5f, rect.bottom + expand * 0.6f
        };
        D2D1_ROUNDED_RECT shadowRR = D2D1::RoundedRect(
            shadowRect, metrics.cardCornerRadius + expand * 0.3f, metrics.cardCornerRadius + expand * 0.3f);

        D2D1_COLOR_F shadow = colors.shadowColor;
        shadow.a = alpha;
        brush->SetColor(shadow);
        ctx->FillRoundedRectangle(shadowRR, brush);
    }

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, metrics.cardCornerRadius, metrics.cardCornerRadius);

    brush->SetColor(hovered ? colors.cardFillHover : colors.cardFill);
    ctx->FillRoundedRectangle(rr, brush);

    brush->SetColor(colors.cardBorder);
    ctx->DrawRoundedRectangle(rr, brush, metrics.cardBorderWidth);
}

} // namespace mosaic::ui::components
