#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xiui
{
    enum class ready_response : uint8_t { pending, ready, not_ready, no_response };
    enum class ready_check_mode : uint8_t { idle, prompt, tracker };

    struct ready_check_member
    {
        uint32_t id{}, slot{}, job{};
        std::string name{};
        ready_response response{};
    };

    struct ready_check_message
    {
        std::string sender{}, body{};
    };

    inline auto ReadyNameEqual(std::string_view first, std::string_view second) -> bool
    {
        const auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        return first.size() == second.size() && std::equal(first.begin(), first.end(), second.begin(),
            [&](unsigned char a, unsigned char b) { return lower(a) == lower(b); });
    }

    inline auto DecodeReadyCheckChat(const std::span<const uint8_t> packet) -> std::optional<ready_check_message>
    {
        // 0x017: party kind, 15-byte sender at 0x08, message at 0x17.
        if (packet.size() < 24 || packet[4] != 4 || (packet[5] & 8) != 0) return std::nullopt;
        const auto field = [&](size_t offset, size_t limit) {
            std::string result;
            const auto end = std::min(packet.size(), offset + limit);
            for (size_t i = offset; i < end && packet[i] != 0; ++i)
            {
                if ((packet[i] == 0x1E || packet[i] == 0x1F) && i + 1 < end) { ++i; continue; }
                result += static_cast<char>(packet[i]);
            }
            return result;
        };
        ready_check_message message{field(8, 15), field(23, 150)};
        const auto first = message.body.find_first_not_of(" \t\r\n");
        if (message.sender.empty() || first == std::string::npos) return std::nullopt;
        message.body = message.body.substr(first, message.body.find_last_not_of(" \t\r\n") - first + 1);
        return message;
    }

    class ready_check_state
    {
    public:
        static constexpr uint64_t duration = 30000;
        auto Clear(void) -> void { *this = {}; }
        auto Mode(void) const -> ready_check_mode { return this->mode_; }
        auto Completed(void) const -> bool { return this->completed_; }
        auto Deadline(void) const -> uint64_t { return this->deadline_; }
        auto PlayerId(void) const -> uint32_t { return this->player_; }
        auto Sender(void) const -> const std::string& { return this->sender_; }
        auto Members(void) const -> const std::vector<ready_check_member>& { return this->members_; }
        auto Remaining(uint64_t now) const -> uint64_t { return now < this->deadline_ ? (this->deadline_ - now + 999) / 1000 : 0; }
        auto Count(ready_response response) const -> size_t
        {
            return static_cast<size_t>(std::count_if(this->members_.begin(), this->members_.end(),
                [response](const auto& member) { return member.response == response; }));
        }

        auto Start(const std::span<const ready_check_member> roster, uint32_t player, uint32_t checker, uint64_t now) -> bool
        {
            const auto sender = std::find_if(roster.begin(), roster.end(), [checker](const auto& member) { return member.id == checker; });
            if (roster.size() < 2 || roster.size() > 18 || player == 0 || sender == roster.end() ||
                std::none_of(roster.begin(), roster.end(), [player](const auto& member) { return member.id == player; })) return false;
            this->Clear();
            this->player_ = player; this->checker_ = checker; this->sender_ = sender->name;
            this->deadline_ = now + duration;
            this->mode_ = player == checker ? ready_check_mode::tracker : ready_check_mode::prompt;
            this->members_.assign(roster.begin(), roster.end());
            for (auto& member : this->members_)
                member.response = member.id == checker ? ready_response::ready : ready_response::pending;
            return true;
        }

        auto Receive(const ready_check_message& message, const std::span<const ready_check_member> roster, uint32_t player, uint64_t now) -> bool
        {
            this->Tick(now);
            const auto sender = std::find_if(roster.begin(), roster.end(), [&](const auto& member) { return ReadyNameEqual(member.name, message.sender); });
            if (sender == roster.end()) return false;
            if (message.body == "Ready Check!" || message.body.starts_with("Ready Check! "))
            {
                if (sender->id == player || this->mode_ == ready_check_mode::tracker ||
                    (this->mode_ == ready_check_mode::prompt && this->checker_ == sender->id)) return false;
                return this->Start(roster, player, sender->id, now);
            }
            const bool yes = ReadyNameEqual(message.body, "Yes") || message.body == "/" || message.body == "\\";
            if (!yes && !ReadyNameEqual(message.body, "No")) return false;
            if (this->mode_ == ready_check_mode::prompt && sender->id == player) { this->Clear(); return false; }
            if (this->mode_ != ready_check_mode::tracker || this->completed_) return false;
            const auto member = std::find_if(this->members_.begin(), this->members_.end(), [&](const auto& entry) { return entry.id == sender->id; });
            if (member == this->members_.end()) return false;
            member->response = yes ? ready_response::ready : ready_response::not_ready;
            this->Tick(now);
            return false;
        }

        auto Answer(uint64_t now) -> bool
        {
            this->Tick(now);
            if (this->mode_ != ready_check_mode::prompt) return false;
            this->Clear();
            return true;
        }

        auto Tick(uint64_t now) -> void
        {
            if (this->mode_ == ready_check_mode::prompt && now >= this->deadline_) { this->Clear(); return; }
            if (this->mode_ != ready_check_mode::tracker || this->completed_) return;
            if (now >= this->deadline_)
                for (auto& member : this->members_)
                    if (member.response == ready_response::pending) member.response = ready_response::no_response;
            this->completed_ = this->Count(ready_response::pending) == 0;
        }

        auto TakeSummary(void) -> std::vector<std::string>
        {
            if (!this->completed_ || this->summary_taken_) return {};
            this->summary_taken_ = true;
            constexpr std::string_view prefix = "Players not ready: ";
            std::vector<std::string> lines;
            std::string line{prefix};
            for (const auto& member : this->members_)
            {
                if (member.response == ready_response::ready) continue;
                if (line.size() + member.name.size() + 2 > 140) { lines.push_back(line); line = prefix; }
                if (line.size() > prefix.size()) line += ", ";
                line += member.name;
            }
            if (line.size() == prefix.size()) line += "none";
            lines.push_back(std::move(line));
            return lines;
        }

    private:
        ready_check_mode mode_{};
        uint64_t deadline_{};
        uint32_t player_{}, checker_{};
        bool completed_{}, summary_taken_{};
        std::string sender_{};
        std::vector<ready_check_member> members_{};
    };
}
