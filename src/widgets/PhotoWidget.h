#pragma once
#include "widgets/IWidget.h"

namespace mosaic::widgets {

// Same placeholder gradient DashboardView drew inline in Phases 1–2, now
// behind IWidget so Phase 5 slots in the real WIC decode/rotation pipeline
// without touching how DashboardView positions or dispatches to it.
class PhotoWidget : public IWidget {
public:
    WidgetId Id() const override { return WidgetId::Photo; }
    WidgetMetadata Metadata() const override {
        return WidgetMetadata{ WidgetId::Photo, L"Photo", { 2, 2 }, { 1, 1 }, { 3, 3 } };
    }
    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;
};

} // namespace mosaic::widgets
