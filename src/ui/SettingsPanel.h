#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <functional>
#include <vector>
#include <string>
#include "ui/Theme.h"
#include "ui/components/IconButton.h"
#include "ui/components/Toggle.h"
#include "ui/components/Slider.h"
#include "app/AppSettings.h"

namespace mosaic::ui {

// Callbacks Window supplies so SettingsPanel never touches Win32 dialogs,
// the registry, or DWM directly — it only mutates the shared AppSettings
// struct and asks Window to apply/persist the result. Keeps this class
// reviewable as pure UI logic, same spirit as DashboardView's separation
// from Window.
struct SettingsCallbacks {
    std::function<void()> onSettingsChanged; // fires after ANY control changes a value — Window applies + persists
    std::function<void()> onResetLayout;
    std::function<void()> onPickPhotoFolder;
};

// The floating settings panel from spec sections 18–27, drawn as an
// in-window overlay (a dimmed scrim behind a glass panel) rather than a
// second top-level OS window — the spec itself calls this a "compact
// floating panel," not a second application, and an overlay avoids a
// second DirectComposition device chain, second DPI handling, and second
// message-routing path for what's fundamentally still one window's worth
// of UI.
//
// Six of the spec's twelve categories have real, working controls today:
// Appearance, Widget Layout, Photo & Media, Desktop, Performance, and
// About (which has nothing to configure — just information). The other
// six — Date & Greeting, To Do, Special Activity, Quick Notes, Privacy &
// Data, General — appear in the category list (so the panel's structure
// matches the spec) but show an honest "not available yet" message
// instead of controls that don't do anything. See README's Phase 7
// section for the full reasoning.
class SettingsPanel {
public:
    SettingsPanel(app::AppSettings* settings, SettingsCallbacks callbacks);

    bool IsOpen() const { return m_open; }
    void Open() { m_open = true; }
    void Close() { m_open = false; }

    void Draw(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush,
              IDWriteTextFormat* titleFormat, IDWriteTextFormat* bodyFormat, IDWriteTextFormat* smallFormat,
              D2D1_RECT_F fullWindowBounds, const ThemeManager& theme);

    // All return true if the panel consumed the input. Callers should only
    // forward events here while IsOpen() — this class assumes it owns all
    // input while visible (the dashboard underneath does not also react).
    bool OnMouseMove(D2D1_POINT_2F pt);
    bool OnLButtonDown(D2D1_POINT_2F pt);
    bool OnLButtonUp(D2D1_POINT_2F pt);
    bool OnKeyDown(unsigned int virtualKey); // Escape closes

private:
    enum class Category {
        Appearance, WidgetLayout, PhotoMedia, DateGreeting, Todo, SpecialActivity,
        QuickNotes, Desktop, Performance, Privacy, General, About
    };

    struct CategoryRow { Category id; std::wstring label; bool implemented; };
    struct ClickZone { D2D1_RECT_F rect; std::function<void()> onClick; };
    struct ToggleBinding { components::Toggle* toggle; std::function<void(bool)> onChange; };

    void DrawScrim(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, D2D1_RECT_F fullBounds, const ThemeManager& theme);
    void DrawCategoryList(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                           D2D1_RECT_F bounds, const ThemeManager& theme);
    void DrawNotImplementedNotice(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                                   D2D1_RECT_F bounds, const ThemeManager& theme);

    void DrawAppearance(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                         IDWriteTextFormat* smallFormat, D2D1_RECT_F bounds, const ThemeManager& theme);
    void DrawWidgetLayout(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                           D2D1_RECT_F bounds, const ThemeManager& theme);
    void DrawPhotoMedia(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                         IDWriteTextFormat* smallFormat, D2D1_RECT_F bounds, const ThemeManager& theme);
    void DrawDesktop(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                      D2D1_RECT_F bounds, const ThemeManager& theme);
    void DrawPerformance(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                          D2D1_RECT_F bounds, const ThemeManager& theme);
    void DrawQuickNotes(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                         IDWriteTextFormat* smallFormat, D2D1_RECT_F bounds, const ThemeManager& theme);
    void DrawAbout(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                    IDWriteTextFormat* smallFormat, D2D1_RECT_F bounds, const ThemeManager& theme);

    // One row: label on the left, a Toggle on the right. Registers the
    // toggle into m_toggleBindings so input dispatch picks it up.
    float DrawToggleRow(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* bodyFormat,
                         D2D1_RECT_F bounds, float y, const std::wstring& label,
                         components::Toggle& toggle, bool currentValue, std::function<void(bool)> onChange,
                         const ThemeManager& theme);

    void NotifyChanged() { if (m_callbacks.onSettingsChanged) m_callbacks.onSettingsChanged(); }

    app::AppSettings* m_settings;
    SettingsCallbacks m_callbacks;

    bool m_open = false;
    Category m_selectedCategory = Category::Appearance;

    D2D1_RECT_F m_panelBounds{};
    components::IconButton m_closeButton;
    std::vector<CategoryRow> m_categories;
    std::vector<D2D1_RECT_F> m_categoryRowBounds; // parallel to m_categories, cached each Draw

    // Per-category persistent control state (Toggle/Slider carry
    // hover/value state across frames, unlike the simple one-shot buttons
    // below which are stateless and rebuilt as click zones every Draw).
    components::Slider m_transparencySlider;
    components::Toggle m_blurToggle;
    components::Toggle m_strongShadowToggle;
    components::Toggle m_widgetToggles[5];
    components::Toggle m_alwaysOnTopToggle;
    components::Toggle m_startWithWindowsToggle;
    components::Toggle m_animationsToggle;
    components::Toggle m_lockOnFocusLossToggle;

    // Rebuilt at the end of every Draw() call to match whatever was just
    // drawn for the currently-selected category — the same "cache from
    // last frame" pattern DashboardView uses for its own hit-testing.
    std::vector<ClickZone> m_clickZones;
    std::vector<ToggleBinding> m_toggleBindings;
    components::Slider* m_activeSlider = nullptr; // non-null only while Appearance is selected
    std::function<void(float)> m_activeSliderOnChange;

    bool m_draggingSlider = false;
};

} // namespace mosaic::ui
