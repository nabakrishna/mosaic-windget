#include "data/PinnedRepository.h"
#include <ctime>

namespace mosaic::data {

namespace {

struct ScopedStmt {
    sqlite3_stmt* stmt = nullptr;
    ~ScopedStmt() { if (stmt) sqlite3_finalize(stmt); }
    sqlite3_stmt** operator&() { return &stmt; }
    operator sqlite3_stmt* () const { return stmt; }
};

void BindText16(sqlite3_stmt* stmt, int index, const std::wstring& text) {
    sqlite3_bind_text16(stmt, index, text.c_str(),
                         static_cast<int>(text.size() * sizeof(wchar_t)),
                         SQLITE_TRANSIENT);
}

std::wstring ColumnText16(sqlite3_stmt* stmt, int index) {
    const void* ptr = sqlite3_column_text16(stmt, index);
    int bytes = sqlite3_column_bytes16(stmt, index);
    if (!ptr || bytes <= 0) return L"";
    return std::wstring(reinterpret_cast<const wchar_t*>(ptr), bytes / sizeof(wchar_t));
}

} // namespace

std::vector<PinnedRecord> PinnedRepository::GetAll() {
    std::vector<PinnedRecord> result;
    static const wchar_t* kSql =
        L"SELECT id, title, subtitle, sort_order FROM pinned_items ORDER BY sort_order ASC, id ASC;";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PinnedRecord p;
        p.id = sqlite3_column_int64(stmt, 0);
        p.title = ColumnText16(stmt, 1);
        p.subtitle = ColumnText16(stmt, 2);
        p.sortOrder = sqlite3_column_int(stmt, 3);
        result.push_back(std::move(p));
    }
    return result;
}

int64_t PinnedRepository::Add(const std::wstring& title) {
    static const wchar_t* kSql =
        L"INSERT INTO pinned_items (title, subtitle, sort_order, created_at) "
        L"VALUES (?, 'Pinned on desktop', (SELECT IFNULL(MAX(sort_order), -1) + 1 FROM pinned_items), ?);";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    BindText16(stmt, 1, title);
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(std::time(nullptr)));

    if (sqlite3_step(stmt) != SQLITE_DONE) return 0;
    return sqlite3_last_insert_rowid(m_database->Handle());
}

void PinnedRepository::Rename(int64_t id, const std::wstring& newTitle) {
    static const wchar_t* kSql = L"UPDATE pinned_items SET title = ? WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    BindText16(stmt, 1, newTitle);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void PinnedRepository::Remove(int64_t id) {
    static const wchar_t* kSql = L"DELETE FROM pinned_items WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
}

void PinnedRepository::SetSortOrder(int64_t id, int newSortOrder) {
    static const wchar_t* kSql = L"UPDATE pinned_items SET sort_order = ? WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, newSortOrder);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void PinnedRepository::SeedDefaultsIfEmpty() {
    static const wchar_t* kCountSql = L"SELECT COUNT(*) FROM pinned_items;";
    {
        ScopedStmt stmt;
        if (sqlite3_prepare16_v2(m_database->Handle(), kCountSql, -1, &stmt, nullptr) != SQLITE_OK) return;
        if (sqlite3_step(stmt) != SQLITE_ROW) return;
        if (sqlite3_column_int(stmt, 0) > 0) return;
    }

    Add(L"Study Plan");
    Add(L"Project Ideas");
}

} // namespace mosaic::data
