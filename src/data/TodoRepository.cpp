#include "data/TodoRepository.h"
#include <ctime>

namespace mosaic::data {

namespace {

// RAII wrapper so an early return (error path) can never leak a prepared
// statement — every method below has multiple early-out points.
struct ScopedStmt {
    sqlite3_stmt* stmt = nullptr;
    ~ScopedStmt() { if (stmt) sqlite3_finalize(stmt); }
    sqlite3_stmt** operator&() { return &stmt; }
    operator sqlite3_stmt* () const { return stmt; }
};

void BindText16(sqlite3_stmt* stmt, int index, const std::wstring& text) {
    // SQLITE_TRANSIENT tells sqlite3 to copy the bytes immediately, so it's
    // safe even though `text` may be destroyed before the statement runs.
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

std::vector<TaskRecord> TodoRepository::GetAll() {
    std::vector<TaskRecord> result;
    static const wchar_t* kSql =
        L"SELECT id, title, completed, sort_order FROM tasks ORDER BY sort_order ASC, id ASC;";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result; // empty — caller treats this the same as "no tasks yet"
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TaskRecord t;
        t.id = sqlite3_column_int64(stmt, 0);
        t.title = ColumnText16(stmt, 1);
        t.completed = sqlite3_column_int(stmt, 2) != 0;
        t.sortOrder = sqlite3_column_int(stmt, 3);
        result.push_back(std::move(t));
    }
    return result;
}

int64_t TodoRepository::Add(const std::wstring& title) {
    static const wchar_t* kSql =
        L"INSERT INTO tasks (title, completed, sort_order, created_at) "
        L"VALUES (?, 0, (SELECT IFNULL(MAX(sort_order), -1) + 1 FROM tasks), ?);";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }
    BindText16(stmt, 1, title);
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(std::time(nullptr)));

    if (sqlite3_step(stmt) != SQLITE_DONE) return 0;
    return sqlite3_last_insert_rowid(m_database->Handle());
}

void TodoRepository::SetCompleted(int64_t id, bool completed) {
    static const wchar_t* kSql = L"UPDATE tasks SET completed = ? WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, completed ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void TodoRepository::Rename(int64_t id, const std::wstring& newTitle) {
    static const wchar_t* kSql = L"UPDATE tasks SET title = ? WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    BindText16(stmt, 1, newTitle);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void TodoRepository::Remove(int64_t id) {
    static const wchar_t* kSql = L"DELETE FROM tasks WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
}

void TodoRepository::SetSortOrder(int64_t id, int newSortOrder) {
    static const wchar_t* kSql = L"UPDATE tasks SET sort_order = ? WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, newSortOrder);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void TodoRepository::SeedDefaultsIfEmpty() {
    static const wchar_t* kCountSql = L"SELECT COUNT(*) FROM tasks;";
    {
        ScopedStmt stmt;
        if (sqlite3_prepare16_v2(m_database->Handle(), kCountSql, -1, &stmt, nullptr) != SQLITE_OK) return;
        if (sqlite3_step(stmt) != SQLITE_ROW) return;
        if (sqlite3_column_int(stmt, 0) > 0) return; // already has data — never overwrite
    }

    // Matches the reference design's default screen so first launch isn't
    // an empty card.
    struct Seed { const wchar_t* title; bool completed; };
    static const Seed kSeeds[] = {
        { L"Complete DSA revision", true },
        { L"Finish Jarvis project", true },
        { L"Read 20 pages of a book", false },
        { L"Workout (30 mins)", false },
        { L"Prepare for next week", false },
        { L"Plan trip (Chennai to Pondicherry)", false },
    };
    for (const Seed& seed : kSeeds) {
        int64_t id = Add(seed.title);
        if (id != 0 && seed.completed) SetCompleted(id, true);
    }
}

} // namespace mosaic::data
