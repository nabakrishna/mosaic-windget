#pragma once
#include <d2d1_1.h>
#include "ui/Theme.h"

namespace mosaic::ui::components {

// The single reusable "glass card" background. Every widget card, the
// Settings panel, and IconButtons all draw through this so the translucent
// rounded-rect look defined in Theme stays consistent everywhere instead of
// being hand-rolled per widget (which is what Phase 1's DashboardView did —
// this replaces that duplication).
class GlassPanel {
public:
    // Draws the fill, border, and an approximate soft shadow into `rect`.
    // `hovered` swaps in the theme's hover fill color.
    //
    // Shadow note: this deliberately does NOT use a real Gaussian-blur
    // ID2D1Effect. A true blur effect is a fine one-time cost for a static
    // card, but stacking that on every card, every redraw, on a design
    // explicitly required to idle at ~0% CPU is the wrong trade. Instead
    // this draws a few progressively larger, progressively fainter rounded
    // rects beneath the card — a standard cheap "poor-man's shadow" — which
    // reads as the same soft, subtle shadow from the reference design at a
    // fraction of the cost. Swapping in a real blur effect later (e.g. for
    // an eventual "Widget Shadow: Strong" setting) only touches this one
    // function.
    static void Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* scratchBrush,
                      D2D1_RECT_F rect, const ThemeManager& theme, bool hovered = false);
};

} // namespace mosaic::ui::components
