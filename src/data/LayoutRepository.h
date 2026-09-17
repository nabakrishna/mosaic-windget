#pragma once
#include <vector>
#include <utility>
#include "data/Database.h"
#include "widgets/IWidget.h"

namespace mosaic::data {

// Persists where each widget card sits on the grid — just a (col, row) per
// WidgetId, since size comes from the widget's own Metadata() and layout
// (Phase 6) repositions widgets, it doesn't resize them. One row per
// widget, upserted on every drop rather than accumulating history.
class LayoutRepository {
public:
    explicit LayoutRepository(Database* database) : m_database(database) {}

    struct SavedPosition {
        widgets::WidgetId id;
        int col;
        int row;
    };

    // Empty if no layout has ever been saved (fresh install) — callers
    // treat that as "use LayoutEngine::Arrange's default placement".
    std::vector<SavedPosition> LoadAll();

    // Upserts every entry in `positions` in a single transaction — called
    // once per completed drag-drop, not continuously during a drag.
    void SaveAll(const std::vector<SavedPosition>& positions);

    // Wipes every saved position — used by Settings' "Reset Layout"
    // (spec section 20). The next arrangement falls back to
    // LayoutEngine::Arrange's default first-fit placement.
    void ClearAll();

private:
    Database* m_database;
};

} // namespace mosaic::data
