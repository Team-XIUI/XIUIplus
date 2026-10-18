#include "action_import.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <functional>
#include <map>
#include <stdexcept>

namespace
{
    struct literal
    {
        enum class kind { nil, scalar, table } type{};
        std::string text{};
        std::vector<std::pair<std::string, literal>> fields{};
    };
    auto Lower(std::string value) -> std::string
    { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); return value; }
    auto Find(const literal& table, std::string_view key) -> const literal*
    { for (auto it = table.fields.rbegin(); it != table.fields.rend(); ++it) if (Lower(it->first) == Lower(std::string{key})) return &it->second; return nullptr; }
    auto Text(const literal& table, std::string_view key, const std::string& fallback = {}) -> std::string
    { const auto* value = Find(table, key); return value != nullptr && value->type == literal::kind::scalar ? value->text : fallback; }
    auto Number(const std::string& value, unsigned fallback = 0) -> unsigned
    { unsigned result{}; const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result); return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() ? result : fallback; }
    class parser
    {
    public:
        explicit parser(std::string_view input) : input_(input) {}
        size_t duplicates{};
        auto Parse(void) -> literal
        {
            this->Skip();
            if (this->input_.substr(this->at_, 6) == "return" && (this->at_ + 6 == this->input_.size() || !this->IdentifierPart(this->input_[this->at_ + 6]))) this->at_ += 6;
            auto result = this->Value(0); this->Skip();
            if (this->at_ < this->input_.size() && this->input_[this->at_] == ';') { ++this->at_; this->Skip(); }
            if (this->at_ != this->input_.size()) this->Fail("Only a returned literal table is supported; executable Lua is not imported");
            if (result.type != literal::kind::table) this->Fail("Expected a literal table");
            return result;
        }
    private:
        std::string_view input_{}; size_t at_{}, nodes_{};
        [[noreturn]] auto Fail(const std::string& message) const -> void { throw std::runtime_error{message + " (byte " + std::to_string(this->at_ + 1) + ")."}; }
        static auto IdentifierPart(unsigned char c) -> bool { return std::isalnum(c) || c == '_'; }
        auto Skip(void) -> void
        {
            while (this->at_ < this->input_.size())
            {
                if (std::isspace(static_cast<unsigned char>(this->input_[this->at_]))) { ++this->at_; continue; }
                if (this->input_.substr(this->at_, 2) != "--") break;
                this->at_ += 2;
                if (this->input_.substr(this->at_, 2) == "[[")
                { const auto end = this->input_.find("]]", this->at_ + 2); if (end == std::string_view::npos) this->Fail("Unterminated comment"); this->at_ = end + 2; }
                else { const auto end = this->input_.find('\n', this->at_); this->at_ = end == std::string_view::npos ? this->input_.size() : end + 1; }
            }
        }
        auto Consume(char expected) -> bool
        { this->Skip(); if (this->at_ < this->input_.size() && this->input_[this->at_] == expected) { ++this->at_; return true; } return false; }
        auto Identifier(void) -> std::string
        {
            this->Skip(); const auto start = this->at_;
            while (this->at_ < this->input_.size() && this->IdentifierPart(this->input_[this->at_])) ++this->at_;
            return std::string{this->input_.substr(start, this->at_ - start)};
        }
        auto String(void) -> std::string
        {
            this->Skip();
            if (this->input_.substr(this->at_, 2) == "[[")
            { const auto start = this->at_ + 2, end = this->input_.find("]]", start); if (end == std::string_view::npos) this->Fail("Unterminated long string"); this->at_ = end + 2; return std::string{this->input_.substr(start, end - start)}; }
            const char quote = this->input_[this->at_++]; std::string result;
            while (this->at_ < this->input_.size())
            {
                char c = this->input_[this->at_++]; if (c == quote) return result;
                if (c == '\\')
                {
                    if (this->at_ == this->input_.size()) break;
                    c = this->input_[this->at_++];
                    if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
                    else if (c != '\\' && c != '\'' && c != '"') this->Fail("Unsupported string escape");
                }
                if (c == '\0') this->Fail("NUL bytes are not supported"); result += c;
                if (result.size() > 32768) this->Fail("String exceeds the import limit");
            }
            this->Fail("Unterminated string");
        }
        auto Value(size_t depth) -> literal
        {
            this->Skip(); if (depth > 24 || ++this->nodes_ > 100000) this->Fail("Literal nesting or record limit exceeded");
            if (this->at_ >= this->input_.size()) this->Fail("Unexpected end of data");
            if (this->input_.substr(this->at_, 2) == "T{" || this->input_.substr(this->at_, 2) == "T ") { ++this->at_; this->Skip(); }
            if (this->at_ >= this->input_.size()) this->Fail("Expected a table after T");
            if (this->input_[this->at_] == '{')
            {
                ++this->at_; literal table{literal::kind::table}; size_t arrayIndex = 1;
                while (!this->Consume('}'))
                {
                    this->Skip(); std::string key; const auto start = this->at_;
                    if (this->input_.substr(this->at_, 2) != "[[" && this->Consume('['))
                    { const auto parsed = this->Value(depth + 1); if (parsed.type != literal::kind::scalar) this->Fail("Table keys must be strings or numbers"); key = parsed.text; if (!this->Consume(']') || !this->Consume('=')) this->Fail("Expected ] = after table key"); }
                    else if (this->at_ < this->input_.size() && (std::isalpha(static_cast<unsigned char>(this->input_[this->at_])) || this->input_[this->at_] == '_'))
                    { key = this->Identifier(); if (!this->Consume('=')) { this->at_ = start; key.clear(); } }
                    if (key.empty()) key = std::to_string(arrayIndex++);
                    auto value = this->Value(depth + 1);
                    if (std::any_of(table.fields.begin(), table.fields.end(), [&](const auto& old) { return old.first == key; })) ++this->duplicates;
                    table.fields.emplace_back(std::move(key), std::move(value));
                    if (this->Consume(',') || this->Consume(';')) continue;
                    if (!this->Consume('}')) this->Fail("Expected a comma or table end");
                    return table;
                }
                return table;
            }
            if (this->input_[this->at_] == '\'' || this->input_[this->at_] == '"' || this->input_.substr(this->at_, 2) == "[[") return {literal::kind::scalar, this->String()};
            const auto start = this->at_;
            if (this->input_[this->at_] == '-' || this->input_[this->at_] == '+') ++this->at_;
            if (this->at_ < this->input_.size() && std::isdigit(static_cast<unsigned char>(this->input_[this->at_])))
            {
                const bool hex = this->input_.substr(this->at_, 2) == "0x" || this->input_.substr(this->at_, 2) == "0X";
                if (hex) this->at_ += 2;
                while (this->at_ < this->input_.size() && (hex ? std::isxdigit(static_cast<unsigned char>(this->input_[this->at_])) != 0 : std::isdigit(static_cast<unsigned char>(this->input_[this->at_])) != 0)) ++this->at_;
                if (!hex && this->at_ < this->input_.size() && this->input_[this->at_] == '.')
                { ++this->at_; while (this->at_ < this->input_.size() && std::isdigit(static_cast<unsigned char>(this->input_[this->at_]))) ++this->at_; }
                if (!hex && this->at_ < this->input_.size() && (this->input_[this->at_] == 'e' || this->input_[this->at_] == 'E'))
                { ++this->at_; if (this->at_ < this->input_.size() && (this->input_[this->at_] == '-' || this->input_[this->at_] == '+')) ++this->at_; while (this->at_ < this->input_.size() && std::isdigit(static_cast<unsigned char>(this->input_[this->at_]))) ++this->at_; }
                return {literal::kind::scalar, std::string{this->input_.substr(start, this->at_ - start)}};
            }
            this->at_ = start;
            const auto word = this->Identifier();
            if (word == "true" || word == "false") return {literal::kind::scalar, word};
            if (word == "nil") return {};
            this->Fail("Expressions, functions and executable Lua are not supported");
        }
    };
    auto Lines(std::string text) -> std::vector<std::string>
    {
        std::vector<std::string> lines;
        while (!text.empty())
        { const auto end = text.find('\n'); auto line = text.substr(0, end); if (!line.empty() && line.back() == '\r') line.pop_back(); if (!line.empty()) lines.push_back(std::move(line)); text = end == std::string::npos ? std::string{} : text.substr(end + 1); }
        return lines;
    }
    auto Binding(const literal& data, xiui::action_import_preview& preview) -> xiui::action_slot
    {
        xiui::action_slot action{};
        const auto type = Text(data, "ActionType", Text(data, "kind"));
        if (type.empty() || Lower(type) == "empty" || Lower(type) == "none") return action;
        action.kind = xiui::ActionKindFromName(type);
        action.name = Text(data, "action", Text(data, "name", Text(data, "Label")));
        action.label = Text(data, "displayName", Text(data, "Label"));
        action.id = Number(Text(data, "id", Text(data, "resourceId", Text(data, "itemId"))));
        action.icon = Text(data, "icon", Text(data, "customIconPath", Text(data, "Image")));
        action.target = Text(data, "target", action.kind == xiui::action_kind::item ? "<me>" : "<t>");
        if (!action.target.empty() && action.target.front() != '<') action.target = "<" + action.target + ">";
        action.equipment_slot = Lower(Text(data, "equipment_slot", Text(data, "equipSlot", Text(data, "slot"))));
        if (const auto* macro = Find(data, "Macro"); macro != nullptr && macro->type == literal::kind::table)
        { for (const auto& [key, line] : macro->fields) if (line.type == literal::kind::scalar && Number(key) != 0) action.lines.push_back(line.text); }
        if (action.lines.empty()) action.lines = Lines(Text(data, "macroText"));
        if (!action.lines.empty()) action.kind = xiui::action_kind::macro;
        if ((action.kind == xiui::action_kind::macro || action.kind == xiui::action_kind::command) && action.lines.empty() && !action.name.empty()) action.lines = Lines(action.name);
        if (action.kind == xiui::action_kind::native_macro)
        {
            const auto book = Number(Text(data, "book"), 1), set = Number(Text(data, "set"), 1), slot = Number(Text(data, "slot"), 1);
            action.native.book = static_cast<uint8_t>(book <= 40 ? book : 0); action.native.set = static_cast<uint8_t>(set <= 10 ? set : 0); action.native.slot = static_cast<uint8_t>(slot <= 10 ? slot : 0); action.native.alt = Lower(Text(data, "alt")) == "true";
        }
        auto reason = action.kind == xiui::action_kind::none ? "Unsupported action type: " + type : xiui::ValidateAction(action);
        if (!reason.empty()) { ++preview.unsupported; if (preview.warnings.size() < 32) preview.warnings.push_back(reason); return {}; }
        return action;
    }
    auto Bank(const std::string& key) -> int
    {
        const auto name = Lower(key);
        if (name == "lt" || name == "l2" || name == "lefttrigger") return 0;
        if (name == "rt" || name == "r2" || name == "righttrigger") return 1;
        if (name == "ltrt" || name == "l2r2" || name == "bothtriggersleft") return 2;
        if (name == "rtlt" || name == "r2l2" || name == "bothtriggersright") return 3;
        if (name == "lt2" || name == "l2x2" || name == "doubletapleft") return 4;
        if (name == "rt2" || name == "r2x2" || name == "doubletapright") return 5;
        return -1;
    }
    auto Address(const std::string& key, xiui::action_surface& surface, size_t& bar, size_t& slot) -> bool
    {
        const auto colon = key.find(':');
        if (colon != std::string::npos)
        { const auto bank = Bank(key.substr(0, colon)); const auto index = Number(key.substr(colon + 1)); if (bank < 0 || index < 1 || index > 8) return false; surface = xiui::action_surface::crossbar; bar = bank; slot = index - 1; return true; }
        if (key.empty()) return false;
        bar = 0; std::string digits = key;
        if (key.front() == 'C' || key.front() == '^') { bar = 1; digits = key.substr(1); }
        else if (key.front() == 'A' || key.front() == '!') { bar = 2; digits = key.substr(1); }
        else if (key.front() == 'S' || key.front() == '+') { bar = 3; digits = key.substr(1); }
        auto index = Number(digits, 100); if (index == 0) index = 10; if (index < 1 || index > 12) return false;
        surface = xiui::action_surface::hotbar; slot = index - 1; return true;
    }
    auto Put(xiui::action_palette& palette, xiui::action_surface surface, size_t bar, size_t slot, xiui::action_slot action, xiui::action_import_preview& preview) -> void
    {
        if (action.Empty()) return;
        auto& destination = surface == xiui::action_surface::hotbar ? palette.actions.hotbar[bar][slot] : palette.actions.crossbar[bar][slot];
        if (!destination.Empty()) ++preview.conflicts; else ++preview.actions;
        destination = std::move(action);
    }
    auto Bindings(xiui::action_palette& palette, const literal& data, xiui::action_import_preview& preview) -> void
    {
        for (const auto& [key, binding] : data.fields)
        {
            xiui::action_surface surface{}; size_t bar{}, slot{};
            if (binding.type != literal::kind::table) continue;
            if (!Address(key, surface, bar, slot)) { if (Find(binding, "ActionType") != nullptr) ++preview.unsupported; continue; }
            Put(palette, surface, bar, slot, Binding(binding, preview), preview);
        }
    }
}

namespace xiui
{
    auto PreviewActionImport(std::string_view text, std::string_view sourceName) -> action_import_preview
    {
        action_import_preview preview{};
        if (text.size() > 2 * 1024 * 1024) { preview.error = "The import exceeds 2 MB."; return preview; }
        if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
        if (text.find("[actionbars.data]") != std::string_view::npos)
        {
            preview.source_context = true;
            const auto settings = settings_store::FromIniText(text);
            if (settings.ReadInt("actionbars.data", "version", 0) != 1) { preview.error = "Unsupported action palette version."; return preview; }
            action_store store; store.Load(settings);
            for (const auto& palette : store.AllPalettes())
            {
                auto copy = palette;
                const auto expand = [&](action_layout& layout) { for (auto& bar : layout.hotbar) for (auto& slot : bar) slot = store.ExpandMacro(slot); for (auto& bank : layout.crossbar) for (auto& slot : bank) slot = store.ExpandMacro(slot); };
                expand(copy.actions); for (auto& pet : copy.pets) expand(pet.actions);
                preview.palettes.push_back(std::move(copy));
                for (const auto& bar : palette.actions.hotbar) for (const auto& slot : bar) if (!slot.Empty()) ++preview.actions;
                for (const auto& bank : palette.actions.crossbar) for (const auto& slot : bank) if (!slot.Empty()) ++preview.actions;
                for (const auto& pet : palette.pets)
                { for (const auto& bar : pet.actions.hotbar) for (const auto& slot : bar) if (!slot.Empty()) ++preview.actions; for (const auto& bank : pet.actions.crossbar) for (const auto& slot : bank) if (!slot.Empty()) ++preview.actions; }
            }
            if (preview.actions == 0) preview.error = "No supported action bindings were found.";
            return preview;
        }
        try
        {
            parser reader{text}; auto root = reader.Parse(); preview.conflicts = reader.duplicates;
            if (const auto* database = Find(root, "macroDB"))
            {
                std::map<std::string, literal> macros;
                for (const auto& [paletteKey, entries] : database->fields)
                    for (const auto& [key, macro] : entries.fields)
                        if (const auto id = Text(macro, "id"); !id.empty()) { macros[id] = macro; macros[paletteKey + ":" + id] = macro; }
                std::function<void(literal&)> expand = [&](literal& data) {
                    const auto id = Text(data, "macroRef");
                    if (!id.empty())
                    {
                        auto source = macros.find(Text(data, "macroPaletteKey") + ":" + id);
                        if (source == macros.end()) source = macros.find(id);
                        if (source != macros.end()) data = source->second;
                        else { ++preview.unsupported; if (preview.warnings.size() < 32) preview.warnings.push_back("Missing source macro reference: " + id); data = {}; }
                    }
                    for (auto& [key, child] : data.fields) if (Lower(key) != "macrodb") expand(child);
                };
                expand(root);
            }
            const auto add = [&](const literal& bindings, std::string name) {
                if (preview.palettes.size() >= 64) { ++preview.unsupported; return; }
                action_palette palette{}; palette.name = name.empty() ? "Imported" : std::move(name); Bindings(palette, bindings, preview);
                preview.palettes.push_back(std::move(palette));
            };
            if (const auto* defaults = Find(root, "Default")) add(*defaults, "Imported Default");
            if (const auto* palettes = Find(root, "Palettes"))
                for (const auto& [key, data] : palettes->fields) if (const auto* bindings = Find(data, "Bindings")) add(*bindings, Text(data, "Name", "Imported"));
            if (const auto* bindings = Find(root, "Bindings")) add(*bindings, Text(root, "Name", "Imported"));
            if (preview.palettes.empty())
            {
                action_palette legacy{}; legacy.name = sourceName.empty() ? "Imported" : std::string{sourceName.substr(sourceName.find_last_of("/\\") == std::string_view::npos ? 0 : sourceName.find_last_of("/\\") + 1)};
                Bindings(legacy, root, preview);
                for (const auto& [key, value] : root.fields)
                {
                    const auto bank = Bank(key); if (bank < 0) continue;
                    for (const auto& [slotKey, binding] : value.fields)
                    { const auto index = Number(slotKey.starts_with("Macro") ? slotKey.substr(5) : slotKey); if (index >= 1 && index <= 8) Put(legacy, action_surface::crossbar, bank, index - 1, Binding(binding, preview), preview); }
                }
                if (preview.actions != 0) preview.palettes.push_back(std::move(legacy));
            }
            // XIUI stores each bar's slots under a job/subjob/palette storage key.
            if (preview.palettes.empty())
            {
                const auto* hotbar = Find(root, "hotbar"); const auto* crossbar = Find(root, "hotbarCrossbar"); if (crossbar == nullptr) crossbar = Find(root, "crossbar");
                std::map<std::string, action_palette> grouped;
                const auto process = [&](const literal& storage, action_surface surface, size_t bar) {
                    for (const auto& [storageKey, slots] : storage.fields)
                    {
                        if (!grouped.contains(storageKey) && grouped.size() >= 128) { ++preview.unsupported; continue; }
                        auto& palette = grouped[storageKey]; palette.name = storageKey;
                        const auto split = storageKey.find(':');
                        if (split != std::string::npos)
                        {
                            const auto job = Number(storageKey.substr(0, split), 255); const auto next = storageKey.find(':', split + 1);
                            const auto subjob = Number(storageKey.substr(split + 1, next == std::string::npos ? next : next - split - 1));
                            if (job >= 1 && job <= 22 && subjob <= 22) { palette.job = static_cast<uint8_t>(job); palette.subjob = static_cast<uint8_t>(subjob); preview.source_context = true; }
                        }
                        else if (Lower(storageKey) == "global") preview.source_context = true;
                        if (const auto name = storageKey.find(":palette:"); name != std::string::npos) palette.name = storageKey.substr(name + 9);
                        if (surface == action_surface::hotbar)
                        { for (const auto& [slotKey, binding] : slots.fields) { const auto index = Number(slotKey); if (index >= 1 && index <= 12) Put(palette, surface, bar, index - 1, Binding(binding, preview), preview); } }
                        else
                        { for (const auto& [bankKey, entries] : slots.fields) { const auto bank = Bank(bankKey); if (bank < 0) continue; for (const auto& [slotKey, binding] : entries.fields) { const auto index = Number(slotKey); if (index >= 1 && index <= 8) Put(palette, surface, bank, index - 1, Binding(binding, preview), preview); } } }
                    }
                };
                if (hotbar != nullptr)
                {
                    const auto* bars = Find(*hotbar, "bars"); if (bars == nullptr) bars = hotbar;
                    for (const auto& [barKey, data] : bars->fields)
                    { const auto bar = Number(barKey); if (bar >= 1 && bar <= 10) if (const auto* storage = Find(data, "slotActions")) process(*storage, action_surface::hotbar, bar - 1); }
                }
                for (size_t bar = 0; bar < 10; ++bar)
                    if (const auto* data = Find(root, "hotbarBar" + std::to_string(bar + 1)))
                        if (const auto* storage = Find(*data, "slotActions")) process(*storage, action_surface::hotbar, bar);
                if (crossbar != nullptr) if (const auto* storage = Find(*crossbar, "slotActions")) process(*storage, action_surface::crossbar, 0);
                std::vector<std::pair<std::string, action_layout>> petLayouts;
                for (auto it = grouped.begin(); it != grouped.end();)
                {
                    if (it->first.find(':') == std::string::npos && Lower(it->first) != "global")
                    {
                        const auto legacyJob = Number(it->first);
                        if (legacyJob >= 1 && legacyJob <= 22) { it->second.job = static_cast<uint8_t>(legacyJob); preview.source_context = true; ++it; }
                        else { petLayouts.emplace_back(it->first, std::move(it->second.actions)); it = grouped.erase(it); }
                    }
                    else ++it;
                }
                for (auto& [petName, layout] : petLayouts)
                {
                    constexpr const char* avatars[]{"Carbuncle", "Ifrit", "Shiva", "Garuda", "Titan", "Ramuh", "Leviathan", "Fenrir", "Diabolos", "Cait Sith", "Alexander", "Odin", "Atomos", "Siren"};
                    const bool avatar = std::find(std::begin(avatars), std::end(avatars), petName) != std::end(avatars) || petName.ends_with("Spirit");
                    const uint8_t job = petName == "Wyvern" ? 14 : petName == "Automaton" ? 18 : avatar ? 15 : 9;
                    bool attached{};
                    for (auto& [key, palette] : grouped)
                        if (palette.job == job || palette.job == 0)
                        { if (palette.pets.size() < 32) palette.pets.push_back({petName, layout}); else ++preview.unsupported; attached = true; }
                    if (!attached)
                    {
                        auto& palette = grouped[std::to_string(job) + ":0:palette:Imported"];
                        palette.job = job; palette.name = "Imported";
                        if (palette.pets.size() < 32) palette.pets.push_back({petName, std::move(layout)}); else ++preview.unsupported;
                    }
                    preview.source_context = true;
                }
                for (auto& [key, palette] : grouped) if (preview.palettes.size() < 64) preview.palettes.push_back(std::move(palette)); else ++preview.unsupported;
            }
            if (!preview.source_context && !sourceName.empty())
            {
                auto file = std::string{sourceName.substr(sourceName.find_last_of("/\\") == std::string_view::npos ? 0 : sourceName.find_last_of("/\\") + 1)};
                if (const auto extension = file.find_last_of('.'); extension != std::string::npos) file.resize(extension);
                file = Lower(std::move(file));
                constexpr const char* jobs[]{"", "war", "mnk", "whm", "blm", "rdm", "thf", "pld", "drk", "bst", "brd", "rng", "sam", "nin", "drg", "smn", "blu", "cor", "pup", "dnc", "sch", "geo", "run"};
                for (uint8_t job = 1; job <= 22; ++job) if (file == jobs[job]) { for (auto& palette : preview.palettes) { palette.job = job; palette.subjob = 0; } preview.source_context = true; break; }
                if (file == "global" || file == "globals") preview.source_context = true;
            }
            if (preview.palettes.empty() || preview.actions == 0) preview.error = "No supported action bindings were found.";
            if (preview.conflicts != 0) preview.warnings.push_back("Duplicate source bindings use their final literal value; existing XIUI palettes are preserved.");
        }
        catch (const std::exception& error) { preview.error = error.what(); preview.palettes.clear(); preview.actions = 0; }
        return preview;
    }
    auto CommitActionImport(action_store& store, const action_import_preview& preview, action_surface surface, size_t bar) -> size_t
    {
        if (!preview.Valid()) return 0;
        size_t count{};
        for (const auto& palette : preview.palettes) if (!store.ImportPalette(palette, surface, bar, preview.source_context).empty()) ++count;
        return count;
    }
}
