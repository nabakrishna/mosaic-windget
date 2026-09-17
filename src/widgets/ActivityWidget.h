#pragma once
#include <vector>
#include <cstdint>
#include "widgets/IWidget.h"
#include "ui/components/IconButton.h"
#include "data/ActivityRepository.h"

namespace mosaic::widgets {

// Special Activity, fully real as of Phase 4: every checkbox, the "+"
// button, and per-row delete read/write through ActivityRepository
// (SQLite). Scheduling a real reminder needs a real absolute timestamp,
// which is why creating/editing an activity is a two-stage inline edit —
// title, then a typed date/time (see ActivityDateTime.h for the accepted
// grammar and why it's typed rather than a calendar-picker UI).
//
// Unlike TodoWidget's "add creates the DB row immediately, then edit it",
// a new activity is held entirely in widget memory until both the title
// and a valid date/time are supplied — Escape at any point during
// creation is a true no-op on the database, not a delete-what-we-just-
// inserted cleanup step.
class ActivityWidget : public IWidget {
public:
    explicit ActivityWidget(data::ActivityRepository* repository);

    WidgetId Id() const override { return WidgetId::Activity; }
    WidgetMetadata Metadata() const override {
        // Preferred size is 2x2, not spec section 16's literal "1 x 2"
        // example. That section itself frames its numbers as illustrative
        // ("these are conceptual grid units rather than fixed pixels"),
        // and the reference design image — which sections 3/4 establish
        // as the actual visual authority — clearly shows Special Activity
        // as roughly the same width as To Do and Photo, not a narrow
        // sliver. Phase 6's LayoutEngine is what actually turns these
        // numbers into a real arrangement, so this is the point where
        // that mismatch had to be resolved one way or the other; matching
        // the reference image's visual balance won out over the literal
        // spec-section example.
        return WidgetMetadata{ WidgetId::Activity, L"Special Activity", { 2, 2 }, { 1, 1 }, { 2, 3 } };
    }

    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;

    bool OnMouseMove(D2D1_POINT_2F pt) override;
    bool OnMouseLeave() override;
    bool OnLButtonDown(D2D1_POINT_2F pt) override;
    bool OnDoubleClick(D2D1_POINT_2F pt) override;
    bool OnChar(wchar_t ch) override;
    bool OnKeyDown(unsigned int virtualKey) override;

private:
    enum class EditStage { None, Title, DateTime };

    struct RowLayout {
        int64_t id = 0;
        D2D1_RECT_F rowBounds{};
        D2D1_RECT_F checkboxBounds{};
        D2D1_RECT_F titleBounds{};
        D2D1_RECT_F whenBounds{};
        D2D1_RECT_F deleteBounds{}; // only meaningful when this row is hovered
    };

    void Refresh();
    void StartNewActivity();
    void StartEditingTitle(int64_t id, const std::wstring& currentTitle);
    void StartEditingDateTime(int64_t id, int64_t currentDueAt);
    void AbandonCurrentEdit(); // always a pure no-op on the database — see class comment
    const RowLayout* FindRowAt(D2D1_POINT_2F pt) const;

    data::ActivityRepository* m_repository;
    std::vector<data::ActivityRecord> m_activities;
    std::vector<RowLayout> m_rowLayouts;

    ui::components::IconButton m_addButton;
    int m_hoveredRowIndex = -1;

    EditStage m_editStage = EditStage::None;
    int64_t m_editingId = -1;       // -1 while creating a brand-new activity
    std::wstring m_editBuffer;
    std::wstring m_pendingTitle;    // holds the title while the datetime stage is active for a new activity
    bool m_editHasError = false;    // last Enter on the datetime stage failed to parse
};

} // namespace mosaic::widgets
