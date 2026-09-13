#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include "ui/Theme.h"
#include "ui/components/IconButton.h"

namespace mosaic::ui {

// Placeholder data shapes for Phase 1/2. These get replaced by the real
// models from Data/Models in Phase 3 — kept here only so the renderer has
// something concrete to draw while we prove out the visual pipeline.
struct TodoItemView {
    std::wstring title;
    bool completed = false;
};

struct DashboardSampleData {
    std::wstring greetingLine;   // "Good Afternoon,"
    std::wstring userName;       // "Naba"
    std::wstring motivation;     // "Keep going, great things take time."
    std::wstring weekday;        // "Tue"
    std::wstring fullDate;       // "9 Sep 2026"
    std::vector<TodoItemView> todos;
    std::wstring activityTitle;    // "Gym Session"
    std::wstring activityWhen;     // "Tomorrow, 6:00 PM"
    std::wstring activityNote;     // small motivational sub-line
    std::vector<std::wstring> pinnedItems;
};

// Renders the whole dashboard (header + five widget cards) into the given
// device context, laid out inside `bounds` (DIPs, already DPI-adjusted by
// the caller). Phase 2 change from Phase 1: card chrome now goes through
// the shared GlassPanel/WidgetCard components and real vector Icons instead
// of hand-drawn rounded rects and colored dots, and the header's settings
// button is a real, hoverable components::IconButton rather than a static
// placeholder circle. Still no layout engine (Phase 6) and no per-widget
// classes (Phase 3) — this remains one "dumb" immediate-mode view.
class DashboardView {
public:
    DashboardView(IDWriteFactory* dwriteFactory, const ThemeManager* theme);

    HRESULT CreateDeviceResources(ID2D1DeviceContext* context);
    void ReleaseDeviceResources();

    void Draw(ID2D1DeviceContext* context, D2D1_RECT_F bounds, const DashboardSampleData& data);

    // Called from Window on WM_MOUSEMOVE/WM_MOUSELEAVE with the pointer
    // position in DIPs. Returns true if any hoverable element's visual
    // state changed, so the caller knows whether a repaint is warranted —
    // we do not want to invalidate on every mouse-move, only ones that
    // actually change what's on screen (spec section 67: event-driven,
    // don't redraw when nothing changed).
    bool UpdateHover(D2D1_POINT_2F pointerDip);

    // True if the last UpdateHover'd point was over the settings button.
    // Window uses this on WM_LBUTTONDOWN; Phase 7 wires the actual click
    // to opening the Settings panel.
    bool IsPointerOverSettingsButton() const { return m_gearButton.IsHovered(); }

private:
    void DrawHeader(ID2D1DeviceContext* ctx, D2D1_RECT_F rowBounds, const DashboardSampleData& data);
    void DrawTodoCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect, const DashboardSampleData& data);
    void DrawActivityCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect, const DashboardSampleData& data);
    void DrawPinnedCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect, const DashboardSampleData& data);
    void DrawQuickNotesCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect);
    void DrawPhotoCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect);

    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    const ThemeManager* m_theme; // non-owning; Window owns the ThemeManager instance

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushSolid; // color set per-draw via SetColor

    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtGreetingName;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtGreetingSub;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtDateDay;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtDateFull;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtCardTitle;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtBody;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtSmall;

    components::IconButton m_gearButton;
};

} // namespace mosaic::ui
