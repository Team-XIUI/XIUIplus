#include "experience_source.hpp"

#include <algorithm>

namespace
{
    auto ProcessStarted(void) -> FILETIME
    {
        FILETIME created{}, exited{}, kernel{}, user{};
        ::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &kernel, &user); return created;
    }

    auto Read64(const xiui::settings_store& store, const std::string& section, const char* key) -> uint64_t
    {
        const auto low = static_cast<uint32_t>(store.ReadInt(section.c_str(), (std::string{key} + "_low").c_str(), 0));
        const auto high = static_cast<uint32_t>(store.ReadInt(section.c_str(), (std::string{key} + "_high").c_str(), 0));
        return std::min(uint64_t{low} | (uint64_t{high} << 32), 1000000000000000ull);
    }

    auto Write64(const xiui::settings_store& store, const std::string& section, const char* key, uint64_t value) -> void
    {
        store.WriteInt(section.c_str(), (std::string{key} + "_low").c_str(), static_cast<int>(value));
        store.WriteInt(section.c_str(), (std::string{key} + "_high").c_str(), static_cast<int>(value >> 32));
    }

    auto ReadRecord(const xiui::settings_store& store, const std::string& section) -> xiui::experience_record
    {
        xiui::experience_record result;
        result.player = static_cast<uint32_t>(store.ReadInt(section.c_str(), "player", 0));
        result.job = static_cast<uint32_t>(std::clamp(store.ReadInt(section.c_str(), "job", 0), 0, 22));
        result.utc = static_cast<uint32_t>(std::max(0, store.ReadInt(section.c_str(), "utc", 0)));
        result.elapsed = Read64(store, section, "elapsed");
        result.merits = static_cast<uint32_t>(std::max(0, store.ReadInt(section.c_str(), "merits", 0)));
        result.best_chain = static_cast<uint32_t>(std::max(0, store.ReadInt(section.c_str(), "best_chain", 0)));
        for (size_t i = 0; i < result.totals.size(); ++i)
        {
            auto& total = result.totals[i]; const auto name = section + "." + std::to_string(i);
            total.received = Read64(store, name, "received"); total.gained = Read64(store, name, "gained");
            total.lost = Read64(store, name, "lost"); total.recovered = std::min(total.lost, Read64(store, name, "recovered"));
            total.count = Read64(store, name, "count");
            total.last = static_cast<uint32_t>(std::max(0, store.ReadInt(name.c_str(), "last", 0)));
            total.minimum = static_cast<uint32_t>(std::max(0, store.ReadInt(name.c_str(), "minimum", 0)));
            total.maximum = static_cast<uint32_t>(std::max(0, store.ReadInt(name.c_str(), "maximum", 0)));
        }
        return result;
    }

    auto WriteRecord(const xiui::settings_store& store, const std::string& section, const xiui::experience_record& record) -> void
    {
        store.WriteInt(section.c_str(), "player", static_cast<int>(record.player)); store.WriteInt(section.c_str(), "job", static_cast<int>(record.job));
        store.WriteInt(section.c_str(), "utc", static_cast<int>(record.utc)); Write64(store, section, "elapsed", record.elapsed);
        store.WriteInt(section.c_str(), "merits", static_cast<int>(record.merits)); store.WriteInt(section.c_str(), "best_chain", static_cast<int>(record.best_chain));
        for (size_t i = 0; i < record.totals.size(); ++i)
        {
            const auto& total = record.totals[i]; const auto name = section + "." + std::to_string(i);
            Write64(store, name, "received", total.received); Write64(store, name, "gained", total.gained);
            Write64(store, name, "lost", total.lost); Write64(store, name, "recovered", total.recovered); Write64(store, name, "count", total.count);
            store.WriteInt(name.c_str(), "last", static_cast<int>(total.last)); store.WriteInt(name.c_str(), "minimum", static_cast<int>(total.minimum));
            store.WriteInt(name.c_str(), "maximum", static_cast<int>(total.maximum));
        }
    }
}

namespace xiui
{
    auto experience_source::Load(const settings_store& store) -> void
    {
        this->restore_ = store;
    }

    auto experience_source::LoadCharacter(const uint32_t player) -> void
    {
        if (player == this->loaded_player_) return;
        if (this->loaded_player_ != 0)
            this->characters_[this->loaded_player_] = {this->tracker.Record(::GetTickCount64()), this->tracker.History(), this->tracker.Paused()};
        this->loaded_player_ = player;
        this->tracker = {};
        const auto cached = this->characters_.find(player);
        if (cached != this->characters_.end())
        { this->tracker.Restore(cached->second.record, cached->second.paused); this->tracker.History() = cached->second.history; return; }
        if (!this->restore_) return;
        const auto& store = *this->restore_;
        const auto section = "session.exp." + std::to_string(player);
        const auto count = std::clamp(store.ReadInt(section.c_str(), "history_count", 0), 0, 10);
        for (int i = 0; i < count; ++i)
        {
            const auto record = ReadRecord(store, section + ".history." + std::to_string(i));
            if (record.player == player && record.job != 0 && record.utc != 0) this->tracker.History().push_back(record);
        }
        auto record = ReadRecord(store, section);
        if (record.player != player || record.job == 0 || record.utc == 0) return;
        const auto created = ProcessStarted();
        const bool sameProcess = static_cast<uint32_t>(store.ReadInt(section.c_str(), "process", 0)) == ::GetCurrentProcessId() &&
            static_cast<uint32_t>(store.ReadInt(section.c_str(), "created_low", 0)) == created.dwLowDateTime &&
            static_cast<uint32_t>(store.ReadInt(section.c_str(), "created_high", 0)) == created.dwHighDateTime;
        if (sameProcess)
        {
            this->tracker.Restore(record, store.ReadBool(section.c_str(), "paused", false));
            experience_chain chain;
            chain.count = static_cast<uint32_t>(std::max(0, store.ReadInt(section.c_str(), "chain", 0)));
            chain.best = record.best_chain; chain.deadline = Read64(store, section, "chain_deadline");
            chain.estimated = store.ReadBool(section.c_str(), "chain_estimated", true);
            this->restored_chain_ = chain; this->restored_chain_zone_ = static_cast<uint32_t>(store.ReadInt(section.c_str(), "zone", 0));
        }
        else
        {
            this->tracker.History().push_front(record);
            if (this->tracker.History().size() > 10) this->tracker.History().pop_back();
        }
    }

    auto experience_source::Save(const settings_store& store) const -> void
    {
        const auto created = ProcessStarted();
        const auto save = [&](const uint32_t player, const saved_character& saved) {
            const auto section = "session.exp." + std::to_string(player);
            WriteRecord(store, section, saved.record);
            store.WriteBool(section.c_str(), "paused", saved.paused);
            store.WriteInt(section.c_str(), "process", static_cast<int>(::GetCurrentProcessId()));
            store.WriteInt(section.c_str(), "created_low", static_cast<int>(created.dwLowDateTime));
            store.WriteInt(section.c_str(), "created_high", static_cast<int>(created.dwHighDateTime));
            store.WriteInt(section.c_str(), "history_count", static_cast<int>(saved.history.size()));
            for (size_t i = 0; i < saved.history.size(); ++i) WriteRecord(store, section + ".history." + std::to_string(i), saved.history[i]);
        };
        for (const auto& [id, saved] : this->characters_) if (id != this->loaded_player_) save(id, saved);
        if (this->loaded_player_ != 0)
        {
            save(this->loaded_player_, {this->tracker.Record(::GetTickCount64()), this->tracker.History(), this->tracker.Paused()});
            const auto section = "session.exp." + std::to_string(this->loaded_player_);
            const auto& chain = this->tracker.Chain();
            store.WriteInt(section.c_str(), "chain", static_cast<int>(chain.count));
            Write64(store, section, "chain_deadline", chain.deadline); store.WriteBool(section.c_str(), "chain_estimated", chain.estimated);
            store.WriteInt(section.c_str(), "zone", static_cast<int>(this->tracker.Sample().zone));
        }
    }
}
