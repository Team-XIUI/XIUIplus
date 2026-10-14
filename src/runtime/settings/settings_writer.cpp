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
        std::lock_guard lock(this->mutex_);
        if (!this->worker_.joinable())
        {
            this->stopping_ = false;
            this->worker_ = std::thread{&settings_writer::Run, this};
        }
        if (!this->pending_)
            this->deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds{150};
        // Every job contains all settings, so the newest snapshot replaces older work.
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

    auto settings_writer::HasFailed(void) const -> bool
    {
        std::lock_guard lock(this->mutex_);
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
            auto snapshot = std::move(*this->pending_);
            this->pending_.reset();
            this->writing_ = true;
            lock.unlock();
            bool saved = false;
            try { saved = snapshot.Flush(); }
            catch (...) {}
            lock.lock();
            this->writing_ = false;
            this->failed_ = !saved;
            if (!saved && !this->pending_) this->retry_ = std::move(snapshot);
            this->wake_.notify_all();
        }
    }
} // namespace xiui
