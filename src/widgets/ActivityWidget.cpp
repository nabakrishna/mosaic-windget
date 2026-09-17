#include "widgets/ActivityWidget.h"
#include "widgets/ActivityDateTime.h"
#include "ui/components/WidgetCard.h"
#include "ui/Icons.h"
#include <windows.h>
#include <algorithm>

using Microsoft::WRL::ComPtr;
using namespace mosaic::ui;
using namespace mosaic::ui::components;
namespace dt = mosaic::widgets::activity_datetime;

namespace mosaic::widgets {

namespace {
constexpr float kBlockHeight = 46.0f;
constexpr float kCheckboxSize = 14.0f;
} // namespace

ActivityWidget::ActivityWidget(data::ActivityRepository* repository) : m_repository(repository) {
    Refresh();
}

void ActivityWidget::Refresh() {
    m_activities = m_repository->GetUpcoming();
}

void ActivityWidget::Render(ID2D1DeviceContext* ctx, D2D1_RECT_F bounds, const WidgetRenderResources& res) {
    const ThemeColors& colors = res.theme->Colors();
    const ThemeMetrics& metrics = res.theme->Metrics();

    D2D1_RECT_F content = WidgetCard::DrawFrame(ctx, res.brush, res.titleFormat, *res.theme, bounds, L"Special Activity");

    D2D1_POINT_2F addCenter = { bounds.right - metrics.cardPadding - 10.0f, bounds.top + metrics.cardPadding + 8.0f };
    m_addButton.SetBounds(addCenter, 10.0f);
    m_addButton.Draw(ctx, res.brush, icons::IconKind::Plus, *res.theme);

    m_rowLayouts.clear();
    float y = content.top;

    // --- pending "new activity" block, if creation is in progress ---------
    bool creatingNew = (m_editStage != EditStage::None && m_editingId == -1);
    if (creatingNew && y + kBlockHeight <= content.bottom) {
        D2D1_RECT_F starRect = { content.left, y + 3.0f, content.left + 12.0f, y + 15.0f };
        icons::Draw(ctx, res.brush, icons::IconKind::Star, starRect, colors.accentAmber);

        D2D1_RECT_F titleRect = { content.left + 20.0f, y, content.right, y + 18.0f };
        D2D1_RECT_F whenRect = { content.left + 20.0f, y + 20.0f, content.right, y + 36.0f };

        if (m_editStage == EditStage::Title) {
            res.brush->SetColor(colors.textPrimary);
            ctx->DrawText(m_editBuffer.c_str(), static_cast<UINT32>(m_editBuffer.size()),
                          res.bodyFormat, titleRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
            res.brush->SetColor(colors.textMuted);
            static const wchar_t* kHint = L"type a title, then Enter";
            ctx->DrawText(kHint, static_cast<UINT32>(wcslen(kHint)),
                          res.smallFormat, whenRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        } else { // DateTime stage
            res.brush->SetColor(colors.textPrimary);
            ctx->DrawText(m_pendingTitle.c_str(), static_cast<UINT32>(m_pendingTitle.size()),
                          res.bodyFormat, titleRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);

            D2D1_COLOR_F whenColor = m_editHasError ? D2D1_COLOR_F{ 0.90f, 0.45f, 0.45f, 1.0f } : colors.textSecondary;
            std::wstring shown = m_editBuffer.empty() ? L"e.g. tomorrow 6:00 pm" : m_editBuffer;
            res.brush->SetColor(m_editBuffer.empty() ? colors.textMuted : whenColor);
            ctx->DrawText(shown.c_str(), static_cast<UINT32>(shown.size()),
                          res.smallFormat, whenRect, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        y += kBlockHeight;
    }

    // --- existing activities ----------------------------------------------
    for (size_t i = 0; i < m_activities.size(); ++i) {
        if (y + kBlockHeight > content.bottom) break; // compact area, no overflow

        const data::ActivityRecord& activity = m_activities[i];
        bool isHoveredRow = (static_cast<int>(i) == m_hoveredRowIndex);
        bool isEditingTitle = (activity.id == m_editingId && m_editStage == EditStage::Title);
        bool isEditingWhen = (activity.id == m_editingId && m_editStage == EditStage::DateTime);

        RowLayout layout;
        layout.id = activity.id;
        layout.rowBounds = { content.left, y, content.right, y + kBlockHeight - 6.0f };
        layout.checkboxBounds = { content.left, y + 2.0f, content.left + kCheckboxSize, y + 2.0f + kCheckboxSize };

        D2D1_ROUNDED_RECT box = D2D1::RoundedRect(layout.checkboxBounds, 4.0f, 4.0f);
        res.brush->SetColor(colors.cardBorder);
        ctx->DrawRoundedRectangle(box, res.brush, 1.2f);

        float textRight = content.right;
        if (isHoveredRow) {
            layout.deleteBounds = { content.right - 16.0f, y + 2.0f, content.right - 2.0f, y + 16.0f };
            icons::Draw(ctx, res.brush, icons::IconKind::Close, layout.deleteBounds, colors.textSecondary);
            textRight = layout.deleteBounds.left - 4.0f;
        } else {
            layout.deleteBounds = { 0, 0, 0, 0 };
        }

        D2D1_RECT_F starRect = { content.left + 20.0f, y + 3.0f, content.left + 32.0f, y + 15.0f };
        icons::Draw(ctx, res.brush, icons::IconKind::Star, starRect, colors.accentAmber);

        layout.titleBounds = { content.left + 40.0f, y, textRight, y + 18.0f };
        layout.whenBounds = { content.left + 40.0f, y + 20.0f, textRight, y + 36.0f };

        if (isEditingTitle) {
            res.brush->SetColor(colors.textPrimary);
            ctx->DrawText(m_editBuffer.c_str(), static_cast<UINT32>(m_editBuffer.size()),
                          res.bodyFormat, layout.titleBounds, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        } else {
            res.brush->SetColor(colors.textPrimary);
            ctx->DrawText(activity.title.c_str(), static_cast<UINT32>(activity.title.size()),
                          res.bodyFormat, layout.titleBounds, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        if (isEditingWhen) {
            D2D1_COLOR_F whenColor = m_editHasError ? D2D1_COLOR_F{ 0.90f, 0.45f, 0.45f, 1.0f } : colors.textPrimary;
            res.brush->SetColor(whenColor);
            ctx->DrawText(m_editBuffer.c_str(), static_cast<UINT32>(m_editBuffer.size()),
                          res.smallFormat, layout.whenBounds, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        } else {
            std::wstring whenLabel = dt::FormatForDisplay(activity.dueAt);
            res.brush->SetColor(colors.textSecondary);
            ctx->DrawText(whenLabel.c_str(), static_cast<UINT32>(whenLabel.size()),
                          res.smallFormat, layout.whenBounds, res.brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        m_rowLayouts.push_back(layout);
        y += kBlockHeight;
    }

    // --- static motivational line, only if there's genuinely room left ----
    if (y + 16.0f <= content.bottom) {
        static const std::wstring kNote = L"Small steps every day lead to big results.";
        D2D1_RECT_F noteRect = { content.left, y + 6.0f, content.right, content.bottom };
        res.brush->SetColor(colors.textMuted);
        ctx->DrawText(kNote.c_str(), static_cast<UINT32>(kNote.size()), res.smallFormat, noteRect, res.brush,
                      D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

const ActivityWidget::RowLayout* ActivityWidget::FindRowAt(D2D1_POINT_2F pt) const {
    for (const auto& row : m_rowLayouts) {
        if (pt.x >= row.rowBounds.left && pt.x <= row.rowBounds.right &&
            pt.y >= row.rowBounds.top && pt.y <= row.rowBounds.bottom) {
            return &row;
        }
    }
    return nullptr;
}

bool ActivityWidget::OnMouseMove(D2D1_POINT_2F pt) {
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

bool ActivityWidget::OnMouseLeave() {
    bool changed = false;
    if (m_addButton.IsHovered()) { m_addButton.SetHovered(false); changed = true; }
    if (m_hoveredRowIndex != -1) { m_hoveredRowIndex = -1; changed = true; }
    return changed;
}

bool ActivityWidget::OnLButtonDown(D2D1_POINT_2F pt) {
    bool wasEditing = (m_editStage != EditStage::None);
    if (wasEditing) AbandonCurrentEdit();

    if (m_addButton.HitTest(pt)) {
        StartNewActivity();
        return true;
    }

    const RowLayout* row = FindRowAt(pt);
    if (!row) return wasEditing;

    if (pt.x >= row->checkboxBounds.left && pt.x <= row->checkboxBounds.right &&
        pt.y >= row->checkboxBounds.top && pt.y <= row->checkboxBounds.bottom) {
        m_repository->SetCompleted(row->id, true);
        Refresh();
        return true;
    }

    bool hasDeleteZone = (row->deleteBounds.right > row->deleteBounds.left);
    if (hasDeleteZone &&
        pt.x >= row->deleteBounds.left && pt.x <= row->deleteBounds.right &&
        pt.y >= row->deleteBounds.top && pt.y <= row->deleteBounds.bottom) {
        m_repository->Remove(row->id);
        Refresh();
        return true;
    }

    return wasEditing;
}

bool ActivityWidget::OnDoubleClick(D2D1_POINT_2F pt) {
    if (m_editStage != EditStage::None) return false; // one edit at a time, kept simple

    const RowLayout* row = FindRowAt(pt);
    if (!row) return false;

    auto it = std::find_if(m_activities.begin(), m_activities.end(),
                            [&](const data::ActivityRecord& a) { return a.id == row->id; });
    if (it == m_activities.end()) return false;

    if (pt.x >= row->titleBounds.left && pt.x <= row->titleBounds.right &&
        pt.y >= row->titleBounds.top && pt.y <= row->titleBounds.bottom) {
        StartEditingTitle(it->id, it->title);
        return true;
    }
    if (pt.x >= row->whenBounds.left && pt.x <= row->whenBounds.right &&
        pt.y >= row->whenBounds.top && pt.y <= row->whenBounds.bottom) {
        StartEditingDateTime(it->id, it->dueAt);
        return true;
    }
    return false;
}

bool ActivityWidget::OnChar(wchar_t ch) {
    if (m_editStage == EditStage::None) return false;
    if (ch < 0x20) return false;
    m_editBuffer.push_back(ch);
    m_editHasError = false; // typing again means "trying to fix it"
    return true;
}

bool ActivityWidget::OnKeyDown(unsigned int virtualKey) {
    if (m_editStage == EditStage::None) return false;

    switch (virtualKey) {
    case VK_BACK:
        if (!m_editBuffer.empty()) { m_editBuffer.pop_back(); m_editHasError = false; return true; }
        return false;

    case VK_RETURN: {
        if (m_editStage == EditStage::Title) {
            if (m_editBuffer.empty()) {
                AbandonCurrentEdit();
                return true;
            }
            if (m_editingId == -1) {
                m_pendingTitle = m_editBuffer;
                m_editBuffer.clear();
                m_editStage = EditStage::DateTime;
            } else {
                m_repository->Rename(m_editingId, m_editBuffer);
                Refresh();
                AbandonCurrentEdit();
            }
            return true;
        }

        // DateTime stage.
        auto parsed = dt::Parse(m_editBuffer);
        if (!parsed) {
            m_editHasError = true;
            return true; // stay in edit mode — spec section 60: never lose input to a silent failure
        }
        if (m_editingId == -1) {
            m_repository->Add(m_pendingTitle, *parsed, 0);
        } else {
            m_repository->Reschedule(m_editingId, *parsed);
        }
        Refresh();
        AbandonCurrentEdit();
        return true;
    }

    case VK_ESCAPE:
        AbandonCurrentEdit();
        return true;

    default:
        return false;
    }
}

void ActivityWidget::StartNewActivity() {
    m_editStage = EditStage::Title;
    m_editingId = -1;
    m_editBuffer.clear();
    m_pendingTitle.clear();
    m_editHasError = false;
}

void ActivityWidget::StartEditingTitle(int64_t id, const std::wstring& currentTitle) {
    m_editStage = EditStage::Title;
    m_editingId = id;
    m_editBuffer = currentTitle;
    m_editHasError = false;
}

void ActivityWidget::StartEditingDateTime(int64_t id, int64_t currentDueAt) {
    m_editStage = EditStage::DateTime;
    m_editingId = id;
    m_editBuffer = dt::FormatForEditing(currentDueAt);
    m_editHasError = false;
}

void ActivityWidget::AbandonCurrentEdit() {
    // Always safe: nothing is ever written to the database until Enter
    // succeeds on the DateTime stage (see class comment) or an existing
    // title's Enter is pressed — so "abandon" never needs to undo anything.
    m_editStage = EditStage::None;
    m_editingId = -1;
    m_editBuffer.clear();
    m_pendingTitle.clear();
    m_editHasError = false;
}

} // namespace mosaic::widgets
