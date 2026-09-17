#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <string>
#include "ui/Theme.h"

namespace mosaic::widgets {

// Which widget this is. A plain enum (not a string) because it's compared
// and used as a map key constantly; WidgetRegistry is what maps this to an
// actual factory function, so adding a widget kind later means registering
// a factory, not touching every switch statement that mentions widgets.
enum class WidgetId : int {
    Todo = 0,
    Photo = 1,
    Activity = 2,
    Pinned = 3,
    QuickNotes = 4,
};

// Conceptual grid units (not pixels) — see spec section 16. Unused by the
// fixed layout DashboardView still computes today; declared now so the
// Phase 6 LayoutEngine has real metadata to consume instead of retrofitting
// it onto every widget class then.
struct GridSize {
    int cols = 1;
    int rows = 1;
};

struct WidgetMetadata {
    WidgetId id;
    std::wstring displayName;
    GridSize preferred;
    GridSize minimum;
    GridSize maximum;
};

// Bundles everything a widget needs to draw text/shapes without owning its
// own brushes or text formats — those stay centralized in DashboardView
// (one brush, a handful of text formats, reused across every widget every
// frame) rather than each widget allocating its own device resources.
struct WidgetRenderResources {
    ID2D1SolidColorBrush* brush;
    IDWriteTextFormat* titleFormat;
    IDWriteTextFormat* bodyFormat;
    IDWriteTextFormat* smallFormat;
    IDWriteFactory* dwriteFactory; // for widgets that need exact text measurement (e.g. caret placement)
    const ui::ThemeManager* theme;
};

// The contract every dashboard widget implements. Phase 3 only gives
// TodoWidget real interactivity and persistence; Photo/Activity/Pinned/
// QuickNotes implement this interface today with the same static-sample
// visuals they had inline in DashboardView before Phase 3 — moving them
// behind IWidget now means Phases 4/5/8 slot real data into an existing
// shape instead of inventing the interface under time pressure later.
class IWidget {
public:
    virtual ~IWidget() = default;

    virtual WidgetId Id() const = 0;
    virtual WidgetMetadata Metadata() const = 0;

    // Draws the widget's full card (chrome + content) into `bounds` (DIPs,
    // already positioned by DashboardView's layout logic).
    virtual void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) = 0;

    // Input hooks. Each returns true if something visibly changed (caller
    // should invalidate/repaint) and false otherwise. Default no-ops mean
    // widgets that don't need interactivity yet (everything except Todo,
    // for now) don't have to override anything.
    virtual bool OnMouseMove(D2D1_POINT_2F /*pointDip*/) { return false; }
    virtual bool OnMouseLeave() { return false; }
    virtual bool OnLButtonDown(D2D1_POINT_2F /*pointDip*/) { return false; }
    virtual bool OnDoubleClick(D2D1_POINT_2F /*pointDip*/) { return false; }
    virtual bool OnChar(wchar_t /*ch*/) { return false; }
    virtual bool OnKeyDown(unsigned int /*virtualKey*/) { return false; }

    virtual void OnSettingsChanged() {}
    virtual void OnResize(const GridSize& /*newSize*/) {}

    // Called after a D3D device-lost recovery, before the next Render.
    // Every widget so far only ever draws through the shared brush/text
    // formats DashboardView owns and recreates itself — except PhotoWidget
    // (Phase 5), which owns its own ID2D1Bitmap texture tied to the old,
    // now-destroyed device. Default no-op costs nothing for every other
    // widget; PhotoWidget overrides it to drop its stale texture rather
    // than risk drawing with a bitmap that belongs to a dead device.
    virtual void OnDeviceLost() {}
};

} // namespace mosaic::widgets
