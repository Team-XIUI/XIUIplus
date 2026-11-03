#include "ready_check_module.hpp"
#include "ui/game_icon_cache.hpp"

#include <charconv>
#include <cmath>

namespace
{
    constexpr const char* text_sections[]{"heading", "names", "status", "details", "buttons"};
    auto Inside(const xiui::ui::rect& bounds, float x, float y) -> bool
    {
        return bounds.width > 0 && bounds.height > 0 && x >= bounds.x && y >= bounds.y &&
            x < bounds.x + bounds.width && y < bounds.y + bounds.height;
    }
}

namespace xiui
{
    auto ready_check_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->settings_.text = {module_text_profile{theme.title_text}, module_text_profile{theme.meter_text},
            module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.meter_text}};
        constexpr float scales[]{0.9f, 0.85f, 0.78f, 0.75f, 0.85f};
        for (size_t i = 0; i < this->settings_.text.size(); ++i) this->settings_.text[i].style.scale = scales[i];
    }

    auto ready_check_module::ResetDefaults(const ui::theme& theme) -> void
    {
        this->Stop();
        this->state_.enabled = true;
        this->settings_ = {};
        this->ApplyTextDefaults(theme);
        this->ConfigureSound();
    }

    auto ready_check_module::Load(const settings_store& store) -> void
    {
        this->state_.bounds = store.ReadRect("layout.readycheck", this->state_.bounds);
        this->state_.enabled = store.ReadBool("readycheck", "enabled", true);
        for (const auto& [key, value] : {std::pair{"announce", &this->settings_.announce}, {"sound_checker", &this->settings_.sound_checker},
            {"sound_prompt", &this->settings_.sound_prompt}, {"job_icons", &this->settings_.job_icons}})
            *value = store.ReadBool("readycheck", key, *value);
        const auto number = [&](const char* key, float fallback, float minimum, float maximum) {
            const auto value = store.ReadFloat("readycheck", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->settings_.volume = number("volume", 50, 0, 150);
        this->settings_.sound_choice = std::clamp(store.ReadInt("readycheck", "sound_choice", 0), 0, 1);
        this->settings_.sound_file = store.ReadString("readycheck", "sound_file", "");
        if (!ReadyCheckWavFilename(this->settings_.sound_file)) this->settings_.sound_file.clear();
        this->ConfigureSound();
        this->settings_.column_width = number("column_width", 280, 220, 480);
        this->settings_.row_spacing = number("row_spacing", 4, 0, 20);
        this->settings_.icon_size = number("icon_size", 24, 16, 48);
        this->settings_.container.opacity = number("opacity", 0.95f, 0, 1);
        this->settings_.container.padding = number("padding", 10, 0, 24);
        this->settings_.ready = store.ReadArgb("readycheck", "ready_color", this->settings_.ready);
        this->settings_.no = store.ReadArgb("readycheck", "not_ready_color", this->settings_.no);
        this->settings_.pending = store.ReadArgb("readycheck", "pending_color", this->settings_.pending);
        this->settings_.selected_text = std::clamp(store.ReadInt("readycheck", "selected_text", 0), 0, 4);
        for (size_t i = 0; i < this->settings_.text.size(); ++i)
            this->settings_.text[i] = LoadTextProfile(store, (std::string{"text.readycheck."} + text_sections[i]).c_str(), this->settings_.text[i]);
        if (!this->state_.enabled) this->Stop();
    }

    auto ready_check_module::Save(const settings_store& store) const -> void
    {
        store.WriteRect("layout.readycheck", this->state_.bounds);
        store.WriteBool("readycheck", "enabled", this->state_.enabled);
        store.WriteInt("readycheck", "sound_choice", this->settings_.sound_choice);
        store.WriteString("readycheck", "sound_file", ReadyCheckWavFilename(this->settings_.sound_file) ? this->settings_.sound_file : "");
        for (const auto& [key, value] : {std::pair{"announce", this->settings_.announce}, {"sound_checker", this->settings_.sound_checker},
            {"sound_prompt", this->settings_.sound_prompt}, {"job_icons", this->settings_.job_icons}})
            store.WriteBool("readycheck", key, value);
        for (const auto& [key, value] : {std::pair{"volume", this->settings_.volume}, {"column_width", this->settings_.column_width},
            {"row_spacing", this->settings_.row_spacing}, {"icon_size", this->settings_.icon_size},
            {"opacity", this->settings_.container.opacity}, {"padding", this->settings_.container.padding}})
            store.WriteFloat("readycheck", key, value);
        store.WriteArgb("readycheck", "ready_color", this->settings_.ready);
        store.WriteArgb("readycheck", "not_ready_color", this->settings_.no);
        store.WriteArgb("readycheck", "pending_color", this->settings_.pending);
        store.WriteInt("readycheck", "selected_text", this->settings_.selected_text);
        for (size_t i = 0; i < this->settings_.text.size(); ++i)
            SaveTextProfile(store, (std::string{"text.readycheck."} + text_sections[i]).c_str(), this->settings_.text[i]);
    }

    auto ready_check_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Party-chat ready checks, optional summaries and notification sound.", "general"},
            {"Layout", "Party columns, entry spacing and shared job icons.", "layout"},
            {"Appearance", "Native Window Type container and response colors.", "appearance"},
            {"Text", "Independent heading, names, responses, details and button styles.", "text"}};
        return sections;
    }

    auto ready_check_module::ContentConfigSectionContains(size_t section, size_t index) -> bool
    {
        if (index >= sound_options_start) return index < this->OptionCount() && section == 0;
        return index < this->OptionCount() && section == (index <= test_sound ? size_t{0} : index <= icon_size ? size_t{1} :
            index <= pending_color ? size_t{2} : size_t{3});
    }

    auto ready_check_module::ContentOption(size_t index) -> module_option
    {
        auto& settings = this->settings_;
        if (index >= sound_options_start)
        {
            static constexpr const char* sounds[]{"Built-in chime", "Custom WAV"};
            switch (index - sound_options_start)
            {
            case 0: return module_option::Choice("Sound", "Choose a sound; use Test sound for an explicit preview.", &settings.sound_choice, sounds, 2);
            case 1: return module_option::Text("Custom WAV filename", "Filename in config/XIUIplus/sounds. PCM 8/16-bit mono/stereo, up to 30 seconds and 8 MiB. Missing or unsupported files use the built-in chime.", &settings.sound_file, 128).VisibleWhen(settings.sound_choice == 1);
            default: return module_option::Action("Reload custom WAV", this->sound_.Status().c_str(), "Reload").VisibleWhen(settings.sound_choice == 1);
            }
        }
        static constexpr const char* profiles[]{"Heading", "Member names", "Responses", "Details", "Buttons"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Receive ready checks and show their prompts and results.", &this->state_.enabled).Ordered(-50);
        case start:
        {
            auto result = module_option::Action("Start ready check", "Ask the party or alliance to reply within 30 seconds.", "Start");
            result.available = this->state_.enabled && this->source_ != nullptr && this->source_->Count(0) + this->source_->Count(1) + this->source_->Count(2) > 1 &&
                (this->check_.Mode() == ready_check_mode::idle || this->check_.Completed());
            return result;
        }
        case announce: return module_option::Toggle("Announce results", "Send the list of players not ready to party chat when your check finishes.", &settings.announce);
        case sound_checker: return module_option::Toggle("Sound when sending", "Play the ready-check chime when starting a check.", &settings.sound_checker);
        case sound_prompt: return module_option::Toggle("Sound when receiving", "Play the ready-check chime when another member asks.", &settings.sound_prompt);
        case volume: return module_option::Slider("Sound volume", "Chime volume in percent, without changing the game's or system's volume.", &settings.volume, 0, 150, 5);
        case test_sound: return module_option::Action("Test sound", "Play the selected ready-check sound at the selected volume.", "Play");
        case column_width: return module_option::Slider("Party column width", "Width of each party's column in pixels.", &settings.column_width, 220, 480, 4).Ordered(100, true);
        case row_spacing: return module_option::Slider("Entry spacing", "Extra pixels between member rows.", &settings.row_spacing, 0, 20, 1).Ordered(300, true);
        case job_icons: return module_option::Toggle("Job icons", "Use the shared XIVParty job artwork beside member names.", &settings.job_icons);
        case icon_size: return module_option::Slider("Icon size", "Job icon size in pixels.", &settings.icon_size, 16, 48, 1).Ordered(110).VisibleWhen(settings.job_icons);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the native frame and background.", &settings.container.opacity, 0, 1, 0.05f).Ordered(450);
        case padding: return module_option::Slider("Container padding", "Space inside the native frame.", &settings.container.padding, 0, 24, 1).Ordered(402);
        case ready_color: return module_option::Color("Ready", "Tint for ready responses; retains text gradients and effects.", &settings.ready);
        case no_color: return module_option::Color("Not ready", "Tint for negative responses and members who did not answer.", &settings.no);
        case pending_color: return module_option::Color("Waiting", "Tint for pending responses.", &settings.pending);
        case selected_text: return module_option::Element("Selected text", "Style each text element independently.", &settings.selected_text, profiles, 5, module_option_group::text);
        default: return TextProfileOption(index - base_options, settings.text[settings.selected_text]);
        }
    }

    auto ready_check_module::OnContentOptionChanged(size_t index) -> void
    {
        if (index == enabled && !this->state_.enabled) this->Stop();
        if (index == start) this->Start(::GetTickCount64());
        if (index == test_sound) this->sound_.Play(this->settings_.volume);
        if (index >= sound_options_start) this->ConfigureSound();
    }

    auto ready_check_module::Roster(void) const -> std::vector<ready_check_member>
    {
        std::vector<ready_check_member> roster;
        if (this->source_ != nullptr)
            for (const auto& member : this->source_->Members())
                if (member.active && member.id != 0 && !member.name.empty())
                    roster.push_back({member.id, member.slot, member.main_job, member.name});
        return roster;
    }

    auto ready_check_module::Send(std::string_view body) -> void
    {
        if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->QueueCommand(-1, (std::string{"/p "} + std::string{body}).c_str());
    }

    auto ready_check_module::Report(const char* message) const -> void
    {
        if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->Writef(1, false, "[XIUI+] %s", message);
    }

    auto ready_check_module::Start(uint64_t now) -> void
    {
        if (!this->state_.enabled) { this->Report("Ready Check is disabled. Enable it in Ready Check settings."); return; }
        if (this->check_.Mode() != ready_check_mode::idle && !this->check_.Completed())
        { this->Report("A ready check is already active. Use /readycheck close to dismiss it."); return; }
        const auto roster = this->Roster();
        const auto player = this->source_ != nullptr ? this->source_->Members()[0].id : 0;
        if (!this->check_.Start(roster, player, player, now)) { this->Report("Join a party or alliance before starting a ready check."); return; }
        this->Send("Ready Check! Are you ready? Answer Yes, No, or /");
        if (this->settings_.sound_checker) this->sound_.Play(this->settings_.volume);
    }

    auto ready_check_module::Answer(bool yes, uint64_t now) -> void
    {
        if (this->check_.Answer(now)) this->Send(yes ? "Yes" : "No");
    }

    auto ready_check_module::Update(IAshitaCore* core, uint64_t now) -> void
    {
        this->core_ = core;
        this->sound_.Tick();
        if (!this->state_.enabled) { this->check_.Clear(); this->CancelPointer(); return; }
        if (this->source_ == nullptr || !this->source_->Available() ||
            (this->check_.Mode() != ready_check_mode::idle && this->source_->Count(0) + this->source_->Count(1) + this->source_->Count(2) < 2) ||
            (this->check_.PlayerId() != 0 && this->source_->Members()[0].id != this->check_.PlayerId()))
        { this->Stop(); return; }
        this->check_.Tick(now);
        for (const auto& line : this->check_.TakeSummary())
            if (this->settings_.announce) this->Send(line);
    }

    auto ready_check_module::HandlePacket(IAshitaCore* core, uint16_t id, std::span<const uint8_t> packet, uint64_t now) -> void
    {
        this->core_ = core;
        if (id == 0xA || id == 0xB) { this->Stop(); return; }
        if (id != 0x17 || !this->state_.enabled || this->source_ == nullptr) return;
        const auto message = DecodeReadyCheckChat(packet);
        if (!message) return;
        this->source_->Update(core, now);
        this->Update(core, now);
        if (this->check_.Receive(*message, this->Roster(), this->source_->Members()[0].id, now) && this->settings_.sound_prompt)
            this->sound_.Play(this->settings_.volume);
    }

    auto ready_check_module::HandleCommand(IAshitaCore* core, std::string_view command) -> void
    {
        this->core_ = core;
        const auto now = ::GetTickCount64();
        if (this->source_ != nullptr) this->source_->Update(core, now);
        this->Update(core, now);
        if (command == "start") this->Start(now);
        else if (command == "yes" || command == "no") this->Answer(command == "yes", now);
        else if (command == "close" || command == "cancel") this->Stop();
        else if (command == "test" || command == "sound") this->sound_.Play(this->settings_.volume);
        else if (command.starts_with("volume "))
        {
            int value{};
            const auto number = command.substr(7);
            const auto parsed = std::from_chars(number.data(), number.data() + number.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || value < 0 || value > 150)
                this->Report("Usage: /readycheck volume <0-150>");
            else this->settings_.volume = static_cast<float>(value);
        }
        else if (command == "status")
        {
            const auto text = "Ready Check: " + std::string{!this->state_.enabled ? "disabled" : this->check_.Mode() == ready_check_mode::idle ? "idle" :
                this->check_.Mode() == ready_check_mode::prompt ? "awaiting your answer" : this->check_.Completed() ? "complete" : "tracking responses"} +
                "; ready " + std::to_string(this->check_.Count(ready_response::ready)) + "/" + std::to_string(this->check_.Members().size()) +
                "; sound " + this->sound_.Status() + ".";
            this->Report(text.c_str());
        }
        else this->Report("/readycheck [start|yes|no|close|status|test|volume <0-150>]; /xiuiplus readycheck opens settings.");
    }

    auto ready_check_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->state_.enabled && this->check_.Mode() != ready_check_mode::idle;
    }

    auto ready_check_module::LayoutContains(float x, float y) const -> bool
    {
        return this->ShouldRender({}) && Inside(this->state_.bounds, x, y);
    }

    auto ready_check_module::LayoutHandleMouse(uint32_t message, float x, float y, uint64_t now) -> bool
    {
        this->mouse_x_ = x; this->mouse_y_ = y;
        if (message == WM_LBUTTONUP && this->pointer_down_)
        {
            const auto pressed = this->pressed_;
            this->CancelPointer();
            this->check_.Tick(now);
            if (pressed >= 0 && this->check_.Mode() != ready_check_mode::idle && this->pressed_deadline_ == this->check_.Deadline() &&
                Inside(this->buttons_[pressed], x, y))
            {
                if (this->check_.Mode() == ready_check_mode::prompt) this->Answer(pressed == 0, now);
                else this->Stop();
            }
            return true;
        }
        if (!this->LayoutContains(x, y)) return this->pointer_down_;
        if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && !this->pointer_down_)
        {
            if (this->ui_sounds_ != nullptr && this->LayoutButtonAt(x, y) != 0) this->ui_sounds_->Select(now);
            this->pointer_down_ = true;
            this->pressed_deadline_ = this->check_.Deadline();
            this->pressed_ = Inside(this->buttons_[0], x, y) ? 0 : Inside(this->buttons_[1], x, y) ? 1 : -1;
        }
        return true;
    }

    auto ready_check_module::ControllerButtonCount(void) const -> size_t
    {
        if (!this->state_.enabled || this->check_.Mode() == ready_check_mode::idle) return 0;
        return this->check_.Mode() == ready_check_mode::prompt ? 2 : 1;
    }

    auto ready_check_module::ActivateControllerButton(const size_t index, const uint64_t now) -> bool
    {
        this->check_.Tick(now);
        if (index >= this->ControllerButtonCount()) return false;
        if (this->ui_sounds_ != nullptr) this->ui_sounds_->Select(now);
        if (this->check_.Mode() == ready_check_mode::prompt) this->Answer(index == 0, now);
        else this->Stop();
        return true;
    }

    auto ready_check_module::LayoutButtonAt(const float x, const float y) const -> uint32_t
    {
        if (!this->LayoutContains(x, y)) return 0;
        for (size_t index = 0; index < this->buttons_.size(); ++index)
            if (Inside(this->buttons_[index], x, y)) return static_cast<uint32_t>(index + 1);
        return 0;
    }

    auto ready_check_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        static const std::vector<ready_check_member> samples{
            {1, 0, 1, "Warrior", ready_response::ready}, {2, 1, 3, "WhiteMage", ready_response::ready},
            {3, 2, 5, "RedMage", ready_response::not_ready}, {4, 3, 10, "Bard", ready_response::pending},
            {5, 4, 12, "Samurai", ready_response::pending}, {6, 5, 17, "Corsair", ready_response::pending}};
        const bool preview = context.IsEditing() && this->check_.Mode() == ready_check_mode::idle;
        const bool prompt = this->check_.Mode() == ready_check_mode::prompt;
        const auto& members = preview ? samples : this->check_.Members();
        const auto& settings = this->settings_;
        const auto lineHeight = [&](size_t profile) { return std::max(18.0f, context.Design().TextHeight(settings.text[profile].style) + 4); };
        const float headingHeight = lineHeight(0), detailsHeight = lineHeight(3);
        const float rowHeight = std::max({lineHeight(1), lineHeight(2), settings.job_icons ? settings.icon_size : 0.0f}) + settings.row_spacing;
        const float buttonHeight = std::max(30.0f, lineHeight(4) + 12);
        std::array<std::array<const ready_check_member*, 6>, 3> parties{};
        std::array<size_t, 3> counts{};
        for (const auto& member : members)
            if (member.slot < 18 && counts[member.slot / 6] < 6) parties[member.slot / 6][counts[member.slot / 6]++] = &member;
        size_t columns{}, rows{};
        for (const auto count : counts) if (count != 0) { ++columns; rows = std::max(rows, count); }
        columns = prompt ? 1 : std::max(size_t{1}, columns);
        const float inset = settings.container.edge_size + settings.container.padding, gap = 12;
        const auto viewport = context.Viewport();
        const float available = viewport.width > 0 ? viewport.width : 4096;
        const float width = std::min(available, inset * 2 + settings.column_width * columns + gap * (columns - 1));
        this->state_.bounds.width = width;
        this->state_.bounds.height = inset * 2 + headingHeight + detailsHeight + 18 + buttonHeight +
            (prompt ? detailsHeight + 8 : rows * rowHeight + (columns > 1 ? detailsHeight + 6 : 0));
        auto& bounds = this->state_.bounds;
        if (bounds.x < 0) bounds.x = viewport.x + (available - bounds.width) * 0.5f;
        if (bounds.y < 0) bounds.y = viewport.y + std::max(0.0f, (viewport.height - bounds.height) * 0.4f);
        if (viewport.width > 0) bounds.x = std::clamp(bounds.x, viewport.x, viewport.x + std::max(0.0f, viewport.width - bounds.width));
        if (viewport.height > 0) bounds.y = std::clamp(bounds.y, viewport.y, viewport.y + std::max(0.0f, viewport.height - bounds.height));
        const auto content = context.DrawContainer(bounds, settings.container);
        const auto draw = [&](const std::string& text, const ui::rect& at, size_t profile) { context.DrawText(text.c_str(), at, at, settings.text[profile]); };
        const auto remaining = preview ? uint64_t{30} : this->check_.Remaining(::GetTickCount64());
        const auto timer = this->check_.Completed() ? std::string{i18n::Text("Complete")} : std::to_string(remaining) + "s";
        const float timerWidth = context.Design().MeasureText(timer.c_str(), settings.text[3].style) + 8;
        draw(i18n::Text("READY CHECK"), {content.x, content.y, std::max(0.0f, content.width - timerWidth), headingHeight}, 0);
        auto timerStyle = settings.text[3]; timerStyle.style.alignment = ui::text_alignment::right;
        const ui::rect timerBounds{content.x + content.width - timerWidth, content.y, timerWidth, headingHeight};
        context.DrawText(timer.c_str(), timerBounds, timerBounds, timerStyle);
        float y = content.y + headingHeight + 6;
        if (prompt)
        {
            draw(i18n::Format("%s asks: are you ready?", this->check_.Sender().c_str()), {content.x, y, content.width, detailsHeight}, 1);
            draw(i18n::Text("Choose Yes or No, or reply in party chat."), {content.x, y + detailsHeight + 4, content.width, detailsHeight}, 3);
        }
        else
        {
            const auto ready = preview ? size_t{2} : this->check_.Count(ready_response::ready);
            const auto no = preview ? size_t{1} : this->check_.Count(ready_response::not_ready) + this->check_.Count(ready_response::no_response);
            draw(i18n::Format("Ready %zu/%zu   Not ready %zu   Waiting %zu", ready, members.size(), no, members.size() - ready - no), {content.x, y, content.width, detailsHeight}, 3);
            y += detailsHeight + 6;
            const float columnWidth = (content.width - gap * (columns - 1)) / columns;
            size_t column{};
            for (size_t group = 0; group < parties.size(); ++group)
            {
                if (counts[group] == 0) continue;
                const float x = content.x + column++ * (columnWidth + gap);
                float rowY = y;
                if (columns > 1) { draw(i18n::Format("Party %zu", group + 1), {x, rowY, columnWidth, detailsHeight}, 3); rowY += detailsHeight + 6; }
                for (size_t row = 0; row < counts[group]; ++row)
                {
                    const auto* member = parties[group][row];
                    float nameX = x;
                    if (settings.job_icons)
                    {
                        auto* icons = context.Icons();
                        if (member->job > 0 && member->job <= 24 && icons != nullptr && icons->JobAtlas() != nullptr)
                        {
                            const auto icon = member->job - 1;
                            context.Design().DrawTexture(icons->JobAtlas(), {x, rowY, settings.icon_size, settings.icon_size}, 0xFFFFFFFF,
                                {(static_cast<float>(icon % 8) * 64 + 0.5f) / 512, (static_cast<float>(icon / 8) * 64 + 0.5f) / 256, 63.0f / 512, 63.0f / 256});
                        }
                        nameX += settings.icon_size + 6;
                    }
                    const auto response = member->response;
                    const char* label = response == ready_response::ready ? "Ready" : response == ready_response::not_ready ? "Not ready" :
                        response == ready_response::no_response ? "No response" : "Waiting";
                    auto responseStyle = TintTextProfile(settings.text[2], response == ready_response::ready ? settings.ready :
                        response == ready_response::pending ? settings.pending : settings.no);
                    responseStyle.style.alignment = ui::text_alignment::right;
                    const float responseWidth = context.Design().MeasureText(i18n::Text("No response"), responseStyle.style) + 8;
                    draw(member->name, {nameX, rowY, std::max(0.0f, x + columnWidth - responseWidth - nameX), rowHeight - settings.row_spacing}, 1);
                    const ui::rect at{x + columnWidth - responseWidth, rowY, responseWidth, rowHeight - settings.row_spacing};
                    context.DrawText(i18n::Text(label), at, at, responseStyle);
                    rowY += rowHeight;
                }
            }
        }
        this->buttons_ = {};
        const float buttonY = bounds.y + bounds.height - inset - buttonHeight;
        const float buttonWidth = prompt ? (content.width - 12) * 0.5f : std::min(120.0f, content.width);
        this->buttons_[0] = {prompt ? content.x : content.x + (content.width - buttonWidth) * 0.5f, buttonY, buttonWidth, buttonHeight};
        if (prompt) this->buttons_[1] = {content.x + buttonWidth + 12, buttonY, buttonWidth, buttonHeight};
        for (size_t i = 0; i < (prompt ? size_t{2} : size_t{1}); ++i)
            context.DrawButton(this->buttons_[i], i18n::Text(prompt ? i == 0 ? "Yes" : "No" : "Close"),
                !context.IsEditing() && Inside(this->buttons_[i], this->mouse_x_, this->mouse_y_), settings.text[4]);
    }
}
