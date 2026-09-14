#pragma once
#include <windows.h>
#include <string>
#include "sqlite3.h"

namespace mosaic::data {

// Owns exactly one sqlite3 connection to Mosaic's local database, stored at
// %LOCALAPPDATA%\Mosaic\mosaic.db (a real per-user Windows location, not a
// path relative to the executable — the app should work the same way
// whether launched from Explorer, a shortcut, or Task Scheduler at
// startup). Local-first per spec section 40: nothing here ever touches the
// network.
class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Opens (creating if necessary) the database file and its directory,
    // then runs schema creation. Safe to call once at startup.
    HRESULT Open();

    sqlite3* Handle() const { return m_db; }

private:
    HRESULT EnsureDataDirectory(std::wstring& outDbPath);
    HRESULT RunMigrations();

    sqlite3* m_db = nullptr;
};

} // namespace mosaic::data
