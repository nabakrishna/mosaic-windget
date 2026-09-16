#pragma once
#include <vector>
#include <optional>
#include "layout/GridTypes.h"

namespace mosaic::layout {

// One widget's identity, preferred footprint, and (optionally) a
// previously-known position to place it at, as input to the engine.
struct WidgetInput {
    widgets::WidgetId id;
    widgets::GridSize size; // from IWidget::Metadata().preferred — layout repositions, never resizes
    std::optional<GridPosition> savedPosition; // from LayoutRepository, if a prior layout exists
};

// Pure grid-unit placement logic — no D2D, no DIPs, no rendering. Spec
// section 32 explicitly asks for this: "The engine should be independent
// from the visual layer. This allows layout testing without rendering the
// UI." Every method here takes and returns plain data; DashboardView is
// the only thing that knows how to turn a LayoutResult into actual pixel
// rects and drag interactions.
//
// Algorithm, in short: an occupancy grid (which cells are taken by which
// widget) plus a deterministic "first fit, reading order" placement scan,
// plus a compaction pass that pulls every widget as far up-and-left as it
// can go without overlapping anything. This is the same family of
// algorithm real masonry/grid-dashboard libraries use (e.g. the
// vertical-compaction approach react-grid-layout is built around) — not
// a full bin-packing optimizer, but genuinely collision-free, genuinely
// gap-minimizing, and easy to reason about and test.
class LayoutEngine {
public:
    // Places every widget in `widgets` (in the given order) using each
    // one's savedPosition if present, otherwise the first available slot
    // in reading order. Always collision-free; always compacted afterward.
    // This is what produces the very first layout on a fresh install (no
    // saved positions at all) and what re-derives a full LayoutResult from
    // LayoutRepository's saved positions on every subsequent launch.
    LayoutResult Arrange(const std::vector<WidgetInput>& widgets, const LayoutConstraints& constraints) const;

    // Relocates `movedId` to start as close to `targetTopLeft` as its size
    // and the grid bounds allow (clamped/first-fit-adjusted if the exact
    // target is out of bounds or would only partially fit), then compacts
    // every other widget up-and-left around it. Used both for the live
    // drag preview (called on every mouse move with the cell under the
    // cursor) and to commit the final drop.
    LayoutResult MoveWidget(const LayoutResult& current, widgets::WidgetId movedId,
                             GridPosition targetTopLeft, const LayoutConstraints& constraints) const;

private:
    // Internal working representation: which WidgetId (if any) occupies
    // each cell. Rows grow on demand — there's no fixed grid height, only
    // a fixed column count.
    class OccupancyGrid {
    public:
        bool CanPlace(int col, int row, int w, int h, int columns) const;
        void Occupy(widgets::WidgetId id, int col, int row, int w, int h, int columns);
        void Clear(int col, int row, int w, int h);

    private:
        // A vector of rows of optional<WidgetId> is simplest to get right,
        // and five widgets means this is never large enough for the
        // simplicity/performance tradeoff to matter.
        std::vector<std::vector<std::optional<widgets::WidgetId>>> m_cells;

        void EnsureRow(int row, int columns);
    };

    // Finds the first position (in row-major reading order) where a
    // w×h widget fits without colliding, preferring positions at or after
    // `searchHint` before falling back to a scan from the very top (which
    // always succeeds eventually, since rows are unbounded).
    GridPosition FindFirstFit(const OccupancyGrid& grid, int w, int h, int columns,
                               GridPosition searchHint) const;

    // Removes every widget from the grid and re-places them (in their
    // current placement order, skipping `skipId` which is assumed already
    // placed) as far up-and-left as they'll go. This is the "gap
    // elimination" pass from spec section 17.
    void Compact(OccupancyGrid& grid, std::vector<PlacedWidget>& placements,
                 int columns, std::optional<widgets::WidgetId> skipId) const;
};

} // namespace mosaic::layout
