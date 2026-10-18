#pragma once

#include "settings/settings_store.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace xiui
{
    inline constexpr const char* cast_job_names[]{"WAR", "MNK", "WHM", "BLM", "RDM", "THF", "PLD", "DRK", "BST", "BRD", "RNG", "SAM", "NIN", "DRG", "SMN", "BLU", "COR", "PUP", "DNC", "SCH", "GEO", "RUN"};

    struct cast_adjustments
    {
        bool enabled{};
        std::array<float, 22> jobs{};
        float rdm_sub{}, cure{}, singing{}, summoning{};

        auto Load(const settings_store& store) -> void
        {
            this->enabled = store.ReadBool("castbar.fastcast", "enabled", false);
            const auto read = [&](const char* key) { return std::clamp(store.ReadFloat("castbar.fastcast", key, 0), 0.0f, 80.0f); };
            for (size_t i = 0; i < this->jobs.size(); ++i) this->jobs[i] = read(cast_job_names[i]);
            this->rdm_sub = read("rdm_sub"); this->cure = read("cure"); this->singing = read("singing"); this->summoning = read("summoning");
        }
        auto Save(const settings_store& store) const -> void
        {
            store.WriteBool("castbar.fastcast", "enabled", this->enabled);
            for (size_t i = 0; i < this->jobs.size(); ++i) store.WriteFloat("castbar.fastcast", cast_job_names[i], this->jobs[i]);
            for (const auto& [key, value] : {std::pair{"rdm_sub", this->rdm_sub}, {"cure", this->cure}, {"singing", this->singing}, {"summoning", this->summoning}})
                store.WriteFloat("castbar.fastcast", key, value);
        }
        auto Progress(float progress, const uint32_t job, const uint32_t subjob, const uint32_t spell, const uint32_t skill) const -> float
        {
            if (!this->enabled || job == 0 || job > this->jobs.size()) return progress;
            const auto percent = [](float value) { return std::isfinite(value) ? std::clamp(value / 100, 0.0f, 0.8f) : 0.0f; };
            float reduction = percent(this->jobs[job - 1]);
            const bool cureSpell = (spell >= 1 && spell <= 11) || spell == 855;
            const float special = job == 3 && skill == 33 && cureSpell ? this->cure :
                job == 10 && skill == 40 ? this->singing : job == 15 && skill == 38 ? this->summoning : 0;
            reduction = 1 - (1 - reduction) * (1 - percent(special));
            if (subjob == 5) reduction += percent(this->rdm_sub);
            return std::clamp(progress / (0.75f * (1 - std::min(reduction, 0.8f))), 0.0f, 1.0f);
        }
    };
}
