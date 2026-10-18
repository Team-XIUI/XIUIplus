#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>

namespace xiui
{
    template<typename T>
    class callback_gate
    {
    public:
        class lease
        {
        public:
            lease(void) = default;
            lease(callback_gate* owner, T* value) : owner_(owner), value_(value) {}
            lease(const lease&) = delete;
            auto operator=(const lease&) -> lease& = delete;
            lease(lease&& other) noexcept : owner_(other.owner_), value_(other.value_)
            {
                other.owner_ = nullptr;
                other.value_ = nullptr;
            }
            ~lease(void)
            {
                if (this->owner_ != nullptr)
                    this->owner_->Leave();
            }

            explicit operator bool(void) const { return this->value_ != nullptr; }
            auto operator->(void) const -> T* { return this->value_; }

        private:
            callback_gate* owner_{};
            T* value_{};
        };

        auto Set(T* value) -> void
        {
            std::lock_guard lock(this->mutex_);
            this->value_ = value;
        }

        auto Enter(void) -> lease
        {
            this->entries_.fetch_add(1, std::memory_order_acq_rel);
            std::lock_guard lock(this->mutex_);
            return lease{this, this->value_};
        }

        auto Close(void) -> void
        {
            std::lock_guard lock(this->mutex_);
            this->value_ = nullptr;
        }

        auto Wait(void) -> void
        {
            auto entries = this->entries_.load(std::memory_order_acquire);
            while (entries != 0)
            {
                this->entries_.wait(entries, std::memory_order_acquire);
                entries = this->entries_.load(std::memory_order_acquire);
            }
        }

    private:
        std::mutex mutex_{};
        std::atomic<uint32_t> entries_{};
        T* value_{};

        auto Leave(void) -> void
        {
            if (this->entries_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                this->entries_.notify_all();
        }
    };
} // namespace xiui
