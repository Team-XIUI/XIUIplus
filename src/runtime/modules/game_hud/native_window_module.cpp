#include "native_window_module.hpp"
#include "ui/text_fit.hpp"

namespace xiui
{
    auto native_window_module::Load(const settings_store& store) -> void
    {
        this->split_pending_ = false;
        this->split_shared_ = false;
        this->preferences_known_ = !store.ReadString(this->Id(), "custom_position").empty();
        this->custom_ = store.ReadBool(this->Id(), "custom_position", false);
        this->hidden_ = store.ReadBool(this->Id(), "hidden", false);
        this->state_.bounds.x = store.ReadFloat(this->Id(), "x", 0);
        this->state_.bounds.y = store.ReadFloat(this->Id(), "y", 0);
        this->native_bounds_ = store.ReadRect((this->id_ + ".native").c_str(), {});
        this->observed_ = store.ReadBool(this->Id(), "geometry_known", false) && this->native_bounds_.width > 0 && this->native_bounds_.height > 0;
        if (this->index_ >= native_hud_window_count && store.ReadInt(this->Id(), "per_window_version", 0) < 1)
        {
            this->observed_ = false;
            this->split_pending_ = store.ReadBool(this->Id(), "split_pending", false);
            if (this->split_pending_)
            {
                this->split_offset_ = {store.ReadFloat(this->Id(), "split_x", 0), store.ReadFloat(this->Id(), "split_y", 0), 0, 0};
                this->split_absolute_ = store.ReadBool(this->Id(), "split_absolute", false);
                this->split_shared_ = store.ReadBool(this->Id(), "split_shared", false);
                this->split_top_ = store.ReadBool(this->Id(), "split_top", false);
                this->split_origin_x_ = store.ReadFloat(this->Id(), "split_origin_x", 0);
                this->split_offset_.width = store.ReadFloat(this->Id(), "split_width", 0);
                this->split_offset_.height = store.ReadFloat(this->Id(), "split_height", 0);
            }
            else
            {
                const auto category = native_menu_windows[this->index_ - native_hud_window_count].legacy_category;
                const auto legacy = std::string{"native."} + native_dialog_keys[static_cast<size_t>(category)];
                if (store.ReadInt(legacy.c_str(), "per_window_version", 0) < 1)
                {
                    this->hidden_ = store.ReadBool(legacy.c_str(), "hidden", this->hidden_);
                    this->split_pending_ = store.ReadBool(legacy.c_str(), "custom_position", false);
                    const auto original = store.ReadRect((legacy + ".native").c_str(), {});
                    this->split_absolute_ = original.width <= 0 || original.height <= 0;
                    this->split_offset_ = {store.ReadFloat(legacy.c_str(), "x", 0) - (this->split_absolute_ ? 0 : original.x),
                        store.ReadFloat(legacy.c_str(), "y", 0) - (this->split_absolute_ ? 0 : original.y), 0, 0};
                }
            }
            if (!this->split_pending_)
            {
                const std::string_view resource{native_menu_windows[this->index_ - native_hud_window_count].resource};
                const bool shared = resource == "playermo" || resource == "magic   " || resource == "ability " || resource == "magselec" || resource == "abiselec" || resource == "mount   ";
                const auto placement = store.ReadRect("layout.commands", {});
                if (shared && store.ReadBool("commands", "enabled", true) && store.ReadBool("commands", "position_initialized", false) &&
                    placement.x >= 0 && placement.y >= 0 && placement.width > 0 && placement.height > 0)
                {
                    this->split_pending_ = this->split_shared_ = true; this->split_offset_ = placement;
                    this->split_top_ = store.ReadInt("commands", "window_anchor", 1) == 0;
                    const auto command = store.ReadRect("native.commands.native", {}), actions = store.ReadRect("native.actions.native", {});
                    this->split_origin_x_ = command.width > 0 ? command.x : actions.width > 0 ? actions.x : 0;
                }
            }
            this->custom_ = false;
        }
        if (!this->observed_ && !this->custom_) this->state_.bounds = {};
    }

    auto native_window_module::Save(const settings_store& store) const -> void
    {
        if (!this->observed_ && !this->custom_ && !this->hidden_ && !this->split_pending_ && !this->preferences_known_) return;
        if (this->index_ >= native_hud_window_count)
        {
            store.WriteInt(this->Id(), "per_window_version", this->split_pending_ ? 0 : 1);
            store.WriteBool(this->Id(), "split_pending", this->split_pending_);
            if (this->split_pending_)
            {
                store.WriteFloat(this->Id(), "split_x", this->split_offset_.x); store.WriteFloat(this->Id(), "split_y", this->split_offset_.y);
                store.WriteBool(this->Id(), "split_absolute", this->split_absolute_);
                store.WriteBool(this->Id(), "split_shared", this->split_shared_); store.WriteBool(this->Id(), "split_top", this->split_top_);
                store.WriteFloat(this->Id(), "split_origin_x", this->split_origin_x_);
                store.WriteFloat(this->Id(), "split_width", this->split_offset_.width); store.WriteFloat(this->Id(), "split_height", this->split_offset_.height);
            }
        }
        store.WriteBool(this->Id(), "custom_position", this->custom_);
        store.WriteBool(this->Id(), "hidden", this->hidden_);
        store.WriteFloat(this->Id(), "x", this->state_.bounds.x);
        store.WriteFloat(this->Id(), "y", this->state_.bounds.y);
        store.WriteBool(this->Id(), "geometry_known", this->observed_);
        if (this->observed_) store.WriteRect((this->id_ + ".native").c_str(), this->native_bounds_);
    }

    auto native_window_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        if (!this->OnScreen()) return;
        auto text = module_text_profile{};
        text.style = context.Design().GetTheme().caption_text;
        const auto& bounds = this->state_.bounds;
        const ui::rect label{bounds.x, bounds.y >= 20 ? bounds.y - 20 : bounds.y + bounds.height, this->HeaderWidth() - 50, 20};
        const float measured = context.Design().MeasureText(i18n::Text(this->DisplayName()), text.style);
        if (measured > label.width && measured > 0) text.style.scale *= std::max(.7f, label.width / measured);
        const auto name = ui::FitText(context.Design(), i18n::Text(this->DisplayName()), label.width, text.style);
        context.DrawText(name.c_str(), label, label, text);
    }
}
