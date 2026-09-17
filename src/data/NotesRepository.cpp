#include "data/NotesRepository.h"
#include "security/NoteCrypto.h"
#include <ctime>

namespace mosaic::data {

namespace {

struct ScopedStmt {
    sqlite3_stmt* stmt = nullptr;
    ~ScopedStmt() { if (stmt) sqlite3_finalize(stmt); }
    sqlite3_stmt** operator&() { return &stmt; }
    operator sqlite3_stmt* () const { return stmt; }
};

std::vector<uint8_t> ColumnBlob(sqlite3_stmt* stmt, int index) {
    const void* data = sqlite3_column_blob(stmt, index);
    int bytes = sqlite3_column_bytes(stmt, index);
    if (!data || bytes <= 0) return {};
    const auto* begin = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(begin, begin + bytes);
}

} // namespace

void NotesRepository::EnsureRow() {
    static const wchar_t* kSql = L"INSERT OR IGNORE INTO secure_notes (id, updated_at) VALUES (1, 0);";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_step(stmt);
}

bool NotesRepository::LoadNote(std::wstring& outPlaintext) {
    static const wchar_t* kSql = L"SELECT content FROM secure_notes WHERE id = 1;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    if (sqlite3_step(stmt) != SQLITE_ROW) return false;

    std::vector<uint8_t> blob = ColumnBlob(stmt, 0);
    if (blob.empty()) return false;

    return security::NoteCrypto::Decrypt(blob, outPlaintext);
}

bool NotesRepository::SaveNote(const std::wstring& plaintext) {
    std::vector<uint8_t> blob = security::NoteCrypto::Encrypt(plaintext);
    if (blob.empty() && !plaintext.empty()) {
        // Encryption failed on non-empty input — refuse to write rather
        // than clobber the previous good ciphertext with nothing.
        return false;
    }

    EnsureRow();

    static const wchar_t* kSql = L"UPDATE secure_notes SET content = ?, updated_at = ? WHERE id = 1;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    if (blob.empty()) {
        sqlite3_bind_null(stmt, 1);
    } else {
        sqlite3_bind_blob(stmt, 1, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(std::time(nullptr)));
    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool NotesRepository::HasPassword() {
    static const wchar_t* kSql = L"SELECT password_hash FROM secure_notes WHERE id = 1;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    if (sqlite3_step(stmt) != SQLITE_ROW) return false;
    return !ColumnBlob(stmt, 0).empty();
}

bool NotesRepository::SetPassword(const std::wstring& password) {
    std::vector<uint8_t> salt, hash;
    if (!security::NoteCrypto::HashPassword(password, salt, hash)) return false;

    EnsureRow();

    static const wchar_t* kSql = L"UPDATE secure_notes SET password_salt = ?, password_hash = ? WHERE id = 1;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_blob(stmt, 1, salt.data(), static_cast<int>(salt.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);
    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool NotesRepository::VerifyPassword(const std::wstring& password) {
    static const wchar_t* kSql = L"SELECT password_salt, password_hash FROM secure_notes WHERE id = 1;";
    ScopedStmt stmt;
    if (sqlite3_prepare16_v2(m_database->Handle(), kSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    if (sqlite3_step(stmt) != SQLITE_ROW) return false;

    std::vector<uint8_t> salt = ColumnBlob(stmt, 0);
    std::vector<uint8_t> hash = ColumnBlob(stmt, 1);
    return security::NoteCrypto::VerifyPassword(password, salt, hash);
}

} // namespace mosaic::data
