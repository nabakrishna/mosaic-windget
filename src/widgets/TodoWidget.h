#pragma once
#include <vector>
#include <cstdint>
#include "widgets/IWidget.h"
#include "ui/components/IconButton.h"
#include "data/TodoRepository.h"

namespace mosaic::widgets {

// The one widget Phase 3 makes fully real: every checkbox, the "+" button,
// and per-row delete all read/write through TodoRepository (SQLite), and
// inline editing (add a task / double-click to rename) uses real keyboard
// input, not a placeholder.
//
// Intentionally NOT implemented here: drag-to-reorder within the list.
// `sort_order` already exists in the schema, but the actual drag gesture
// needs the same hit-testing/animation machinery the Phase 6 adaptive grid
// builds for moving whole widgets — implementing a second, different drag
// system now just to reorder list rows would mean throwing it away when
// Phase 6 lands. Manual reordering arrives together with widget dragging.
class TodoWidget : public IWidget {
public:
    explicit TodoWidget(data::TodoRepository* repository);

    WidgetId Id() const override { return WidgetId::Todo; }
    WidgetMetadata Metadata() const override;

    void Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) override;

    bool OnMouseMove(D2D1_POINT_2F pt) override;
    bool OnMouseLeave() override;
    bool OnLButtonDown(D2D1_POINT_2F pt) override;
    bool OnDoubleClick(D2D1_POINT_2F pt) override;
    bool OnChar(wchar_t ch) override;
    bool OnKeyDown(unsigned int virtualKey) override;

private:
    struct RowLayout {
        int64_t id = 0;
        D2D1_RECT_F rowBounds{};      // full row (used for hover/double-click hit-testing)
        D2D1_RECT_F checkboxBounds{};
        D2D1_RECT_F deleteBounds{};    // only meaningful when this row is hovered
    };

    void Refresh();               // reloads m_tasks from the repository
    void StartAddingNewTask();
    void StartEditing(int64_t id, const std::wstring& currentTitle);
    void CommitEdit();
    void CancelEdit();
    const RowLayout* FindRowAt(D2D1_POINT_2F pt) const;

    data::TodoRepository* m_repository;
    std::vector<data::TaskRecord> m_tasks;
    std::vector<RowLayout> m_rowLayouts; // rebuilt every Render call

    ui::components::IconButton m_addButton;
    int m_hoveredRowIndex = -1;

    int64_t m_editingId = -1;      // -1 == not editing anything
    std::wstring m_editBuffer;
    bool m_editingIsNewTask = false; // Escape deletes instead of reverting
};

} // namespace mosaic::widgets
