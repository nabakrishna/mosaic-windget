#pragma once
#include <string>
#include "data/Database.h"

namespace mosaic::data {

// A generic string key-value store, backed by one `settings` table. Every
// setting Mosaic has — today's and any future one — goes through this
// rather than each getting a bespoke column somewhere, so adding a new
// setting later never means a schema migration.
class SettingsRepository {
public:
    explicit SettingsRepository(Database* database) : m_database(database) {}

    std::wstring GetString(const std::wstring& key, const std::wstring& defaultValue);
    void SetString(const std::wstring& key, const std::wstring& value);

    int GetInt(const std::wstring& key, int defaultValue);
    void SetInt(const std::wstring& key, int value);

    bool GetBool(const std::wstring& key, bool defaultValue);
    void SetBool(const std::wstring& key, bool value);

    float GetFloat(const std::wstring& key, float defaultValue);
    void SetFloat(const std::wstring& key, float value);

private:
    Database* m_database;
};

} // namespace mosaic::data
