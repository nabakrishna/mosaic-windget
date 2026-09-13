#pragma once
#include <d2d1_1.h>
#include <string>

namespace mosaic::ui {

// --- Theme data -----------------------------------------------------------
// These used to be `static constexpr` values baked into the binary. As of
// Phase 2 they're plain runtime data owned by a ThemeManager instance
// instead, because Phase 7's Appearance settings (transparency slider, blur
// level, corner radius, accent color picker, dark/light/system) need to
// mutate them at runtime. Nothing about the *values* changed here — only
// that they're now instance fields instead of compile-time constants.

struct ThemeColors {
    D2D1_COLOR_F backgroundFallback; // shown behind everything when DWM's
                                      // acrylic backdrop isn't available
    D2D1_COLOR_F cardFill;
    D2D1_COLOR_F cardFillHover;
    D2D1_COLOR_F cardBorder;

    D2D1_COLOR_F textPrimary;
    D2D1_COLOR_F textSecondary;
    D2D1_COLOR_F textMuted;
    D2D1_COLOR_F textCompleted;   // dimmed style for finished to-do items

    D2D1_COLOR_F accentGreen;     // greeting leaf, completed checkboxes
    D2D1_COLOR_F accentViolet;    // pinned item markers
    D2D1_COLOR_F accentAmber;     // special-activity star

    D2D1_COLOR_F shadowColor;
};

struct ThemeMetrics {
    float cardCornerRadius;
    float cardBorderWidth;
    float gridGutter;    // space between cards
    float pagePadding;   // dashboard outer margin
    float cardPadding;   // inner card padding
    float shadowSpread;  // how far the poor-man's shadow (see GlassPanel) extends
    float shadowMaxAlpha;
};

struct ThemeTypography {
    // Segoe UI Variable ships on Windows 11; DirectWrite automatically
    // falls back to plain Segoe UI on Windows 10 if the family is missing
    // (see DashboardView::MakeFormat's fallback path).
    std::wstring fontFamily = L"Segoe UI Variable Display";
    std::wstring fontFamilyFallback = L"Segoe UI";

    float sizeGreetingName = 22.0f;
    float sizeGreetingSub  = 13.0f;
    float sizeDateDay      = 13.0f;
    float sizeDateFull     = 20.0f;
    float sizeCardTitle    = 14.0f;
    float sizeBody         = 13.5f;
    float sizeSmall        = 11.5f;
};

// Bundles the three theme aspects and knows how to produce the built-in
// presets. Settings (Phase 7) will extend this with CreateLight() and a
// path that rebuilds a ThemeManager instance from persisted user choices;
// today it only has the one preset the reference design uses.
class ThemeManager {
public:
    static ThemeManager CreateDark();

    const ThemeColors&     Colors()     const { return m_colors; }
    const ThemeMetrics&    Metrics()    const { return m_metrics; }
    const ThemeTypography& Typography() const { return m_typography; }

    // Mutators exist now (rather than being added later) because Phase 7
    // needs to tweak individual values — e.g. the transparency slider only
    // touches cardFill.a — without replacing the whole theme object.
    ThemeColors&     MutableColors()     { return m_colors; }
    ThemeMetrics&    MutableMetrics()    { return m_metrics; }
    ThemeTypography& MutableTypography() { return m_typography; }

private:
    ThemeColors m_colors{};
    ThemeMetrics m_metrics{};
    ThemeTypography m_typography{};
};

// Returns `color` with its alpha channel replaced. Used by hover/press
// states so widgets don't need a second full palette.
D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha);

} // namespace mosaic::ui
