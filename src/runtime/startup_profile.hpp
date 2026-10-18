#pragma once

#include <Windows.h>

#include <array>
#include <cstddef>

namespace xiui
{
    struct startup_profile
    {
        struct entry { const char* name{}; double milliseconds{}; };
        std::array<entry, 32> entries{};
        size_t count{};

        class scope
        {
        public:
            scope(startup_profile& profile, const char* name, const bool enabled = true)
                : profile_(enabled ? &profile : nullptr), name_(name)
            {
                if (this->profile_) ::QueryPerformanceCounter(&this->start_);
            }
            ~scope(void) { if (this->name_) this->Mark(this->name_); }
            auto Mark(const char* name) -> void
            {
                if (!this->profile_ || this->profile_->count == this->profile_->entries.size()) return;
                LARGE_INTEGER end{}, frequency{};
                ::QueryPerformanceCounter(&end);
                ::QueryPerformanceFrequency(&frequency);
                this->profile_->entries[this->profile_->count++] = {name,
                    (end.QuadPart - this->start_.QuadPart) * 1000.0 / frequency.QuadPart};
                this->start_ = end;
            }
            scope(const scope&) = delete;
            auto operator=(const scope&) -> scope& = delete;

        private:
            startup_profile* profile_{};
            const char* name_{};
            LARGE_INTEGER start_{};
        };
    };
}
