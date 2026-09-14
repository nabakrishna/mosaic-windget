#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <map>
#include "ui/Theme.h"
#include "ui/components/IconButton.h"
#include "widgets/IWidget.h"
#include "widgets/WidgetManager.h"

namespace mosaic::ui {

// Only the header (greeting/date/settings button) lives here now — it
// isn't a widget in the spec's sense (it's not draggable, resizable, or
// optional). Everything else in DashboardSampleData up through Phase 2
// (todos, activity, pinned items) has moved into the widgets themselves
// (see src/widgets/) as of Phase 3, since that's real, persisted, or
// at least individually-owned data now rather than one shared blob.
struct DashboardHeaderData {
    std::wstring greetingLine;   // "Good Afternoon,"
    std::wstring userName;       // "Naba"
    std::wstring motivation;     // "Keep going, great things take time."
    std::wstring weekday;        // "Tue"
    std::wstring fullDate;       // "9 Sep 2026"
};

// Computes the fixed grid layout (Phase 6 replaces this with the real
// adaptive LayoutEngine), draws the header, and dispatches Render() plus
// mouse/keyboard input to whichever IWidget owns the rect the pointer is
// over. DashboardView itself no longer knows anything about to-do items,
// activities, or pins — it only knows "there are 5 widget slots in this
// arrangement" and "here's who's focused right now".
class DashboardView {
public:
    DashboardView(IDWriteFactory* dwriteFactory, const ThemeManager* theme, widgets::WidgetManager* widgetManager);

    HRESULT CreateDeviceResources(ID2D1DeviceContext* context);
    void ReleaseDeviceResources();

    void Draw(ID2D1DeviceContext* context, D2D1_RECT_F bounds, const DashboardHeaderData& header);

    // Pointer/keyboard events, all in DIPs for pointer position. Each
    // returns true if a repaint is warranted.
    bool OnMouseMove(D2D1_POINT_2F pointerDip);
    bool OnMouseLeave();
    bool OnLButtonDown(D2D1_POINT_2F pointerDip);
    bool OnDoubleClick(D2D1_POINT_2F pointerDip);
    bool OnChar(wchar_t ch);
    bool OnKeyDown(unsigned int virtualKey);

    bool IsPointerOverSettingsButton() const { return m_gearButton.IsHovered(); }

private:
    void DrawHeader(ID2D1DeviceContext* ctx, D2D1_RECT_F rowBounds, const DashboardHeaderData& data);

    // Finds which widget's last-drawn rect contains `pt`, or nullptr.
    widgets::IWidget* WidgetAt(D2D1_POINT_2F pt) const;

    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    const ThemeManager* m_theme;              // non-owning; Window owns it
    widgets::WidgetManager* m_widgetManager;  // non-owning; Window owns it

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushSolid;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtGreetingName;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtGreetingSub;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtDateDay;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtDateFull;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtCardTitle;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtBody;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtSmall;

    components::IconButton m_gearButton;

    // Rebuilt every Draw() call; used by input dispatch to map a pointer
    // position to a widget without recomputing the grid math on every move.
    std::map<widgets::WidgetId, D2D1_RECT_F> m_widgetBounds;

    // Which widget most recently had the pointer over it — needed so we
    // can send it a clean OnMouseLeave when the pointer moves to a
    // different widget or off the dashboard entirely.
    widgets::WidgetId m_hoveredWidgetId = widgets::WidgetId::Todo;
    bool m_anyWidgetHovered = false;

    // Which widget most recently received a click — keyboard input (typing
    // while editing a to-do title) goes to this one regardless of current
    // mouse position.
    widgets::IWidget* m_focusedWidget = nullptr;
};

} // namespace mosaic::ui
