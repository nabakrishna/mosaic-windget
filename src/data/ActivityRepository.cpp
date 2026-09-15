#include "data/ActivityRepository.h"
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

ActivityRecord ReadRow(sqlite3_stmt* stmt) {
    ActivityRecord a;
    a.id = sqlite3_column_int64(stmt, 0);
    a.title = ColumnText16(stmt, 1);
    a.dueAt = sqlite3_column_int64(stmt, 2);
    a.reminderOffsetSeconds = sqlite3_column_int64(stmt, 3);
    a.completed = sqlite3_column_int(stmt, 4) != 0;
    a.notified = sqlite3_column_int(stmt, 5) != 0;
    return a;
}

} // namespace

std::vector<ActivityRecord> ActivityRepository::GetUpcoming() {
    std::vector<ActivityRecord> result;
    static const wchar_t* kSql =
        L"SELECT id, title, due_at, reminder_offset_seconds, completed, notified "
        L"FROM activities WHERE completed = 0 ORDER BY due_at ASC;";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(ReadRow(stmt));
    }
    return result;
}

int64_t ActivityRepository::Add(const std::wstring& title, int64_t dueAt, int64_t reminderOffsetSeconds) {
    static const wchar_t* kSql =
        L"INSERT INTO activities (title, due_at, reminder_offset_seconds, completed, notified, created_at) "
        L"VALUES (?, ?, ?, 0, 0, ?);";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return 0;
    BindText16(stmt, 1, title);
    sqlite3_bind_int64(stmt, 2, dueAt);
    sqlite3_bind_int64(stmt, 3, reminderOffsetSeconds);
    sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(std::time(nullptr)));

    if (sqlite3_step(stmt) != SQLITE_DONE) return 0;
    return sqlite3_last_insert_rowid(m_database->Handle());
}

void ActivityRepository::Rename(int64_t id, const std::wstring& newTitle) {
    static const wchar_t* kSql = L"UPDATE activities SET title = ? WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    BindText16(stmt, 1, newTitle);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void ActivityRepository::Reschedule(int64_t id, int64_t newDueAt) {
    // Clearing `notified` here is deliberate: if the user moves an
    // activity's time, a reminder that already fired for the *old* time
    // should be eligible to fire again for the new one.
    static const wchar_t* kSql = L"UPDATE activities SET due_at = ?, notified = 0 WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int64(stmt, 1, newDueAt);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void ActivityRepository::SetCompleted(int64_t id, bool completed) {
    static const wchar_t* kSql = L"UPDATE activities SET completed = ? WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(stmt, 1, completed ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
}

void ActivityRepository::Remove(int64_t id) {
    static const wchar_t* kSql = L"DELETE FROM activities WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
}

void ActivityRepository::MarkNotified(int64_t id) {
    static const wchar_t* kSql = L"UPDATE activities SET notified = 1 WHERE id = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
}

std::vector<ActivityRecord> ActivityRepository::GetDueForNotification(int64_t nowEpoch) {
    std::vector<ActivityRecord> result;
    static const wchar_t* kSql =
        L"SELECT id, title, due_at, reminder_offset_seconds, completed, notified "
        L"FROM activities "
        L"WHERE completed = 0 AND notified = 0 AND (due_at - reminder_offset_seconds) <= ? "
        L"ORDER BY due_at ASC;";

    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    sqlite3_bind_int64(stmt, 1, nowEpoch);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(ReadRow(stmt));
    }
    return result;
}

void ActivityRepository::SeedDefaultIfEmpty() {
    static const wchar_t* kCountSql = L"SELECT COUNT(*) FROM activities;";
    {
        ScopedStmt stmt;
        if (sqlite3_prepare16_v2(m_database->Handle(), kCountSql, -1, &stmt, nullptr) != SQLITE_OK) return;
        if (sqlite3_step(stmt) != SQLITE_ROW) return;
        if (sqlite3_column_int(stmt, 0) > 0) return;
    }

    // Matches the reference design: "Gym Session — Tomorrow, 6:00 PM".
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    local.tm_mday += 1; // tomorrow
    local.tm_hour = 18;
    local.tm_min = 0;
    local.tm_sec = 0;
    std::time_t tomorrow6pm = std::mktime(&local); // mktime normalizes month/day rollover correctly

    Add(L"Gym Session", static_cast<int64_t>(tomorrow6pm), 0);
}

} // namespace mosaic::data
