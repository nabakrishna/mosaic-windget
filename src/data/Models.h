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

} // namespace mosaic::data
