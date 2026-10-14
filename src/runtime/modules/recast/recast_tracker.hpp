#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace xiui
{
    enum class recast_kind : uint8_t { ability, spell };

    struct recast_sample
    {
        recast_kind kind{};
        uint32_t id{}, charges{}, maximum_charges{};
        float remaining{}, charge_duration{};
        std::string name{};
    };

    struct recast_entry
    {
        recast_kind kind{};
        uint32_t id{}, charges{}, maximum_charges{};
        std::string name{};
        float duration{};
        uint64_t expires{}, started{}, ready_at{}, sampled{};
        bool ready{};

        auto Remaining(uint64_t now) const -> float;
        auto Progress(uint64_t now) const -> float;
        auto Visible(uint64_t now, float readySeconds) const -> bool;
    };

    class recast_tracker
    {
    public:
        auto Clear(void) -> void { this->entries_.clear(); }
        auto Observe(const recast_sample& sample, uint64_t now) -> void;
        auto FinishSample(uint64_t now) -> void;
        auto Entries(void) const -> std::span<const recast_entry> { return this->entries_; }

    private:
        std::vector<recast_entry> entries_{};
    };

    auto FormatRecastTime(float seconds, bool tenths) -> std::string;
}
