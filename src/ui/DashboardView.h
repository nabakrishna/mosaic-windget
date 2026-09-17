#pragma once
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <map>
#include <optional>
#include "ui/Theme.h"
#include "ui/components/IconButton.h"
#include "ui/SettingsPanel.h"
#include "widgets/IWidget.h"
#include "widgets/WidgetManager.h"
#include "layout/LayoutEngine.h"
#include "data/LayoutRepository.h"
#include "app/AppSettings.h"

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

// As of Phase 6, DashboardView owns the real adaptive grid: it runs the
// LayoutEngine to turn grid-unit placements into DIP rects, persists
// layout changes through LayoutRepository, and implements whole-widget
// drag-and-drop directly (rather than pushing that state machine up into
// Window, which stays a thin input-forwarding shell).
//
// The click-vs-drag problem: a mouse-down on a card could mean "I want to
// click the checkbox under my cursor" or "I want to drag this whole card
// somewhere else." This is resolved the standard way — a mouse-down never
// immediately dispatches a click. Instead:
//   - OnLButtonDown remembers which widget was hit and enters a "pending"
//     state, but does NOT call the widget's OnLButtonDown yet.
//   - OnMouseMove, while pending, watches for movement past a small
//     threshold. Cross it, and this becomes a real drag: the pending
//     click is abandoned entirely (the widget never sees it), and every
//     further mouse-move updates a live drag preview via LayoutEngine.
//   - OnLButtonUp is where it resolves: if a drag was happening, the drop
//     is committed and persisted. If it never crossed the threshold, the
//     original click is dispatched *now*, to whatever widget was
//     originally hit — checkboxes, delete buttons, and everything else
//     work exactly as before, just resolved on release instead of press.
//
// As of Phase 7, DashboardView also owns the SettingsPanel — clicking the
// gear button opens it, and every input method here checks
// m_settingsPanel.IsOpen() first and routes there instead of normal
// dashboard processing when it's true. The panel behaves modally: while
// open, drags, clicks, and keystrokes all go to it, never to the widgets
// underneath.
class DashboardView {
public:
    DashboardView(IDWriteFactory* dwriteFactory, const ThemeManager* theme,
                  widgets::WidgetManager* widgetManager, data::LayoutRepository* layoutRepository,
                  app::AppSettings* appSettings, SettingsCallbacks settingsCallbacks);

    HRESULT CreateDeviceResources(ID2D1DeviceContext* context);
    void ReleaseDeviceResources();

    void Draw(ID2D1DeviceContext* context, D2D1_RECT_F bounds, const DashboardHeaderData& header);

    // Pointer/keyboard events, all in DIPs for pointer position. Each
    // returns true if a repaint is warranted.
    bool OnMouseMove(D2D1_POINT_2F pointerDip);
    bool OnMouseLeave();
    bool OnLButtonDown(D2D1_POINT_2F pointerDip);
    bool OnLButtonUp(D2D1_POINT_2F pointerDip);
    bool OnDoubleClick(D2D1_POINT_2F pointerDip);
    bool OnChar(wchar_t ch);
    bool OnKeyDown(unsigned int virtualKey);

    // Re-derives m_currentLayout from scratch: which widgets are enabled
    // (per AppSettings) and their saved positions (per LayoutRepository).
    // Called once from the constructor, and again whenever Settings
    // toggles a widget on/off or resets the layout — see SettingsPanel's
    // callbacks, wired up in Window.
    void RebuildLayout();

    // Opens the Settings panel programmatically — used by the tray menu's
    // "Settings…" item, which needs to get there without a gear click.
    void OpenSettings() { m_settingsPanel.Open(); }

    // True while a drag is in progress or a post-drop settle animation is
    // still playing. Window keeps its short animation timer alive for as
    // long as this — or PhotoWidget's own transition — reports true; see
    // Window.h's comment on the shared timer.
    bool IsAnimating() const { return m_dragging || m_settling; }

private:
    void DrawHeader(ID2D1DeviceContext* ctx, D2D1_RECT_F rowBounds, const DashboardHeaderData& data);

    // Recomputes cell geometry from `bounds` and the given LayoutResult's
    // row count, caching it for hit-testing/drag math, and returns the DIP
    // rect for one grid cell footprint.
    D2D1_RECT_F ComputeGridGeometry(D2D1_RECT_F bounds, const layout::LayoutResult& activeLayout);
    D2D1_RECT_F GridRectToDip(const layout::GridRect& rect) const;
    layout::GridPosition PointToGridCell(D2D1_POINT_2F pt) const;

    // Finds which widget occupies the grid cell at `pt`, under whichever
    // LayoutResult is currently active for rendering (preview while
    // dragging, committed otherwise), or nullptr.
    widgets::IWidget* WidgetAt(D2D1_POINT_2F pt) const;

    void BeginDrag(widgets::WidgetId id, D2D1_POINT_2F mouseDownPt);
    void CommitDrag();
    D2D1_RECT_F ComputeDraggedWidgetRect() const;

    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    const ThemeManager* m_theme;              // non-owning; Window owns it
    widgets::WidgetManager* m_widgetManager;  // non-owning; Window owns it
    data::LayoutRepository* m_layoutRepository; // non-owning; Window owns it
    app::AppSettings* m_appSettings;          // non-owning; Window owns it

    SettingsPanel m_settingsPanel;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushSolid;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtGreetingName;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtGreetingSub;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtDateDay;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtDateFull;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtCardTitle;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtBody;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_fmtSmall;

    components::IconButton m_gearButton;

    // --- layout state ------------------------------------------------
    layout::LayoutEngine m_layoutEngine;
    layout::LayoutConstraints m_constraints; // fixed 6 columns — see GridTypes.h
    layout::LayoutResult m_currentLayout;    // the committed arrangement
    std::optional<layout::LayoutResult> m_previewLayout; // live, while dragging only

    // Cached from the most recent Draw() call — used by hit-testing and
    // drag math so those don't need to redo the grid math independently.
    D2D1_RECT_F m_gridContentRect{};
    float m_cellWidth = 1.0f;
    float m_cellHeight = 1.0f;

    // Rebuilt every Draw() call from whichever LayoutResult is active.
    std::map<widgets::WidgetId, D2D1_RECT_F> m_widgetBounds;

    // --- drag state machine --------------------------------------------
    bool m_pendingClick = false;   // mouse down, not yet resolved as click or drag
    bool m_dragging = false;
    widgets::WidgetId m_dragWidgetId{};
    D2D1_POINT_2F m_mouseDownPt{};
    D2D1_POINT_2F m_dragCurrentPt{};
    D2D1_POINT_2F m_dragGrabOffset{}; // cursor position relative to the widget's top-left at drag start

    // --- post-drop settle animation --------------------------------
    bool m_settling = false;
    widgets::WidgetId m_settlingWidgetId{};
    D2D1_RECT_F m_settleFromRect{};
    D2D1_RECT_F m_settleToRect{};
    ULONGLONG m_settleStartTick = 0;

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
