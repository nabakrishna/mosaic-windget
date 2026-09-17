#include "ui/DashboardView.h"
#include "ui/Icons.h"
#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;
using namespace mosaic::widgets;

namespace mosaic::ui {

namespace {

constexpr float kDragThresholdDip = 5.0f;      // movement past this turns a click into a drag
constexpr ULONGLONG kSettleDurationMs = 180;   // spec section 44: "Dropping: snap + settle"

ComPtr<IDWriteTextFormat> MakeFormat(IDWriteFactory* factory, const ThemeTypography& type, float size,
                                      DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL) {
    ComPtr<IDWriteTextFormat> fmt;
    HRESULT hr = factory->CreateTextFormat(
        type.fontFamily.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt);
    if (FAILED(hr)) {
        factory->CreateTextFormat(
            type.fontFamilyFallback.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt);
    }
    if (fmt) fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return fmt;
}

void DrawText(ID2D1DeviceContext* ctx, ID2D1SolidColorBrush* brush, IDWriteTextFormat* fmt,
              const std::wstring& text, D2D1_RECT_F rect, D2D1_COLOR_F color) {
    if (text.empty() || !fmt) return;
    brush->SetColor(color);
    ctx->DrawText(text.c_str(), static_cast<UINT32>(text.size()), fmt, rect, brush,
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

D2D1_RECT_F LerpRect(D2D1_RECT_F a, D2D1_RECT_F b, float t) {
    return {
        a.left + (b.left - a.left) * t,
        a.top + (b.top - a.top) * t,
        a.right + (b.right - a.right) * t,
        a.bottom + (b.bottom - a.bottom) * t
    };
}

} // namespace

DashboardView::DashboardView(IDWriteFactory* dwriteFactory, const ThemeManager* theme,
                              WidgetManager* widgetManager, data::LayoutRepository* layoutRepository,
                              app::AppSettings* appSettings, SettingsCallbacks settingsCallbacks)
    : m_dwriteFactory(dwriteFactory), m_theme(theme),
      m_widgetManager(widgetManager), m_layoutRepository(layoutRepository),
      m_appSettings(appSettings), m_settingsPanel(appSettings, std::move(settingsCallbacks)) {
    RebuildLayout();
}

void DashboardView::RebuildLayout() {
    // Build the arrangement: saved positions where they exist (every
    // subsequent launch), first-fit placement in this fixed reading order
    // otherwise (fresh install — matches the reference design's reading
    // order). Widgets Settings has disabled are simply left out of the
    // input list entirely — LayoutEngine never sees them, so they occupy
    // no grid cells and don't affect anyone else's compaction.
    static const WidgetId kDefaultOrder[] = {
        WidgetId::Todo, WidgetId::Photo, WidgetId::Activity, WidgetId::Pinned, WidgetId::QuickNotes
    };

    auto saved = m_layoutRepository->LoadAll();

    std::vector<layout::WidgetInput> inputs;
    for (WidgetId id : kDefaultOrder) {
        if (m_appSettings && !m_appSettings->IsWidgetEnabled(id)) continue;

        IWidget* w = m_widgetManager->Get(id);
        if (!w) continue;

        layout::WidgetInput input;
        input.id = id;
        input.size = w->Metadata().preferred;
        for (const auto& s : saved) {
            if (s.id == id) {
                input.savedPosition = layout::GridPosition{ s.col, s.row };
                break;
            }
        }
        inputs.push_back(input);
    }

    m_currentLayout = m_layoutEngine.Arrange(inputs, m_constraints);
}

HRESULT DashboardView::CreateDeviceResources(ID2D1DeviceContext* context) {
    const ThemeColors& colors = m_theme->Colors();
    const ThemeTypography& type = m_theme->Typography();

    HRESULT hr = context->CreateSolidColorBrush(colors.textPrimary, &m_brushSolid);
    if (FAILED(hr)) return hr;

    m_fmtGreetingName = MakeFormat(m_dwriteFactory.Get(), type, type.sizeGreetingName, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    m_fmtGreetingSub  = MakeFormat(m_dwriteFactory.Get(), type, type.sizeGreetingSub);
    m_fmtDateDay      = MakeFormat(m_dwriteFactory.Get(), type, type.sizeDateDay);
    m_fmtDateFull     = MakeFormat(m_dwriteFactory.Get(), type, type.sizeDateFull, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    m_fmtCardTitle    = MakeFormat(m_dwriteFactory.Get(), type, type.sizeCardTitle, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    m_fmtBody         = MakeFormat(m_dwriteFactory.Get(), type, type.sizeBody);
    m_fmtSmall        = MakeFormat(m_dwriteFactory.Get(), type, type.sizeSmall);

    return (m_fmtGreetingName && m_fmtGreetingSub && m_fmtDateDay && m_fmtDateFull &&
            m_fmtCardTitle && m_fmtBody && m_fmtSmall) ? S_OK : E_FAIL;
}

void DashboardView::ReleaseDeviceResources() {
    m_brushSolid.Reset();
    m_fmtGreetingName.Reset();
    m_fmtGreetingSub.Reset();
    m_fmtDateDay.Reset();
    m_fmtDateFull.Reset();
    m_fmtCardTitle.Reset();
    m_fmtBody.Reset();
    m_fmtSmall.Reset();
}

void DashboardView::DrawHeader(ID2D1DeviceContext* ctx, D2D1_RECT_F r, const DashboardHeaderData& data) {
    const ThemeColors& colors = m_theme->Colors();

    D2D1_RECT_F greetRow = { r.left, r.top, r.left + 320.0f, r.top + 20.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtGreetingSub.Get(), data.greetingLine, greetRow, colors.textSecondary);

    D2D1_RECT_F nameRow = { r.left, r.top + 18.0f, r.left + 320.0f, r.top + 48.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtGreetingName.Get(), data.userName, nameRow, colors.textPrimary);

    D2D1_RECT_F subRow = { r.left, r.top + 46.0f, r.left + 340.0f, r.top + 66.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtGreetingSub.Get(), data.motivation, subRow, colors.textMuted);

    D2D1_RECT_F leafRect = { r.left - 20.0f, r.top + 1.0f, r.left - 6.0f, r.top + 15.0f };
    icons::Draw(ctx, m_brushSolid.Get(), icons::IconKind::Leaf, leafRect, colors.accentGreen);

    D2D1_POINT_2F gearCenter = { r.left + 360.0f, r.top + 16.0f };
    m_gearButton.SetBounds(gearCenter, 16.0f);
    m_gearButton.Draw(ctx, m_brushSolid.Get(), icons::IconKind::Gear, *m_theme);

    D2D1_RECT_F dayRow = { r.left + 400.0f, r.top, r.left + 600.0f, r.top + 18.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtDateDay.Get(), data.weekday, dayRow, colors.textSecondary);

    D2D1_RECT_F dateRow = { r.left + 400.0f, r.top + 16.0f, r.left + 640.0f, r.top + 44.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtDateFull.Get(), data.fullDate, dateRow, colors.textPrimary);
}

// --- grid geometry -------------------------------------------------

D2D1_RECT_F DashboardView::ComputeGridGeometry(D2D1_RECT_F bounds, const layout::LayoutResult& activeLayout) {
    const ThemeMetrics& metrics = m_theme->Metrics();
    float gutter = metrics.gridGutter;

    m_gridContentRect = bounds;
    int columns = m_constraints.columns;
    int rows = std::max(1, activeLayout.totalRows);

    float totalWidth = bounds.right - bounds.left;
    float totalHeight = bounds.bottom - bounds.top;

    m_cellWidth = (totalWidth - (columns - 1) * gutter) / columns;
    m_cellHeight = (totalHeight - (rows - 1) * gutter) / rows;

    m_widgetBounds.clear();
    for (const auto& placed : activeLayout.placements) {
        m_widgetBounds[placed.id] = GridRectToDip(placed.rect);
    }
    return bounds;
}

D2D1_RECT_F DashboardView::GridRectToDip(const layout::GridRect& gr) const {
    const ThemeMetrics& metrics = m_theme->Metrics();
    float gutter = metrics.gridGutter;
    float left = m_gridContentRect.left + gr.col * (m_cellWidth + gutter);
    float top = m_gridContentRect.top + gr.row * (m_cellHeight + gutter);
    float width = gr.cols * m_cellWidth + (gr.cols - 1) * gutter;
    float height = gr.rows * m_cellHeight + (gr.rows - 1) * gutter;
    return { left, top, left + width, top + height };
}

layout::GridPosition DashboardView::PointToGridCell(D2D1_POINT_2F pt) const {
    const ThemeMetrics& metrics = m_theme->Metrics();
    float gutter = metrics.gridGutter;
    float relX = pt.x - m_gridContentRect.left;
    float relY = pt.y - m_gridContentRect.top;
    int col = static_cast<int>(std::floor(relX / (m_cellWidth + gutter)));
    int row = static_cast<int>(std::floor(relY / (m_cellHeight + gutter)));
    col = std::clamp(col, 0, m_constraints.columns - 1);
    row = std::max(row, 0);
    return { col, row };
}

D2D1_RECT_F DashboardView::ComputeDraggedWidgetRect() const {
    IWidget* widget = m_widgetManager->Get(m_dragWidgetId);
    GridSize size = widget ? widget->Metadata().preferred : GridSize{ 1, 1 };
    const ThemeMetrics& metrics = m_theme->Metrics();
    float gutter = metrics.gridGutter;

    float w = size.cols * m_cellWidth + (size.cols - 1) * gutter;
    float h = size.rows * m_cellHeight + (size.rows - 1) * gutter;
    float left = m_dragCurrentPt.x - m_dragGrabOffset.x;
    float top = m_dragCurrentPt.y - m_dragGrabOffset.y;
    return { left, top, left + w, top + h };
}

IWidget* DashboardView::WidgetAt(D2D1_POINT_2F pt) const {
    for (const auto& [id, rect] : m_widgetBounds) {
        if (pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top && pt.y <= rect.bottom) {
            return m_widgetManager->Get(id);
        }
    }
    return nullptr;
}

// --- top-level draw --------------------------------------------------------

void DashboardView::Draw(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const DashboardHeaderData& header) {
    const ThemeMetrics& metrics = m_theme->Metrics();
    const float gutter = metrics.gridGutter;
    const float pad = metrics.pagePadding;

    D2D1_RECT_F content = { bounds.left + pad, bounds.top + pad, bounds.right - pad, bounds.bottom - pad };

    float headerHeight = 70.0f;
    D2D1_RECT_F headerRect = { content.left, content.top, content.right, content.top + headerHeight };
    DrawHeader(ctx, headerRect, header);

    D2D1_RECT_F gridArea = { content.left, headerRect.bottom + gutter, content.right, content.bottom };

    const layout::LayoutResult& activeLayout = m_previewLayout ? *m_previewLayout : m_currentLayout;
    ComputeGridGeometry(gridArea, activeLayout);

    WidgetRenderResources res{
        m_brushSolid.Get(), m_fmtCardTitle.Get(), m_fmtBody.Get(), m_fmtSmall.Get(),
        m_dwriteFactory.Get(), m_theme
    };

    // Normal widgets first. The dragged widget (drawn separately, on top,
    // following the cursor) and the settling widget (drawn separately,
    // interpolating into place) are skipped here so they aren't drawn
    // twice or in the wrong z-order.
    for (const auto& [id, rect] : m_widgetBounds) {
        if (m_dragging && id == m_dragWidgetId) continue;
        if (m_settling && id == m_settlingWidgetId) continue;
        if (IWidget* widget = m_widgetManager->Get(id)) {
            widget->Render(ctx, rect, res);
        }
    }

    if (m_settling) {
        ULONGLONG elapsed = GetTickCount64() - m_settleStartTick;
        float t = std::clamp(static_cast<float>(elapsed) / static_cast<float>(kSettleDurationMs), 0.0f, 1.0f);
        float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); // ease-out cubic
        D2D1_RECT_F interpolated = LerpRect(m_settleFromRect, m_settleToRect, eased);
        if (IWidget* widget = m_widgetManager->Get(m_settlingWidgetId)) {
            widget->Render(ctx, interpolated, res);
        }
        if (elapsed >= kSettleDurationMs) {
            m_settling = false;
        }
    }

    if (m_dragging) {
        // Drop-zone highlight: where it would land if released right now.
        auto it = m_widgetBounds.find(m_dragWidgetId);
        if (it != m_widgetBounds.end()) {
            D2D1_ROUNDED_RECT hi = D2D1::RoundedRect(it->second, metrics.cardCornerRadius, metrics.cardCornerRadius);
            m_brushSolid->SetColor(WithAlpha(m_theme->Colors().accentViolet, 0.12f));
            ctx->FillRoundedRectangle(hi, m_brushSolid.Get());
            m_brushSolid->SetColor(WithAlpha(m_theme->Colors().accentViolet, 0.55f));
            ctx->DrawRoundedRectangle(hi, m_brushSolid.Get(), 2.0f);
        }

        D2D1_RECT_F dragRect = ComputeDraggedWidgetRect();
        if (IWidget* widget = m_widgetManager->Get(m_dragWidgetId)) {
            widget->Render(ctx, dragRect, res);
        }
    }

    // Settings overlays everything, including a dragged card — it's drawn
    // last so its scrim dims the whole dashboard beneath it. Draw() is a
    // no-op when the panel is closed.
    m_settingsPanel.Draw(ctx, m_brushSolid.Get(), m_fmtCardTitle.Get(), m_fmtBody.Get(), m_fmtSmall.Get(),
                          bounds, *m_theme);
}

// --- drag state machine --------------------------------------------------

void DashboardView::BeginDrag(WidgetId id, D2D1_POINT_2F mouseDownPt) {
    m_pendingClick = false;
    m_dragging = true;
    m_dragWidgetId = id;
    m_settling = false; // a new drag interrupts any in-progress settle animation

    auto it = m_widgetBounds.find(id);
    D2D1_RECT_F widgetRect = (it != m_widgetBounds.end())
        ? it->second
        : D2D1_RECT_F{ mouseDownPt.x, mouseDownPt.y, mouseDownPt.x, mouseDownPt.y };
    m_dragGrabOffset = { mouseDownPt.x - widgetRect.left, mouseDownPt.y - widgetRect.top };
}

void DashboardView::CommitDrag() {
    m_dragging = false;

    layout::LayoutResult finalLayout = m_previewLayout.value_or(m_currentLayout);
    m_previewLayout.reset();

    // Persist once, on drop — spec section 17's "optimize layout after
    // every move" read as "after every completed move," not continuously
    // mid-drag (which would just be wasted writes).
    std::vector<data::LayoutRepository::SavedPosition> toSave;
    for (const auto& p : finalLayout.placements) {
        toSave.push_back({ p.id, p.rect.col, p.rect.row });
    }
    m_layoutRepository->SaveAll(toSave);

    // Only the dropped widget animates into place — every other widget
    // was already tracking its live preview position during the drag
    // itself, so it's already exactly where it should be.
    m_settleFromRect = ComputeDraggedWidgetRect();
    if (const layout::GridRect* r = finalLayout.Find(m_dragWidgetId)) {
        m_settleToRect = GridRectToDip(*r);
        m_settling = true;
        m_settlingWidgetId = m_dragWidgetId;
        m_settleStartTick = GetTickCount64();
    } else {
        m_settling = false;
    }

    m_currentLayout = std::move(finalLayout);
}

// --- input dispatch --------------------------------------------------------

bool DashboardView::OnMouseMove(D2D1_POINT_2F pt) {
    // While Settings is open it owns all input — the dashboard beneath
    // must not also react (no phantom hovers on cards under the scrim).
    if (m_settingsPanel.IsOpen()) return m_settingsPanel.OnMouseMove(pt);

    bool changed = false;

    bool gearHovered = m_gearButton.HitTest(pt);
    if (gearHovered != m_gearButton.IsHovered()) {
        m_gearButton.SetHovered(gearHovered);
        changed = true;
    }

    if (m_dragging) {
        m_dragCurrentPt = pt;
        layout::GridPosition targetCell = PointToGridCell(pt);
        m_previewLayout = m_layoutEngine.MoveWidget(m_currentLayout, m_dragWidgetId, targetCell, m_constraints);
        return true;
    }

    if (m_pendingClick) {
        float dx = pt.x - m_mouseDownPt.x;
        float dy = pt.y - m_mouseDownPt.y;
        if (dx * dx + dy * dy > kDragThresholdDip * kDragThresholdDip) {
            BeginDrag(m_dragWidgetId, m_mouseDownPt);
            m_dragCurrentPt = pt;
            layout::GridPosition targetCell = PointToGridCell(pt);
            m_previewLayout = m_layoutEngine.MoveWidget(m_currentLayout, m_dragWidgetId, targetCell, m_constraints);
            return true;
        }
        return changed;
    }

    IWidget* current = WidgetAt(pt);
    IWidget* previouslyHovered = m_anyWidgetHovered ? m_widgetManager->Get(m_hoveredWidgetId) : nullptr;

    if (current != previouslyHovered) {
        if (previouslyHovered && previouslyHovered->OnMouseLeave()) changed = true;
        if (current) {
            m_hoveredWidgetId = current->Id();
            m_anyWidgetHovered = true;
        } else {
            m_anyWidgetHovered = false;
        }
    }

    if (current && current->OnMouseMove(pt)) changed = true;

    return changed;
}

bool DashboardView::OnMouseLeave() {
    if (m_settingsPanel.IsOpen()) return false; // panel stays as-is; nothing beneath it to un-hover

    bool changed = false;
    if (m_gearButton.IsHovered()) { m_gearButton.SetHovered(false); changed = true; }
    if (m_anyWidgetHovered) {
        if (IWidget* w = m_widgetManager->Get(m_hoveredWidgetId)) {
            if (w->OnMouseLeave()) changed = true;
        }
        m_anyWidgetHovered = false;
    }
    return changed;
}

bool DashboardView::OnLButtonDown(D2D1_POINT_2F pt) {
    if (m_settingsPanel.IsOpen()) return m_settingsPanel.OnLButtonDown(pt);

    // The gear opens Settings. Handled here rather than deferred to
    // button-up like widget clicks are, because the gear isn't part of a
    // draggable card — there's no click-vs-drag ambiguity to resolve.
    if (m_gearButton.HitTest(pt)) {
        m_focusedWidget = nullptr;
        m_pendingClick = false;
        m_settingsPanel.Open();
        return true;
    }

    IWidget* target = WidgetAt(pt);
    if (!target) {
        // Clicking empty space still un-focuses whatever previously had
        // keyboard focus, so a stray keypress after clicking elsewhere
        // doesn't land in an old edit box.
        m_focusedWidget = nullptr;
        m_pendingClick = false;
        return false;
    }

    // Deferred — see the class comment on click-vs-drag resolution. The
    // widget does not see this click yet; OnLButtonUp resolves it either
    // as a real click (dispatched there) or abandons it entirely (a drag
    // started instead).
    m_pendingClick = true;
    m_mouseDownPt = pt;
    m_dragWidgetId = target->Id();
    return false;
}

bool DashboardView::OnLButtonUp(D2D1_POINT_2F pt) {
    if (m_settingsPanel.IsOpen()) return m_settingsPanel.OnLButtonUp(pt);

    if (m_dragging) {
        CommitDrag();
        return true;
    }

    if (m_pendingClick) {
        m_pendingClick = false;
        IWidget* target = m_widgetManager->Get(m_dragWidgetId);
        if (!target) return false;
        m_focusedWidget = target;
        return target->OnLButtonDown(pt);
    }

    return false;
}

bool DashboardView::OnDoubleClick(D2D1_POINT_2F pt) {
    if (m_settingsPanel.IsOpen()) return false; // no double-click affordances in Settings
    if (m_dragging || m_pendingClick) return false; // shouldn't happen given capture semantics, but never double-dispatch mid-gesture
    IWidget* target = WidgetAt(pt);
    if (!target) return false;
    m_focusedWidget = target;
    return target->OnDoubleClick(pt);
}

bool DashboardView::OnChar(wchar_t ch) {
    if (m_settingsPanel.IsOpen()) return false; // absorbed; no text entry in Settings today
    return m_focusedWidget ? m_focusedWidget->OnChar(ch) : false;
}

bool DashboardView::OnKeyDown(unsigned int virtualKey) {
    if (m_settingsPanel.IsOpen()) return m_settingsPanel.OnKeyDown(virtualKey); // Escape closes
    return m_focusedWidget ? m_focusedWidget->OnKeyDown(virtualKey) : false;
}

} // namespace mosaic::ui
