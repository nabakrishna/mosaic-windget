#include "data/Database.h"
#include <shlobj.h>
#include <filesystem>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace mosaic::data {

Database::~Database() {
    if (m_db) sqlite3_close(m_db);
}

HRESULT Database::EnsureDataDirectory(std::wstring& outDbPath) {
    PWSTR localAppData = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData);
    if (FAILED(hr)) return hr;

    std::filesystem::path dir = std::filesystem::path(localAppData) / L"Mosaic";
    CoTaskMemFree(localAppData);

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        // Directory couldn't be created (permissions, disk full, etc).
        // Per spec section 60, a storage failure must never crash the
        // dashboard — the caller (Application::Run) treats a failed Open()
        // as "run without persistence this session" rather than exiting.
        return HRESULT_FROM_WIN32(ec.value());
    }

    outDbPath = (dir / L"mosaic.db").wstring();
    return S_OK;
}

HRESULT Database::Open() {
    std::wstring dbPath;
    HRESULT hr = EnsureDataDirectory(dbPath);
    if (FAILED(hr)) return hr;

    // sqlite3_open16 takes the filename as UTF-16 directly — no manual
    // UTF-8 conversion needed for a path that may contain non-ASCII
    // characters in the Windows username.
    int rc = sqlite3_open16(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return E_FAIL;
    }

    // A busy timeout means a second Mosaic process (there shouldn't be one,
    // but tray-icon relaunch races are exactly the kind of thing that
    // happens in practice) waits briefly instead of failing immediately.
    sqlite3_busy_timeout(m_db, 2000);

    return RunMigrations();
}

HRESULT Database::RunMigrations() {
    static const char* kSchema =
        "CREATE TABLE IF NOT EXISTS tasks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  completed INTEGER NOT NULL DEFAULT 0,"
        "  sort_order INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS activities ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  due_at INTEGER NOT NULL,"
        "  reminder_offset_seconds INTEGER NOT NULL DEFAULT 0,"
        "  completed INTEGER NOT NULL DEFAULT 0,"
        "  notified INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS pinned_items ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  subtitle TEXT NOT NULL DEFAULT '',"
        "  sort_order INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS widget_layout ("
        "  widget_id INTEGER PRIMARY KEY,"
        "  grid_col INTEGER NOT NULL,"
        "  grid_row INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");"
        // Single-row table (id is always 1). `content` holds DPAPI
        // ciphertext as a BLOB — plaintext note text is never written to
        // this database. password_salt/password_hash are NULL until the
        // user sets a password; the hash is PBKDF2-SHA256, never the
        // password itself. See security/NoteCrypto.h.
        "CREATE TABLE IF NOT EXISTS secure_notes ("
        "  id INTEGER PRIMARY KEY,"
        "  content BLOB,"
        "  password_salt BLOB,"
        "  password_hash BLOB,"
        "  updated_at INTEGER NOT NULL DEFAULT 0"
        ");";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, kSchema, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        return E_FAIL;
    }
    return S_OK;
}

} // namespace mosaic::data
