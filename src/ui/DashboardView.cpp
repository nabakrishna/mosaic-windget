#include "ui/DashboardView.h"
#include "ui/components/GlassPanel.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"
#include <cassert>

using Microsoft::WRL::ComPtr;
using namespace mosaic::ui::components;

namespace mosaic::ui {

namespace {

ComPtr<IDWriteTextFormat> MakeFormat(IDWriteFactory* factory, const ThemeTypography& type, float size,
                                      DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL) {
    ComPtr<IDWriteTextFormat> fmt;
    HRESULT hr = factory->CreateTextFormat(
        type.fontFamily.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt);
    if (FAILED(hr)) {
        // Windows 10 fallback: "Segoe UI Variable Display" may not exist.
        factory->CreateTextFormat(
            type.fontFamilyFallback.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt);
    }
    if (fmt) {
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
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

DashboardView::DashboardView(IDWriteFactory* dwriteFactory, const ThemeManager* theme)
    : m_dwriteFactory(dwriteFactory), m_theme(theme) {}

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

void DashboardView::DrawHeader(ID2D1DeviceContext* ctx, D2D1_RECT_F r, const DashboardSampleData& data) {
    const ThemeColors& colors = m_theme->Colors();

    // Greeting block, left-aligned.
    D2D1_RECT_F greetRow = { r.left, r.top, r.left + 320.0f, r.top + 20.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtGreetingSub.Get(), data.greetingLine, greetRow, colors.textSecondary);

    D2D1_RECT_F nameRow = { r.left, r.top + 18.0f, r.left + 320.0f, r.top + 48.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtGreetingName.Get(), data.userName, nameRow, colors.textPrimary);

    D2D1_RECT_F subRow = { r.left, r.top + 46.0f, r.left + 340.0f, r.top + 66.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtGreetingSub.Get(), data.motivation, subRow, colors.textMuted);

    // Real leaf icon next to the greeting, replacing Phase 1's plain dot.
    D2D1_RECT_F leafRect = { r.left - 20.0f, r.top + 1.0f, r.left - 6.0f, r.top + 15.0f };
    icons::Draw(ctx, m_brushSolid.Get(), icons::IconKind::Leaf, leafRect, colors.accentGreen);

    // Settings gear — now a real IconButton with hover feedback (see
    // UpdateHover) instead of a static placeholder circle.
    D2D1_POINT_2F gearCenter = { r.left + 360.0f, r.top + 16.0f };
    m_gearButton.SetBounds(gearCenter, 16.0f);
    m_gearButton.Draw(ctx, m_brushSolid.Get(), icons::IconKind::Gear, *m_theme);

    // Date block, right of the gear.
    D2D1_RECT_F dayRow = { r.left + 400.0f, r.top, r.left + 600.0f, r.top + 18.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtDateDay.Get(), data.weekday, dayRow, colors.textSecondary);

    D2D1_RECT_F dateRow = { r.left + 400.0f, r.top + 16.0f, r.left + 640.0f, r.top + 44.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtDateFull.Get(), data.fullDate, dateRow, colors.textPrimary);
}

void DashboardView::DrawTodoCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect, const DashboardSampleData& data) {
    const ThemeColors& colors = m_theme->Colors();

    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, m_brushSolid.Get(), m_fmtCardTitle.Get(), *m_theme, rect, L"To Do");

    float y = content.top;
    const float rowHeight = 26.0f;
    const float checkboxSize = 14.0f;

    for (const auto& item : data.todos) {
        if (y + rowHeight > content.bottom) break; // compact scroll area lands here in Phase 3

        D2D1_RECT_F boxRect = { content.left, y + 4.0f, content.left + checkboxSize, y + 4.0f + checkboxSize };
        D2D1_ROUNDED_RECT box = D2D1::RoundedRect(boxRect, 4.0f, 4.0f);

        if (item.completed) {
            m_brushSolid->SetColor(colors.accentGreen);
            ctx->FillRoundedRectangle(box, m_brushSolid.Get());
            // A small white check mark drawn over the filled box reads more
            // clearly than the fill color alone (matches the reference
            // design's checkmark, not just a colored square).
            D2D1_RECT_F checkInset = { boxRect.left + 2.0f, boxRect.top + 2.0f, boxRect.right - 2.0f, boxRect.bottom - 2.0f };
            icons::Draw(ctx, m_brushSolid.Get(), icons::IconKind::Check, checkInset, colors.textPrimary);
        } else {
            m_brushSolid->SetColor(colors.cardBorder);
            ctx->DrawRoundedRectangle(box, m_brushSolid.Get(), 1.2f);
        }

        D2D1_RECT_F textRect = { content.left + checkboxSize + 10.0f, y, content.right, y + rowHeight };
        DrawText(ctx, m_brushSolid.Get(), m_fmtBody.Get(), item.title, textRect,
                 item.completed ? colors.textCompleted : colors.textPrimary);

        y += rowHeight;
    }
}

void DashboardView::DrawActivityCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect, const DashboardSampleData& data) {
    const ThemeColors& colors = m_theme->Colors();

    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, m_brushSolid.Get(), m_fmtCardTitle.Get(), *m_theme, rect, L"Special Activity");

    float y = content.top;

    D2D1_RECT_F starRect = { content.left, y + 3.0f, content.left + 12.0f, y + 15.0f };
    icons::Draw(ctx, m_brushSolid.Get(), icons::IconKind::Star, starRect, colors.accentAmber);

    D2D1_RECT_F activityTitleRect = { content.left + 20.0f, y, content.right, y + 18.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtBody.Get(), data.activityTitle, activityTitleRect, colors.textPrimary);
    y += 20.0f;

    D2D1_RECT_F whenRect = { content.left + 20.0f, y, content.right, y + 16.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtSmall.Get(), data.activityWhen, whenRect, colors.textSecondary);
    y += 34.0f;

    D2D1_RECT_F noteRect = { content.left, y, content.right, content.bottom };
    DrawText(ctx, m_brushSolid.Get(), m_fmtSmall.Get(), data.activityNote, noteRect, colors.textMuted);
}

void DashboardView::DrawPinnedCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect, const DashboardSampleData& data) {
    const ThemeColors& colors = m_theme->Colors();

    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, m_brushSolid.Get(), m_fmtCardTitle.Get(), *m_theme, rect, L"Pinned");

    float y = content.top;
    for (const auto& item : data.pinnedItems) {
        if (y + 20.0f > content.bottom) break;

        D2D1_RECT_F pinRect = { content.left, y + 1.0f, content.left + 12.0f, y + 13.0f };
        icons::Draw(ctx, m_brushSolid.Get(), icons::IconKind::Pin, pinRect, colors.accentViolet);

        D2D1_RECT_F textRect = { content.left + 20.0f, y, content.right, y + 18.0f };
        DrawText(ctx, m_brushSolid.Get(), m_fmtBody.Get(), item, textRect, colors.textPrimary);
        y += 26.0f;
    }
}

void DashboardView::DrawQuickNotesCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect) {
    const ThemeColors& colors = m_theme->Colors();

    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, m_brushSolid.Get(), m_fmtCardTitle.Get(), *m_theme, rect, L"Quick Notes");

    float y = content.top;
    D2D1_RECT_F placeholderRect = { content.left, y, content.right, y + 18.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtBody.Get(), L"Write something...", placeholderRect, colors.textMuted);

    // Lock affordance pinned to the bottom of the card — contents stay
    // hidden until Phase 8 wires up real DPAPI/Windows Hello unlock.
    float lockY = content.bottom - 18.0f;
    D2D1_RECT_F lockIconRect = { content.left, lockY + 1.0f, content.left + 14.0f, lockY + 15.0f };
    icons::Draw(ctx, m_brushSolid.Get(), icons::IconKind::Lock, lockIconRect, colors.textSecondary);

    D2D1_RECT_F lockTextRect = { content.left + 20.0f, lockY, content.right, lockY + 18.0f };
    DrawText(ctx, m_brushSolid.Get(), m_fmtSmall.Get(), L"Use Password or Fingerprint", lockTextRect, colors.textSecondary);
}

void DashboardView::DrawPhotoCard(ID2D1DeviceContext* ctx, D2D1_RECT_F rect) {
    const ThemeMetrics& metrics = m_theme->Metrics();

    // Phase 1/2 have no image pipeline yet (that's Phase 5). Still route
    // the card frame through GlassPanel for the shadow/border consistency,
    // then paint a placeholder gradient over the fill area.
    GlassPanel::Draw(ctx, m_brushSolid.Get(), rect, *m_theme);

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, metrics.cardCornerRadius, metrics.cardCornerRadius);

    D2D1_GRADIENT_STOP stops[3] = {
        { 0.0f, { 0.10f, 0.10f, 0.22f, 1.0f } },
        { 0.55f, { 0.16f, 0.14f, 0.30f, 1.0f } },
        { 1.0f, { 0.45f, 0.30f, 0.35f, 1.0f } },
    };
    ComPtr<ID2D1GradientStopCollection> stopCollection;
    ctx->CreateGradientStopCollection(stops, 3, &stopCollection);

    ComPtr<ID2D1LinearGradientBrush> gradientBrush;
    ctx->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties({ rect.left, rect.bottom }, { rect.right, rect.top }),
        stopCollection.Get(), &gradientBrush);

    // FillRoundedRectangle already clips to the rounded-corner geometry on
    // its own — no PushLayer/PopLayer needed for a plain fill.
    ctx->FillRoundedRectangle(rr, gradientBrush.Get());
}

// --- hover handling --------------------------------------------------------

bool DashboardView::UpdateHover(D2D1_POINT_2F pointerDip) {
    bool wasHovered = m_gearButton.IsHovered();
    bool isHovered = m_gearButton.HitTest(pointerDip);
    if (wasHovered == isHovered) return false;
    m_gearButton.SetHovered(isHovered);
    return true;
}

// --- top-level layout --------------------------------------------------

void DashboardView::Draw(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const DashboardSampleData& data) {
    const ThemeMetrics& metrics = m_theme->Metrics();
    const float gutter = metrics.gridGutter;
    const float pad = metrics.pagePadding;

    D2D1_RECT_F content = {
        bounds.left + pad, bounds.top + pad, bounds.right - pad, bounds.bottom - pad
    };

    // Header row.
    float headerHeight = 70.0f;
    D2D1_RECT_F headerRect = { content.left, content.top, content.right, content.top + headerHeight };
    DrawHeader(ctx, headerRect, data);

    // Remaining area splits into a 3-column top row and a 2-column bottom
    // row, matching the reference screenshot's default arrangement. This
    // fixed split is exactly what the real LayoutEngine (Phase 6) replaces —
    // here it just proves the visual proportions are right.
    float gridTop = headerRect.bottom + gutter;
    float gridBottom = content.bottom;
    float totalGridHeight = gridBottom - gridTop;

    float topRowHeight = totalGridHeight * 0.62f;

    float totalWidth = content.right - content.left;
    float colWidth3 = (totalWidth - 2 * gutter) / 3.0f;

    // Top row: To Do | Photo | Special Activity
    D2D1_RECT_F todoRect = { content.left, gridTop, content.left + colWidth3, gridTop + topRowHeight };
    D2D1_RECT_F photoRect = { todoRect.right + gutter, gridTop, todoRect.right + gutter + colWidth3, gridTop + topRowHeight };
    D2D1_RECT_F activityRect = { photoRect.right + gutter, gridTop, content.right, gridTop + topRowHeight };

    DrawTodoCard(ctx, todoRect, data);
    DrawPhotoCard(ctx, photoRect);
    DrawActivityCard(ctx, activityRect, data);

    // Bottom row: Pinned | Quick Notes
    float bottomTop = topRowHeight + gridTop + gutter;
    float colWidth2 = (totalWidth - gutter) * 0.42f;

    D2D1_RECT_F pinnedRect = { content.left, bottomTop, content.left + colWidth2, gridBottom };
    D2D1_RECT_F notesRect = { pinnedRect.right + gutter, bottomTop, content.right, gridBottom };

    DrawPinnedCard(ctx, pinnedRect, data);
    DrawQuickNotesCard(ctx, notesRect);
}

} // namespace mosaic::ui
