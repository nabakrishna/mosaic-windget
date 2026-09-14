#pragma once
#include <d2d1_1.h>
#include <wrl/client.h>

namespace mosaic::ui::icons {

// Every icon in Mosaic is drawn, not imported from an icon font or PNG
// sprite sheet — this keeps the binary small, keeps icons crisp at any DPI
// with zero extra work, and matches "minimal icons" from the design brief
// better than a mismatched icon-font aesthetic would.
enum class IconKind {
    None,
    Leaf,          // greeting accent
    Gear,          // settings button
    Star,          // special activity
    Pin,           // pinned items
    Lock,          // quick notes lock affordance
    Check,         // completed to-do (drawn over the checkbox fill)
    Close,         // per-row delete "x", shown on hover
    Plus,          // add-task / add-item buttons
    ChevronRight,  // settings category rows (Phase 7)
};

// Draws `kind` inside `bounds` (a square-ish region is assumed; icons scale
// to fit and preserve aspect ratio) using `strokeColor` for fills/strokes.
// `ctx` and `brush` are supplied by the caller so icons don't own their own
// brush — one scratch ID2D1SolidColorBrush is reused for everything Mosaic
// draws, recolored per call via SetColor.
void Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush,
          IconKind kind, D2D1_RECT_F bounds, D2D1_COLOR_F color);

} // namespace mosaic::ui::icons
