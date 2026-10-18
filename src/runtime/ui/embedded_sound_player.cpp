#include "embedded_sound_player.hpp"

#include <cstring>

#pragma comment(lib, "winmm.lib")

namespace xiui::ui
{
    auto embedded_sound_player::Initialize(const HMODULE module, const uint16_t firstResource) -> void
    {
        this->Stop(); this->clips_ = {};
        for (size_t index = 0; index < this->clips_.size(); ++index)
        {
            const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(firstResource + index), MAKEINTRESOURCEW(10));
            const auto size = resource != nullptr ? ::SizeofResource(module, resource) : 0;
            const auto loaded = resource != nullptr ? ::LoadResource(module, resource) : nullptr;
            const auto* bytes = loaded != nullptr ? static_cast<const char*>(::LockResource(loaded)) : nullptr;
            if (bytes == nullptr || size < 44 || std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0) continue;
            clip value{};
            for (size_t offset = 12; offset + 8 <= size;)
            {
                uint32_t length{}; std::memcpy(&length, bytes + offset + 4, 4);
                if (length > size - offset - 8) break;
                const auto* chunk = bytes + offset + 8;
                if (std::memcmp(bytes + offset, "fmt ", 4) == 0 && length >= 16) std::memcpy(&value.format, chunk, 16);
                if (std::memcmp(bytes + offset, "data", 4) == 0) { value.data = chunk; value.size = length; }
                offset += 8 + static_cast<size_t>(length) + (length & 1);
            }
            const auto& format = value.format;
            if (value.data != nullptr && value.size > 0 && format.wFormatTag == WAVE_FORMAT_PCM &&
                (format.nChannels == 1 || format.nChannels == 2) && (format.wBitsPerSample == 8 || format.wBitsPerSample == 16) &&
                format.nSamplesPerSec >= 8000 && format.nSamplesPerSec <= 192000 &&
                format.nBlockAlign == format.nChannels * format.wBitsPerSample / 8 &&
                format.nAvgBytesPerSec == format.nSamplesPerSec * format.nBlockAlign && value.size % format.nBlockAlign == 0)
                this->clips_[index] = value;
        }
    }

    auto embedded_sound_player::Stop(void) const -> void
    {
        if (this->output_ == nullptr) return;
        ::waveOutReset(this->output_);
        ::waveOutUnprepareHeader(this->output_, &this->header_, sizeof(this->header_));
        ::waveOutClose(this->output_);
        this->output_ = nullptr; this->header_ = {};
    }

    auto embedded_sound_player::Play(const size_t index) const -> bool
    {
        if (!this->Available(index)) return false;
        this->Stop();
        const auto& sound = this->clips_[index];
        if (::waveOutOpen(&this->output_, WAVE_MAPPER, &sound.format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        { this->output_ = nullptr; return false; }
        this->header_.lpData = const_cast<char*>(sound.data); this->header_.dwBufferLength = sound.size;
        if (::waveOutPrepareHeader(this->output_, &this->header_, sizeof(this->header_)) == MMSYSERR_NOERROR &&
            ::waveOutWrite(this->output_, &this->header_, sizeof(this->header_)) == MMSYSERR_NOERROR) return true;
        this->Stop();
        return false;
    }
}
