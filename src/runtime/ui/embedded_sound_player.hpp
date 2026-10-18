#pragma once

#include "Ashita.h"
#include <mmsystem.h>
#include <array>

namespace xiui::ui
{
    class embedded_sound_player
    {
    public:
        embedded_sound_player(void) = default;
        ~embedded_sound_player(void) { this->Stop(); }
        embedded_sound_player(const embedded_sound_player&) = delete;
        auto operator=(const embedded_sound_player&) -> embedded_sound_player& = delete;
        auto Initialize(HMODULE module, uint16_t firstResource = 401) -> void;
        auto Available(size_t index) const -> bool { return index < this->clips_.size() && this->clips_[index].data != nullptr; }
        auto Play(size_t index) const -> bool;
        auto Stop(void) const -> void;

    private:
        struct clip { WAVEFORMATEX format{}; const char* data{}; DWORD size{}; };
        std::array<clip, 5> clips_{};
        mutable HWAVEOUT output_{};
        mutable WAVEHDR header_{};
    };
}
