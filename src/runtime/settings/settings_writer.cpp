#include "settings_writer.hpp"

#include <utility>

namespace xiui
{
    settings_writer::~settings_writer(void)
    {
        this->Stop();
    }

    auto settings_writer::Queue(settings_store snapshot) -> void
    {
        // Replaced snapshots are large; free them after the lock is released.
        std::optional<settings_store> replaced, retried;
        std::lock_guard lock(this->mutex_);
        if (!this->worker_.joinable())
        {
            this->stopping_ = false;
            this->worker_ = std::thread{&settings_writer::Run, this};
        }
        if (!this->pending_)
            this->deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds{150};
        // Every job contains all settings, so the newest snapshot replaces older work.
        replaced = std::move(this->pending_); retried = std::move(this->retry_);
        this->pending_ = std::move(snapshot);
        this->retry_.reset();
        this->wake_.notify_all();
    }

    auto settings_writer::Flush(void) -> bool
    {
        std::unique_lock lock(this->mutex_);
        if (!this->worker_.joinable()) return !this->failed_;
        if (this->retry_ && !this->pending_)
        {
            this->pending_ = std::move(this->retry_);
            this->retry_.reset();
        }
        this->immediate_ = true;
        this->wake_.notify_all();
        this->wake_.wait(lock, [this] { return !this->writing_ && !this->pending_; });
        this->immediate_ = false;
        return !this->failed_;
    }

    // Polled every frame, so it must not wait on a write in progress.
    auto settings_writer::HasFailed(void) const -> bool
    {
        return this->failed_;
    }

    auto settings_writer::Stop(void) -> void
    {
        this->Flush();
        {
            std::lock_guard lock(this->mutex_);
            this->stopping_ = true;
            this->wake_.notify_all();
        }
        if (this->worker_.joinable()) this->worker_.join();
    }

    auto settings_writer::Run(void) -> void
    {
        std::unique_lock lock(this->mutex_);
        for (;;)
        {
            this->wake_.wait(lock, [this] { return this->stopping_ || this->pending_.has_value(); });
            if (this->stopping_) return;
            this->wake_.wait_until(lock, this->deadline_, [this] { return this->stopping_ || this->immediate_; });
            if (this->stopping_) return;
            std::optional<settings_store> snapshot{std::move(*this->pending_)};
            this->pending_.reset();
            this->writing_ = true;
            lock.unlock();
            bool saved = false;
            try { saved = snapshot->Flush(); }
            catch (...) {}
            if (saved) snapshot.reset();
            lock.lock();
            this->writing_ = false;
            this->failed_ = !saved;
            this->attempts_ = saved || this->pending_ ? 0 : this->attempts_ + 1;
            if (!saved && !this->pending_)
            {
                // Try a blocked write again shortly; after a few attempts it waits for the next save.
                if (this->attempts_ < 4)
                {
                    this->pending_ = std::move(snapshot); snapshot.reset();
                    this->deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds{2};
                }
                else this->retry_ = std::move(snapshot);
            }
            this->wake_.notify_all();
            if (snapshot && !this->retry_.has_value())
            {
                lock.unlock(); snapshot.reset(); lock.lock();
            }
        }
    }
} // namespace xiui
