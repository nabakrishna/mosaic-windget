#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <string>
#include "ui/Theme.h"
#include "ui/Icons.h"

namespace mosaic::ui::components {

// Draws the part every widget card shares — glass background, padded title
// row with an optional leading icon — and returns the remaining rect below
// the title so the caller (e.g. TodoWidget in Phase 3) only has to draw its
// own content, not reimplement card chrome. This is what each of Phase 1's
// hand-rolled DrawXCard functions collapses into.
class WidgetCard {
public:
    static D2D1_RECT_F DrawFrame(
        ID2D1DeviceContext* ctx,
        ID2D1SolidColorBrush* brush,
        IDWriteTextFormat* titleFormat,
        const ThemeManager& theme,
        D2D1_RECT_F bounds,
        const std::wstring& title,
        icons::IconKind icon = icons::IconKind::None,
        D2D1_COLOR_F iconColor = D2D1::ColorF(D2D1::ColorF::White),
        bool hovered = false);
};

} // namespace mosaic::ui::components
