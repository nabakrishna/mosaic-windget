#include "widgets/ActivityDateTime.h"
#include <ctime>
#include <cwctype>
#include <algorithm>

namespace mosaic::widgets::activity_datetime {

namespace {

std::wstring Trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

bool StartsWithWord(const std::wstring& lower, const wchar_t* word, std::wstring& outRemainder) {
    size_t len = wcslen(word);
    if (lower.size() < len) return false;
    if (lower.compare(0, len, word) != 0) return false;
    // Require the match to end at a word boundary (whitespace or end of
    // string) so "todayxyz 6:00" doesn't false-match "today".
    if (lower.size() > len && !iswspace(lower[len])) return false;
    outRemainder = Trim(lower.substr(len));
    return true;
}

// Parses the "HH:MM" or "HH:MM am/pm" tail. Returns false on malformed
// input; on success, hour24 is in [0,23].
bool ParseTimeOfDay(const std::wstring& text, int& hour24, int& minute) {
    std::wstring t = Trim(text);
    size_t colon = t.find(L':');
    if (colon == std::wstring::npos || colon == 0) return false;

    std::wstring hourStr = t.substr(0, colon);
    if (hourStr.empty() || hourStr.size() > 2) return false;
    for (wchar_t c : hourStr) if (!iswdigit(c)) return false;

    size_t minuteStart = colon + 1;
    size_t minuteEnd = minuteStart;
    while (minuteEnd < t.size() && iswdigit(t[minuteEnd]) && (minuteEnd - minuteStart) < 2) ++minuteEnd;
    if (minuteEnd == minuteStart) return false;
    std::wstring minuteStr = t.substr(minuteStart, minuteEnd - minuteStart);

    int hour = std::stoi(hourStr);
    int minuteVal = std::stoi(minuteStr);
    if (minuteVal > 59) return false;

    std::wstring meridiem = ToLower(Trim(t.substr(minuteEnd)));
    if (!meridiem.empty()) {
        if (meridiem == L"am") {
            if (hour < 1 || hour > 12) return false;
            hour24 = (hour == 12) ? 0 : hour;
        } else if (meridiem == L"pm") {
            if (hour < 1 || hour > 12) return false;
            hour24 = (hour == 12) ? 12 : hour + 12;
        } else {
            return false; // unrecognized trailing text
        }
    } else {
        if (hour < 0 || hour > 23) return false;
        hour24 = hour;
    }
    minute = minuteVal;
    return true;
}

bool ParseIsoDate(const std::wstring& text, int& year, int& month, int& day, std::wstring& outRemainder) {
    // Expects "YYYY-MM-DD" at the start of `text`, followed by whitespace
    // and the time portion.
    if (text.size() < 10) return false;
    for (size_t i : { 0, 1, 2, 3, 5, 6, 8, 9 }) {
        if (!iswdigit(text[i])) return false;
    }
    if (text[4] != L'-' || text[7] != L'-') return false;

    year = std::stoi(text.substr(0, 4));
    month = std::stoi(text.substr(5, 2));
    day = std::stoi(text.substr(8, 2));
    if (month < 1 || month > 12 || day < 1 || day > 31) return false;

    outRemainder = Trim(text.substr(10));
    return true;
}

} // namespace

std::optional<int64_t> Parse(const std::wstring& input) {
    std::wstring trimmed = Trim(input);
    if (trimmed.empty()) return std::nullopt;
    std::wstring lower = ToLower(trimmed);

    std::time_t now = std::time(nullptr);
    std::tm base{};

    std::wstring timePart;
    bool haveDate = false;

    if (StartsWithWord(lower, L"today", timePart)) {
        localtime_s(&base, &now);
        haveDate = true;
    } else if (StartsWithWord(lower, L"tomorrow", timePart)) {
        localtime_s(&base, &now);
        base.tm_mday += 1;
        haveDate = true;
    } else {
        int year = 0, month = 0, day = 0;
        std::wstring remainder;
        if (ParseIsoDate(trimmed, year, month, day, remainder)) {
            localtime_s(&base, &now); // seed with today so unrelated fields (tm_isdst) are sane
            base.tm_year = year - 1900;
            base.tm_mon = month - 1;
            base.tm_mday = day;
            timePart = ToLower(remainder);
            haveDate = true;
        }
    }

    if (!haveDate) return std::nullopt;

    int hour24 = 0, minute = 0;
    if (!ParseTimeOfDay(timePart, hour24, minute)) return std::nullopt;

    base.tm_hour = hour24;
    base.tm_min = minute;
    base.tm_sec = 0;

    std::time_t result = std::mktime(&base); // normalizes overflowed tm_mday etc.
    if (result == -1) return std::nullopt;
    return static_cast<int64_t>(result);
}

namespace {

bool IsSameLocalDay(const std::tm& a, const std::tm& b) {
    return a.tm_year == b.tm_year && a.tm_mon == b.tm_mon && a.tm_mday == b.tm_mday;
}

std::wstring FormatTimeOfDay(const std::tm& t) {
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const wchar_t* meridiem = (t.tm_hour < 12) ? L"AM" : L"PM";
    wchar_t buf[16];
    swprintf_s(buf, L"%d:%02d %s", hour12, t.tm_min, meridiem);
    return buf;
}

} // namespace

std::wstring FormatForDisplay(int64_t epochSeconds) {
    std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm due{};
    localtime_s(&due, &t);

    std::time_t now = std::time(nullptr);
    std::tm today{};
    localtime_s(&today, &now);
    std::tm tomorrow = today;
    tomorrow.tm_mday += 1;
    std::mktime(&tomorrow); // normalize month/year rollover so the comparison below is correct

    std::wstring timeStr = FormatTimeOfDay(due);

    if (IsSameLocalDay(due, today)) return L"Today, " + timeStr;
    if (IsSameLocalDay(due, tomorrow)) return L"Tomorrow, " + timeStr;

    static const wchar_t* kMonth[] = {
        L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
    };
    wchar_t buf[64];
    swprintf_s(buf, L"%d %s, %s", due.tm_mday, kMonth[due.tm_mon], timeStr.c_str());
    return buf;
}

std::wstring FormatForEditing(int64_t epochSeconds) {
    std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm due{};
    localtime_s(&due, &t);

    std::time_t now = std::time(nullptr);
    std::tm today{};
    localtime_s(&today, &now);
    std::tm tomorrow = today;
    tomorrow.tm_mday += 1;
    std::mktime(&tomorrow);

    std::wstring timeStr = FormatTimeOfDay(due);
    // Lowercase the AM/PM to match Parse's accepted grammar's usual case;
    // Parse itself is case-insensitive either way, this is purely cosmetic
    // for what the user sees sitting in the edit field.
    std::transform(timeStr.begin(), timeStr.end(), timeStr.begin(),
                    [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

    if (IsSameLocalDay(due, today)) return L"today " + timeStr;
    if (IsSameLocalDay(due, tomorrow)) return L"tomorrow " + timeStr;

    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d %s", due.tm_year + 1900, due.tm_mon + 1, due.tm_mday, timeStr.c_str());
    return buf;
}

} // namespace mosaic::widgets::activity_datetime
