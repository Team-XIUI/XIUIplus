#include "chat_module.hpp"
#include "chat_theme.hpp"
#include "chat/chat_token.hpp"
#include <algorithm>

namespace
{
    constexpr const char* channel_names[]{"Say", "Party", "Linkshell 1", "Linkshell 2", "Shout", "Yell", "Unity", "Tell", "Reply", "Game default"};
    constexpr const char* channel_commands[]{"/say ", "/party ", "/linkshell ", "/linkshell2 ", "/shout ", "/yell ", "/unity ", "/tell ", "/reply ", ""};

    auto Contains(const xiui::ui::rect& bounds, const float x, const float y) -> bool
    { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }
}

namespace xiui
{
    auto chat_module::ChannelPickerLayout(const ui::design_system& design) const -> channel_picker_layout
    {
        channel_picker_layout result{};
        const auto anchor = this->Layout(design).destination;
        const auto viewport = this->channel_picker_viewport_;
        const auto style = ChatThemeStyle(design, this->chrome_.style);
        const float rowHeight = std::max(26.0f, design.TextHeight(style) + 8);
        const auto tooltip = ui::container_renderer::TooltipStyle();
        const float inset = tooltip.edge_size + tooltip.padding;
        const float width = std::min(std::max(1.0f, viewport.width - 16),
            std::max(240.0f, design.MeasureText("Linkshell 2", style) * 2 + inset * 2 + 24));
        const float height = rowHeight * 5 + inset * 2 + 16;
        const float x = std::clamp(anchor.x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8));
        const float y = std::clamp(anchor.y - height - 6, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8));
        result.bounds = {x, y, width, height};
        const auto content = ui::container_renderer::ContentBounds(result.bounds, tooltip);
        const float columnWidth = std::max(1.0f, (content.width - 4) * 0.5f);
        for (size_t index = 0; index < result.rows.size(); ++index)
            result.rows[index] = {content.x + (index % 2) * (columnWidth + 4), content.y + (index / 2) * (rowHeight + 4), columnWidth, rowHeight};
        return result;
    }

    auto chat_module::ChooseSendChannel(const int choice, bool& save) -> bool
    {
        if (choice < 0 || choice >= static_cast<int>(std::size(channel_names))) return false;
        const auto text = this->editor_.Text();
        const auto prepared = this->InputCommand(text);
        const bool explicitCommand = text.starts_with('/') || prepared != std::string{channel_commands[this->channel_]} + text;
        std::string body = text, recipient;
        size_t caret = this->editor_.Caret();
        if (explicitCommand)
        {
            const auto destination = DescribeChatDestination(prepared, this->native_reply_recipient_);
            if (destination.mode == 0)
            { this->error_ = "Edit this /command to change its destination."; return false; }
            const auto end = prepared.find_first_of(" \t");
            auto start = end == std::string::npos ? end : prepared.find_first_not_of(" \t", end);
            if (destination.mode == 4 && destination.label.starts_with("Tell: ")) recipient = destination.label.substr(6);
            auto name = prepared.substr(0, end);
            std::transform(name.begin(), name.end(), name.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if ((name == "/t" || name == "/tell") && start != std::string::npos)
            {
                const auto nameEnd = prepared.find_first_of(" \t", start);
                start = nameEnd == std::string::npos ? nameEnd : prepared.find_first_not_of(" \t", nameEnd);
            }
            body = start == std::string::npos ? std::string{} : prepared.substr(start);
            const auto preparedCaret = static_cast<int64_t>(caret) + static_cast<int64_t>(prepared.size()) - static_cast<int64_t>(text.size());
            caret = start == std::string::npos || preparedCaret < static_cast<int64_t>(start) ? 0 :
                std::min(body.size(), static_cast<size_t>(preparedCaret - static_cast<int64_t>(start)));
        }
        if (choice == 7 || choice == 8)
        {
            if (choice == 8) recipient = this->native_reply_recipient_;
            if (recipient.empty() && choice == 7)
            {
                const auto native = DescribeChatDestination(this->native_send_prefix_.value_or(""));
                if (native.label.starts_with("Tell: ")) recipient = native.label.substr(6);
            }
            if (recipient.empty()) recipient = this->native_reply_recipient_;
            if (!IsTellRecipient(recipient))
            { this->error_ = "Enter /tell Name to choose a recipient first."; return false; }
            const auto prefix = "/t " + recipient + " ";
            this->editor_.Set(prefix + body, prefix.size() + caret);
        }
        else
        {
            if (body.starts_with('/'))
            {
                const auto prefix = choice == game_channel ? this->native_send_prefix_.value_or("") : std::string{channel_commands[choice]};
                if (prefix.empty()) { this->error_ = "Keep a chat command before this message."; return false; }
                this->editor_.Set(prefix + body, prefix.size() + caret);
            }
            else this->editor_.Set(body, caret);
            save = save || this->channel_ != choice;
            this->channel_ = choice;
        }
        this->editor_.ResetCompletion();
        this->reply_destination_ = choice == 8 ? recipient : std::string{};
        this->reply_recipient_.clear(); this->error_.clear(); this->input_start_ = 0;
        if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
        return true;
    }

    auto chat_module::HandleChannelMouse(const uint32_t message, const float x, const float y,
        const ui::design_system& design, bool& save) -> bool
    {
        if (this->secondary_) return false;
        const auto anchor = this->Layout(design).destination;
        const ui::rect pill{anchor.x - 3, anchor.y + 1, anchor.width + 6, std::max(0.0f, anchor.height - 2)};
        const bool down = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK;
        if (message == WM_LBUTTONUP && this->channel_pointer_press_ != -2)
        {
            const auto pressed = this->channel_pointer_press_;
            this->channel_pointer_press_ = -2; this->pointer_down_ = false;
            if (pressed == -1 && Contains(pill, x, y))
            {
                this->Focus(true); this->channel_picker_open_ = true;
                this->editor_.ResetCompletion();
                if (this->sounds_ != nullptr) this->sounds_->MenuOpened(::GetTickCount64());
            }
            else if (pressed >= 0 && this->channel_picker_open_)
            {
                const auto picker = this->ChannelPickerLayout(design);
                if (Contains(picker.rows[pressed], x, y)) this->ChooseSendChannel(pressed, save);
                this->channel_picker_open_ = false;
            }
            return true;
        }
        if (this->channel_picker_open_)
        {
            if (down)
            {
                this->pointer_down_ = true; this->channel_pointer_press_ = -3;
                const auto picker = this->ChannelPickerLayout(design);
                for (size_t index = 0; index < picker.rows.size(); ++index)
                    if (Contains(picker.rows[index], x, y)) this->channel_pointer_press_ = static_cast<int>(index);
                if (this->channel_pointer_press_ == -3) this->channel_picker_open_ = false;
            }
            return true;
        }
        if (anchor.height <= 0 || !Contains(pill, x, y)) return false;
        this->destination_hovered_ = true;
        if (down)
        {
            this->pointer_down_ = true; this->channel_pointer_press_ = -1;
            if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
        }
        return down || message == WM_MOUSEMOVE;
    }

    auto chat_module::RenderChannelPicker(const module_render_context& context) const -> void
    {
        if (!this->channel_picker_open_) return;
        const auto layout = this->ChannelPickerLayout(context.Design());
        context.DrawTooltip(layout.bounds);
        auto profile = ChatThemeProfile(context.Design(), this->chrome_); profile.style.alignment = ui::text_alignment::center;
        for (size_t index = 0; index < layout.rows.size(); ++index)
            context.DrawButton(layout.rows[index], channel_names[index], static_cast<int>(index) == this->channel_ ||
                Contains(layout.rows[index], this->hover_point_.x, this->hover_point_.y), profile);
    }
}
