#include "ui/Theme.h"

namespace mosaic::ui {

ThemeManager ThemeManager::CreateDark() {
    ThemeManager theme;

    ThemeColors& c = theme.MutableColors();
    c.backgroundFallback = { 0.05f, 0.06f, 0.10f, 0.92f };
    c.cardFill        = { 0.13f, 0.14f, 0.20f, 0.55f };
    c.cardFillHover   = { 0.16f, 0.17f, 0.24f, 0.62f };
    c.cardBorder      = { 1.0f,  1.0f,  1.0f,  0.08f };
    c.textPrimary     = { 0.95f, 0.95f, 0.97f, 1.0f };
    c.textSecondary   = { 0.65f, 0.66f, 0.74f, 1.0f };
    c.textMuted       = { 0.45f, 0.46f, 0.54f, 1.0f };
    c.textCompleted   = { 0.42f, 0.44f, 0.52f, 0.7f };
    c.accentGreen     = { 0.42f, 0.85f, 0.55f, 1.0f };
    c.accentViolet    = { 0.55f, 0.50f, 0.95f, 1.0f };
    c.accentAmber     = { 0.95f, 0.75f, 0.40f, 1.0f };
    c.shadowColor     = { 0.0f,  0.0f,  0.0f,  0.35f };

    ThemeMetrics& m = theme.MutableMetrics();
    m.cardCornerRadius = 18.0f;
    m.cardBorderWidth  = 1.0f;
    m.gridGutter       = 16.0f;
    m.pagePadding      = 24.0f;
    m.cardPadding      = 18.0f;
    m.shadowSpread     = 10.0f;
    m.shadowMaxAlpha   = 0.22f;

    // Typography left at ThemeTypography's own defaults — the reference
    // design's type scale, no per-theme overrides needed yet.
    return theme;
}

D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha) {
    color.a = alpha;
    return color;
}

} // namespace mosaic::ui
