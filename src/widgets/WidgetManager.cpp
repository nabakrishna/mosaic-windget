#include "widgets/WidgetManager.h"
#include "widgets/TodoWidget.h"
#include "widgets/PhotoWidget.h"
#include "widgets/ActivityWidget.h"
#include "widgets/PinnedWidget.h"
#include "widgets/QuickNotesWidget.h"

namespace mosaic::widgets {

WidgetManager::WidgetManager(data::TodoRepository* todoRepository)
    : m_todoRepository(todoRepository) {}

void WidgetManager::Initialize() {
    m_registry.Register(WidgetId::Todo,
        [this] { return std::make_unique<TodoWidget>(m_todoRepository); });
    m_registry.Register(WidgetId::Photo,
        [] { return std::make_unique<PhotoWidget>(); });
    m_registry.Register(WidgetId::Activity,
        [] { return std::make_unique<ActivityWidget>(); });
    m_registry.Register(WidgetId::Pinned,
        [] { return std::make_unique<PinnedWidget>(); });
    m_registry.Register(WidgetId::QuickNotes,
        [] { return std::make_unique<QuickNotesWidget>(); });

    for (WidgetId id : { WidgetId::Todo, WidgetId::Photo, WidgetId::Activity,
                         WidgetId::Pinned, WidgetId::QuickNotes }) {
        m_widgets[id] = m_registry.Create(id);
    }
}

IWidget* WidgetManager::Get(WidgetId id) const {
    auto it = m_widgets.find(id);
    return it == m_widgets.end() ? nullptr : it->second.get();
}

} // namespace mosaic::widgets
