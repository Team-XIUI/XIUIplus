#include "chains_module.hpp"
#include "ui/text_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    namespace
    {
        constexpr const char* text_sections[]{"text.chains.title", "text.chains.timer", "text.chains.properties",
            "text.chains.heading", "text.chains.action", "text.chains.result"};
        constexpr const char* group_names[]{"Weapon skills", "Spells", "Pet skills"};
    }
    auto chains_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.meter_text},
            module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text},
            module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text}};
        this->text_[0].style.scale = 0.85f;
        this->text_[1].style.scale = 0.95f;
        for (size_t i = 2; i < this->text_.size(); ++i) this->text_[i].style.scale = 0.75f;
        for (auto& profile : this->text_) profile.style.alignment = ui::text_alignment::left;
    }
    auto chains_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds; const auto* tracker = this->tracker_; const auto* actions = this->actions_;
        *this = {}; this->state_.bounds = bounds; this->SetSources(tracker, actions); this->ApplyTextDefaults(theme);
    }
    auto chains_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("chains", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.chains", this->state_.bounds);
        this->state_.bounds.width = std::clamp(this->state_.bounds.width, 240.0f, 800.0f);
        for (const auto& [key, value] : {std::pair{"weapon", &this->filters_.weapon}, {"spell", &this->filters_.spell},
            {"pet", &this->filters_.pet}, {"avatar", &this->filters_.avatar}, {"jug", &this->filters_.jug}, {"frame", &this->filters_.frame},
            {"container", &this->container_}, {"color", &this->color_}, {"burst_elements", &this->burst_elements_}})
            *value = store.ReadBool("chains", key, true);
        this->filters_.require_buff = store.ReadBool("chains", "require_buff", false);
        this->direction_ = std::clamp(store.ReadInt("chains", "direction", 0), 0, 1);
        this->max_rows_ = std::clamp(store.ReadFloat("chains", "max_rows", 8), 1.0f, 24.0f);
        this->container_style_.opacity = std::clamp(store.ReadFloat("chains", "opacity", 1), 0.0f, 1.0f);
        this->container_style_.padding = std::clamp(store.ReadFloat("chains", "padding", 8), 0.0f, 24.0f);
        this->selected_text_ = std::clamp(store.ReadInt("chains", "selected_text", 0), 0, 5);
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, text_sections[i], this->text_[i]);
    }
    auto chains_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("chains", "enabled", this->state_.enabled); store.WriteRect("layout.chains", this->state_.bounds);
        for (const auto& [key, value] : {std::pair{"weapon", this->filters_.weapon}, {"spell", this->filters_.spell}, {"pet", this->filters_.pet},
            {"require_buff", this->filters_.require_buff}, {"avatar", this->filters_.avatar}, {"jug", this->filters_.jug}, {"frame", this->filters_.frame},
            {"container", this->container_}, {"color", this->color_}, {"burst_elements", this->burst_elements_}})
            store.WriteBool("chains", key, value);
        store.WriteInt("chains", "direction", this->direction_); store.WriteFloat("chains", "max_rows", this->max_rows_);
        store.WriteFloat("chains", "opacity", this->container_style_.opacity); store.WriteFloat("chains", "padding", this->container_style_.padding);
        store.WriteInt("chains", "selected_text", this->selected_text_);
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, text_sections[i], this->text_[i]);
    }
    auto chains_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Observed skillchain windows and follow-up suggestions. Server rules are shared with Action Bars / Skillchains.", "general"},
            {"Layout", "Window width, growth direction and number of suggestions per group.", "layout"},
            {"Appearance", "Native window frame, property colors and burst elements.", "appearance"},
            {"Text", "Independent title, countdown, property, heading, action and result text profiles.", "text"}};
        return sections;
    }
    auto chains_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->ContentOptionCount()) return false;
        if (index >= selected_text) return section == 3;
        if (index >= container) return section == 2;
        if (index >= width) return section == 1;
        return section == 0;
    }
    auto chains_module::ContentOption(const size_t index) -> module_option
    {
        static constexpr const char* directions[]{"Top down", "Bottom up"};
        static constexpr const char* profiles[]{"Title / step", "Countdown", "Properties / elements", "Group headings", "Action names", "Chain results"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show Chains while your selected enemy has an observed skillchain window.", &this->state_.enabled).Ordered(-50);
        case weapon: return module_option::Toggle("Weapon skills", "Suggest the weapon skills currently available for your equipped weapons.", &this->filters_.weapon);
        case spell: return module_option::Toggle("Spells", "Suggest equipped Blue Magic and learned Scholar spells that can continue the chain.", &this->filters_.spell);
        case pet: return module_option::Toggle("Pet skills", "Suggest Blood Pacts, jug Ready moves and automaton weapon skills.", &this->filters_.pet);
        case require_buff: return module_option::Toggle("Require spell ability", "Only list spells while Chain Affinity, Azure Lore or Immanence is active. Otherwise show them dimmed as planning hints.", &this->filters_.require_buff).VisibleWhen(this->filters_.spell);
        case avatar: return module_option::Toggle("Current avatar", "Limit Blood Pact suggestions to the summoned avatar.", &this->filters_.avatar).VisibleWhen(this->filters_.pet);
        case jug: return module_option::Toggle("Current jug pet", "Limit Ready suggestions to the current jug pet's known moves.", &this->filters_.jug).VisibleWhen(this->filters_.pet);
        case frame: return module_option::Toggle("Current automaton frame", "Limit automaton suggestions to the equipped frame and observed automaton skills.", &this->filters_.frame).VisibleWhen(this->filters_.pet);
        case width: return ModuleWidthOption(this->state_);
        case direction: return module_option::Choice("Direction", "Top down places timing above the list. Bottom up keeps the bottom edge fixed as the list grows upward.", &this->direction_, directions, 2);
        case max_rows: return module_option::Slider("Suggestions per group", "Show the strongest results first, with an omitted count for longer lists. Also fits the available viewport height.", &this->max_rows_, 1, 24, 1);
        case container: return module_option::Toggle("Container", "Use the shared native window frame and backing.", &this->container_);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the native frame and backing.", &this->container_style_.opacity, 0, 1, 0.05f).VisibleWhen(this->container_);
        case padding: return module_option::Slider("Container padding", "Space inside the native window frame.", &this->container_style_.padding, 0, 24, 1).VisibleWhen(this->container_);
        case color: return module_option::Toggle("Property colors", "Tint properties and results while retaining their normal text effects.", &this->color_);
        case burst_elements: return module_option::Toggle("Burst elements", "List spell elements after an observed skillchain, alongside the separate Magic Burst module.", &this->burst_elements_);
        case selected_text: return module_option::Element("Selected text", "Choose which Chains text element to style.", &this->selected_text_, profiles, 6);
        default: return TextProfileOption(index - base_options, this->text_[this->selected_text_]);
        }
    }
    auto chains_module::ShouldRender(const hud_snapshot&) const -> bool
    { return this->state_.enabled && this->tracker_ != nullptr && this->tracker_->Current(::GetTickCount64()) != nullptr; }

    auto chains_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        if (!this->state_.enabled) return;
        const auto now = ::GetTickCount64(); const bool sample = context.IsEditing();
        const auto* window = this->tracker_ != nullptr ? this->tracker_->Current(now) : nullptr;
        skillchain_window preview{};
        if (sample)
        {
            preview.depth = 1; preview.chain = skillchain_property::distortion; preview.properties = 1u << 11; preview.order = 11;
            preview.updated = now; preview.opens = now; preview.closes = now + 6800; preview.burst_started = now; window = &preview;
        }
        if (window == nullptr) return;
        auto rows = !sample && this->tracker_ != nullptr && this->actions_ != nullptr ?
            ChainsRows(*this->tracker_, *window, this->actions_->Actions(), this->filters_) : std::vector<chains_row>{};
        if (sample)
        {
            if (this->filters_.weapon) rows.push_back({chains_group::weapon, "Tachi: Gekko", skillchain_property::darkness, true});
            if (this->filters_.weapon) rows.push_back({chains_group::weapon, "Tachi: Kasha", skillchain_property::fusion, true});
            if (this->filters_.spell) rows.push_back({chains_group::spell, "Stone", skillchain_property::darkness, false});
            if (this->filters_.pet) rows.push_back({chains_group::pet, "Mountain Buster", skillchain_property::darkness, true});
        }
        const auto& design = context.Design();
        const auto lineHeight = [&](const size_t index) { return std::ceil(std::max(16.0f, design.TextHeight(this->text_[index].style) + 3)); };
        const float gap = 5, actionHeight = std::max(lineHeight(4), lineHeight(5));
        const float inset = this->container_ ? this->container_style_.edge_size + this->container_style_.padding : 0;
        struct property_line { std::string name; skillchain_property property; };
        std::vector<property_line> properties;
        for (auto order = window->order; order != 0; order >>= 5)
        {
            const auto property = static_cast<skillchain_property>(order & 31);
            if (property != skillchain_property::none) properties.push_back({SkillchainName(property), property});
        }
        const float propertyHeight = static_cast<float>(properties.size()) * lineHeight(2);
        const bool showElements = this->burst_elements_ && window->Burst(now);
        const float summaryHeight = lineHeight(0) + lineHeight(1) + propertyHeight + (showElements ? lineHeight(2) : 0) + gap;
        std::array<size_t, 3> counts{}, shown{};
        for (const auto& row : rows) ++counts[static_cast<size_t>(row.group)];
        const float viewportHeight = context.Viewport().height;
        const auto groups = static_cast<float>(std::count_if(counts.begin(), counts.end(), [](size_t n) { return n != 0; }));
        const float available = std::max(0.0f, viewportHeight - inset * 2 - summaryHeight - 32 - groups * (lineHeight(3) * 2 + gap));
        size_t remaining = viewportHeight > 0 ? static_cast<size_t>(available / actionHeight) : 72;
        const auto limit = static_cast<size_t>(std::clamp(this->max_rows_, 1.0f, 24.0f));
        for (size_t row = 0; row < limit && remaining > 0; ++row)
            for (size_t group = 0; group < counts.size() && remaining > 0; ++group)
                if (shown[group] < counts[group]) { ++shown[group]; --remaining; }
        float listHeight = rows.empty() ? lineHeight(3) : 0;
        for (size_t i = 0; i < counts.size(); ++i) if (counts[i] != 0)
            listHeight += gap + lineHeight(3) + static_cast<float>(shown[i]) * actionHeight + (shown[i] < counts[i] ? lineHeight(3) : 0);
        const float height = inset * 2 + summaryHeight + listHeight;
        if (this->direction_ == 1) this->state_.bounds.y += this->state_.bounds.height - height;
        this->state_.bounds.height = height;
        const auto bounds = this->state_.bounds;
        const auto content = this->container_ ? context.DrawContainer(bounds, this->container_style_) : bounds;
        auto y = content.y;
        const auto draw = [&](const std::string& value, const ui::rect& rect, const size_t profile, const D3DCOLOR tint = 0xFFFFFFFF) {
            context.DrawText(ui::FitText(design, value, rect.width, this->text_[profile].style).c_str(), rect, rect,
                tint == 0xFFFFFFFF ? this->text_[profile] : TintTextProfile(this->text_[profile], tint));
        };
        const auto line = [&](const std::string& value, const size_t profile, const D3DCOLOR tint = 0xFFFFFFFF) {
            draw(value, {content.x, y, content.width, lineHeight(profile)}, profile, tint); y += lineHeight(profile);
        };
        const bool terminal = window->depth >= 5 || window->chain >= skillchain_property::light2;
        const auto summary = [&]() {
            line("Chains  /  Step " + std::to_string(window->depth + 1), 0);
            char timing[64]{}; const bool waiting = now < window->opens && !terminal;
            std::snprintf(timing, sizeof(timing), "%s  %.1fs", terminal ? "Burst" : waiting ? "Wait" : "Go!",
                static_cast<float>((waiting ? window->opens : window->closes) - now) / 1000);
            auto timerProfile = this->text_[1]; timerProfile.style.tabular_numbers = true;
            const ui::rect timerBounds{content.x, y, content.width, lineHeight(1)};
            context.DrawText(timing, timerBounds, timerBounds, this->color_ ? TintTextProfile(timerProfile, waiting ? 0xFFFFBB75 : 0xFF9EDAA2) : timerProfile);
            y += lineHeight(1);
            for (const auto& property : properties) line(property.name, 2, this->color_ ? SkillchainColor(property.property) : 0xFFFFFFFF);
            if (showElements)
            {
                constexpr const char* names[]{"Fire", "Ice", "Wind", "Earth", "Lightning", "Water", "Light", "Dark"};
                std::string elements;
                for (uint8_t i = 0; i < 8; ++i) if (SkillchainElements(window->chain) & (1u << i))
                { if (!elements.empty()) elements += " / "; elements += names[i]; }
                line(elements, 2);
            }
            y += gap;
        };
        const auto list = [&]() {
            if (rows.empty()) { line(terminal ? "Burst window" : "No matching follow-ups", 3); return; }
            for (size_t group = 0; group < counts.size(); ++group)
            {
                if (counts[group] == 0) continue;
                y += gap; line(group_names[group], 3);
                size_t drawn{};
                for (const auto& row : rows)
                {
                    if (static_cast<size_t>(row.group) != group || drawn >= shown[group]) continue;
                    const auto result = "Lv." + std::to_string(ChainsLevel(row.result)) + " " + SkillchainName(row.result);
                    const float resultWidth = std::min(content.width * 0.53f, design.MeasureText(result.c_str(), this->text_[5].style) + 4);
                    const D3DCOLOR alpha = row.ready ? 0xFFFFFFFF : 0x80FFFFFF;
                    draw(row.name, {content.x, y, std::max(1.0f, content.width - resultWidth - 8), actionHeight}, 4, alpha);
                    const auto tint = this->color_ ? SkillchainColor(row.result) : 0xFFFFFFFF;
                    draw(result, {content.x + content.width - resultWidth, y, resultWidth, actionHeight}, 5, (tint & 0xFFFFFF) | (alpha & 0xFF000000));
                    y += actionHeight; ++drawn;
                }
                if (shown[group] < counts[group]) line("+ " + std::to_string(counts[group] - shown[group]) + " more", 3);
            }
        };
        const auto clip = design.Clip(content);
        if (this->direction_ == 0) { summary(); list(); }
        else { list(); summary(); }
    }
}
