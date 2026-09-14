#include "widgets/PinnedWidget.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"

using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

void PinnedWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();

    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, res.brush, res.titleFormat, *res.theme, bounds, L"Pinned");

    float y = content.top;
    for (const auto& item : m_items) {
        if (y + 20.0f > content.bottom) break;

        D2D1_RECT_F pinRect = { content.left, y + 1.0f, content.left + 12.0f, y + 13.0f };
        icons::Draw(ctx, res.brush, icons::IconKind::Pin, pinRect, colors.accentViolet);

        D2D1_RECT_F textRect = { content.left + 20.0f, y, content.right, y + 18.0f };
        res.brush->SetColor(colors.textPrimary);
        ctx->DrawText(item.c_str(), static_cast<UINT32>(item.size()), res.bodyFormat, textRect, res.brush,
                      D2D1_DRAW_TEXT_OPTIONS_CLIP);
        y += 26.0f;
    }
}

} // namespace mosaic::widgets
