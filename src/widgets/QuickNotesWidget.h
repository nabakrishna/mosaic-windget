#pragma once
#include "widgets/IWidget.h"

namespace mosaic::widgets {

// Shows the locked placeholder state only. Phase 8 adds the actual
// DPAPI-encrypted note storage and Windows Hello/password unlock flow —
// intentionally not stubbed further here, since a half-working "unlock"
// button with no real encryption behind it would be worse than an honest
// placeholder.
class QuickNotesWidget : public IWidget {
public:
    WidgetId Id() const override { return WidgetId::QuickNotes; }
    WidgetMetadata Metadata() const override {
        return WidgetMetadata{ WidgetId::QuickNotes, L"Quick Notes", { 2, 2 }, { 1, 1 }, { 2, 3 } };
    }
    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;
};

} // namespace mosaic::widgets
