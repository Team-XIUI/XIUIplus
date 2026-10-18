#pragma once

#include "action_store.hpp"

#include <string_view>

namespace xiui
{
    struct action_import_preview
    {
        std::vector<action_palette> palettes{};
        std::vector<std::string> warnings{};
        std::string error{};
        size_t actions{}, unsupported{}, conflicts{};
        bool source_context{};
        auto Valid(void) const -> bool { return this->error.empty() && !this->palettes.empty(); }
    };
    auto PreviewActionImport(std::string_view text, std::string_view sourceName = {}) -> action_import_preview;
    auto CommitActionImport(action_store& store, const action_import_preview& preview,
        action_surface surface = action_surface::hotbar, size_t bar = 0) -> size_t;
}
