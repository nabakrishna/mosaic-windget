#include "widgets/TodoWidget.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"
#include <windows.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;
using namespace mosaic::ui;
using namespace mosaic::ui::components;

namespace mosaic::widgets {

TodoWidget::TodoWidget(data::TodoRepository* repository) : m_repository(repository) {
    Refresh();
}

WidgetMetadata TodoWidget::Metadata() const {
    return WidgetMetadata{ WidgetId::Todo, L"To Do", { 2, 3 }, { 1, 2 }, { 3, 5 } };
}

void TodoWidget::Refresh() {
    m_tasks = m_repository->GetAll();
}

void TodoWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();
    const ThemeMetrics& metrics = res.theme->Metrics();

    D2D1_RECT_F content = WidgetCard::DrawFrame(ctx, res.brush, res.titleFormat, *res.theme, bounds, L"To Do");

    // "+" add-task button, top-right of the card, vertically centered on
    // the title row.
    D2D1_POINT_2F addCenter = { bounds.right - metrics.cardPadding - 10.0f, bounds.top + metrics.cardPadding + 8.0f };
    m_addButton.SetBounds(addCenter, 10.0f);
    m_addButton.Draw(ctx, res.brush, icons::IconKind::Plus, *res.theme);

    m_rowLayouts.clear();

    const float rowHeight = 26.0f;
    const float checkboxSize = 14.0f;
    float y = content.top;

    for (size_t i = 0; i < m_tasks.size(); ++i) {
        if (y + rowHeight > content.bottom) break; // spec section 11: compact scroll area, not unbounded growth

        const data::TaskRecord& task = m_tasks[i];
        bool isHoveredRow = (static_cast<int>(i) == m_hoveredRowIndex);
        bool isEditingRow = (task.id == m_editingId);

        RowLayout layout;
        layout.id = task.id;
        layout.rowBounds = { content.left, y, content.right, y + rowHeight };
        layout.checkboxBounds = { content.left, y + 4.0f, content.left + checkboxSize, y + 4.0f + checkboxSize };

        D2D1_ROUNDED_RECT box = D2D1::RoundedRect(layout.checkboxBounds, 4.0f, 4.0f);
        if (task.completed) {
            res.brush->SetColor(colors.accentGreen);
            ctx->FillRoundedRectangle(box, res.brush);
            D2D1_RECT_F checkInset = {
                layout.checkboxBounds.left + 2.0f, layout.checkboxBounds.top + 2.0f,
                layout.checkboxBounds.right - 2.0f, layout.checkboxBounds.bottom - 2.0f
            };
            icons::Draw(ctx, res.brush, icons::IconKind::Check, checkInset, colors.textPrimary);
        } else {
            res.brush->SetColor(colors.cardBorder);
            ctx->DrawRoundedRectangle(box, res.brush, 1.2f);
        }

        // Reserve space on the right for the delete "x" only while this
        // row is hovered — otherwise the rect stays zero-width, which
        // OnLButtonDown's hit-test treats as "not clickable" automatically.
        float textRight = content.right;
        if (isHoveredRow) {
            layout.deleteBounds = { content.right - 16.0f, y + 5.0f, content.right - 2.0f, y + 19.0f };
            icons::Draw(ctx, res.brush, icons::IconKind::Close, layout.deleteBounds, colors.textSecondary);
            textRight = layout.deleteBounds.left - 4.0f;
        } else {
            layout.deleteBounds = { 0, 0, 0, 0 };
        }

        D2D1_RECT_F textRect = { content.left + checkboxSize + 10.0f, y, textRight, y + rowHeight };

        if (isEditingRow) {
            // Editable state: draw the live buffer plus a measured caret,
            // and a thin underline so it reads as "currently editable"
            // rather than plain text.
            res.brush->SetColor(colors.textPrimary);
            ctx->DrawText(m_editBuffer.c_str(), static_cast<UINT32>(m_editBuffer.size()),
                          res.bodyFormat, textRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);

            float caretX = textRect.left;
            if (res.dwriteFactory && !m_editBuffer.empty()) {
                ComPtr<IDWriteTextLayout> measureLayout;
                HRESULT hr = res.dwriteFactory->CreateTextLayout(
                    m_editBuffer.c_str(), static_cast<UINT32>(m_editBuffer.size()), res.bodyFormat,
                    textRect.right - textRect.left, textRect.bottom - textRect.top, &measureLayout);
                if (SUCCEEDED(hr)) {
                    DWRITE_TEXT_METRICS tm{};
                    measureLayout->GetMetrics(&tm);
                    caretX = textRect.left + tm.widthIncludingTrailingWhitespace;
                }
            }
            caretX = std::min(caretX, textRect.right - 1.0f);
            res.brush->SetColor(colors.accentViolet);
            ctx->DrawLine({ caretX, textRect.top + 2.0f }, { caretX, textRect.bottom - 6.0f }, res.brush, 1.5f);

            D2D1_RECT_F underline = { textRect.left, textRect.bottom - 3.0f, textRect.right, textRect.bottom - 2.0f };
            res.brush->SetColor(colors.cardBorder);
            ctx->FillRectangle(underline, res.brush);
        } else {
            res.brush->SetColor(task.completed ? colors.textCompleted : colors.textPrimary);
            ctx->DrawText(task.title.c_str(), static_cast<UINT32>(task.title.size()),
                          res.bodyFormat, textRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        m_rowLayouts.push_back(layout);
        y += rowHeight;
    }
}

const TodoWidget::RowLayout* TodoWidget::FindRowAt(D2D1_POINT_2F pt) const {
    for (const auto& row : m_rowLayouts) {
        if (pt.x >= row.rowBounds.left && pt.x <= row.rowBounds.right &&
            pt.y >= row.rowBounds.top && pt.y <= row.rowBounds.bottom) {
            return &row;
        }
    }
    return nullptr;
}

bool TodoWidget::OnMouseMove(D2D1_POINT_2F pt) {
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
        changed = true; // the delete "x" needs to appear/disappear
    }

    return changed;
}

bool TodoWidget::OnMouseLeave() {
    bool changed = false;
    if (m_addButton.IsHovered()) { m_addButton.SetHovered(false); changed = true; }
    if (m_hoveredRowIndex != -1) { m_hoveredRowIndex = -1; changed = true; }
    return changed;
}

bool TodoWidget::OnLButtonDown(D2D1_POINT_2F pt) {
    if (m_addButton.HitTest(pt)) {
        if (m_editingId != -1) CommitEdit();
        StartAddingNewTask();
        return true;
    }

    const RowLayout* row = FindRowAt(pt);
    bool changed = false;

    if (m_editingId != -1 && (!row || row->id != m_editingId)) {
        CommitEdit();
        changed = true;
    }

    if (!row) return changed;

    if (pt.x >= row->checkboxBounds.left && pt.x <= row->checkboxBounds.right &&
        pt.y >= row->checkboxBounds.top && pt.y <= row->checkboxBounds.bottom) {
        auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                                [&](const data::TaskRecord& t) { return t.id == row->id; });
        if (it != m_tasks.end()) {
            m_repository->SetCompleted(it->id, !it->completed);
            Refresh();
        }
        return true;
    }

    bool hasDeleteZone = (row->deleteBounds.right > row->deleteBounds.left);
    if (hasDeleteZone &&
        pt.x >= row->deleteBounds.left && pt.x <= row->deleteBounds.right &&
        pt.y >= row->deleteBounds.top && pt.y <= row->deleteBounds.bottom) {
        m_repository->Remove(row->id);
        if (m_editingId == row->id) { m_editingId = -1; m_editingIsNewTask = false; }
        Refresh();
        return true;
    }

    return changed;
}

bool TodoWidget::OnDoubleClick(D2D1_POINT_2F pt) {
    const RowLayout* row = FindRowAt(pt);
    if (!row) return false;

    auto it = std::find_if(m_tasks.begin(), m_tasks.end(),
                            [&](const data::TaskRecord& t) { return t.id == row->id; });
    if (it == m_tasks.end()) return false;

    StartEditing(it->id, it->title);
    return true;
}

bool TodoWidget::OnChar(wchar_t ch) {
    if (m_editingId == -1) return false;
    if (ch < 0x20) return false; // control characters (Enter/Backspace/Escape) are handled in OnKeyDown
    m_editBuffer.push_back(ch);
    return true;
}

bool TodoWidget::OnKeyDown(unsigned int virtualKey) {
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

void TodoWidget::StartAddingNewTask() {
    int64_t id = m_repository->Add(L"New task");
    Refresh();
    if (id != 0) {
        m_editingId = id;
        m_editBuffer = L"New task";
        m_editingIsNewTask = true;
    }
}

void TodoWidget::StartEditing(int64_t id, const std::wstring& currentTitle) {
    m_editingId = id;
    m_editBuffer = currentTitle;
    m_editingIsNewTask = false;
}

void TodoWidget::CommitEdit() {
    if (m_editingId == -1) return;
    if (!m_editBuffer.empty()) {
        m_repository->Rename(m_editingId, m_editBuffer);
    } else if (m_editingIsNewTask) {
        // Typed nothing for a brand-new task — remove it rather than
        // leaving an empty row behind.
        m_repository->Remove(m_editingId);
    }
    m_editingId = -1;
    m_editingIsNewTask = false;
    Refresh();
}

void TodoWidget::CancelEdit() {
    if (m_editingId == -1) return;
    if (m_editingIsNewTask) {
        m_repository->Remove(m_editingId);
    }
    m_editingId = -1;
    m_editingIsNewTask = false;
    Refresh();
}

} // namespace mosaic::widgets
