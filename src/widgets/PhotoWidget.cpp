#include "widgets/PhotoWidget.h"
#include "ui/components/GlassPanel.h"
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

void PhotoWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeMetrics& metrics = res.theme->Metrics();

    // No image pipeline yet (Phase 5). Still route through GlassPanel for
    // consistent shadow/border, then paint a placeholder gradient.
    GlassPanel::Draw(ctx, res.brush, bounds, *res.theme);

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(bounds, metrics.cardCornerRadius, metrics.cardCornerRadius);

    D2D1_GRADIENT_STOP stops[3] = {
        { 0.0f, { 0.10f, 0.10f, 0.22f, 1.0f } },
        { 0.55f, { 0.16f, 0.14f, 0.30f, 1.0f } },
        { 1.0f, { 0.45f, 0.30f, 0.35f, 1.0f } },
    };
    ComPtr<ID2D1GradientStopCollection> stopCollection;
    ctx->CreateGradientStopCollection(stops, 3, &stopCollection);

    ComPtr<ID2D1LinearGradientBrush> gradientBrush;
    ctx->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties({ bounds.left, bounds.bottom }, { bounds.right, bounds.top }),
        stopCollection.Get(), &gradientBrush);

    ctx->FillRoundedRectangle(rr, gradientBrush.Get());
}

} // namespace mosaic::widgets
