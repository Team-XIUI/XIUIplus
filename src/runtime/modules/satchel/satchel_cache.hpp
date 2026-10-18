#pragma once

#include "satchel_model.hpp"
#include "settings/settings_writer.hpp"

#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace xiui
{
    class satchel_cache
    {
    public:
        auto Initialize(std::string path) -> void;
        auto Observe(const satchel_snapshot& snapshot, uint64_t now) -> void;
        auto Characters(void) const -> const std::vector<satchel_snapshot>& { return this->characters_; }
        auto ReadOrder(const std::string& identity, int view) const -> std::vector<std::string>;
        auto WriteOrder(const std::string& identity, int view, const std::vector<std::string>& order) -> void;
        auto Stop(void) -> void;

    private:
#if defined(XIUI_ENABLE_SATCHEL_TESTS)
        friend struct satchel_data_test;
#endif
        std::string path_{};
        std::vector<satchel_snapshot> characters_{};
        std::map<std::pair<std::string, int>, std::vector<std::string>> orders_{};
        std::optional<satchel_snapshot> pending_{};
        uint64_t pending_since_{};
        std::map<std::string, std::unique_ptr<settings_writer>> writers_{};

        auto Queue(const std::string& identity) -> void;
    };
}
