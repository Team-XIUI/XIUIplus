#include "satchel_cache.hpp"
#include "satchel_identity.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <limits>
#include <set>
#include <span>
#include <string_view>

namespace xiui
{
    namespace
    {
        template<typename T>
        auto Number(const std::string_view text, T& result) -> bool
        {
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
            return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
        }

        auto StableIdentity(const std::string_view identity, const uint32_t character = 0) -> bool
        {
            return SatchelStableIdentity(identity, character);
        }

        auto Plain(const std::string_view text, const size_t limit) -> bool
        {
            return !text.empty() && text.size() <= limit && text.find_first_of("\r\n\0", 0, 3) == std::string_view::npos;
        }

        auto Hex(const std::span<const uint8_t> bytes) -> std::string
        {
            constexpr char digits[] = "0123456789ABCDEF";
            std::string result(bytes.size() * 2, '0');
            for (size_t index = 0; index < bytes.size(); ++index)
            {
                result[index * 2] = digits[bytes[index] >> 4];
                result[index * 2 + 1] = digits[bytes[index] & 15];
            }
            return result;
        }

        auto Unhex(const std::string_view text, const std::span<uint8_t> output) -> bool
        {
            if (text.size() != output.size() * 2) return false;
            for (size_t index = 0; index < output.size(); ++index)
            {
                unsigned value{};
                const auto parsed = std::from_chars(text.data() + index * 2, text.data() + index * 2 + 2, value, 16);
                if (parsed.ec != std::errc{} || parsed.ptr != text.data() + index * 2 + 2) return false;
                output[index] = static_cast<uint8_t>(value);
            }
            return true;
        }

        auto EncodeItem(const satchel_item& item) -> std::string
        {
            return std::to_string(item.id) + ',' + std::to_string(item.count) + ',' + std::to_string(item.flags) + ',' +
                std::to_string(item.price) + ',' + std::to_string(item.equipped ? 1 : 0) + ',' + std::to_string(item.generation) + ',' + Hex(item.extra);
        }

        auto DecodeItem(std::string_view text, satchel_item& item) -> bool
        {
            std::array<std::string_view, 7> fields;
            for (size_t index = 0; index < fields.size(); ++index)
            {
                const auto comma = text.find(',');
                if ((index + 1 < fields.size()) != (comma != std::string_view::npos)) return false;
                fields[index] = text.substr(0, comma);
                if (comma != std::string_view::npos) text.remove_prefix(comma + 1);
            }
            unsigned equipped{};
            const bool valid = Number(fields[0], item.id) && item.id != 0 && item.id != 0xFFFF && Number(fields[1], item.count) && item.count != 0 &&
                Number(fields[2], item.flags) && Number(fields[3], item.price) && Number(fields[4], equipped) && equipped <= 1 &&
                Number(fields[5], item.generation) && Unhex(fields[6], item.extra);
            item.equipped = equipped != 0;
            return valid;
        }

        auto SameData(const satchel_snapshot& left, const satchel_snapshot& right) -> bool
        {
            if (left.identity != right.identity || left.name != right.name || left.character_id != right.character_id || left.gil != right.gil) return false;
            for (size_t bag = 0; bag < left.bags.size(); ++bag)
            {
                if (bag == 3) continue;
                const auto& a = left.bags[bag]; const auto& b = right.bags[bag];
                if (a.capacity != b.capacity || a.items.size() != b.items.size()) return false;
                for (size_t index = 0; index < a.items.size(); ++index)
                {
                    const auto& x = a.items[index]; const auto& y = b.items[index];
                    if (x.id != y.id || x.slot != y.slot || x.count != y.count || x.flags != y.flags || x.price != y.price || x.extra != y.extra || x.equipped != y.equipped) return false;
                }
            }
            return true;
        }

        auto Complete(const satchel_snapshot& snapshot) -> bool
        {
            if (!snapshot.ready || snapshot.read_only || !StableIdentity(snapshot.identity, snapshot.character_id) || !Plain(snapshot.name, 64)) return false;
            if (snapshot.bags[0].capacity == 0) return false;
            for (size_t bag = 0; bag < snapshot.bags.size(); ++bag)
            {
                if (bag == 3) continue;
                const auto& storage = snapshot.bags[bag];
                if (storage.capacity > 80 || (storage.capacity != 0 && !storage.ready) || storage.items.size() > storage.capacity) return false;
                std::set<uint8_t> slots;
                for (const auto& item : storage.items)
                    if (item.id == 0 || item.id == 0xFFFF || item.count == 0 || item.bag != bag || item.slot == 0 ||
                        item.slot > storage.capacity || !slots.insert(item.slot).second) return false;
            }
            return true;
        }

        auto ReadUnsigned(const settings_store& store, const std::string& section, const char* key, uint64_t& value) -> bool
        {
            return Number(store.ReadString(section.c_str(), key), value);
        }
    }

    auto satchel_cache::Initialize(std::string path) -> void
    {
        this->Stop();
        this->path_ = std::move(path);
        this->characters_.clear(); this->orders_.clear(); this->writers_.clear(); this->pending_.reset();
        if (this->path_.empty()) return;
        std::error_code error;
        std::filesystem::create_directories(this->path_, error);
        if (error) return;
        std::vector<std::filesystem::path> files;
        for (std::filesystem::directory_iterator file{this->path_, error}, end; !error && file != end && files.size() < 64; file.increment(error))
        {
            if (file->is_regular_file(error) && file->path().extension() == ".ini" && StableIdentity(file->path().stem().string()))
                files.push_back(file->path());
        }
        std::sort(files.begin(), files.end());
        for (const auto& file : files)
        {
            const auto store = settings_store::CachedFile(file.string());
            const auto identity = file.stem().string();
            if (store.ReadInt("satchel", "version", 0) != 1 || store.ReadString("satchel", "identity") != identity) continue;
            this->writers_.emplace(identity, std::make_unique<settings_writer>());
            const std::string section = "character";
            satchel_snapshot snapshot;
            snapshot.identity = identity; snapshot.name = store.ReadString(section.c_str(), "name");
            uint64_t id{}, gil{}, observed{}, zone{};
            bool valid = store.ReadInt("satchel", "snapshot", 0) == 1 && ReadUnsigned(store, section, "id", id) && id != 0 && id <= UINT32_MAX &&
                StableIdentity(snapshot.identity, static_cast<uint32_t>(id)) && Plain(snapshot.name, 64) && ReadUnsigned(store, section, "gil", gil) && gil <= UINT32_MAX &&
                ReadUnsigned(store, section, "observed", observed) && ReadUnsigned(store, section, "zone", zone) && zone <= UINT32_MAX;
            snapshot.character_id = static_cast<uint32_t>(id); snapshot.gil = static_cast<uint32_t>(gil); snapshot.zone = static_cast<uint32_t>(zone);
            snapshot.observed = observed; snapshot.ready = snapshot.read_only = true;
            for (uint8_t bag = 0; valid && bag < snapshot.bags.size(); ++bag)
            {
                if (bag == 3) continue;
                const auto bagSection = "bag." + std::to_string(bag);
                const auto capacity = store.ReadInt(bagSection.c_str(), "capacity", -1);
                if (capacity < 0 || capacity > 80) { valid = false; break; }
                auto& storage = snapshot.bags[bag]; storage.capacity = static_cast<uint32_t>(capacity); storage.ready = capacity != 0;
                for (uint8_t slot = 1; slot <= capacity; ++slot)
                {
                    const auto key = "item." + std::to_string(slot);
                    const auto text = store.ReadString(bagSection.c_str(), key.c_str());
                    if (text.empty()) continue;
                    satchel_item item; item.bag = bag; item.slot = slot; item.read_only = true;
                    if (!DecodeItem(text, item)) { valid = false; break; }
                    storage.items.push_back(item);
                }
                if (!valid) break;
                const auto expected = store.ReadInt(bagSection.c_str(), "items", -1);
                if (expected < 0 || static_cast<size_t>(expected) != storage.items.size()) { valid = false; break; }
            }
            if (valid && snapshot.bags[0].capacity != 0)
                this->characters_.push_back(std::move(snapshot));

            const auto layouts = std::clamp(store.ReadInt("satchel", "layouts", 0), 0, 64);
            for (int index = 0; index < layouts; ++index)
            {
                const auto layoutSection = "layout." + std::to_string(index);
                const auto view = store.ReadInt(layoutSection.c_str(), "view", -2), entries = store.ReadInt(layoutSection.c_str(), "count", 0);
                if (view < -1 || view > 30000 || entries <= 0 || entries > 1360) continue;
                std::vector<std::string> order;
                for (int entry = 0; entry < entries; ++entry)
                {
                    auto value = store.ReadString(layoutSection.c_str(), std::to_string(entry).c_str());
                    if (!Plain(value, 128)) { order.clear(); break; }
                    order.push_back(std::move(value));
                }
                if (!order.empty()) this->orders_.emplace(std::make_pair(identity, view), std::move(order));
            }
        }
    }

    auto satchel_cache::Observe(const satchel_snapshot& snapshot, const uint64_t now) -> void
    {
        if (!Complete(snapshot) || (this->writers_.size() >= 64 && !this->writers_.contains(snapshot.identity))) { this->pending_.reset(); return; }
        const auto found = std::find_if(this->characters_.begin(), this->characters_.end(), [&](const auto& current) { return current.identity == snapshot.identity; });
        const auto observed = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        if (found != this->characters_.end() && SameData(snapshot, *found) && observed >= found->observed && observed - found->observed < 30)
        { this->pending_.reset(); return; }
        if (!this->pending_ || !SameData(snapshot, *this->pending_))
        {
            this->pending_ = snapshot; this->pending_since_ = now;
            return;
        }
        if (now < this->pending_since_ || now - this->pending_since_ < 300) return;
        auto cached = snapshot;
        cached.read_only = true; cached.observed = observed; cached.bags[3] = {};
        for (auto& bag : cached.bags)
        {
            bag.accessible = false;
            for (auto& item : bag.items) item.read_only = true;
        }
        if (found != this->characters_.end()) *found = std::move(cached);
        else if (this->characters_.size() < 64) this->characters_.push_back(std::move(cached));
        this->pending_.reset();
        this->Queue(snapshot.identity);
    }

    auto satchel_cache::ReadOrder(const std::string& identity, const int view) const -> std::vector<std::string>
    {
        const auto found = this->orders_.find({identity, view});
        return found != this->orders_.end() ? found->second : std::vector<std::string>{};
    }

    auto satchel_cache::WriteOrder(const std::string& identity, const int view, const std::vector<std::string>& order) -> void
    {
        if (!StableIdentity(identity) || view < -1 || view > 30000 || order.size() > 1360 ||
            (this->writers_.size() >= 64 && !this->writers_.contains(identity)) ||
            std::any_of(order.begin(), order.end(), [](const auto& value) { return !Plain(value, 128); })) return;
        const auto key = std::make_pair(identity, view);
        const auto found = this->orders_.find(key);
        if (found != this->orders_.end() && found->second == order) return;
        if (found == this->orders_.end() && !order.empty() && std::count_if(this->orders_.begin(), this->orders_.end(), [&](const auto& entry) { return entry.first.first == identity; }) >= 64) return;
        if (order.empty()) this->orders_.erase(key);
        else this->orders_[key] = order;
        this->Queue(identity);
    }

    auto satchel_cache::Queue(const std::string& identity) -> void
    {
        if (this->path_.empty() || (this->writers_.size() >= 64 && !this->writers_.contains(identity))) return;
        auto& writer = this->writers_[identity];
        if (!writer) writer = std::make_unique<settings_writer>();
        auto store = settings_store::AtomicFile((std::filesystem::path{this->path_} / (identity + ".ini")).string());
        store.WriteInt("satchel", "version", 1);
        store.WriteString("satchel", "identity", identity);
        const auto found = std::find_if(this->characters_.begin(), this->characters_.end(), [&](const auto& current) { return current.identity == identity; });
        store.WriteBool("satchel", "snapshot", found != this->characters_.end());
        if (found != this->characters_.end())
        {
            const std::string section = "character";
            const auto& snapshot = *found;
            store.WriteString(section.c_str(), "name", snapshot.name);
            store.WriteString(section.c_str(), "id", std::to_string(snapshot.character_id)); store.WriteString(section.c_str(), "gil", std::to_string(snapshot.gil));
            store.WriteString(section.c_str(), "observed", std::to_string(snapshot.observed)); store.WriteString(section.c_str(), "zone", std::to_string(snapshot.zone));
            for (size_t bag = 0; bag < snapshot.bags.size(); ++bag)
            {
                if (bag == 3) continue;
                const auto bagSection = "bag." + std::to_string(bag);
                const auto& storage = snapshot.bags[bag];
                store.WriteInt(bagSection.c_str(), "capacity", static_cast<int>(storage.capacity));
                store.WriteInt(bagSection.c_str(), "items", static_cast<int>(storage.items.size()));
                for (const auto& item : storage.items)
                    store.WriteString(bagSection.c_str(), ("item." + std::to_string(item.slot)).c_str(), EncodeItem(item));
            }
        }
        size_t index{};
        for (const auto& [key, order] : this->orders_)
        {
            if (key.first != identity) continue;
            const auto section = "layout." + std::to_string(index++);
            store.WriteInt(section.c_str(), "view", key.second);
            store.WriteInt(section.c_str(), "count", static_cast<int>(order.size()));
            for (size_t entry = 0; entry < order.size(); ++entry) store.WriteString(section.c_str(), std::to_string(entry).c_str(), order[entry]);
        }
        store.WriteInt("satchel", "layouts", static_cast<int>(index));
        writer->Queue(std::move(store));
    }

    auto satchel_cache::Stop(void) -> void
    {
        for (auto& [identity, writer] : this->writers_) writer->Stop();
        this->pending_.reset();
    }
}
