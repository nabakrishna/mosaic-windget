#pragma once
#include <vector>
#include "data/Database.h"
#include "data/Models.h"

namespace mosaic::data {

// Pinned-item persistence. `subtitle` defaults to "Pinned on desktop" for
// every new item today — see Models.h's PinnedRecord comment on why that's
// a placeholder rather than a real field yet.
class PinnedRepository {
public:
    explicit PinnedRepository(Database* database) : m_database(database) {}

    std::vector<PinnedRecord> GetAll(); // ordered by sort_order ascending
    int64_t Add(const std::wstring& title);
    void Rename(int64_t id, const std::wstring& newTitle);
    void Remove(int64_t id);
    void SetSortOrder(int64_t id, int newSortOrder); // ready for Phase 6's drag reorder

    void SeedDefaultsIfEmpty();

private:
    Database* m_database;
};

} // namespace mosaic::data
