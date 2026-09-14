#pragma once
#include <string>
#include <vector>
#include "widgets/IWidget.h"

namespace mosaic::widgets {

// Same static sample list DashboardView drew inline in Phases 1–2. Phase 4
// gives this a real PinnedRepository plus add/remove/reorder/open.
class PinnedWidget : public IWidget {
public:
    WidgetId Id() const override { return WidgetId::Pinned; }
    WidgetMetadata Metadata() const override {
        return WidgetMetadata{ WidgetId::Pinned, L"Pinned", { 2, 2 }, { 1, 1 }, { 2, 3 } };
    }
    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;

private:
    std::vector<std::wstring> m_items = { L"Study Plan", L"Project Ideas" };
};

} // namespace mosaic::widgets
