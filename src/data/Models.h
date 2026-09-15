#pragma once
#include <cstdint>
#include <string>

namespace mosaic::data {

// Mirrors the `tasks` table (see Database::Open's schema). Deliberately a
// plain struct with no behavior — persistence lives in TodoRepository, not
// on the model, so widgets can hold these by value freely.
struct TaskRecord {
    int64_t id = 0;
    std::wstring title;
    bool completed = false;
    int sortOrder = 0;
};

// Mirrors the `activities` table. `dueAt` is a local-wall-clock epoch value
// (produced by `mktime` on a local `tm`, compared against `time(nullptr)` —
// both are ordinary UTC epoch seconds under the hood, so no explicit
// timezone math is needed anywhere that touches this field).
struct ActivityRecord {
    int64_t id = 0;
    std::wstring title;
    int64_t dueAt = 0;
    int64_t reminderOffsetSeconds = 0; // 0 = remind at the event's own time
    bool completed = false;
    bool notified = false; // true once the toast for this reminder has fired
};

// Mirrors the `pinned_items` table. `subtitle` is fixed to "Pinned on
// desktop" today (matching the reference design) rather than a free field —
// it becomes meaningful once pinned items can represent an actual file,
// folder, or link (spec section 13) with type-specific subtitles.
struct PinnedRecord {
    int64_t id = 0;
    std::wstring title;
    std::wstring subtitle;
    int sortOrder = 0;
};

} // namespace mosaic::data
