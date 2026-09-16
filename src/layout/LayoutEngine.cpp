#include "layout/LayoutEngine.h"
#include <algorithm>

namespace mosaic::layout {

// --- OccupancyGrid --------------------------------------------------------

void LayoutEngine::OccupancyGrid::EnsureRow(int row, int columns) {
    while (static_cast<int>(m_cells.size()) <= row) {
        m_cells.emplace_back(columns, std::nullopt);
    }
}

bool LayoutEngine::OccupancyGrid::CanPlace(int col, int row, int w, int h, int columns) const {
    if (col < 0 || row < 0 || col + w > columns) return false;
    for (int r = row; r < row + h; ++r) {
        if (r >= static_cast<int>(m_cells.size())) continue; // unpopulated rows are always free
        for (int c = col; c < col + w; ++c) {
            if (m_cells[r][c].has_value()) return false;
        }
    }
    return true;
}

void LayoutEngine::OccupancyGrid::Occupy(widgets::WidgetId id, int col, int row, int w, int h, int columns) {
    for (int r = row; r < row + h; ++r) {
        EnsureRow(r, columns);
        for (int c = col; c < col + w; ++c) {
            m_cells[r][c] = id;
        }
    }
}

void LayoutEngine::OccupancyGrid::Clear(int col, int row, int w, int h) {
    for (int r = row; r < row + h && r < static_cast<int>(m_cells.size()); ++r) {
        for (int c = col; c < col + w && c < static_cast<int>(m_cells[r].size()); ++c) {
            m_cells[r][c].reset();
        }
    }
}

// --- placement -------------------------------------------------------------

GridPosition LayoutEngine::FindFirstFit(const OccupancyGrid& grid, int w, int h, int columns,
                                         GridPosition searchHint) const {
    // Reasonable upper bound: even in a pathological case, no more than a
    // few dozen widgets will ever exist on this dashboard, so this never
    // needs to scan more than a few hundred rows before finding a fit.
    constexpr int kMaxRowsToScan = 500;

    // Pass 1: prefer positions at or after the hint (drag drops should
    // "stick" near where you dropped rather than always snapping to the
    // very top-left).
    for (int r = std::max(0, searchHint.row); r < kMaxRowsToScan; ++r) {
        int startCol = (r == searchHint.row) ? std::max(0, searchHint.col) : 0;
        for (int c = startCol; c <= columns - w; ++c) {
            if (grid.CanPlace(c, r, w, h, columns)) return { c, r };
        }
    }

    // Pass 2: fallback, scan from the absolute top. Always succeeds.
    for (int r = 0; r < kMaxRowsToScan; ++r) {
        for (int c = 0; c <= columns - w; ++c) {
            if (grid.CanPlace(c, r, w, h, columns)) return { c, r };
        }
    }

    // Should be unreachable (kMaxRowsToScan rows is always enough room for
    // any realistic widget count), but never return a colliding position.
    return { 0, kMaxRowsToScan };
}

void LayoutEngine::Compact(OccupancyGrid& grid, std::vector<PlacedWidget>& placements,
                            int columns, std::optional<widgets::WidgetId> skipId) const {
    // Stable reading-order pass: top-to-bottom, then left-to-right. This
    // is what makes compaction deterministic — the same input arrangement
    // always compacts to the same output, which matters both for the
    // drag preview looking predictable and for layout tests.
    std::sort(placements.begin(), placements.end(), [](const PlacedWidget& a, const PlacedWidget& b) {
        if (a.rect.row != b.rect.row) return a.rect.row < b.rect.row;
        return a.rect.col < b.rect.col;
    });

    for (auto& placed : placements) {
        if (skipId.has_value() && placed.id == *skipId) continue; // the widget that was just dropped stays put

        grid.Clear(placed.rect.col, placed.rect.row, placed.rect.cols, placed.rect.rows);
        GridPosition best = FindFirstFit(grid, placed.rect.cols, placed.rect.rows, columns, { 0, 0 });
        placed.rect.col = best.col;
        placed.rect.row = best.row;
        grid.Occupy(placed.id, placed.rect.col, placed.rect.row, placed.rect.cols, placed.rect.rows, columns);
    }
}

// --- public API --------------------------------------------------------

LayoutResult LayoutEngine::Arrange(const std::vector<WidgetInput>& widgets, const LayoutConstraints& constraints) const {
    OccupancyGrid grid;
    LayoutResult result;

    for (const auto& input : widgets) {
        GridPosition hint = input.savedPosition.value_or(GridPosition{ 0, 0 });
        GridPosition placedAt = FindFirstFit(grid, input.size.cols, input.size.rows, constraints.columns, hint);
        grid.Occupy(input.id, placedAt.col, placedAt.row, input.size.cols, input.size.rows, constraints.columns);
        result.placements.push_back(PlacedWidget{ input.id, GridRect{ placedAt.col, placedAt.row, input.size.cols, input.size.rows } });
    }

    // A saved layout could, in principle, have widgets placed in an order
    // that leaves avoidable gaps (e.g. after a widget was removed from a
    // previous version). One compaction pass tidies that up without
    // requiring a special "migration" code path — it's the same pass a
    // fresh install's own placement already doesn't need but doesn't hurt
    // either, since a fresh arrangement is already maximally compact.
    Compact(grid, result.placements, constraints.columns, std::nullopt);

    int totalRows = 0;
    for (const auto& p : result.placements) totalRows = std::max(totalRows, p.rect.Bottom());
    result.totalRows = totalRows;
    return result;
}

LayoutResult LayoutEngine::MoveWidget(const LayoutResult& current, widgets::WidgetId movedId,
                                       GridPosition targetTopLeft, const LayoutConstraints& constraints) const {
    LayoutResult result;
    result.placements = current.placements;

    const GridRect* movedRect = result.Find(movedId);
    if (!movedRect) return current; // unknown widget id — no-op, defensively

    widgets::GridSize movedSize{ movedRect->cols, movedRect->rows };

    OccupancyGrid grid;
    // Occupy everything except the widget being moved, at its *current*
    // position, so the moved widget's own old footprint doesn't block its
    // new placement search.
    for (const auto& p : result.placements) {
        if (p.id == movedId) continue;
        grid.Occupy(p.id, p.rect.col, p.rect.row, p.rect.cols, p.rect.rows, constraints.columns);
    }

    GridPosition placedAt = FindFirstFit(grid, movedSize.cols, movedSize.rows, constraints.columns, targetTopLeft);
    grid.Occupy(movedId, placedAt.col, placedAt.row, movedSize.cols, movedSize.rows, constraints.columns);

    for (auto& p : result.placements) {
        if (p.id == movedId) {
            p.rect.col = placedAt.col;
            p.rect.row = placedAt.row;
        }
    }

    // Everyone else compacts up-and-left around the now-fixed moved
    // widget — spec section 15's "automatically resize positions where
    // necessary... avoid collisions... avoid awkward gaps" in one pass.
    Compact(grid, result.placements, constraints.columns, movedId);

    int totalRows = 0;
    for (const auto& p : result.placements) totalRows = std::max(totalRows, p.rect.Bottom());
    result.totalRows = totalRows;
    return result;
}

} // namespace mosaic::layout
