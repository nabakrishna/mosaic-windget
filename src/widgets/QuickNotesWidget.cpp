#include "widgets/QuickNotesWidget.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"

using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

void QuickNotesWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();

    D2D1_RECT_F content = WidgetCard::DrawFrame(
        ctx, res.brush, res.titleFormat, *res.theme, bounds, L"Quick Notes");

    float y = content.top;
    static const std::wstring kPlaceholder = L"Write something...";
    res.brush->SetColor(colors.textMuted);
    D2D1_RECT_F placeholderRect = { content.left, y, content.right, y + 18.0f };
    ctx->DrawText(kPlaceholder.c_str(), static_cast<UINT32>(kPlaceholder.size()), res.bodyFormat,
                  placeholderRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);

    float lockY = content.bottom - 18.0f;
    D2D1_RECT_F lockIconRect = { content.left, lockY + 1.0f, content.left + 14.0f, lockY + 15.0f };
    icons::Draw(ctx, res.brush, icons::IconKind::Lock, lockIconRect, colors.textSecondary);

    static const std::wstring kLockLabel = L"Use Password or Fingerprint";
    D2D1_RECT_F lockTextRect = { content.left + 20.0f, lockY, content.right, lockY + 18.0f };
    res.brush->SetColor(colors.textSecondary);
    ctx->DrawText(kLockLabel.c_str(), static_cast<UINT32>(kLockLabel.size()), res.smallFormat,
                  lockTextRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

} // namespace mosaic::widgets
