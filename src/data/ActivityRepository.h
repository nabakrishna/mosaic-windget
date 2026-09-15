#pragma once
#include <vector>
#include "data/Database.h"
#include "data/Models.h"

namespace mosaic::data {

// All Special Activity persistence. Same shape as TodoRepository: every
// query parameterized, ActivityWidget never touches sqlite3 directly.
class ActivityRepository {
public:
    explicit ActivityRepository(Database* database) : m_database(database) {}

    // Not-completed activities, soonest first. This is what the widget
    // renders — an activity is time-bound rather than recurring, so
    // "mark complete" more naturally means "off the upcoming list"
    // entirely (spec section 12), unlike To Do's dimmed-but-visible
    // completed items.
    std::vector<ActivityRecord> GetUpcoming();

    int64_t Add(const std::wstring& title, int64_t dueAt, int64_t reminderOffsetSeconds);
    void Rename(int64_t id, const std::wstring& newTitle);
    void Reschedule(int64_t id, int64_t newDueAt); // also clears `notified` so a moved reminder can fire again
    void SetCompleted(int64_t id, bool completed);
    void Remove(int64_t id);
    void MarkNotified(int64_t id);

    // Activities whose reminder moment (due_at - reminder_offset_seconds)
    // has passed, aren't completed, and haven't already been notified.
    // The scheduler (Window's once-a-minute timer) calls this, fires a
    // toast per result, then calls MarkNotified so it never fires twice.
    std::vector<ActivityRecord> GetDueForNotification(int64_t nowEpoch);

    // First-run sample data matching the reference design, only if the
    // table is empty (never resurrects something the user deleted).
    void SeedDefaultIfEmpty();

private:
    Database* m_database;
};

} // namespace mosaic::data
