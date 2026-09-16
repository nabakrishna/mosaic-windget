#pragma once
#include <vector>
#include "widgets/IWidget.h"

namespace mosaic::layout {

// A cell coordinate in the abstract grid — conceptual units, not pixels
// (spec section 16: "These are conceptual grid units rather than fixed
// pixels"). Column/row 0,0 is top-left.
struct GridPosition {
    int col = 0;
    int row = 0;

    bool operator==(const GridPosition& other) const { return col == other.col && row == other.row; }
};

// One widget's footprint on the grid: where it starts, and how many
// columns/rows it occupies (from its GridSize, unchanged by layout —
// Phase 6 repositions widgets, it doesn't resize them).
struct GridRect {
    int col = 0;
    int row = 0;
    int cols = 1;
    int rows = 1;

    int Right() const { return col + cols; }  // exclusive
    int Bottom() const { return row + rows; } // exclusive
};

struct PlacedWidget {
    widgets::WidgetId id;
    GridRect rect;
};

// Fixed column count for the whole grid. Spec section 16 frames widget
// sizes as grid units independent of window size — so it's the column
// *width in DIPs* that adapts to the window, not the column *count*.
// Six columns is what makes the reference design's default arrangement
// (three roughly-equal cards on top, two wider ones below) fall out
// naturally from the widgets' own preferred GridSizes — see
// ActivityWidget.h's comment on why its preferred width was widened from
// spec section 16's literal "1×2" example to match the reference image.
struct LayoutConstraints {
    int columns = 6;
};

// One full arrangement: where every widget currently sits, plus the total
// row count it occupies (DashboardView needs this to convert row units
// into DIPs — see its grid-to-DIP conversion).
struct LayoutResult {
    std::vector<PlacedWidget> placements;
    int totalRows = 0;

    const GridRect* Find(widgets::WidgetId id) const {
        for (const auto& p : placements) {
            if (p.id == id) return &p.rect;
        }
        return nullptr;
    }
};

} // namespace mosaic::layout
