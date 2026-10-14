#pragma once

#include <Windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#pragma comment(lib, "winmm.lib")

namespace xiui
{
    class ready_check_sound
    {
    public:
        ready_check_sound(void) = default;
        ~ready_check_sound(void) { this->Stop(); }
        ready_check_sound(const ready_check_sound&) = delete;
        auto operator=(const ready_check_sound&) -> ready_check_sound& = delete;

        auto Stop(void) -> void
        {
            if (this->device_ == nullptr) return;
            ::waveOutReset(this->device_);
            if ((this->header_.dwFlags & WHDR_PREPARED) != 0)
                ::waveOutUnprepareHeader(this->device_, &this->header_, sizeof(this->header_));
            ::waveOutClose(this->device_);
            this->device_ = nullptr; this->header_ = {}; this->samples_.clear();
        }

        auto Tick(void) -> void
        {
            if (this->device_ != nullptr && (this->header_.dwFlags & WHDR_DONE) != 0) this->Stop();
        }

        auto Play(float volume) -> void
        {
            this->Stop();
            if (!std::isfinite(volume) || volume <= 0) return;
            constexpr uint32_t rate = 22050;
            WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, rate, rate * 2, 2, 16, 0};
            if (::waveOutOpen(&this->device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            { this->device_ = nullptr; return; }
            this->samples_.resize(rate * 3 / 4);
            constexpr double tau = 6.283185307179586;
            for (size_t i = 0; i < this->samples_.size(); ++i)
            {
                const double t = static_cast<double>(i) / rate;
                const auto note = [&](double start, double frequency) {
                    const double age = t - start;
                    return age < 0 ? 0.0 : std::sin(tau * frequency * age) * std::min(1.0, age * 160) * std::exp(-age * 9);
                };
                const double sample = (note(0, 660) + note(0.16, 880)) * std::clamp(volume, 0.0f, 150.0f) * 75;
                this->samples_[i] = static_cast<int16_t>(std::clamp(sample, -32767.0, 32767.0));
            }
            this->header_ = {};
            this->header_.lpData = reinterpret_cast<char*>(this->samples_.data());
            this->header_.dwBufferLength = static_cast<DWORD>(this->samples_.size() * sizeof(int16_t));
            if (::waveOutPrepareHeader(this->device_, &this->header_, sizeof(this->header_)) != MMSYSERR_NOERROR ||
                ::waveOutWrite(this->device_, &this->header_, sizeof(this->header_)) != MMSYSERR_NOERROR) this->Stop();
        }

    private:
        HWAVEOUT device_{};
        WAVEHDR header_{};
        std::vector<int16_t> samples_{};
    };
}
