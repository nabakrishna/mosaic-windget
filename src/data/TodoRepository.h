#pragma once
#include <vector>
#include "data/Database.h"
#include "data/Models.h"

namespace mosaic::data {

// All real To Do persistence goes through here — TodoWidget never touches
// sqlite3 directly. Every query is parameterized (sqlite3_bind_*), never
// string-concatenated, even though today's only input source is the
// dashboard's own text box: a task title is still untrusted user input.
class TodoRepository {
public:
    explicit TodoRepository(Database* database) : m_database(database) {}

    // Ordered by sort_order ascending (manual ordering — see spec section
    // 23's "Sorting: Manual" default). Returns an empty vector on failure
    // rather than throwing; callers already treat "no tasks" as valid.
    std::vector<TaskRecord> GetAll();

    // Inserts at the end (max(sort_order) + 1) and returns the new row's
    // id, or 0 on failure.
    int64_t Add(const std::wstring& title);

    void SetCompleted(int64_t id, bool completed);
    void Rename(int64_t id, const std::wstring& newTitle);
    void Remove(int64_t id);

    // Explicit reordering (drag-to-reorder within the list) is deferred to
    // Phase 6 alongside the grid's drag machinery — see TodoWidget.h for
    // why. sort_order already exists in the schema so that phase only adds
    // behavior, not a migration.
    void SetSortOrder(int64_t id, int newSortOrder);

    // Populates the reference design's sample tasks, but only the very
    // first time the app runs (i.e. only if the table is empty) — repeat
    // launches must never resurrect deleted tasks.
    void SeedDefaultsIfEmpty();

private:
    Database* m_database;
};

} // namespace mosaic::data
