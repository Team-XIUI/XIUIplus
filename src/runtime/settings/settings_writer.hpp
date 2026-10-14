#pragma once

#include "settings_store.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace xiui
{
    class settings_writer
    {
    public:
        settings_writer(void) = default;
        ~settings_writer(void);
        settings_writer(const settings_writer&) = delete;
        auto operator=(const settings_writer&) -> settings_writer& = delete;

        auto Queue(settings_store snapshot) -> void;
        auto Flush(void) -> bool;
        auto HasFailed(void) const -> bool;
        auto Stop(void) -> void;

    private:
        mutable std::mutex mutex_{};
        std::condition_variable wake_{};
        std::thread worker_{};
        std::optional<settings_store> pending_{};
        std::optional<settings_store> retry_{};
        std::chrono::steady_clock::time_point deadline_{};
        bool writing_{};
        bool immediate_{};
        bool stopping_{};
        bool failed_{};

        auto Run(void) -> void;
    };
} // namespace xiui
