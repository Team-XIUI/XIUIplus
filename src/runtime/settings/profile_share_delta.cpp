#include "profile_share_delta.hpp"

#include <algorithm>
#include <vector>

namespace xiui::profile_share_detail
{
    auto Checksum(const std::string_view data) -> uint32_t
    {
        uint32_t crc = ~0u;
        for (const unsigned char value : data)
        {
            crc ^= value;
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
        }
        return ~crc;
    }

    auto ValidName(const std::string_view name) -> bool
    {
        return !name.empty() && name.size() <= 64 && std::none_of(name.begin(), name.end(), [](const unsigned char c) { return c < 32 || c == 127; });
    }

    namespace
    {
        using field_name = std::pair<std::string, std::string>;
        using fields = std::map<field_name, std::string>;

        auto Fields(const settings_store& store) -> fields
        {
            fields result;
            store.VisitPreferences([&](std::string_view section, std::string_view key, std::string_view value) {
                const auto lower = [](const std::string_view input) {
                    std::string text{input};
                    for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
                    return text;
                };
                result[{lower(section), lower(key)}] = value;
            });
            return result;
        }

        auto Baseline(void) -> const fields&
        {
            static const auto baseline = [] {
                HMODULE module{};
                ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&EncodeDelta), &module);
                const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(305), MAKEINTRESOURCEW(10));
                const auto loaded = resource != nullptr ? ::LoadResource(module, resource) : nullptr;
                const auto* bytes = loaded != nullptr ? static_cast<const char*>(::LockResource(loaded)) : nullptr;
                const std::string_view text{bytes != nullptr ? bytes : "", resource != nullptr ? ::SizeofResource(module, resource) : 0};
                // This preset is immutable for XIUI2, independent of future release defaults.
                if (Checksum(text) != 0xB0E857FE) return fields{};
                return Fields(settings_store::FromIniText(text));
            }();
            return baseline;
        }

        auto Number(std::string& output, size_t value) -> void
        {
            while (value >= 128) { output += static_cast<char>((value & 127) | 128); value >>= 7; }
            output += static_cast<char>(value);
        }

        auto Text(std::string& output, const std::string_view text) -> void
        { Number(output, text.size()); output += text; }

        struct reader
        {
            std::string_view input;
            bool valid{true};
            auto Number(void) -> uint32_t
            {
                uint32_t result{};
                for (unsigned shift = 0; shift < 32 && !this->input.empty(); shift += 7)
                {
                    const auto byte = static_cast<uint8_t>(this->input.front()); this->input.remove_prefix(1);
                    if (shift == 28 && byte > 15) break;
                    result |= uint32_t{byte & 127u} << shift;
                    if ((byte & 128) == 0) return result;
                }
                this->valid = false; return 0;
            }
            auto Text(void) -> std::string
            {
                const auto size = this->Number();
                if (!this->valid || size > this->input.size()) { this->valid = false; return {}; }
                std::string result{this->input.substr(0, size)}; this->input.remove_prefix(size);
                if (result.find_first_of("\r\n") != std::string::npos || result.find('\0') != std::string::npos) this->valid = false;
                return result;
            }
        };
    }

    auto EncodeDelta(const std::string& name, const settings_store& snapshot) -> std::string
    {
        const auto& baseline = Baseline();
        if (baseline.empty()) return {};
        const auto current = Fields(snapshot);
        std::string changes; size_t index{}, count{};
        for (const auto& [field, value] : baseline)
        {
            ++index;
            const auto saved = current.find(field);
            if (saved == current.end()) { Number(changes, (index << 1) | 1); ++count; }
            else if (saved->second != value) { Number(changes, index << 1); Text(changes, saved->second); ++count; }
        }
        for (const auto& [field, value] : current)
            if (!baseline.contains(field))
            { Number(changes, 0); Text(changes, field.first); Text(changes, field.second); Text(changes, value); ++count; }
        std::string result; Text(result, name); Number(result, count); result += changes;
        return result.size() <= maximum_profile_payload ? result : std::string{};
    }

    auto DecodeDelta(const std::string_view data) -> std::optional<shared_profile>
    {
        if (data.empty() || data.size() > maximum_profile_payload) return std::nullopt;
        const auto& baseline = Baseline();
        if (baseline.empty()) return std::nullopt;
        reader read{data};
        const auto name = read.Text();
        const auto count = read.Number();
        if (!read.valid || !ValidName(name) || count > read.input.size()) return std::nullopt;
        auto values = baseline;
        std::vector<fields::iterator> indexed;
        for (auto it = values.begin(); it != values.end(); ++it) indexed.push_back(it);
        std::vector<bool> changed(indexed.size());
        for (size_t i = 0; i < count && read.valid; ++i)
        {
            const auto operation = read.Number();
            if (operation == 0)
            {
                const auto section = read.Text(), key = read.Text(), value = read.Text();
                if (section.empty() || key.empty() || section.find_first_of("[]") != std::string::npos || key.find_first_of("=[]") != std::string::npos ||
                    values.contains({section, key})) return std::nullopt;
                values[{section, key}] = value;
            }
            else
            {
                const auto id = operation >> 1;
                if (id == 0 || id > indexed.size() || changed[id - 1]) return std::nullopt;
                changed[id - 1] = true;
                if ((operation & 1) != 0) values.erase(indexed[id - 1]);
                else indexed[id - 1]->second = read.Text();
            }
        }
        if (!read.valid || !read.input.empty()) return std::nullopt;
        auto store = settings_store::FromIniText("");
        for (const auto& [field, value] : values) store.WriteString(field.first.c_str(), field.second.c_str(), value);
        if (store.ToIniText().size() > maximum_profile_payload) return std::nullopt;
        return shared_profile{name, store.Preferences("")};
    }
}
