#pragma once

#include "../shared/keyboard_input.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace xiui
{
    class keyboard_suppression
    {
    public:
        static auto ScanCode(const uint32_t flags, const uint32_t fallback) -> uint32_t
        {
            const auto scan = (flags >> 16) & 0xffu;
            return scan != 0 ? scan | ((flags & 0x01000000u) != 0 ? 0x80u : 0u) : fallback;
        }

        auto Observe(const uint32_t scan, const bool pressed, const bool repeated, const bool owned) -> bool
        {
            if (scan >= this->keys_.size()) return owned;
            std::lock_guard lock(this->mutex_);
            auto& key = this->keys_[scan];
            if (pressed)
            {
                if (repeated && (key.wm_down || key.release_owned))
                {
                    key.wm_owned = key.wm_owned || owned;
                    key.state_owned = key.state_owned || key.wm_owned;
                    if (owned)
                    {
                        for (size_t index = key.count; index != 0; --index)
                        {
                            auto& pending = key.pending[(key.head + index - 1) % history_size];
                            if (!pending.pressed) break;
                            pending.owned = true;
                        }
                    }
                    return owned || key.wm_owned || key.release_owned;
                }

                // A fresh gesture replaces immediate-state ownership, not buffered history.
                key.wm_down = true;
                key.wm_owned = key.state_owned = owned;
                key.release_owned = false;
                Push(key, {true, owned});
                return owned;
            }

            const bool suppress = owned || key.wm_owned || key.release_owned;
            Push(key, {false, suppress});
            key.wm_down = key.wm_owned = key.release_owned = false;
            key.state_owned = key.state_owned || suppress;
            return suppress;
        }

        auto FilterState(uint8_t* data, const size_t size, const uint32_t capture) -> void
        {
            if (data == nullptr || size != this->keys_.size()) return;
            std::lock_guard lock(this->mutex_);
            for (size_t scan = 0; scan < this->keys_.size(); ++scan)
            {
                auto& key = this->keys_[scan];
                const bool down = (data[scan] & 0x80u) != 0;
                const bool captured = KeyboardCapturesScan(capture, static_cast<uint32_t>(scan));
                if (captured && down) key.state_owned = true;
                if (captured || key.wm_owned || key.state_owned) data[scan] = 0;
                if (!down) key.state_owned = false;
            }
        }

        auto FilterData(void* data, const size_t stride, const size_t count, const uint32_t capture, const bool peek, const int32_t result) -> void
        {
            if (result < 0 || data == nullptr || stride < 8 || stride > 20 || count > 4096) return;
            std::lock_guard lock(this->mutex_);
            if (peek)
            {
                auto preview = this->keys_;
                Filter(preview, data, stride, count, capture);
            }
            else Filter(this->keys_, data, stride, count, capture);
        }

        auto Suspend(void) -> void
        {
            std::lock_guard lock(this->mutex_);
            for (auto& key : this->keys_)
            {
                key.release_owned = key.release_owned || key.wm_owned;
                key.state_owned = key.state_owned || key.release_owned || key.data_owned;
                key.wm_down = key.wm_owned = false;
            }
        }

        auto Reset(void) -> void
        {
            std::lock_guard lock(this->mutex_);
            this->keys_ = {};
        }

    private:
        static constexpr size_t history_size = 64;
        struct edge
        {
            bool pressed{}, owned{};
        };
        struct key_state
        {
            std::array<edge, history_size> pending{};
            size_t head{}, count{};
            bool wm_down{}, wm_owned{}, release_owned{}, state_owned{}, data_owned{}, overflow_owned{};
        };

        std::mutex mutex_{};
        std::array<key_state, 256> keys_{};

        static auto Push(key_state& key, const edge value) -> void
        {
            if (key.count == history_size)
            {
                key.overflow_owned = key.overflow_owned || key.pending[key.head].owned;
                key.head = (key.head + 1) % history_size;
                --key.count;
            }
            key.pending[(key.head + key.count) % history_size] = value;
            ++key.count;
        }

        static auto Consume(key_state& key, const bool pressed) -> bool
        {
            bool owned = key.data_owned || key.overflow_owned;
            bool matched = false;
            while (key.count != 0)
            {
                const auto pending = key.pending[key.head];
                key.head = (key.head + 1) % history_size;
                --key.count;
                if (pending.pressed == pressed)
                {
                    owned = pressed ? pending.owned : owned || pending.owned;
                    if (pressed) key.overflow_owned = false;
                    matched = true;
                    break;
                }
                owned = owned || pending.owned;
            }
            if (!matched) owned = owned || key.wm_owned;
            return owned;
        }

        static auto Filter(std::array<key_state, 256>& keys, void* data, const size_t stride, const size_t count, const uint32_t capture) -> void
        {
            auto* bytes = static_cast<uint8_t*>(data);
            for (size_t index = 0; index < count; ++index)
            {
                auto* entry = bytes + index * stride;
                uint32_t scan{}, value{};
                std::memcpy(&scan, entry, sizeof(scan));
                std::memcpy(&value, entry + sizeof(scan), sizeof(value));
                if (scan >= keys.size()) continue;
                auto& key = keys[scan];
                const bool pressed = (value & 0x80u) != 0;
                const bool owned = Consume(key, pressed) || KeyboardCapturesScan(capture, scan);
                key.data_owned = pressed && owned;
                if (!pressed) key.overflow_owned = false;
                if (owned)
                {
                    value = 0;
                    std::memcpy(entry + sizeof(scan), &value, sizeof(value));
                }
            }
        }
    };
}
