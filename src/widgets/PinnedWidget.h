#pragma once
#include <vector>
#include <cstdint>
#include "widgets/IWidget.h"
#include "ui/components/IconButton.h"
#include "data/PinnedRepository.h"

namespace mosaic::widgets {

// Pinned, fully real as of Phase 4. Simpler than Special Activity — a
// pinned item is just a title, so this follows TodoWidget's single-field
// inline-edit pattern rather than ActivityWidget's two-stage one: "+"
// immediately creates the row and starts editing it, matching TodoWidget
// exactly, because there's no second field that could be invalid.
//
// Not implemented: drag-to-reorder (same reasoning as To Do — deferred to
// Phase 6's shared drag machinery) and "open" (spec section 13) — a pinned
// item doesn't yet carry a file/folder/link association to open, only a
// title. Both are documented gaps, not silent omissions.
class PinnedWidget : public IWidget {
public:
    explicit PinnedWidget(data::PinnedRepository* repository);

    WidgetId Id() const override { return WidgetId::Pinned; }
    WidgetMetadata Metadata() const override {
        return WidgetMetadata{ WidgetId::Pinned, L"Pinned", { 2, 2 }, { 1, 1 }, { 2, 3 } };
    }

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
        D2D1_RECT_F rowBounds{};
        D2D1_RECT_F deleteBounds{};
    };

    void Refresh();
    void StartAddingNewItem();
    void StartEditing(int64_t id, const std::wstring& currentTitle);
    void CommitEdit();
    void CancelEdit();
    const RowLayout* FindRowAt(D2D1_POINT_2F pt) const;

    data::PinnedRepository* m_repository;
    std::vector<data::PinnedRecord> m_items;
    std::vector<RowLayout> m_rowLayouts;

    ui::components::IconButton m_addButton;
    int m_hoveredRowIndex = -1;

    int64_t m_editingId = -1;
    std::wstring m_editBuffer;
    bool m_editingIsNewItem = false;
};

} // namespace mosaic::widgets
