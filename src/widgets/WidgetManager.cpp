#include "widgets/WidgetManager.h"
#include "widgets/TodoWidget.h"
#include "widgets/PhotoWidget.h"
#include "widgets/ActivityWidget.h"
#include "widgets/PinnedWidget.h"
#include "widgets/QuickNotesWidget.h"
#include "data/ActivityRepository.h"
#include "data/PinnedRepository.h"
#include "data/NotesRepository.h"
#include "media/IPhotoProvider.h"
#include "media/ImagePipeline.h"

namespace mosaic::widgets {

WidgetManager::WidgetManager(data::TodoRepository* todoRepository,
                              data::ActivityRepository* activityRepository,
                              data::PinnedRepository* pinnedRepository,
                              data::NotesRepository* notesRepository,
                              media::IPhotoProvider* photoProvider,
                              media::ImagePipeline* imagePipeline,
                              QuickNotesWidget::HelloRequestFn requestHello)
    : m_todoRepository(todoRepository),
      m_activityRepository(activityRepository),
      m_pinnedRepository(pinnedRepository),
      m_notesRepository(notesRepository),
      m_requestHello(std::move(requestHello)),
      m_photoProvider(photoProvider),
      m_imagePipeline(imagePipeline) {}

void WidgetManager::Initialize() {
    m_registry.Register(WidgetId::Todo,
        [this] { return std::make_unique<TodoWidget>(m_todoRepository); });
    m_registry.Register(WidgetId::Photo,
        [this] { return std::make_unique<PhotoWidget>(m_photoProvider, m_imagePipeline); });
    m_registry.Register(WidgetId::Activity,
        [this] { return std::make_unique<ActivityWidget>(m_activityRepository); });
    m_registry.Register(WidgetId::Pinned,
        [this] { return std::make_unique<PinnedWidget>(m_pinnedRepository); });
    m_registry.Register(WidgetId::QuickNotes,
        [this] { return std::make_unique<QuickNotesWidget>(m_notesRepository, m_requestHello); });

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
