#include "data/SettingsRepository.h"
#include <cstdlib>
#include <cwchar>

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

std::wstring SettingsRepository::GetString(const std::wstring& key, const std::wstring& defaultValue) {
    static const wchar_t* kSql = L"SELECT value FROM settings WHERE key = ?;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return defaultValue;
    }
    BindText16(stmt, 1, key);
    if (sqlite3_step(stmt) != SQLITE_ROW) return defaultValue;
    return ColumnText16(stmt, 0);
}

void SettingsRepository::SetString(const std::wstring& key, const std::wstring& value) {
    static const wchar_t* kSql =
        L"INSERT INTO settings (key, value) VALUES (?, ?) "
        L"ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    BindText16(stmt, 1, key);
    BindText16(stmt, 2, value);
    sqlite3_step(stmt);
}

int SettingsRepository::GetInt(const std::wstring& key, int defaultValue) {
    std::wstring raw = GetString(key, L"");
    if (raw.empty()) return defaultValue;
    return static_cast<int>(std::wcstol(raw.c_str(), nullptr, 10));
}

void SettingsRepository::SetInt(const std::wstring& key, int value) {
    SetString(key, std::to_wstring(value));
}

bool SettingsRepository::GetBool(const std::wstring& key, bool defaultValue) {
    std::wstring raw = GetString(key, L"");
    if (raw.empty()) return defaultValue;
    return raw == L"1";
}

void SettingsRepository::SetBool(const std::wstring& key, bool value) {
    SetString(key, value ? L"1" : L"0");
}

float SettingsRepository::GetFloat(const std::wstring& key, float defaultValue) {
    std::wstring raw = GetString(key, L"");
    if (raw.empty()) return defaultValue;
    return std::wcstof(raw.c_str(), nullptr);
}

void SettingsRepository::SetFloat(const std::wstring& key, float value) {
    SetString(key, std::to_wstring(value));
}

} // namespace mosaic::data
