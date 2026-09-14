#include "ui/DashboardView.h"
#include "ui/Icons.h"

using Microsoft::WRL::ComPtr;
using namespace mosaic::widgets;

namespace mosaic::ui {

namespace {

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

} // namespace

DashboardView::DashboardView(IDWriteFactory* dwriteFactory, const ThemeManager* theme, WidgetManager* widgetManager)
    : m_dwriteFactory(dwriteFactory), m_theme(theme), m_widgetManager(widgetManager) {}

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

void DashboardView::Draw(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const DashboardHeaderData& header) {
    const ThemeMetrics& metrics = m_theme->Metrics();
    const float gutter = metrics.gridGutter;
    const float pad = metrics.pagePadding;

    D2D1_RECT_F content = { bounds.left + pad, bounds.top + pad, bounds.right - pad, bounds.bottom - pad };

    float headerHeight = 70.0f;
    D2D1_RECT_F headerRect = { content.left, content.top, content.right, content.top + headerHeight };
    DrawHeader(ctx, headerRect, header);

    // Fixed 3-over-2 grid matching the reference design's default
    // arrangement. This is exactly what Phase 6's LayoutEngine replaces —
    // for now it just proves widget dispatch works through real rects.
    float gridTop = headerRect.bottom + gutter;
    float gridBottom = content.bottom;
    float totalGridHeight = gridBottom - gridTop;
    float topRowHeight = totalGridHeight * 0.62f;

    float totalWidth = content.right - content.left;
    float colWidth3 = (totalWidth - 2 * gutter) / 3.0f;

    D2D1_RECT_F todoRect = { content.left, gridTop, content.left + colWidth3, gridTop + topRowHeight };
    D2D1_RECT_F photoRect = { todoRect.right + gutter, gridTop, todoRect.right + gutter + colWidth3, gridTop + topRowHeight };
    D2D1_RECT_F activityRect = { photoRect.right + gutter, gridTop, content.right, gridTop + topRowHeight };

    float bottomTop = topRowHeight + gridTop + gutter;
    float colWidth2 = (totalWidth - gutter) * 0.42f;
    D2D1_RECT_F pinnedRect = { content.left, bottomTop, content.left + colWidth2, gridBottom };
    D2D1_RECT_F notesRect = { pinnedRect.right + gutter, bottomTop, content.right, gridBottom };

    m_widgetBounds[WidgetId::Todo] = todoRect;
    m_widgetBounds[WidgetId::Photo] = photoRect;
    m_widgetBounds[WidgetId::Activity] = activityRect;
    m_widgetBounds[WidgetId::Pinned] = pinnedRect;
    m_widgetBounds[WidgetId::QuickNotes] = notesRect;

    WidgetRenderResources res{
        m_brushSolid.Get(), m_fmtCardTitle.Get(), m_fmtBody.Get(), m_fmtSmall.Get(),
        m_dwriteFactory.Get(), m_theme
    };

    for (const auto& [id, rect] : m_widgetBounds) {
        if (IWidget* widget = m_widgetManager->Get(id)) {
            widget->Render(ctx, rect, res);
        }
    }
}

IWidget* DashboardView::WidgetAt(D2D1_POINT_2F pt) const {
    for (const auto& [id, rect] : m_widgetBounds) {
        if (pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top && pt.y <= rect.bottom) {
            return m_widgetManager->Get(id);
        }
    }
    return nullptr;
}

bool DashboardView::OnMouseMove(D2D1_POINT_2F pt) {
    bool changed = false;

    bool gearHovered = m_gearButton.HitTest(pt);
    if (gearHovered != m_gearButton.IsHovered()) {
        m_gearButton.SetHovered(gearHovered);
        changed = true;
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
    IWidget* target = WidgetAt(pt);
    if (!target) {
        // Clicking empty space (or the gear — Phase 7 handles that click)
        // still un-focuses whatever previously had keyboard focus, so a
        // stray keypress after clicking elsewhere doesn't land in an old
        // edit box.
        m_focusedWidget = nullptr;
        return false;
    }
    m_focusedWidget = target;
    return target->OnLButtonDown(pt);
}

bool DashboardView::OnDoubleClick(D2D1_POINT_2F pt) {
    IWidget* target = WidgetAt(pt);
    if (!target) return false;
    m_focusedWidget = target;
    return target->OnDoubleClick(pt);
}

bool DashboardView::OnChar(wchar_t ch) {
    return m_focusedWidget ? m_focusedWidget->OnChar(ch) : false;
}

bool DashboardView::OnKeyDown(unsigned int virtualKey) {
    return m_focusedWidget ? m_focusedWidget->OnKeyDown(virtualKey) : false;
}

} // namespace mosaic::ui
