#include "widgets/PinnedWidget.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"
#include <windows.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;
using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

namespace {
constexpr float kRowHeight = 26.0f;
} // namespace

PinnedWidget::PinnedWidget(data::PinnedRepository* repository) : m_repository(repository) {
    Refresh();
}

void PinnedWidget::Refresh() {
    m_items = m_repository->GetAll();
}

void PinnedWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();
    const ThemeMetrics& metrics = res.theme->Metrics();

    D2D1_RECT_F content = WidgetCard::DrawFrame(ctx, res.brush, res.titleFormat, *res.theme, bounds, L"Pinned");

    D2D1_POINT_2F addCenter = { bounds.right - metrics.cardPadding - 10.0f, bounds.top + metrics.cardPadding + 8.0f };
    m_addButton.SetBounds(addCenter, 10.0f);
    m_addButton.Draw(ctx, res.brush, icons::IconKind::Plus, *res.theme);

    m_rowLayouts.clear();
    float y = content.top;

    for (size_t i = 0; i < m_items.size(); ++i) {
        if (y + kRowHeight > content.bottom) break;

        const data::PinnedRecord& item = m_items[i];
        bool isHoveredRow = (static_cast<int>(i) == m_hoveredRowIndex);
        bool isEditing = (item.id == m_editingId);

        RowLayout layout;
        layout.id = item.id;
        layout.rowBounds = { content.left, y, content.right, y + kRowHeight };

        D2D1_RECT_F pinRect = { content.left, y + 1.0f, content.left + 12.0f, y + 13.0f };
        icons::Draw(ctx, res.brush, icons::IconKind::Pin, pinRect, colors.accentViolet);

        float textRight = content.right;
        if (isHoveredRow) {
            layout.deleteBounds = { content.right - 16.0f, y + 5.0f, content.right - 2.0f, y + 19.0f };
            icons::Draw(ctx, res.brush, icons::IconKind::Close, layout.deleteBounds, colors.textSecondary);
            textRight = layout.deleteBounds.left - 4.0f;
        } else {
            layout.deleteBounds = { 0, 0, 0, 0 };
        }

        D2D1_RECT_F textRect = { content.left + 20.0f, y, textRight, y + 18.0f };

        if (isEditing) {
            res.brush->SetColor(colors.textPrimary);
            ctx->DrawText(m_editBuffer.c_str(), static_cast<UINT32>(m_editBuffer.size()),
                          res.bodyFormat, textRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);

            D2D1_RECT_F underline = { textRect.left, y + 19.0f, textRect.right, y + 20.0f };
            res.brush->SetColor(colors.cardBorder);
            ctx->FillRectangle(underline, res.brush);
        } else {
            res.brush->SetColor(colors.textPrimary);
            ctx->DrawText(item.title.c_str(), static_cast<UINT32>(item.title.size()),
                          res.bodyFormat, textRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        m_rowLayouts.push_back(layout);
        y += kRowHeight;
    }
}

const PinnedWidget::RowLayout* PinnedWidget::FindRowAt(D2D1_POINT_2F pt) const {
    for (const auto& row : m_rowLayouts) {
        if (pt.x >= row.rowBounds.left && pt.x <= row.rowBounds.right &&
            pt.y >= row.rowBounds.top && pt.y <= row.rowBounds.bottom) {
            return &row;
        }
    }
    return nullptr;
}

bool PinnedWidget::OnMouseMove(D2D1_POINT_2F pt) {
    bool changed = false;

    bool addHovered = m_addButton.HitTest(pt);
    if (addHovered != m_addButton.IsHovered()) {
        m_addButton.SetHovered(addHovered);
        changed = true;
    }

    int newHoverIndex = -1;
    for (size_t i = 0; i < m_rowLayouts.size(); ++i) {
        const auto& r = m_rowLayouts[i];
        if (pt.x >= r.rowBounds.left && pt.x <= r.rowBounds.right &&
            pt.y >= r.rowBounds.top && pt.y <= r.rowBounds.bottom) {
            newHoverIndex = static_cast<int>(i);
            break;
        }
    }
    if (newHoverIndex != m_hoveredRowIndex) {
        m_hoveredRowIndex = newHoverIndex;
        changed = true;
    }

    return changed;
}

bool PinnedWidget::OnMouseLeave() {
    bool changed = false;
    if (m_addButton.IsHovered()) { m_addButton.SetHovered(false); changed = true; }
    if (m_hoveredRowIndex != -1) { m_hoveredRowIndex = -1; changed = true; }
    return changed;
}

bool PinnedWidget::OnLButtonDown(D2D1_POINT_2F pt) {
    if (m_addButton.HitTest(pt)) {
        if (m_editingId != -1) CommitEdit();
        StartAddingNewItem();
        return true;
    }

    const RowLayout* row = FindRowAt(pt);
    bool changed = false;

    if (m_editingId != -1 && (!row || row->id != m_editingId)) {
        CommitEdit();
        changed = true;
    }

    if (!row) return changed;

    bool hasDeleteZone = (row->deleteBounds.right > row->deleteBounds.left);
    if (hasDeleteZone &&
        pt.x >= row->deleteBounds.left && pt.x <= row->deleteBounds.right &&
        pt.y >= row->deleteBounds.top && pt.y <= row->deleteBounds.bottom) {
        m_repository->Remove(row->id);
        if (m_editingId == row->id) { m_editingId = -1; m_editingIsNewItem = false; }
        Refresh();
        return true;
    }

    return changed;
}

bool PinnedWidget::OnDoubleClick(D2D1_POINT_2F pt) {
    const RowLayout* row = FindRowAt(pt);
    if (!row) return false;

    auto it = std::find_if(m_items.begin(), m_items.end(),
                            [&](const data::PinnedRecord& p) { return p.id == row->id; });
    if (it == m_items.end()) return false;

    StartEditing(it->id, it->title);
    return true;
}

bool PinnedWidget::OnChar(wchar_t ch) {
    if (m_editingId == -1) return false;
    if (ch < 0x20) return false;
    m_editBuffer.push_back(ch);
    return true;
}

bool PinnedWidget::OnKeyDown(unsigned int virtualKey) {
    if (m_editingId == -1) return false;

    switch (virtualKey) {
    case VK_BACK:
        if (!m_editBuffer.empty()) { m_editBuffer.pop_back(); return true; }
        return false;
    case VK_RETURN:
        CommitEdit();
        return true;
    case VK_ESCAPE:
        CancelEdit();
        return true;
    default:
        return false;
    }
}

void PinnedWidget::StartAddingNewItem() {
    int64_t id = m_repository->Add(L"New pin");
    Refresh();
    if (id != 0) {
        m_editingId = id;
        m_editBuffer = L"New pin";
        m_editingIsNewItem = true;
    }
}

void PinnedWidget::StartEditing(int64_t id, const std::wstring& currentTitle) {
    m_editingId = id;
    m_editBuffer = currentTitle;
    m_editingIsNewItem = false;
}

void PinnedWidget::CommitEdit() {
    if (m_editingId == -1) return;
    if (!m_editBuffer.empty()) {
        m_repository->Rename(m_editingId, m_editBuffer);
    } else if (m_editingIsNewItem) {
        m_repository->Remove(m_editingId);
    }
    m_editingId = -1;
    m_editingIsNewItem = false;
    Refresh();
}

void PinnedWidget::CancelEdit() {
    if (m_editingId == -1) return;
    if (m_editingIsNewItem) {
        m_repository->Remove(m_editingId);
    }
    m_editingId = -1;
    m_editingIsNewItem = false;
    Refresh();
}

} // namespace mosaic::widgets
