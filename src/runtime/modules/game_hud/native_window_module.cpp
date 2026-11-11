#include "native_window_module.hpp"
#include "ui/text_fit.hpp"

namespace xiui
{
    auto native_window_module::Load(const settings_store& store) -> void
    {
        this->preferences_known_ = !store.ReadString(this->Id(), "custom_position").empty();
        this->custom_ = store.ReadBool(this->Id(), "custom_position", false);
        this->hidden_ = store.ReadBool(this->Id(), "hidden", false);
        this->state_.bounds.x = store.ReadFloat(this->Id(), "x", 0);
        this->state_.bounds.y = store.ReadFloat(this->Id(), "y", 0);
        this->native_bounds_ = store.ReadRect((this->id_ + ".native").c_str(), {});
        this->observed_ = store.ReadBool(this->Id(), "geometry_known", false) && this->native_bounds_.width > 0 && this->native_bounds_.height > 0;
        if (!this->observed_ && !this->custom_) this->state_.bounds = {};
    }

    auto native_window_module::Save(const settings_store& store) const -> void
    {
        if (!this->observed_ && !this->custom_ && !this->hidden_ && !this->preferences_known_) return;
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
