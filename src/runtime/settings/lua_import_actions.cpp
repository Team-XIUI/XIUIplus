#include "lua_import_context.hpp"
#include "modules/actionbars/action_import.hpp"

#include <algorithm>
#include <charconv>
#include <functional>
#include <tuple>

namespace xiui::lua_import
{
    namespace
    {
        auto ClassifyActions(context& data, const lua_data::value& item, const std::string& path) -> void
        {
            static const std::set<std::string> supported{"id", "actionType", "action", "name", "target", "displayName", "macroText", "macroRef", "macroPaletteKey", "cleared", "icon", "itemId", "resourceId", "equipSlot", "equipment_slot", "book", "set", "slot", "alt", "recastSourceType", "recastSourceAction", "recastSourceItemId"};
            if (item.fields.empty()) { data.Mark(path, "action data"); return; }
            for (const auto& [key, child] : item.fields)
            {
                const auto nested = path + "." + key;
                if (child.type == lua_data::value::kind::table) ClassifyActions(data, child, nested);
                else if (supported.contains(key) || key == "alwaysAvailable" || key == "inheritAppearance") data.Mark(nested, "actionbars.data");
            }
        }
        auto KeyContext(const std::string& key) -> action_context
        {
            action_context result{}; const auto colon = key.find(':'); unsigned job{}, subjob{};
            if (colon == key.npos) return result;
            const auto a = std::from_chars(key.data(), key.data() + colon, job);
            const auto b = std::from_chars(key.data() + colon + 1, key.data() + key.size(), subjob);
            if (a.ec == std::errc{} && b.ec == std::errc{} && job <= 22 && subjob <= 22) { result.job = static_cast<uint8_t>(job); result.subjob = static_cast<uint8_t>(subjob); }
            return result;
        }
        auto Appearance(context& data, const std::string& source, const std::string& section, const size_t bar) -> void
        {
            auto& out = data.result.preferences;
            out.WriteBool(section.c_str(), "custom_text", true);
            data.Map(source + "slotYPadding", section, "vertical_spacing", data.scale);
            data.Map(source + "actionLabelWordWrap", section, "label_wrap");
            for (const auto& [name, role] : {std::pair{"label", 1}, {"keybind", 2}, {"recastTimer", 3}, {"mpCost", 6}})
            {
                const auto text = "text.actionbars.bar" + std::to_string(bar) + "." + std::to_string(role);
                data.TextSize(source + name + "FontSize", text);
                data.TextColor(source + name + "FontColor", text);
            }
            const auto slots = section + ".slots";
            for (const auto& [name, key] : {std::pair{"keybind", "binding"}, {"quantity", "count"}, {"mpCost", "cost"}})
            {
                data.Map(source + name + "OffsetX", slots, (std::string{key} + "_x").c_str(), data.scale);
                data.Map(source + name + "OffsetY", slots, (std::string{key} + "_y").c_str(), data.scale);
                data.Choice(source + name + "Anchor", slots, (std::string{key} + "_anchor").c_str(),
                    {"topLeft", "top", "topRight", "left", "center", "right", "bottomLeft", "bottom", "bottomRight"});
            }
            data.Maps(source + "actionLabel", "text.actionbars.bar" + std::to_string(bar) + ".1", {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
            data.Color(source + "slotBackgroundColor", slots, "fill");
            data.Color(source + "slotBackgroundColor", slots, "empty_fill");
            if (data.Get(source + "showSlotFrame"))
            {
                out.WriteInt(slots.c_str(), "frame", data.Boolean(source + "showSlotFrame") ? 4 : 0);
                data.Mark(source + "showSlotFrame", slots + ".frame (Native Window Type)");
            }
        }
    }
    auto ConvertActions(context& data) -> void
    {
        auto& out = data.result.preferences;
        const auto imported = PreviewActionImport(lua_data::ToLiteral(data.source), data.result.name);
        action_store store;
        if (imported.unsupported != 0 || imported.conflicts != 0 || (!imported.error.empty() && imported.error != "No supported action bindings were found."))
            data.Issue(lua_import_severity::error, "hotbars", imported.error.empty() ? "Unsupported or conflicting actions require resolution before import." : imported.error);
        if (imported.actions != 0 || !imported.palettes.empty() || !imported.macros.empty())
        {
            if (!imported.Valid() || imported.unsupported != 0 || CommitActionImport(store, imported) != imported.palettes.size())
                data.Issue(lua_import_severity::error, "hotbars", imported.error.empty() ? "Some source actions or library records could not be imported." : imported.error);
        }
        else
            for (auto record : imported.macros) if (store.ImportMacro(std::move(record)).empty()) data.Issue(lua_import_severity::error, "macroDB", "Library record could not be imported.");
        for (const auto& warning : imported.warnings) data.Issue(lua_import_severity::warning, "hotbars", warning);
        if (const auto* library = data.Get("macroDB")) ClassifyActions(data, *library, "macroDB");
        const auto mode = data.String("hotbarCrossbar.mode", "hotbar");
        for (const auto& [source, target, multiplier] : {std::tuple{"enableExpandedCrossbar", "expanded", 1.0}, {"enableDoubleTap", "double_tap", 1.0},
            {"doubleTapWindow", "double_tap_ms", 1000.0}, {"minTriggerHold", "min_hold_ms", 1000.0}, {"triggerPressThreshold", "trigger_press", 1.0}, {"triggerReleaseThreshold", "trigger_release", 1.0}})
        {
            const auto path = std::string{"hotbarCrossbar."} + source;
            if (const auto* value = data.Get(path))
            {
                if (value->scalar_type == lua_data::scalar_kind::boolean) data.result.local_input.WriteBool("controller.crossbar.device", target, value->Boolean());
                else data.result.local_input.WriteFloat("controller.crossbar.device", target, static_cast<float>(value->Number() * multiplier));
                data.Mark(path, "optional local controller behavior: " + std::string{target});
            }
        }
        data.Issue(lua_import_severity::warning, "controller", "Device identities, backend mappings and physical button IDs are not copied. Optional timing/expanded-bank behavior can be applied when activating a single imported profile.");
        data.Mark("hotbarCrossbar.mode", "hotbar/crossbar visibility");
        for (size_t bar = 1; bar <= 10; ++bar)
        {
            const auto source = "hotbarBar" + std::to_string(bar) + ".";
            const auto id = bar == 1 ? std::string{"hotbar"} : "hotbar" + std::to_string(bar);
            const auto section = "actionbars." + id;
            const auto layout = "layout." + id;
            if (const auto* slots = data.Get(source + "slotActions")) ClassifyActions(data, *slots, source + "slotActions");
            const auto shared = data.Boolean(source + "useGlobalSettings", true) ? "hotbarGlobal." : source;
            data.Mark(source + "useGlobalSettings", section + " effective visual settings");
            data.Map(source + "enabled", section, "enabled");
            if (mode == "crossbar" || !data.Boolean("hotbarEnabled", true)) out.WriteBool(section.c_str(), "enabled", false);
            data.Map(source + "columns", section, "columns"); data.Map(source + "slots", section, "slots");
            data.Mark(source + "rows", section + ".columns/slots");
            data.Window(shared, section);
            data.Map(shared + "slotSize", section, "icon_size", data.scale);
            data.Map(shared + "slotXPadding", section, "spacing", data.scale);
            Appearance(data, shared, section, bar - 1);
            data.Map(shared + "showHotbarNumber", section, "heading"); data.Map(shared + "showActionLabels", section, "labels");
            data.Map(shared + "showKeybinds", section, "bindings"); data.Map(shared + "showMpCost", section, "resources");
            data.Bool(shared + "hideEmptySlots", section, "empty", true);
            data.Map("hotbarLockMovement", section, "locked");
            store.SetScope(action_surface::hotbar, bar - 1, data.Boolean(source + "jobSpecific", true) ? action_scope::job_subjob : action_scope::global);
            store.SetPetAware(action_surface::hotbar, bar - 1, data.Boolean(source + "petAware"));
            data.Mark(source + "jobSpecific", "actionbars.data.scope"); data.Mark(source + "petAware", "actionbars.data.pet");
            const auto position = "windowPositions.Hotbar" + std::to_string(bar) + ".";
            data.Maps(position, layout, {{"x", "x"}, {"y", "y"}});
            if (const auto* keys = data.Get(source + "keyBindings"))
                for (const auto& [key, binding] : keys->fields)
                {
                    const auto slot = std::strtoul(key.c_str(), nullptr, 10); if (slot < 1 || slot > 12) continue;
                    const auto field = "slot" + std::to_string(slot), prefix = source + "keyBindings." + key + ".";
                    out.WriteInt((section + ".keys").c_str(), field.c_str(), static_cast<int>(data.Number(prefix + "key")));
                    out.WriteInt((section + ".modifiers").c_str(), field.c_str(), (data.Boolean(prefix + "ctrl") ? 1 : 0) | (data.Boolean(prefix + "alt") ? 2 : 0) | (data.Boolean(prefix + "shift") ? 4 : 0));
                    for (const auto part : {"key", "ctrl", "alt", "shift"}) data.Mark(prefix + part, section + ".keys/modifiers");
                }
            if (data.Get(source + "keyBindings") && data.Get(source + "keyBindings")->fields.empty()) data.Mark(source + "keyBindings", "unbound keyboard slots");
            if (data.Get(source + "paletteOrder") && data.Get(source + "paletteOrder")->fields.empty()) data.Mark(source + "paletteOrder", "empty legacy palette order");
            out.WriteInt((section + ".slots").c_str(), "frame_version", 1);
        }
        data.Mark("hotbarEnabled", "all action bars enabled state");
        data.Window("hotbarCrossbar.", "actionbars.crossbar");
        data.Map("hotbarCrossbar.slotSize", "actionbars.crossbar", "icon_size", data.scale);
        data.Map("hotbarCrossbar.slotGapH", "actionbars.crossbar", "spacing", data.scale);
        Appearance(data, "hotbarCrossbar.", "actionbars.crossbar", 10);
        data.Maps("hotbarCrossbar.", "actionbars.crossbar", {{"showButtonIcons", "button_icons"}, {"buttonIconSize", "prompt_size"},
            {"buttonIconGapH", "prompt_gap_x"}, {"buttonIconGapV", "prompt_gap_y"}, {"diamondSpacing", "diamond_spacing"}, {"groupSpacing", "group_spacing"}});
        out.WriteBool("actionbars.crossbar", "enabled", data.Boolean("hotbarEnabled", true) && mode != "hotbar");
        for (size_t bank = 0; bank < 6; ++bank)
        {
            constexpr const char* names[]{"L2", "R2", "L2R2", "R2L2", "L2x2", "R2x2"};
            const auto path = std::string{"hotbarCrossbar.comboModeSettings."} + names[bank] + ".petAware";
            store.SetPetAware(action_surface::crossbar, bank, data.Boolean(path)); data.Mark(path, "crossbar bank pet awareness");
            store.SetScope(action_surface::crossbar, bank, data.Boolean("hotbarCrossbar.jobSpecific", true) ? action_scope::job_subjob : action_scope::global);
        }
        data.Mark("hotbarCrossbar.jobSpecific", "crossbar action scope");
        if (const auto* slots = data.Get("hotbarCrossbar.slotActions")) ClassifyActions(data, *slots, "hotbarCrossbar.slotActions");
        for (const auto& [path, surface] : {std::pair{"hotbar.paletteOrder", action_surface::hotbar}, {"hotbarCrossbar.crossbarPaletteOrder", action_surface::crossbar}})
        {
            std::vector<std::string> order;
            if (const auto* contexts = data.Get(path))
            {
                for (const auto& [key, names] : contexts->fields)
                {
                    const auto context = KeyContext(key);
                    for (const auto& [index, name] : names.fields)
                        for (const auto& palette : store.AllPalettes())
                            if (palette.job == context.job && palette.subjob == context.subjob && palette.name == name.text && (palette.surfaces & (1 << static_cast<int>(surface)))) order.push_back(palette.id);
                }
                store.SetPaletteOrder(surface, std::move(order)); data.Tree(path, "surface-specific palette order");
            }
        }
        if (const auto* selections = data.Get("hotbar.activePalettePerJob"))
        {
            for (const auto& [key, name] : selections->fields)
            {
                const auto context = KeyContext(key); store.SetContext(context);
                for (const auto& palette : store.AllPalettes()) if (palette.job == context.job && palette.subjob == context.subjob && palette.name == name.text) { store.Select(action_surface::hotbar, palette.id); break; }
            }
            data.Tree("hotbar.activePalettePerJob", "hotbar palette selections");
        }
        store.Save(out);
        out.WriteInt("actionbars", "presentation_version", 1); out.WriteInt("actionbars", "slot_text_version", 2); out.WriteInt("actionbars", "slot_frame_version", 1);
    }
}
