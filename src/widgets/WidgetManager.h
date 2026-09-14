#pragma once
#include <map>
#include <memory>
#include "widgets/IWidget.h"
#include "widgets/WidgetRegistry.h"

namespace mosaic::data { class TodoRepository; }

namespace mosaic::widgets {

// Owns the registry and the actual widget instances that make up "the
// dashboard" today: one of each of Todo/Photo/Activity/Pinned/QuickNotes.
// Which widgets exist and in what order becomes settings-driven in Phase 7
// (spec section 20's enable/disable/reorder); for now it's simply "create
// one of everything", which is exactly what the reference design shows.
class WidgetManager {
public:
    // `todoRepository` must outlive the WidgetManager — Window/Application
    // owns the Database and TodoRepository and passes a non-owning pointer
    // in here, the same non-owning-pointer pattern DashboardView already
    // uses for ThemeManager.
    explicit WidgetManager(data::TodoRepository* todoRepository);

    void Initialize();

    IWidget* Get(WidgetId id) const;

    const std::map<WidgetId, std::unique_ptr<IWidget>>& Widgets() const { return m_widgets; }

private:
    WidgetRegistry m_registry;
    std::map<WidgetId, std::unique_ptr<IWidget>> m_widgets;
    data::TodoRepository* m_todoRepository;
};

} // namespace mosaic::widgets
