#pragma once

#include "module.hpp"

#include <cstddef>
#include <vector>

namespace xiui
{
    class module_registry
    {
    public:
        auto Add(hud_module& module) -> void
        {
            this->modules_.push_back(&module);
        }

        auto Count(void) const -> size_t
        {
            return this->modules_.size();
        }

        auto At(const size_t index) const -> hud_module*
        {
            return index < this->modules_.size() ? this->modules_[index] : nullptr;
        }

        auto All(void) -> std::vector<hud_module*>&
        {
            return this->modules_;
        }

        auto All(void) const -> const std::vector<hud_module*>&
        {
            return this->modules_;
        }

    private:
        std::vector<hud_module*> modules_{};
    };
} // namespace xiui
