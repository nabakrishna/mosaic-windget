#pragma once
#include <string>
#include "widgets/IWidget.h"

namespace mosaic::widgets {

// Same static sample content DashboardView drew inline in Phases 1–2, now
// behind IWidget. Phase 4 replaces the hardcoded fields below with a real
// ActivityRepository (SQLite, same pattern as TodoRepository) plus
// create/edit/delete/snooze and Windows toast notifications.
class ActivityWidget : public IWidget {
public:
    WidgetId Id() const override { return WidgetId::Activity; }
    WidgetMetadata Metadata() const override {
        return WidgetMetadata{ WidgetId::Activity, L"Special Activity", { 1, 2 }, { 1, 1 }, { 2, 3 } };
    }
    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;

private:
    // Placeholder until Phase 4's ActivityRepository exists.
    std::wstring m_title = L"Gym Session";
    std::wstring m_when = L"Tomorrow, 6:00 PM";
    std::wstring m_note = L"Small steps every day lead to big results.";
};

} // namespace mosaic::widgets
