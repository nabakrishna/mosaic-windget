#include "app/AppSettings.h"
#include <algorithm>

namespace mosaic::app {

void AppSettings::LoadFrom(data::SettingsRepository& repo) {
    transparency = repo.GetFloat(L"appearance.transparency", transparency);
    cornerRadiusPreset = repo.GetInt(L"appearance.cornerRadiusPreset", cornerRadiusPreset);
    accentPreset = repo.GetInt(L"appearance.accentPreset", accentPreset);
    strongShadow = repo.GetBool(L"appearance.strongShadow", strongShadow);
    blurEnabled = repo.GetBool(L"appearance.blurEnabled", blurEnabled);

    static const wchar_t* kWidgetKeys[5] = {
        L"layout.widgetEnabled.todo", L"layout.widgetEnabled.photo", L"layout.widgetEnabled.activity",
        L"layout.widgetEnabled.pinned", L"layout.widgetEnabled.quicknotes"
    };
    for (int i = 0; i < 5; ++i) {
        widgetEnabled[i] = repo.GetBool(kWidgetKeys[i], widgetEnabled[i]);
    }

    photoRotationMinutes = repo.GetInt(L"photo.rotationMinutes", photoRotationMinutes);
    alwaysOnTop = repo.GetBool(L"desktop.alwaysOnTop", alwaysOnTop);
    startWithWindows = repo.GetBool(L"desktop.startWithWindows", startWithWindows);
    noteAutoLockSeconds = repo.GetInt(L"notes.autoLockSeconds", noteAutoLockSeconds);
    lockNotesOnFocusLoss = repo.GetBool(L"notes.lockOnFocusLoss", lockNotesOnFocusLoss);
    animationsEnabled = repo.GetBool(L"performance.animationsEnabled", animationsEnabled);
}

void AppSettings::SaveTo(data::SettingsRepository& repo) const {
    repo.SetFloat(L"appearance.transparency", transparency);
    repo.SetInt(L"appearance.cornerRadiusPreset", cornerRadiusPreset);
    repo.SetInt(L"appearance.accentPreset", accentPreset);
    repo.SetBool(L"appearance.strongShadow", strongShadow);
    repo.SetBool(L"appearance.blurEnabled", blurEnabled);

    static const wchar_t* kWidgetKeys[5] = {
        L"layout.widgetEnabled.todo", L"layout.widgetEnabled.photo", L"layout.widgetEnabled.activity",
        L"layout.widgetEnabled.pinned", L"layout.widgetEnabled.quicknotes"
    };
    for (int i = 0; i < 5; ++i) {
        repo.SetBool(kWidgetKeys[i], widgetEnabled[i]);
    }

    repo.SetInt(L"photo.rotationMinutes", photoRotationMinutes);
    repo.SetBool(L"desktop.alwaysOnTop", alwaysOnTop);
    repo.SetBool(L"desktop.startWithWindows", startWithWindows);
    repo.SetInt(L"notes.autoLockSeconds", noteAutoLockSeconds);
    repo.SetBool(L"notes.lockOnFocusLoss", lockNotesOnFocusLoss);
    repo.SetBool(L"performance.animationsEnabled", animationsEnabled);
}

void ApplyThemeSettings(const AppSettings& settings, ui::ThemeManager& theme) {
    ui::ThemeColors& colors = theme.MutableColors();
    ui::ThemeMetrics& metrics = theme.MutableMetrics();

    // Start from the built-in dark preset every time rather than mutating
    // whatever the previous settings left behind — otherwise repeated
    // changes would compound (e.g. hover alpha derived from an already-
    // modified fill alpha, drifting further each time).
    ui::ThemeManager base = ui::ThemeManager::CreateDark();
    colors = base.Colors();
    metrics = base.Metrics();

    // Transparency: the slider's 0..1 maps onto the card fill's alpha.
    // Clamped to a floor of 0.10 because a fully transparent card would
    // make text unreadable against an arbitrary wallpaper — the setting is
    // meant to tune the glass effect, not to hide the dashboard.
    float fillAlpha = 0.10f + settings.transparency * 0.85f;
    colors.cardFill.a = fillAlpha;
    colors.cardFillHover.a = (std::min)(1.0f, fillAlpha + 0.07f);

    static const float kCornerRadii[4] = { 0.0f, 8.0f, 18.0f, 28.0f };
    int cornerIndex = (settings.cornerRadiusPreset >= 0 && settings.cornerRadiusPreset < 4)
        ? settings.cornerRadiusPreset : 2;
    metrics.cardCornerRadius = kCornerRadii[cornerIndex];

    // Must stay in sync with SettingsPanel::DrawAppearance's swatch
    // palette — same order, same colors.
    static const D2D1_COLOR_F kAccentPalette[5] = {
        { 0.55f, 0.50f, 0.95f, 1.0f }, // violet (default)
        { 0.35f, 0.55f, 0.95f, 1.0f }, // blue
        { 0.42f, 0.85f, 0.55f, 1.0f }, // green
        { 0.95f, 0.75f, 0.40f, 1.0f }, // amber
        { 0.90f, 0.45f, 0.55f, 1.0f }, // rose
    };
    int accentIndex = (settings.accentPreset >= 0 && settings.accentPreset < 5) ? settings.accentPreset : 0;
    colors.accentViolet = kAccentPalette[accentIndex];

    if (settings.strongShadow) {
        metrics.shadowSpread = 18.0f;
        metrics.shadowMaxAlpha = 0.38f;
    }
}

} // namespace mosaic::app
