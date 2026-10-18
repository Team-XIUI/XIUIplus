#pragma once

#include <Windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>
#include "ready_check_wav.hpp"

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
        auto Configure(const std::string& directory, const std::string& filename, const bool custom) -> void
        {
            this->Stop(); this->custom_ = {}; this->status_ = "Built-in chime";
            if (!custom) return;
            this->status_ = "Custom WAV unavailable; using built-in chime";
            if (directory.empty() || !ReadyCheckWavFilename(filename)) return;
            std::ifstream file(std::filesystem::path{directory} / filename, std::ios::binary | std::ios::ate);
            if (!file) return;
            const auto size = file.tellg();
            if (size < 44 || size > 8 * 1024 * 1024) return;
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            file.seekg(0);
            if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return;
            this->custom_ = DecodeReadyCheckWav(bytes);
            if (!this->custom_.samples.empty()) this->status_ = "Custom WAV: " + filename;
        }
        auto Status(void) const -> const std::string& { return this->status_; }

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
            bool custom = !this->custom_.samples.empty();
            auto rate = custom ? this->custom_.rate : 22050u;
            auto channels = custom ? this->custom_.channels : uint16_t{1};
            WAVEFORMATEX format{WAVE_FORMAT_PCM, channels, rate, rate * channels * 2, static_cast<WORD>(channels * 2), 16, 0};
            if (::waveOutOpen(&this->device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            {
                this->device_ = nullptr;
                if (!custom) return;
                custom = false; rate = 22050; channels = 1;
                format = {WAVE_FORMAT_PCM, 1, rate, rate * 2, 2, 16, 0};
                if (::waveOutOpen(&this->device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
                { this->device_ = nullptr; return; }
                this->status_ = "Custom WAV format unavailable; using built-in chime";
            }
            this->samples_.resize(custom ? this->custom_.samples.size() : rate * 3 / 4);
            constexpr double tau = 6.283185307179586;
            for (size_t i = 0; i < this->samples_.size(); ++i)
            {
                if (custom)
                {
                    this->samples_[i] = static_cast<int16_t>(std::clamp(this->custom_.samples[i] * std::clamp(volume, 0.0f, 150.0f) / 100.0f, -32767.0f, 32767.0f));
                    continue;
                }
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
        ready_check_wav custom_{};
        std::string status_{"Built-in chime"};
    };
}
