#include "modules/readycheck/ready_check_module.hpp"

#include <cstdio>

namespace xiui
{
    auto RunReadyCheckSoundChecks(void) -> bool
    {
        const auto check = [](bool value, const char* message) { if (!value) std::printf("FAIL: %s\n", message); return value; };
        std::vector<uint8_t> bytes(48);
        const auto put = [&](size_t at, uint32_t value, size_t size) { std::memcpy(bytes.data() + at, &value, size); };
        std::memcpy(bytes.data(), "RIFF", 4); put(4, 40, 4); std::memcpy(bytes.data() + 8, "WAVEfmt ", 8);
        put(16, 16, 4); put(20, 1, 2); put(22, 1, 2); put(24, 8000, 4); put(28, 16000, 4); put(32, 2, 2); put(34, 16, 2);
        std::memcpy(bytes.data() + 36, "data", 4); put(40, 4, 4); put(44, 1234, 2); put(46, 4321, 2);
        const auto decoded = DecodeReadyCheckWav(bytes);
        if (!check(decoded.rate == 8000 && decoded.channels == 1 && decoded.samples.size() == 2 && decoded.samples[0] == 1234,
            "bounded PCM WAV decoding retains actual sample rate, channels and samples")) return false;
        bytes[20] = 3;
        if (!check(DecodeReadyCheckWav(bytes).samples.empty() && !ReadyCheckWavFilename("../sound.wav") &&
            !ReadyCheckWavFilename("C:\\sound.wav") && ReadyCheckWavFilename("party bell.WAV"), "unsupported WAV encoding and nonportable paths are rejected")) return false;
        ready_check_sound sound; sound.Configure("", "party bell.wav", true);
        if (!check(sound.Status().find("built-in chime") != std::string::npos, "missing custom audio keeps the built-in fallback")) return false;
        auto store = settings_store::FromIniText("[readycheck]\nsound_choice=1\nsound_file=party bell.wav\nvolume=125\n");
        ready_check_module module; module.Load(store);
        auto saved = settings_store::FromIniText(""); module.Save(saved);
        return check(saved.ReadString("readycheck", "sound_file", "") == "party bell.wav" && saved.ReadInt("readycheck", "sound_choice", 0) == 1 &&
            saved.ReadFloat("readycheck", "volume", 0) == 125, "portable sound filename, selection and existing volume persist without playback");
    }
}
