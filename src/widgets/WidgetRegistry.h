#pragma once
#include <functional>
#include <map>
#include <memory>
#include "widgets/IWidget.h"

namespace mosaic::widgets {

// Maps a WidgetId to a function that constructs it. This is what spec
// section 49 means by "future widgets should be addable without rewriting
// the entire application" — adding a new widget kind later is one
// Register() call in WidgetManager::Initialize, not new branches scattered
// through DashboardView.
class WidgetRegistry {
public:
    using Factory = std::function<std::unique_ptr<IWidget>()>;

    void Register(WidgetId id, Factory factory) {
        m_factories[id] = std::move(factory);
    }

    std::unique_ptr<IWidget> Create(WidgetId id) const {
        auto it = m_factories.find(id);
        if (it == m_factories.end()) return nullptr;
        return it->second();
    }

private:
    std::map<WidgetId, Factory> m_factories;
};

} // namespace mosaic::widgets
