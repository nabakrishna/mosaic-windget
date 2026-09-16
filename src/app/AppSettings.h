#pragma once
#include "data/SettingsRepository.h"
#include "widgets/IWidget.h"
#include "ui/Theme.h"

namespace mosaic::app {

// Every setting Mosaic currently has real, functional behavior for. This
// covers 6 of spec sections 19–27's 12 categories (Appearance, Widget
// Layout, Photo & Media, Desktop, Performance, plus About which has
// nothing to store) — see README's Phase 7 notes for exactly which
// categories are and aren't wired up, and why. Loaded once at startup,
// mutated in place as Settings controls change, and immediately persisted
// back to SettingsRepository on every change — there's no separate
// "unsaved changes" state or explicit Save button, matching how every
// other real control in Mosaic (To Do, Activity, layout position) already
// behaves.
struct AppSettings {
    // --- Appearance ---------------------------------------------------
    float transparency = 0.55f;      // maps directly to ThemeColors::cardFill.a
    int cornerRadiusPreset = 2;      // 0=Sharp 1=Small 2=Medium 3=Large
    int accentPreset = 0;            // index into SettingsPanel's fixed palette
    bool strongShadow = false;
    bool blurEnabled = true;         // DWM system backdrop on/off

    // --- Widget Layout --------------------------------------------------
    bool widgetEnabled[5] = { true, true, true, true, true }; // indexed by WidgetId

    // --- Photo & Media --------------------------------------------------
    int photoRotationMinutes = 5;

    // --- Desktop ---------------------------------------------------
    bool alwaysOnTop = false;
    bool startWithWindows = false;

    // --- Performance -----------------------------------------------
    bool animationsEnabled = true;

    bool IsWidgetEnabled(widgets::WidgetId id) const {
        int index = static_cast<int>(id);
        return (index >= 0 && index < 5) ? widgetEnabled[index] : true;
    }
    void SetWidgetEnabled(widgets::WidgetId id, bool enabled) {
        int index = static_cast<int>(id);
        if (index >= 0 && index < 5) widgetEnabled[index] = enabled;
    }

    // Loads every value from `repo`, falling back to the struct's own
    // defaults (already set above) for anything never saved before —
    // exactly the "fresh install" case.
    void LoadFrom(data::SettingsRepository& repo);
    void SaveTo(data::SettingsRepository& repo) const;
};

// Maps `settings`' appearance values onto a live ThemeManager, in place.
// Called once at startup and again after every Appearance change, so the
// dashboard reflects the new look on the very next repaint without
// rebuilding any device resources (colors and metrics are read fresh from
// the theme on every Draw; only text formats are cached, and none of these
// settings touch typography).
void ApplyThemeSettings(const AppSettings& settings, ui::ThemeManager& theme);

} // namespace mosaic::app
