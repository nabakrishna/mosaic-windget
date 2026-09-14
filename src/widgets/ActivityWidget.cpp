#include "widgets/ActivityWidget.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"

using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

void ActivityWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();

    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, res.brush, res.titleFormat, *res.theme, bounds, L"Special Activity");

    float y = content.top;

    D2D1_RECT_F starRect = { content.left, y + 3.0f, content.left + 12.0f, y + 15.0f };
    icons::Draw(ctx, res.brush, icons::IconKind::Star, starRect, colors.accentAmber);

    D2D1_RECT_F titleRect = { content.left + 20.0f, y, content.right, y + 18.0f };
    res.brush->SetColor(colors.textPrimary);
    ctx->DrawText(m_title.c_str(), static_cast<UINT32>(m_title.size()), res.bodyFormat, titleRect, res.brush,
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
    y += 20.0f;

    D2D1_RECT_F whenRect = { content.left + 20.0f, y, content.right, y + 16.0f };
    res.brush->SetColor(colors.textSecondary);
    ctx->DrawText(m_when.c_str(), static_cast<UINT32>(m_when.size()), res.smallFormat, whenRect, res.brush,
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
    y += 34.0f;

    D2D1_RECT_F noteRect = { content.left, y, content.right, content.bottom };
    res.brush->SetColor(colors.textMuted);
    ctx->DrawText(m_note.c_str(), static_cast<UINT32>(m_note.size()), res.smallFormat, noteRect, res.brush,
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

} // namespace mosaic::widgets
