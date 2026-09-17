#pragma once
#include <map>
#include <memory>
#include "widgets/IWidget.h"
#include "widgets/WidgetRegistry.h"
#include "widgets/QuickNotesWidget.h"

namespace mosaic::data { class TodoRepository; class ActivityRepository; class PinnedRepository; class NotesRepository; }
namespace mosaic::media { class IPhotoProvider; class ImagePipeline; }

namespace mosaic::widgets {

// Owns the registry and the actual widget instances that make up "the
// dashboard" today: one of each of Todo/Photo/Activity/Pinned/QuickNotes.
// Which widgets exist and in what order becomes settings-driven in Phase 7
// (spec section 20's enable/disable/reorder); for now it's simply "create
// one of everything", which is exactly what the reference design shows.
class WidgetManager {
public:
    // Each pointer must outlive the WidgetManager — Window owns the
    // database, all repositories, and the photo provider/pipeline, and
    // passes non-owning pointers in here, same pattern DashboardView
    // already uses for ThemeManager.
    WidgetManager(data::TodoRepository* todoRepository,
                  data::ActivityRepository* activityRepository,
                  data::PinnedRepository* pinnedRepository,
                  data::NotesRepository* notesRepository,
                  media::IPhotoProvider* photoProvider,
                  media::ImagePipeline* imagePipeline,
                  QuickNotesWidget::HelloRequestFn requestHello);

    void Initialize();

    IWidget* Get(WidgetId id) const;

    const std::map<WidgetId, std::unique_ptr<IWidget>>& Widgets() const { return m_widgets; }

private:
    WidgetRegistry m_registry;
    std::map<WidgetId, std::unique_ptr<IWidget>> m_widgets;
    data::TodoRepository* m_todoRepository;
    data::ActivityRepository* m_activityRepository;
    data::PinnedRepository* m_pinnedRepository;
    data::NotesRepository* m_notesRepository;
    QuickNotesWidget::HelloRequestFn m_requestHello;
    media::IPhotoProvider* m_photoProvider;
    media::ImagePipeline* m_imagePipeline;
};

} // namespace mosaic::widgets
