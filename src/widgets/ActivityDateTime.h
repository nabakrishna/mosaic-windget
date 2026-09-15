#pragma once
#include <string>
#include <cstdint>
#include <optional>

namespace mosaic::widgets::activity_datetime {

// Special Activity needs a real absolute timestamp to compute a real
// reminder delay — the reference design's "Tomorrow, 6:00 PM" is a display
// format, not something a notification scheduler can compute from. Rather
// than building a full calendar/clock picker UI (a substantial feature on
// its own, and one that doesn't fit through the same caret-based inline
// text editing To Do already uses), Special Activity accepts a small,
// forgiving typed grammar and displays it back in the reference design's
// friendly relative style. This is a deliberate, documented scope
// boundary — not a stand-in for a picker that's secretly missing; typing
// "tomorrow 6:00 pm" is a real, complete way to schedule a real reminder,
// just not a mouse-driven one.
//
// Accepted input (case-insensitive, extra whitespace tolerated):
//   today HH:MM[am|pm]
//   tomorrow HH:MM[am|pm]
//   YYYY-MM-DD HH:MM[am|pm]
// Examples: "today 6:00 pm", "tomorrow 18:00", "2026-09-18 11:59 pm"
//
// Returns nullopt on anything that doesn't match — callers keep the field
// in edit mode rather than committing garbage.
std::optional<int64_t> Parse(const std::wstring& input);

// The reverse of Parse, but not a strict round-trip: formats `epochSeconds`
// (local wall-clock, same convention as Parse's output) into the
// reference design's display style — "Today, 6:00 PM", "Tomorrow, 6:00 PM",
// or "18 Sep, 11:59 PM" for anything further out.
std::wstring FormatForDisplay(int64_t epochSeconds);

// Formats back into the same grammar Parse accepts, so editing an existing
// activity's date starts from a string that round-trips through Parse
// unchanged if left untouched.
std::wstring FormatForEditing(int64_t epochSeconds);

} // namespace mosaic::widgets::activity_datetime
