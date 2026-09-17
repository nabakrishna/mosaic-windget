#include "data/LayoutRepository.h"

namespace mosaic::data {

namespace {

struct ScopedStmt {
    sqlite3_stmt* stmt = nullptr;
    ~ScopedStmt() { if (stmt) sqlite3_finalize(stmt); }
    sqlite3_stmt** operator&() { return &stmt; }
    operator sqlite3_stmt* () const { return stmt; }
};

} // namespace

std::vector<LayoutRepository::SavedPosition> LayoutRepository::LoadAll() {
    std::vector<SavedPosition> result;
    static const wchar_t* kSql = L"SELECT widget_id, grid_col, grid_row FROM widget_layout;";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int rawId = sqlite3_column_int(stmt, 0);
        // Defensive: a corrupted or hand-edited database could contain a
        // widget_id outside the known enum range. Skip it rather than
        // fabricate a widget that doesn't exist — LayoutEngine::Arrange
        // simply won't have a saved position for whatever's missing and
        // falls back to first-fit placement for it, which is a safe,
        // sensible default per spec section 60's error-handling spirit.
        if (rawId < static_cast<int>(widgets::WidgetId::Todo) ||
            rawId > static_cast<int>(widgets::WidgetId::QuickNotes)) {
            continue;
        }
        SavedPosition pos;
        pos.id = static_cast<widgets::WidgetId>(rawId);
        pos.col = sqlite3_column_int(stmt, 1);
        pos.row = sqlite3_column_int(stmt, 2);
        result.push_back(pos);
    }
    return result;
}

void LayoutRepository::SaveAll(const std::vector<SavedPosition>& positions) {
    sqlite3_exec(m_database->Handle(), "BEGIN;", nullptr, nullptr, nullptr);

    static const wchar_t* kSql =
        L"INSERT INTO widget_layout (widget_id, grid_col, grid_row) VALUES (?, ?, ?) "
        L"ON CONFLICT(widget_id) DO UPDATE SET grid_col = excluded.grid_col, grid_row = excluded.grid_row;";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& pos : positions) {
            sqlite3_bind_int(stmt, 1, static_cast<int>(pos.id));
            sqlite3_bind_int(stmt, 2, pos.col);
            sqlite3_bind_int(stmt, 3, pos.row);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
    }

    sqlite3_exec(m_database->Handle(), "COMMIT;", nullptr, nullptr, nullptr);
}

void LayoutRepository::ClearAll() {
    sqlite3_exec(m_database->Handle(), "DELETE FROM widget_layout;", nullptr, nullptr, nullptr);
}

} // namespace mosaic::data
