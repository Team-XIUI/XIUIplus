#include "hud_runtime.hpp"
#include "combat/server_rules.hpp"
#include "input/keyboard_policy.hpp"
#include "config/contributor_list.hpp"
#include "config/option_layout.hpp"
#include "config/config_window_resize.hpp"
#include "ui/sound_choices.hpp"
#include "settings/settings_store.hpp"
#include "Memory.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>
#include <sstream>
#include <stdexcept>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    auto LoadEmbeddedText(const int id) -> std::string
    {
        const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
        const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
        const auto size = resource != nullptr ? ::SizeofResource(module, resource) : 0;
        if (size == 0 || size > 1024 * 1024) return {};
        const auto loaded = ::LoadResource(module, resource);
        const auto* text = loaded != nullptr ? static_cast<const char*>(::LockResource(loaded)) : nullptr;
        return text != nullptr ? std::string{text, size} : std::string{};
    }

    auto SeedEmbeddedFile(const int id, const std::string& path) -> void
    {
        if (::GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;
        const auto text = LoadEmbeddedText(id);
        if (text.empty()) return;
        const HANDLE file = ::CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        DWORD written{};
        const bool saved = ::WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) != FALSE && written == text.size();
        ::CloseHandle(file);
        if (!saved) ::DeleteFileA(path.c_str());
    }

    class render_frame_guard
    {
    public:
        explicit render_frame_guard(xiui::ui::design_system& design) : design_(design) {}
        ~render_frame_guard(void) { this->design_.EndFrame(); }
        render_frame_guard(const render_frame_guard&) = delete;
        auto operator=(const render_frame_guard&) -> render_frame_guard& = delete;

    private:
        xiui::ui::design_system& design_;
    };

    auto signed_low_word(const LPARAM value) -> int
    {
        return static_cast<int>(static_cast<short>(LOWORD(value)));
    }

    auto signed_high_word(const LPARAM value) -> int
    {
        return static_cast<int>(static_cast<short>(HIWORD(value)));
    }
}

namespace xiui
{
    hud_runtime::hud_runtime(void) : active_module_(&this->general_)
    {
        this->config_controls_.SetTextSettings(this->general_.ConfigText());
        this->modules_.Add(this->player_);
        this->cast_bar_.SetSource(&this->player_cast_source_);
        this->cast_cost_.SetSource(&this->cast_cost_source_);
        this->modules_.Add(this->cast_bar_);
        this->modules_.Add(this->cast_cost_);
        this->recast_.SetSource(&this->recast_source_);
        this->modules_.Add(this->recast_);
        this->modules_.Add(this->target_);
        this->modules_.Add(this->target_cast_);
        this->modules_.Add(this->target_recipient_);
        this->modules_.Add(this->subtarget_);
        for (auto& group : this->mob_info_groups_) this->modules_.Add(group);
        this->modules_.Add(this->enemy_list_);
        this->pet_.SetSource(&this->pet_source_);
        this->pet_.SetRecasts(&this->recast_source_);
        this->pet_target_.SetOwner(&this->pet_);
        this->modules_.Add(this->pet_);
        this->modules_.Add(this->pet_target_);
        this->pet_config_members_ = {&this->pet_, &this->pet_, &this->pet_, &this->pet_, &this->pet_, &this->pet_target_};
        this->party_.SetSource(&this->party_source_);
        this->alliance_first_.SetSource(&this->party_source_);
        this->alliance_second_.SetSource(&this->party_source_);
        for (auto* party : {&this->party_, &this->alliance_first_, &this->alliance_second_})
            party->SetModuleEnabled(this->general_.PartyEnabledFlag());
        this->modules_.Add(this->party_);
        this->modules_.Add(this->alliance_first_);
        this->modules_.Add(this->alliance_second_);
        this->modules_.Add(this->experience_);
        for (size_t i = 0; i < this->inventory_.size(); ++i)
        {
            this->inventory_[i].SetSource(&this->inventory_source_);
            this->inventory_[i].SetModuleEnabled(this->general_.InventoryEnabledFlag());
            this->inventory_pages_[i] = &this->inventory_[i];
            this->inventory_config_members_[i] = &this->inventory_[i];
            this->modules_.Add(this->inventory_[i]);
        }
        this->equipment_.SetSource(&this->equipment_source_);
        this->modules_.Add(this->equipment_);
        this->modules_.Add(this->satchel_);
        this->actionbars_.Initialize(nullptr, &this->action_store_, &this->action_executor_);
        this->actionbars_.SetCombatSettings(&this->combat_settings_);
        this->modules_.Add(this->actionbars_);
        this->actionbar_members_[0] = &this->actionbars_;
        for (size_t index = 0; index < this->actionbar_children_.size(); ++index)
        {
            this->modules_.Add(this->actionbar_children_[index]);
            this->actionbar_members_[index + 1] = &this->actionbar_children_[index];
        }
        this->gil_.SetSource(&this->gil_source_);
        this->modules_.Add(this->gil_);
        this->inventory_config_members_[6] = &this->satchel_;
        this->inventory_config_members_[7] = &this->equipment_;
        this->inventory_config_members_[8] = &this->gil_;
        this->modules_.Add(this->notifications_);
        for (auto& group : this->notification_groups_) this->modules_.Add(group);
        this->blue_magic_.SetSource(&this->party_source_);
        this->phantom_roll_.SetSource(&this->party_source_);
        this->phantom_roll_.SetCombatSettings(&this->combat_settings_);
        this->modules_.Add(this->blue_magic_);
        this->modules_.Add(this->phantom_roll_);
        this->magic_burst_.SetSource(&this->skillchain_source_);
        this->modules_.Add(this->magic_burst_);
        this->treasure_.SetSource(&this->treasure_source_);
        this->treasure_.SetSounds(&this->ui_sounds_);
        this->modules_.Add(this->treasure_);
        this->modules_.Add(this->general_);
        this->modules_.Add(this->game_hud_);
        this->chat_.AttachSecondWindow(this->chat_second_);
        this->chat_.SetSounds(&this->ui_sounds_);
        this->chat_second_.SetSounds(&this->ui_sounds_);
        this->chat_.SetNativeFilters(this->chat_filters_);
        this->chat_second_.SetNativeFilters(this->chat_filters_);
        this->modules_.Add(this->chat_);
        this->modules_.Add(this->chat_second_);
        this->modules_.Add(this->compass_);
        this->modules_.Add(this->minimap_);
        this->modules_.Add(this->world_info_);
        for (auto& part : this->world_parts_) this->modules_.Add(part);
        this->modules_.Add(this->commands_);
        this->extras_pages_ = {&this->compass_, &this->world_info_, &this->commands_, &this->game_hud_, &this->blue_magic_, &this->phantom_roll_, &this->ready_check_, &this->magic_burst_, &this->treasure_};
        this->ready_check_.SetSource(&this->party_source_);
        this->ready_check_.SetSounds(&this->ui_sounds_);
        this->modules_.Add(this->ready_check_);
        for (auto* effects : {&this->buffs_, &this->debuffs_})
        {
            effects->SetSource(&this->party_source_);
            this->modules_.Add(*effects);
        }
        this->debuffs_.SetBuffsModule(&this->buffs_);
        this->general_.SetInventoryModule(this->inventory_[0]);
        this->general_.SetPartyModule(this->party_);
        this->party_.SetPresetAlliances(this->alliance_first_, this->alliance_second_);
        std::vector<hud_module*> configurable;
        for (const auto& page : this->ConfigPages())
            if (page.primary != &this->general_)
                for (size_t index = 0; index < (page.primary == &this->party_ ? 1 : page.SectionCount()); ++index)
                    if (auto* module = page.SectionModule(index); std::find(configurable.begin(), configurable.end(), module) == configurable.end())
                        configurable.push_back(module);
        configurable.push_back(&this->target_recipient_);
        configurable.push_back(&this->subtarget_);
        this->general_.SetModules(configurable);
        this->general_.SetTextModules(this->modules_.All());
        this->general_.SetVisibilityModules(this->modules_.All());
        this->player_.SetTiming(&this->timing_source_); this->player_.SetSounds(&this->ui_sounds_);
        this->player_.SetMPPreview(&this->cast_cost_source_);
        this->player_.SetHealPrediction(&this->heal_prediction_source_);
        this->target_.SetHealPrediction(&this->heal_prediction_source_);
        this->player_.SetSource(&this->party_source_);
        this->target_.SetTiming(&this->timing_source_); this->enemy_list_.SetTiming(&this->timing_source_);
        for (auto* group : {&this->party_, &this->alliance_first_, &this->alliance_second_})
        {
            group->SetTiming(&this->party_timing_source_);
            group->SetMPPreview(&this->cast_cost_source_);
            group->SetHealPrediction(&this->heal_prediction_source_);
        }
        for (const auto* module : this->modules_.All()) this->default_module_states_.push_back(module->State());
    }

    hud_runtime::~hud_runtime(void)
    {
        this->Shutdown();
    }

    auto hud_runtime::Initialize(IAshitaCore* core, const char* installPath) -> void
    {
        this->startup_profile_ = {};
        this->startup_render_recorded_ = false;
        startup_profile::scope total(this->startup_profile_, "initialize total");
        startup_profile::scope phase(this->startup_profile_, nullptr);
        this->core_ = core;
        this->game_icons_.SetResources(core != nullptr ? core->GetResourceManager() : nullptr);
        this->container_renderer_.LoadSkin(core != nullptr ? core->GetResourceManager() : nullptr);
        this->native_assets_.Load(core != nullptr ? core->GetResourceManager() : nullptr);
        phase.Mark("native assets");
        this->window_config_.Initialize();
        this->chat_filters_.Initialize(this->window_config_);
        this->visibility_source_.Initialize();
        this->visibility_context_ = {};
        this->visibility_pointer_release_ = false;
        this->ui_sounds_.Initialize();
        this->chat_input_.Initialize(core);
        this->chat_palette_.Initialize(Ashita::Memory::GetModuleBase("FFXiMain.dll"), Ashita::Memory::GetModuleSize("FFXiMain.dll"));
        this->chat_colors_ = {};
        this->next_chat_palette_update_ = 0;
        if (const auto palette = this->native_assets_.ControlPalette()) this->design_system_.SetControlPalette((*palette)[0], (*palette)[1]);
        this->world_source_.Initialize();
        this->minimap_source_.Initialize();
        this->minimap_.SetSource(this->minimap_source_.State(), this->minimap_maps_);
        this->compass_.Update(this->world_source_.State());
        this->world_info_.Update(this->world_source_.State());
        auto* input = core != nullptr ? core->GetInputManager() : nullptr;
        this->mouse_ = input != nullptr ? input->GetMouse() : nullptr;
        this->player_.ResetDefaults(this->design_system_.GetTheme());
        this->player_.SetMPPreview(&this->cast_cost_source_);
        this->player_.SetHealPrediction(&this->heal_prediction_source_);
        this->target_.SetHealPrediction(&this->heal_prediction_source_);
        this->player_.SetSource(&this->party_source_);
        this->player_cast_source_.Clear();
        this->cast_cost_source_.Initialize();
        this->recast_source_.Clear();
        this->cast_bar_.ResetDefaults(this->design_system_.GetTheme());
        this->cast_cost_.ResetDefaults(this->design_system_.GetTheme());
        this->recast_.ResetDefaults(this->design_system_.GetTheme());
        this->target_.ResetDefaults(this->design_system_.GetTheme());
        this->enemy_list_.ResetDefaults(this->design_system_.GetTheme());
        this->pet_source_.Clear();
        this->pet_.ResetDefaults(this->design_system_.GetTheme());
        this->pet_target_.ResetDefaults(this->design_system_.GetTheme());
        this->combat_source_.Clear();
        this->enemy_list_.SetCombat(&this->combat_source_);
        this->party_source_.Clear();
        this->ready_check_.ResetDefaults(this->design_system_.GetTheme());
        this->buffs_.ResetDefaults(this->design_system_.GetTheme());
        this->debuffs_.ResetDefaults(this->design_system_.GetTheme());
        this->party_.ResetDefaults(this->design_system_.GetTheme());
        this->alliance_first_.ResetDefaults(this->design_system_.GetTheme());
        this->alliance_second_.ResetDefaults(this->design_system_.GetTheme());
        this->experience_ = experience_module{};
        this->inventory_source_.Reset();
        this->satchel_source_.Reset();
        this->satchel_.ResetDefaults(this->design_system_.GetTheme());
        this->satchel_.Initialize(core, &this->satchel_source_, &this->satchel_cache_, &this->satchel_actions_);
        this->satchel_.SetSounds(&this->ui_sounds_);
        this->actionbars_.Initialize(core, &this->action_store_, &this->action_executor_);
        this->action_executor_.SetMacroResolver([this](const action_slot& slot) { return this->action_store_.ExpandMacro(slot); });
        this->actionbars_.SetSounds(&this->ui_sounds_);
        this->native_macros_.Initialize();
        if (this->native_macros_.Available())
            this->action_executor_.SetNativeRunner([this](const native_macro_ref& ref) { return this->native_macros_.Run(ref); });
        for (auto& inventory : this->inventory_) inventory.ResetDefaults(this->design_system_.GetTheme());
        this->equipment_source_.Reset();
        this->equipment_.ResetDefaults(this->design_system_.GetTheme());
        this->gil_source_ = {};
        this->gil_.ResetDefaults(this->design_system_.GetTheme());
        this->notifications_ = notification_module{};
        this->blue_magic_.ResetDefaults(this->design_system_.GetTheme());
        this->phantom_roll_.ResetDefaults(this->design_system_.GetTheme());
        this->skillchain_source_.Clear();
        this->magic_burst_.ResetDefaults(this->design_system_.GetTheme());
        this->treasure_source_.Clear(); this->treasure_.ResetDefaults(this->design_system_.GetTheme());
        this->general_.ResetDefaults(this->design_system_.GetTheme());
        this->game_hud_.ResetDefaults(this->design_system_.GetTheme());
        this->notification_source_.Reset(::GetTickCount64());
        this->active_module_ = &this->general_;
        this->interaction_module_ = nullptr;
        this->config_bounds_ = {-1.0f, 20.0f, ui::config_layout::default_width, ui::config_layout::default_height};
        this->active_color_ = nullptr;
        this->active_slider_ = nullptr;
        this->FinishConfigInput(false);
        this->active_option_group_ = module_option_group::general;
        this->active_config_section_ = 0;
        this->option_scroll_ = 0;
        this->layout_initialized_ = false;
        phase.Mark("native sources and defaults");

        const std::string resolvedInstallPath = installPath != nullptr && installPath[0] != '\0'
            ? installPath
            : (core != nullptr && core->GetInstallPath() != nullptr ? core->GetInstallPath() : ".");
        const std::string configDirectory = resolvedInstallPath + "\\config";
        ::CreateDirectoryA(configDirectory.c_str(), nullptr);
        ::CreateDirectoryA((configDirectory + "\\sounds").c_str(), nullptr);
        this->ready_check_.SetSoundDirectory(configDirectory + "\\sounds");
        this->settings_path_ = configDirectory + "\\xiuinative.ini";
        this->satchel_cache_.Initialize(configDirectory + "\\xiuinative-inventories");
        this->default_settings_text_ = LoadEmbeddedText(301);
        const std::string themePath = configDirectory + "\\xiuinative-theme.ini";
        SeedEmbeddedFile(302, themePath);
        const auto notices = configDirectory + "\\xiuinative-licenses";
        ::CreateDirectoryA(notices.c_str(), nullptr);
        SeedEmbeddedFile(202, notices + "\\XIVParty.txt");
        SeedEmbeddedFile(204, notices + "\\XIUI-Notification-Icons.txt");
        SeedEmbeddedFile(303, notices + "\\Kenney-Cursors.txt");
        SeedEmbeddedFile(304, notices + "\\MinHook.txt");
        SeedEmbeddedFile(310, notices + "\\Satchel-Storage-Slips.txt");
        SeedEmbeddedFile(311, notices + "\\Satchel-MIT.txt");
        SeedEmbeddedFile(312, notices + "\\libwebp.txt");
        SeedEmbeddedFile(313, notices + "\\libwebp-PATENTS.txt");
        SeedEmbeddedFile(314, notices + "\\miniz.txt");
        SeedEmbeddedFile(406, notices + "\\ChatMon-Sounds.txt");
        SeedEmbeddedFile(502, notices + "\\Grammara-Font.txt");
        SeedEmbeddedFile(702, notices + "\\HeadsUp-Phoenix-Mob-Data.txt");
        SeedEmbeddedFile(703, notices + "\\HeadsUp-Phoenix-GPL-3.0.txt");
        SeedEmbeddedFile(705, notices + "\\MobDB-Damage-Icons.txt");
        SeedEmbeddedFile(602, notices + "\\XIUI-Blue-Magic.txt");
        SeedEmbeddedFile(806, notices + "\\XIUI-Icon-Themes-GPL-3.0.txt");
        SeedEmbeddedFile(807, notices + "\\XIUI-Icon-Themes.txt");
        SeedEmbeddedFile(901, notices + "\\XIUI-Pet-Portraits.txt");
        SeedEmbeddedFile(1001, notices + "\\XIUI-Minimap.txt");
        SeedEmbeddedFile(1002, notices + "\\Remapster-CC-BY-SA-4.0.txt");
        SeedEmbeddedFile(1101, notices + "\\XIUI-Action-Bar-Artwork.txt");
        SeedEmbeddedFile(1201, notices + "\\XIUI-Skillchains-Magic-Burst.txt");
        SeedEmbeddedFile(1202, notices + "\\Phoenix-Rules-Data.txt");
        SeedEmbeddedFile(1203, notices + "\\Phoenix-GPL-3.0.txt");
        this->design_system_.SetThemePath(themePath.c_str());
        for (auto* module : this->modules_.All())
        {
            module->ApplyMeterDefaults(this->design_system_.GetTheme().meter);
            module->ApplyTextDefaults(this->design_system_.GetTheme());
        }
        phase.Mark("theme and embedded files");
        this->LoadSettings();
        phase.Mark("settings load");
        this->profiles_.Initialize(this->settings_path_);
        phase.Mark("profile catalog");
        this->profile_identity_.Initialize();
        this->UpdateCharacterProfile(::GetTickCount64());
        phase.Mark("character profile");
        this->SaveSettings();
        phase.Mark("settings snapshot");
        this->chat_.LoadCompletions(core);
        phase.Mark("chat completions");
    }

    auto hud_runtime::Shutdown(void) -> void
    {
        this->SuspendActionBarsInput();
        this->action_executor_.Cancel();
        this->SuspendSatchelInput();
        this->satchel_cache_.Stop();
        this->ReleaseControllerFocus();
        this->ready_check_.Stop();
        this->ReleaseChatInput();
        this->SetEditing(false);
        this->ui_sounds_.Shutdown();
        this->settings_writer_.Stop();
        this->profile_writer_.Stop();
        if ((this->settings_writer_.HasFailed() || this->profile_writer_.HasFailed()) && this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->Write(1, false, "[XIUI] Settings could not be saved before shutdown; check config directory permissions.");
        this->cursor_renderer_.Shutdown();
        this->minimap_maps_.Shutdown();
        this->skillchain_source_.Clear();
        this->treasure_source_.Clear(); this->treasure_.EndInput();
        this->game_icons_.Shutdown();
        this->color_picker_.Shutdown();
        this->native_assets_.Shutdown();
        this->container_renderer_.Shutdown();
        this->design_system_.Shutdown();
        this->device_ = nullptr;
        this->mouse_ = nullptr;
        this->core_ = nullptr;
    }

    auto hud_runtime::SetDevice(IDirect3DDevice8* device) -> bool
    {
        startup_profile::scope timing(this->startup_profile_, "device resources", device != nullptr && device != this->device_);
        if (device == nullptr) { this->ReleaseControllerFocus(); this->SuspendSatchelInput(); }
        if (device == nullptr) { this->FinishConfigInput(false); this->CloseProfileControls(); }
        this->device_ = device;
        const bool designReady = this->design_system_.SetDevice(device);
        const bool cursorReady = this->cursor_renderer_.SetDevice(device);
        const bool containersReady = this->container_renderer_.SetDevice(device);
        this->game_icons_.SetDevice(device);
        this->minimap_maps_.SetDevice(device);
        this->native_assets_.SetDevice(device);
        this->color_picker_.SetDevice(device);
        return designReady && cursorReady && containersReady;
    }

#if defined(XIUI_ENABLE_TEST_FAULTS)
    auto hud_runtime::SetRenderFaultForTesting(const bool enabled) -> void
    {
        this->raise_after_begin_for_testing_ = enabled;
    }
#endif

    auto hud_runtime::HandleTargetMouse(const uint32_t message, const bool blocked) -> bool
    {
        if (this->hud_target_pressed_)
        {
            if (message == WM_LBUTTONUP) { this->hud_target_pressed_ = false; this->hud_target_module_ = nullptr; return true; }
            if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) return true;
        }
        if (this->editing_ || blocked || this->chat_.Focused() || (message != WM_LBUTTONDOWN && message != WM_LBUTTONDBLCLK)) return false;
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        if (target == nullptr || entity == nullptr || (target->GetIsMenuOpen() != 0 && target->GetActionTargetActive() == 0 && target->GetIsSubTargetActive() == 0)) return false;
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        auto selected = this->ContextVisible(this->enemy_list_) ? this->enemy_list_.HitTarget(x, y, ::GetTickCount64()) : std::pair<uint32_t, uint32_t>{};
        const hud_module* selectedModule = selected.first != 0 ? &this->enemy_list_ : nullptr;
        for (const auto* party : {&this->alliance_second_, &this->alliance_first_, &this->party_})
        {
            if (selected.first != 0) break;
            if (this->ContextVisible(*party)) selected = party->HitTarget(x, y);
            if (selected.first != 0) selectedModule = party;
        }
        const auto [id, index] = selected;
        if (id == 0 || index == 0 || index >= entity->GetEntityMapSize() || entity->GetServerId(index) != id ||
            (entity->GetRenderFlags0(index) & 0x200) == 0 || (entity->GetRenderFlags0(index) & 0x4000) != 0) return false;
        if (!this->SelectHudTarget(id, index)) return false;
        this->hud_target_pressed_ = true;
        this->hud_target_module_ = selectedModule;
        return true;
    }

    auto hud_runtime::HandleMouseMessage(const uint32_t message, const WPARAM wParam, const LPARAM lParam, const bool blocked) -> bool
    {
        this->UpdateVisibilityContext();
        if (!this->satchel_.PointerDown())
        {
            if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) this->satchel_pointer_release_ &= ~MK_LBUTTON;
            if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) this->satchel_pointer_release_ &= ~MK_RBUTTON;
        }
        if (!this->actionbars_.PointerDown())
        {
            if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) this->actionbar_pointer_release_ &= ~MK_LBUTTON;
            if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) this->actionbar_pointer_release_ &= ~MK_RBUTTON;
        }
        if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP) && this->actionbar_pointer_release_ != 0 && !this->actionbars_.PointerDown() &&
            this->HandleActionBarsMouse(message, wParam, -1, -1, blocked)) return true;
        if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP) && this->satchel_pointer_release_ != 0 && !this->satchel_.PointerDown() &&
            this->HandleSatchelMouse(message, wParam, -1, -1, blocked)) return true;
        if (this->visibility_pointer_release_)
        {
            if (message == WM_LBUTTONUP) { this->visibility_pointer_release_ = false; return true; }
            if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) return true;
        }
        if (!this->visible_ || this->device_ == nullptr)
            return false;

        switch (message)
        {
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONUP:
            break;
        default:
            return false;
        }

        auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        const HWND gameWindow = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
        if (gameWindow == nullptr || ::GetForegroundWindow() != gameWindow)
        { this->hud_target_pressed_ = false; return false; }

        POINT cursor{signed_low_word(lParam), signed_high_word(lParam)};
        if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
        {
            if (::ScreenToClient(gameWindow, &cursor) == FALSE)
                return false;
        }
        this->mouse_x_ = cursor.x;
        this->mouse_y_ = cursor.y;
        if (this->HandleSatchelMouse(message, wParam, static_cast<float>(cursor.x), static_cast<float>(cursor.y), blocked)) return true;
        if (this->HandleActionBarsMouse(message, wParam, static_cast<float>(cursor.x), static_cast<float>(cursor.y), blocked)) return true;
        if (this->HandleTreasureMouse(message, static_cast<float>(cursor.x), static_cast<float>(cursor.y), blocked)) return true;

        D3DVIEWPORT8 viewport{};
        if (FAILED(this->device_->GetViewport(&viewport)))
            return false;
        this->EnsureLayout(viewport);
        if (this->HandleControllerPointer(message, static_cast<float>(cursor.x), static_cast<float>(cursor.y), viewport))
        {
            if (message == WM_LBUTTONDOWN)
            {
                this->SetChatMouseCapture(true);
                ::SetCapture(gameWindow);
                if (::GetCapture() == gameWindow) this->mouse_capture_window_ = gameWindow;
            }
            return true;
        }
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MOUSEWHEEL)
        {
            this->FinishControllerMove(false);
            this->FinishConfigResize(false);
            this->controller_presented_ = false;
            this->controller_config_.active = false;
            if (this->editing_) this->controller_surface_ = controller_surface::none;
            if (!this->editing_ && this->controller_surface_ != controller_surface::chat && this->controller_surface_ != controller_surface::chat2)
                this->ReleaseControllerFocus();
        }
        if (this->ContextVisible(this->ready_check_)) this->ready_check_.SetHover(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
        if (!this->editing_ && this->ContextVisible(this->ready_check_) && !this->chat_.HistoryExpanded() && (this->ready_check_.PointerDown() || (!blocked && this->ready_check_.Contains(static_cast<float>(cursor.x), static_cast<float>(cursor.y)))))
        {
            if (this->ready_check_.HandleMouse(message, static_cast<float>(cursor.x), static_cast<float>(cursor.y), ::GetTickCount64()))
            {
                if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) this->chat_.Focus(false);
                this->SetChatMouseCapture(true);
                if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && ::GetCapture() != gameWindow)
                { ::SetCapture(gameWindow); if (::GetCapture() == gameWindow) this->mouse_capture_window_ = gameWindow; }
                if (message == WM_LBUTTONUP) this->ReleasePointerCapture();
                return true;
            }
        }
        if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && this->slider_input_.Focused() &&
            !Contains(this->slider_input_bounds_, static_cast<float>(cursor.x), static_cast<float>(cursor.y))) this->FinishConfigInput(true, false);
        if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && this->option_text_input_.Focused() &&
            !Contains(this->text_input_bounds_, static_cast<float>(cursor.x), static_cast<float>(cursor.y))) this->FinishConfigInput(true, false);
        if (!this->editing_ && this->hud_target_pressed_) return this->HandleTargetMouse(message, blocked);
        auto* chat = this->chat_pointer_ != nullptr ? this->chat_pointer_ : this->ChatAt(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
        const bool overConfig = this->editing_ && (this->ProfileSurface(viewport) || this->choice_input_.Opened() || this->input_mode_menu_ || this->settings_search_open_ ||
            (this->mouse_captured_ && this->mouse_buttons_ != 0) || Contains(this->ConfigVisibleBounds(), static_cast<float>(cursor.x), static_cast<float>(cursor.y)) ||
            (this->active_color_ != nullptr && Contains(this->ConfigLayout().ColorEditor(viewport), static_cast<float>(cursor.x), static_cast<float>(cursor.y))));
        if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) &&
            (chat == nullptr || overConfig)) this->chat_.Focus(false);
        if (chat != nullptr && (!blocked || this->chat_mouse_block_saved_ || this->ChatResizing()) &&
            (!overConfig || this->ChatResizing()))
        {
            bool save{};
            const bool resizing = chat->Resizing();
            const bool consumed = chat->HandleMouse(message, wParam, static_cast<float>(cursor.x), static_cast<float>(cursor.y), this->editing_, this->design_system_,
                {static_cast<float>(viewport.X), static_cast<float>(viewport.Y), static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)}, save,
                this->core_ != nullptr ? this->core_->GetChatManager() : nullptr);
            if (save) this->SaveSettings();
            if (!this->editing_) this->SetChatMouseCapture(consumed || this->ChatAt(static_cast<float>(cursor.x), static_cast<float>(cursor.y)) != nullptr);
            if (consumed)
            {
                if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
                {
                    if (this->editing_) this->FocusModuleConfig(*chat);
                    this->chat_pointer_ = chat;
                    if (::GetCapture() != gameWindow)
                    { ::SetCapture(gameWindow); if (::GetCapture() == gameWindow) this->mouse_capture_window_ = gameWindow; }
                }
                if (message == WM_LBUTTONUP) { this->chat_pointer_ = nullptr; this->ReleasePointerCapture(); }
                return true;
            }
            if (resizing) { this->chat_pointer_ = nullptr; this->ReleasePointerCapture(); }
        }
        if (message == WM_MOUSEWHEEL && !blocked && !overConfig && this->interaction_ == interaction::none &&
            this->chat_pointer_ == nullptr && this->ContextVisible(this->minimap_) &&
            (this->editing_ ? this->minimap_.PreviewVisible() : this->minimap_.ShouldRender({})))
        {
            bool save{};
            if (this->minimap_.HandleMouseWheel(static_cast<short>(HIWORD(wParam)), static_cast<float>(cursor.x), static_cast<float>(cursor.y), save))
            {
                if (save) this->SaveSettings();
                return true;
            }
        }
        if (!this->editing_) return this->HandleTargetMouse(message, blocked);
        constexpr uint32_t buttonMask = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
        const bool captured = this->ProcessPointerMessage(message, static_cast<uint32_t>(wParam) & buttonMask, viewport);

        if (captured && this->editing_ && (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && ::GetCapture() != gameWindow)
        {
            ::SetCapture(gameWindow);
            if (::GetCapture() == gameWindow)
                this->mouse_capture_window_ = gameWindow;
        }
        if (this->mouse_buttons_ == 0)
            this->ReleasePointerCapture();

        if (message == WM_MOUSEWHEEL && captured && (this->choice_input_.Opened() || this->profile_name_.Focused() || Contains(this->ConfigVisibleBounds(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))))
        {
            if (this->settings_search_open_)
            {
                this->MoveSettingsSearch(-static_cast<short>((wParam >> 16) & 0xFFFF) / WHEEL_DELTA * 3);
                return true;
            }
            if (this->choice_input_.Opened())
            {
                this->choice_input_.Scroll(-static_cast<short>((wParam >> 16) & 0xFFFF) / WHEEL_DELTA);
                return true;
            }
            if (this->config_collapsed_) return captured;
            this->FinishConfigInput(true, false);
            const short delta = static_cast<short>((wParam >> 16) & 0xFFFF);
            if (this->profile_name_.Focused())
            {
                if (this->profile_dialog_ == profile_dialog::share || this->profile_dialog_ == profile_dialog::import_profile)
                {
                    const ui::profile_dialog_layout dialog{this->config_bounds_, viewport, true};
                    if (Contains(dialog.Key(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                        this->profile_key_view_.Scroll(-(delta / WHEEL_DELTA) * 3);
                }
                return captured;
            }
            if (this->profile_actions_) return captured;
            if (this->profile_dropdown_ && Contains(this->ConfigLayout().ProfileMenu(this->profiles_.Entries().size()), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
            {
                this->profile_scroll_ = std::clamp(this->profile_scroll_ - delta / WHEEL_DELTA, 0, std::max(0, static_cast<int>(this->profiles_.Entries().size()) - 7));
                return captured;
            }
            if (this->IsExperienceDisplay() && this->experience_display_.Scroll(this->experience_, this->ConfigLayout(),
                static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), delta / WHEEL_DELTA)) return captured;
            this->option_scroll_ -= (delta / WHEEL_DELTA) * this->ConfigLayout().Columns();
            this->ClampOptionScroll();
            this->active_color_ = nullptr;
        }

        return captured;
    }

    auto hud_runtime::ChatAt(const float x, const float y) -> chat_module*
    {
        if (this->ContextVisible(this->chat_) && this->chat_.HistoryExpanded() && this->chat_.HitTest(this->design_system_, x, y)) return &this->chat_;
        if (this->ContextVisible(this->chat_second_) && this->chat_second_.WindowVisible() && this->chat_second_.HitTest(this->design_system_, x, y)) return &this->chat_second_;
        if (this->ContextVisible(this->chat_) && (this->editing_ || this->chat_.WindowVisible()) && this->chat_.HitTest(this->design_system_, x, y)) return &this->chat_;
        return nullptr;
    }

    auto hud_runtime::ChatResizeAt(const float x, const float y) -> bool
    {
        if (this->chat_.HistoryExpanded()) return false;
        const auto* chat = this->ChatAt(x, y);
        return chat != nullptr && Contains(chat->Grip(this->design_system_), x, y);
    }

    auto hud_runtime::SetChatMouseCapture(const bool capture) -> void
    {
        if (this->mouse_ == nullptr) return;
        if (capture)
        {
            if (!this->chat_mouse_block_saved_)
            {
                this->previous_chat_mouse_blocked_ = this->mouse_->GetBlockInput();
                this->chat_mouse_block_saved_ = true;
            }
            this->mouse_->SetBlockInput(true);
        }
        else if (this->chat_mouse_block_saved_)
        {
            this->mouse_->SetBlockInput(this->previous_chat_mouse_blocked_);
            this->chat_mouse_block_saved_ = false;
        }
    }

    auto hud_runtime::ReleaseChatInput(const bool preserveExpandedHistory) -> void
    {
        this->ready_check_.CancelPointer();
        this->chat_.Focus(false, preserveExpandedHistory);
        this->chat_second_.Focus(false, preserveExpandedHistory);
        this->chat_native_context_suspended_ = false;
        if (this->ChatResizing()) this->SaveSettings();
        this->chat_.CancelPointer();
        this->chat_second_.CancelPointer();
        this->chat_pointer_ = nullptr;
        this->SetChatMouseCapture(false);
        if (!this->editing_) this->ReleasePointerCapture();
    }

    auto hud_runtime::FinishConfigInput(const bool apply, const bool keepInvalid) -> bool
    {
        this->CloseSettingsSearch();
        this->CloseChoiceInput();
        if (this->option_text_input_.Focused())
        {
            bool changed{};
            auto* module = this->text_input_module_;
            const auto index = this->text_input_option_index_;
            if (apply && module != nullptr && module == this->active_module_ && index < module->OptionCount())
            {
                const auto option = module->Option(index);
                if (option.available && option.text != nullptr && option.text == this->text_input_value_)
                {
                    changed = *option.text != this->option_text_input_.Editor().Text();
                    if (changed) { *option.text = this->option_text_input_.Editor().Text(); module->OnOptionChanged(index); }
                }
            }
            this->option_text_input_.Cancel();
            this->text_input_module_ = nullptr;
            this->text_input_value_ = nullptr;
            if (changed) this->SaveSettings();
        }
        if (!this->slider_input_.Focused()) return true;
        bool changed = false;
        auto* module = this->slider_input_module_;
        const auto index = this->slider_input_option_index_;
        if (apply && module != nullptr && module == this->active_module_ && index < module->OptionCount())
        {
            const auto option = module->Option(index);
            if (option.available && option.slider == this->slider_input_.Option().slider)
            {
                if (const auto value = this->slider_input_.Value())
                {
                    changed = *option.slider != *value;
                    if (changed) { *option.slider = *value; module->OnOptionChanged(index); }
                }
                else if (keepInvalid) return false;
            }
        }
        this->slider_input_.Cancel();
        this->slider_input_module_ = nullptr;
        if (changed) this->SaveSettings();
        return true;
    }

    auto hud_runtime::HandleConfigInputKey(const WPARAM key, const LPARAM flags) -> bool
    {
        if (!this->slider_input_.Focused() && !this->option_text_input_.Focused()) return false;
        if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
        if (key == VK_ESCAPE) this->FinishConfigInput(false);
        else if (key == VK_RETURN || key == VK_TAB)
        {
            if ((static_cast<uint32_t>(flags) & 0x40000000u) == 0 && this->FinishConfigInput(true))
                this->ui_sounds_.Select(::GetTickCount64());
        }
        else
        {
            const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
            const auto window = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
            if (this->option_text_input_.Focused()) this->option_text_input_.HandleKey(key, flags, window);
            else this->slider_input_.HandleKey(key, flags, window);
        }
        return true;
    }

    auto hud_runtime::SelectedChatRecipient(void) const -> std::string
    {
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto index = target != nullptr ? target->GetTargetIndex(0) : 0;
        if (entity == nullptr || index == 0 || index >= entity->GetEntityMapSize() || (entity->GetSpawnFlags(index) & 1) == 0) return {};
        const auto* name = entity->GetName(index);
        const std::string_view recipient{name != nullptr ? name : "", name != nullptr ? strnlen_s(name, 64) : 0};
        return IsTellRecipient(recipient) ? std::string{recipient} : std::string{};
    }

    auto hud_runtime::SyncNativeChatInput(IChatManager* manager, const bool nativeMenu, const bool targeting) -> void
    {
        if (!this->ContextVisible(this->chat_)) return;
        if (this->slider_input_.Focused() || this->option_text_input_.Focused() || this->choice_input_.Opened()) return;
        if (this->chat_.HistoryFocused())
        {
            if (manager != nullptr && manager->IsInputOpen() != 0) this->ReleaseChatInput();
            return;
        }
        if ((!nativeMenu && !targeting) || !this->chat_.Focused()) this->chat_native_context_suspended_ = false;
        if (nativeMenu && !this->chat_native_context_suspended_ && !targeting && this->core_ != nullptr)
        {
            this->chat_native_menu_recipient_ = this->SelectedChatRecipient();
            this->chat_native_menu_confirmed_at_ = ::GetTickCount64();
        }
        if (this->chat_activation_enter_down_ && manager != nullptr && manager->IsInputOpen() != 0)
        { this->ReleaseChatInput(); return; }
        const auto selectedRecipient = nativeMenu ? this->SelectedChatRecipient() : std::string{};
        if (auto draft = this->chat_input_.TakeInput(manager))
        {
            this->ReleaseControllerFocus();
            const auto recipient = ::GetTickCount64() - this->chat_native_menu_confirmed_at_ <= 1000
                ? this->chat_native_menu_recipient_ : std::string{};
            if (IsTellDraft(draft->text))
                this->chat_.AdoptTell(std::move(*draft), recipient.empty() ? selectedRecipient : recipient);
            else
                this->chat_.AdoptDraft(std::move(*draft));
            this->chat_native_menu_recipient_.clear();
            this->chat_native_context_suspended_ = nativeMenu || targeting;
        }
        else if ((manager != nullptr && manager->IsInputOpen() != 0) || ((nativeMenu || targeting) && !this->chat_native_context_suspended_))
            this->ReleaseChatInput();
    }

    auto hud_runtime::HandleKey(const WPARAM key, const LPARAM flags, const bool blocked) -> bool
    {
        this->UpdateVisibilityContext();
        if (this->HandleActionBarsKey(key, flags, blocked)) return true;
        if (this->HandleSatchelKey(key, flags, blocked)) return true;
        if (this->HandleTreasureKey(key, flags, blocked)) return true;
        if (this->confirmation_key_release_ == key && key != 0)
        {
            if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) this->confirmation_key_release_ = 0;
            return true;
        }
        if (this->controller_surface_ == controller_surface::picker && (!this->visible_ || this->ControllerNativeBusy()))
            this->ReleaseControllerFocus();
        if (this->controller_surface_ == controller_surface::picker)
        {
            if (blocked) return false;
            if ((static_cast<uint32_t>(flags) & 0x80000000u) != 0) return true;
            const auto action = key == VK_UP ? controller_action::up : key == VK_DOWN ? controller_action::down :
                key == VK_RETURN ? controller_action::confirm : key == VK_ESCAPE ? controller_action::cancel : controller_action::none;
            if (action != controller_action::none) this->HandleController({action, controller_backend::none, 0, true,
                (static_cast<uint32_t>(flags) & 0x40000000u) != 0, false, ::GetTickCount64()});
            this->controller_presented_ = false;
            return true;
        }
        if (key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT || key == VK_ESCAPE || key == VK_TAB)
            this->controller_presented_ = false;
        if (key == VK_RETURN) this->chat_activation_enter_down_ = (static_cast<uint32_t>(flags) & 0x80000000u) == 0;
        if (!this->visible_ || this->device_ == nullptr) return false;
        auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        if (properties == nullptr || ::GetForegroundWindow() != properties->GetFinalFantasyHwnd())
        { this->ReleaseChatInput(true); this->FinishConfigInput(false); return false; }
        if (this->editing_ && !blocked && key == 'F' && (::GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
            (static_cast<uint32_t>(flags) & 0x80000000u) == 0 && !this->profile_name_.Focused() &&
            !this->slider_input_.Focused() && !this->option_text_input_.Focused() && !this->ControllerNativeBusy())
        { this->BeginSettingsSearch(); return true; }
        if (this->settings_search_open_)
        {
            if (this->ControllerNativeBusy()) { this->CloseSettingsSearch(); return false; }
            return this->HandleSettingsSearchKey(key, flags, properties->GetFinalFantasyHwnd());
        }
        if (this->editing_ && !blocked && (this->controller_config_.resizing ||
            (key == VK_F8 && (static_cast<uint32_t>(flags) & 0x20000000u) != 0)))
        {
            const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
            const bool repeated = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
            if (released) return true;
            if (!this->controller_config_.resizing)
            {
                if (!repeated && !this->profile_name_.Focused() && !this->slider_input_.Focused() &&
                    !this->option_text_input_.Focused() && !this->choice_input_.Opened() && !this->ControllerNativeBusy())
                    this->BeginConfigResize(true);
                return true;
            }
            const auto action = key == VK_LEFT ? controller_action::left : key == VK_RIGHT ? controller_action::right :
                key == VK_UP ? controller_action::up : key == VK_DOWN ? controller_action::down :
                key == VK_RETURN ? controller_action::confirm : key == VK_ESCAPE ? controller_action::cancel :
                key == VK_SPACE ? controller_action::actions : controller_action::none;
            if (action != controller_action::none) this->HandleConfigResize(action, repeated);
            this->controller_presented_ = false;
            return true;
        }
        if (this->editing_ && this->HandleProfileKey(key, flags)) return true;
        if (this->editing_ && this->HandleChoiceKey(key, flags)) return true;
        const bool replacement = this->chat_capture_available_ && (this->game_hud_.HiddenMask() & NativeHudBit(native_hud_element::chat1)) != 0;
        const auto* manager = this->core_->GetChatManager();
        const auto* memory = this->core_->GetMemoryManager();
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        const bool targeting = target != nullptr && (target->GetIsSubTargetActive() != 0 || target->GetActionTargetActive() != 0);
        if (this->slider_input_.Focused() || this->option_text_input_.Focused())
        {
            this->NormalizeConfigSelection();
            if (!this->editing_ || targeting || (manager != nullptr && manager->IsInputOpen() != 0))
            { this->FinishConfigInput(false); return false; }
            if (this->slider_input_.Focused() || this->option_text_input_.Focused()) return this->HandleConfigInputKey(key, flags);
        }
        if (key == VK_RETURN && this->chat_activation_enter_down_ && this->native_ui_.menu_visible && !this->chat_.Focused() && !targeting)
        {
            this->chat_native_menu_recipient_ = this->SelectedChatRecipient();
            this->chat_native_menu_confirmed_at_ = ::GetTickCount64();
        }
        return this->HandleChatKey(key, flags, blocked, replacement, targeting, manager != nullptr ? manager->IsInputOpen() : 0);
    }

    auto hud_runtime::HandleChatKey(const WPARAM key, const LPARAM flags, const bool blocked, const bool replacement,
        const bool targeting, const uint8_t nativeInput) -> bool
    {
        if (nativeInput != 0)
        {
            this->ReleaseControllerFocus();
            this->ReleaseChatInput();
            return false;
        }
        auto& history = this->chat_second_.HistoryFocused() ? this->chat_second_ : this->chat_;
        const auto keyFlags = static_cast<uint32_t>(flags);
        const bool statusFocused = this->controller_surface_ == controller_surface::buffs || this->controller_surface_ == controller_surface::debuffs;
        const bool compactKeyboard = KeyboardAllowsCharacterShortcuts(this->window_config_.FullKeyboard());
        if (!this->editing_ && !blocked && key == 'F' && (keyFlags & 0x80000000u) == 0 &&
            (compactKeyboard || history.HistoryFocused() || statusFocused) &&
            !this->chat_.Focused() && !history.HistoryExpanded() && (replacement || history.HistoryFocused() || statusFocused) &&
            (::GetKeyState(VK_CONTROL) & 0x8000) == 0 && (::GetKeyState(VK_MENU) & 0x8000) == 0 &&
            (keyFlags & 0x20000000u) == 0 && ((::GetKeyState(VK_SHIFT) & 0x8000) == 0 || history.HistoryFocused() || statusFocused))
        {
            if ((keyFlags & 0x40000000u) != 0) return history.HistoryFocused() || statusFocused;
            if (this->CycleWindowFocus(false))
            {
                this->chat_native_context_suspended_ = targeting || this->native_ui_.menu_visible;
                return true;
            }
        }
        if (!this->editing_ && statusFocused)
        {
            if (blocked || (keyFlags & 0x80000000u) != 0) return false;
            const auto action = key == VK_LEFT ? controller_action::left : key == VK_RIGHT ? controller_action::right :
                key == VK_UP ? controller_action::up : key == VK_DOWN ? controller_action::down :
                key == VK_ESCAPE ? controller_action::cancel : key == VK_RETURN ? controller_action::confirm :
                key == VK_TAB ? ((::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? controller_action::previous_tab : controller_action::next_tab) : controller_action::none;
            if (action == controller_action::none) return false;
            const bool consumed = this->HandleController({action, controller_backend::none, 0, true,
                (keyFlags & 0x40000000u) != 0, false, ::GetTickCount64()});
            this->controller_presented_ = false;
            return consumed;
        }
        if (!this->editing_ && history.HistoryFocused() && this->ContextVisible(history) && history.HandleHistoryKey(key, flags, blocked, replacement, this->design_system_))
        {
            this->chat_native_context_suspended_ = targeting || this->native_ui_.menu_visible;
            return true;
        }
        if (!this->editing_ && history.HistoryFocused()) return false;
        if (!this->ContextVisible(this->chat_)) return false;
        if ((targeting || this->native_ui_.menu_visible) && !this->chat_native_context_suspended_)
        {
            if (!this->chat_.Focused() && (key == VK_OEM_2 || key == VK_DIVIDE) &&
                this->chat_.HandleKey(this->core_, key, flags, blocked, replacement))
            {
                this->chat_native_context_suspended_ = true;
                return true;
            }
            this->ReleaseChatInput();
            return false;
        }
        const bool reply = key == 'R' && (((static_cast<uint32_t>(flags) & 0x20000000u) != 0) ||
            (::GetKeyState(VK_MENU) & 0x8000) != 0 || (::GetKeyState(VK_CONTROL) & 0x8000) != 0);
        return this->chat_.HandleKey(this->core_, key, flags, blocked, replacement, reply ? this->chat_input_.ReplyTargets() : std::vector<std::string>{});
    }

    auto hud_runtime::CycleWindowFocus(const bool controller) -> bool
    {
        constexpr std::array windows{controller_surface::chat, controller_surface::chat2, controller_surface::buffs, controller_surface::debuffs};
        const auto current = this->chat_second_.HistoryFocused() ? controller_surface::chat2 :
            this->chat_.HistoryFocused() ? controller_surface::chat : this->controller_surface_;
        const auto found = std::find(windows.begin(), windows.end(), current);
        const size_t start = found != windows.end() ? static_cast<size_t>(found - windows.begin()) : windows.size() - 1;
        const auto available = this->ControllerSurfaces();
        for (size_t step = 1; step <= windows.size(); ++step)
        {
            const auto surface = windows[(start + step) % windows.size()];
            if (std::find(available.begin(), available.end(), surface) == available.end()) continue;
            if (!controller && (surface == controller_surface::chat || surface == controller_surface::chat2))
            {
                this->ReleaseControllerFocus();
                this->ReleaseChatInput();
                auto& window = surface == controller_surface::chat ? this->chat_ : this->chat_second_;
                if (!window.BeginHistory()) continue;
            }
            else if (!this->BeginControllerFocus(surface)) continue;
            this->controller_presented_ = controller;
            return true;
        }
        return false;
    }

    auto hud_runtime::PostNotification(notification_event event) -> void
    {
        this->notifications_.Center().Post(std::move(event), ::GetTickCount64());
    }

    auto hud_runtime::HandlePacket(const uint16_t id, const uint8_t* data, const uint32_t size) -> void
    {
        const auto now = ::GetTickCount64();
        if (id == 0x000B && this->profile_tracking_) this->ObserveProfileCharacter("");
        if (id == 0x000A || id == 0x000B) this->ReleaseControllerFocus();
        if (id == 0x000A || id == 0x000B) this->minimap_source_.Clear();
        if (id == 0x000A || id == 0x000B) this->party_timing_source_.Clear();
        if (data != nullptr)
        {
            const combat_action_packet actionPacket{{data, size}};
            if (id == 0xA0) this->minimap_source_.HandlePartyPosition({data, size}, now);
            this->treasure_source_.HandlePacket(id, {data, size}, now);
            this->chat_.ObserveTellPacket(id, {data, size});
            this->notification_source_.HandlePacket(this->core_, this->notifications_.Center(), id, {data, size}, now);
            this->experience_.HandlePacket(this->core_, id, {data, size}, now, &actionPacket);
            this->enemy_list_.HandlePacket(this->core_, id, {data, size}, now, &actionPacket);
            this->party_source_.HandlePacket(this->core_, id, {data, size}, now, &actionPacket);
            this->blue_magic_.HandlePacket(this->core_, id, {data, size}, now);
            this->phantom_roll_.HandlePacket(this->core_, id, {data, size}, now, &actionPacket);
            this->combat_source_.SetRules(this->combat_settings_.Rules());
            this->combat_source_.HandlePacket(this->core_, id, {data, size}, now, &actionPacket);
            this->skillchain_source_.SetRules(this->combat_settings_.Rules());
            if (this->magic_burst_.State().enabled || this->actionbars_.SkillchainPredictionsEnabled()) this->skillchain_source_.HandlePacket(this->core_, id, {data, size}, now, &actionPacket);
            else this->skillchain_source_.Clear();
            if (this->timing_ready_) this->timing_source_.HandlePacket(this->core_, this->combat_source_, id, {data, size}, now, &actionPacket);
            if (this->timing_ready_ && id == 0x0028)
            {
                this->party_timing_source_.Update(this->party_source_.Members(), this->party_source_.Members()[0].id, &this->timing_source_, now);
                if (const auto& action = actionPacket.Action()) this->party_timing_source_.Observe(*action, now);
            }
            this->player_cast_source_.HandlePacket(this->core_, id, {data, size}, now, &actionPacket);
            if (id == 0x000A || id == 0x000B) { this->cast_cost_source_.Clear(); this->heal_prediction_source_.Clear(); }
            if (id == 0x000A || id == 0x000B) this->recast_source_.Clear();
            this->inventory_source_.HandlePacket(id, {data, size});
            this->satchel_source_.HandlePacket(id, {data, size});
            this->gil_source_.HandlePacket(id, {data, size});
            this->ready_check_.HandlePacket(this->core_, id, {data, size}, now);
            if (id == 0x000A || id == 0x000B) this->equipment_source_.Reset();
        }
    }

    auto hud_runtime::HandleCommand(const char* command) -> bool
    {
        if (command != nullptr && this->HandleActionBarsCommand(command)) return true;
        if (command != nullptr && this->HandleSatchelCommand(command)) return true;
        if (command == nullptr)
            return false;
        if (::_stricmp(command, "toggleui") == 0)
        {
            this->SetVisible(!this->visible_);
            return true;
        }
        if (::_stricmp(command, "startup") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                for (size_t index = 0; index < this->startup_profile_.count; ++index)
                {
                    const auto& entry = this->startup_profile_.entries[index];
                    this->core_->GetChatManager()->Writef(1, false, "[XIUI] Startup %s: %.2f ms.", entry.name, entry.milliseconds);
                }
            return true;
        }
        if (this->ControllerCommand(command)) return true;
        const char* message{};
        if (::_stricmp(command,"rings status")==0)
        {
            if (this->core_ && this->core_->GetChatManager())
                this->core_->GetChatManager()->Writef(1,false,"[XIUI] %s",this->target_rings_.Status().c_str());
            return true;
        }
        if (::_stricmp(command, "casting") == 0 || ::_stricmp(command, "castbar") == 0 || ::_stricmp(command, "castcost") == 0 || ::_stricmp(command, "recast") == 0)
        {
            this->active_module_ = ::_stricmp(command, "recast") == 0 ? static_cast<hud_module*>(&this->recast_) :
                ::_stricmp(command, "castcost") == 0 ? static_cast<hud_module*>(&this->cast_cost_) : &this->cast_bar_;
            this->active_option_group_ = module_option_group::general;
            this->active_config_section_ = 0;
            this->active_color_ = nullptr; this->option_scroll_ = 0;
            this->SetVisible(true); this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "fonts status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Fonts: Bahnschrift %s; Segoe UI %s; Tahoma %s; Grammara %s.",
                    this->design_system_.FontAvailable(0) ? "ready" : "unavailable", this->design_system_.FontAvailable(1) ? "ready" : "unavailable",
                    this->design_system_.FontAvailable(2) ? "ready" : "unavailable", this->design_system_.FontAvailable(3) ? "ready" : "unavailable");
            return true;
        }
        else if (::_stricmp(command, "recast status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Recast: source %s; %zu tracked cooldowns; icons %s; module %s.",
                    this->recast_source_.Available() ? "native" : "unavailable", this->recast_source_.Entries().size(),
                    this->game_icons_.RecastAtlas() != nullptr ? "embedded" : "unavailable", this->recast_.State().enabled ? "enabled" : "disabled");
            return true;
        }
        else if (::_stricmp(command, "castbar status") == 0 || ::_stricmp(command, "castcost status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            {
                if (::_stricmp(command, "castbar status") == 0)
                    this->core_->GetChatManager()->Writef(1, false, "[XIUI] Cast Bar: source %s; %s; progress %.0f%%; module %s.",
                        this->player_cast_source_.Available() ? "native" : "unavailable", this->player_cast_source_.State().Active() ? this->player_cast_source_.State().name.c_str() : "idle",
                        this->player_cast_source_.State().progress * 100, this->cast_bar_.State().enabled ? "enabled" : "disabled");
                else
                    this->core_->GetChatManager()->Writef(1, false, "[XIUI] Cast Cost: native menu support 0x%X (spell=1, ability=2, mount=4); selected %s; module %s.",
                        this->cast_cost_source_.AvailableMask(), this->cast_cost_source_.State().Valid() ? this->cast_cost_source_.State().name.c_str() : "none",
                        this->cast_cost_.State().enabled ? "enabled" : "disabled");
            }
            return true;
        }
        else if (::_stricmp(command, "timing status") == 0)
        {
            const auto timer = this->timing_source_.Player(::GetTickCount64() / 1000.0);
            const auto recovery = this->timing_source_.Recovery().Predict(::GetTickCount64() / 1000.0);
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Timing: attack %s, ~%.2fs interval (%s); resting %s; enemy clocks %zu.",
                    timer.active ? "active" : "idle", timer.interval, timer.learned ? "observed" : "modeled", recovery.active ? "active" : "idle", this->timing_source_.Count());
            return true;
        }
        else if (::_stricmp(command, "readycheck") == 0)
        {
            this->active_module_ = &this->ready_check_;
            this->active_option_group_ = module_option_group::general;
            this->active_config_section_ = 0;
            this->active_color_ = nullptr; this->option_scroll_ = 0;
            this->SetVisible(true); this->SetEditing(true);
            return true;
        }
        else if (::_strnicmp(command, "readycheck ", 11) == 0)
        {
            std::string action{command + 11};
            std::transform(action.begin(), action.end(), action.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            this->ready_check_.HandleCommand(this->core_, action);
            if (action.starts_with("volume ")) this->SaveSettings();
            return true;
        }
        else if (::_stricmp(command, "save") == 0)
        {
            this->FinishInteraction();
            this->SaveSettings();
            message = "[XIUI] Current settings queued for saving.";
        }
        else if (::_stricmp(command, "general") == 0 || ::_stricmp(command, "theme") == 0)
        {
            this->active_module_ = &this->general_;
            this->active_option_group_ = module_option_group::appearance;
            this->active_config_section_ = 0;
            this->active_color_ = nullptr; this->option_scroll_ = 0;
            this->SetVisible(true); this->SetEditing(true);
            return true;
        }
        else if (::_strnicmp(command, "theme ", 6) == 0 && command[6] >= '1' && command[6] <= '8' && command[7] == '\0')
        {
            this->general_.UpdateWindowType(this->window_config_.Type());
            if (!this->general_.RequestWindowType(command[6] - '1')) message = "[XIUI] Client Window Type control is unavailable.";
            else return true;
        }
        else if (::_stricmp(command, "party") == 0 || ::_stricmp(command, "alliance1") == 0 || ::_stricmp(command, "alliance2") == 0)
        {
            this->active_module_ = ::_stricmp(command, "party") == 0 ? &this->party_ : ::_stricmp(command, "alliance1") == 0 ? &this->alliance_first_ : &this->alliance_second_;
            this->active_option_group_ = module_option_group::general;
            this->active_config_section_ = 0;
            this->active_color_ = nullptr; this->option_scroll_ = 0;
            this->SetVisible(true); this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "party layout") == 0 || ::_stricmp(command, "party compact") == 0)
        {
            if (::_stricmp(command, "party compact") == 0)
            {
                this->party_.UseCompactLayout();
                this->SaveSettings();
            }
            const auto report = this->party_.DescribeLayout();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Write(1, false, report.c_str());
            return true;
        }
        else if (::_stricmp(command, "party status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Party: %zu/6; alliance: %zu/6, %zu/6; source %s; XIVParty icons %s; module %s.",
                    this->party_source_.Count(0), this->party_source_.Count(1), this->party_source_.Count(2), this->party_source_.Available() ? "live" : "unavailable",
                    this->game_icons_.JobAtlas() != nullptr ? "loaded" : "unavailable", this->party_.State().enabled ? "enabled" : "disabled");
            return true;
        }
        else if (::_stricmp(command, "treasure") == 0 || ::_stricmp(command, "pool") == 0)
        { this->treasure_.Open(); this->SetVisible(true); return true; }
        else if (::_stricmp(command, "treasure settings") == 0)
        { this->SelectConfigModule(&this->treasure_); this->SetVisible(true); this->SetEditing(true); return true; }
        else if (::_stricmp(command, "treasure status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Treasure Pool: %s; %zu items. Rewards use Notifications.", this->treasure_source_.Ready() ? "ready" : "loading", this->treasure_source_.Count());
            return true;
        }
        else if (::_stricmp(command, "treasure lotall") == 0 || ::_stricmp(command, "treasure passall") == 0)
        {
            if (this->TreasureInputAvailable())
                this->treasure_.Batch(::_stricmp(command, "treasure passall") == 0);
            return true;
        }
        else if (::_stricmp(command, "rules phoenix") == 0 || ::_stricmp(command, "rules retail") == 0 || ::_stricmp(command, "rules horizon") == 0)
        {
            const auto rules = ::_stricmp(command, "rules phoenix") == 0 ? server_rules::phoenix :
                ::_stricmp(command, "rules horizon") == 0 ? server_rules::horizon : server_rules::retail;
            this->combat_settings_.SetRules(rules);
            this->skillchain_source_.Clear();
            this->combat_source_.Clear();
            this->SaveSettings();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Combat rules: %s.", ServerRulesName(this->combat_settings_.Rules()));
            return true;
        }
        else if (::_stricmp(command, "skillchain status") == 0 || ::_stricmp(command, "magicburst status") == 0)
        {
            const auto* window = this->skillchain_source_.Current(::GetTickCount64());
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Skillchains: %s rules; %zu tracked targets; selected %u; chain %s; HUD %s; predictions %s.",
                    ServerRulesName(this->combat_settings_.Rules()), this->skillchain_source_.Count(), this->skillchain_source_.Target(),
                    window != nullptr && window->chain != skillchain_property::none ? SkillchainName(window->chain) : "none",
                    this->magic_burst_.State().enabled ? "enabled" : "disabled", this->actionbars_.SkillchainPredictionsEnabled() ? "enabled" : "disabled");
            return true;
        }
        else if (::_stricmp(command, "skillchain") == 0 || ::_stricmp(command, "magicburst") == 0)
        {
            if (::_stricmp(command, "skillchain") == 0)
            {
                this->SelectConfigModule(&this->actionbars_);
                this->active_config_section_ = this->actionbars_.ConfigSections().size() - 1;
            }
            else this->SelectConfigModule(&this->magic_burst_);
            this->SetVisible(true); this->SetEditing(true); return true;
        }
        else if (::_stricmp(command, "minimap status") == 0)
        {
            const auto& state = this->minimap_source_.State();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Minimap: native table %s; zone %u, floor %d; alignment %s; UV %.4f, %.4f; %zu markers; %zu bundled map records.",
                    this->minimap_source_.Available() ? "available" : "unavailable", state.zone, state.floor, state.available ? "available" : "unavailable",
                    state.player.x, state.player.y, state.markers.size(), this->minimap_maps_.Count());
            return true;
        }
        else if (::_stricmp(command, "minimap") == 0)
        {
            this->SelectConfigModule(&this->minimap_);
            this->SetVisible(true); this->SetEditing(true); return true;
        }
        else if (::_stricmp(command, "commands") == 0 || ::_stricmp(command, "menu") == 0 || ::_stricmp(command, "compass") == 0 || ::_stricmp(command, "world") == 0 || ::_stricmp(command, "weather") == 0 || ::_stricmp(command, "clock") == 0)
        {
            this->active_module_ = ::_stricmp(command, "commands") == 0 || ::_stricmp(command, "menu") == 0 ? static_cast<hud_module*>(&this->commands_) :
                ::_stricmp(command, "compass") == 0 ? static_cast<hud_module*>(&this->compass_) : &this->world_info_;
            this->active_option_group_ = module_option_group::general;
            this->active_color_ = nullptr;
            this->option_scroll_ = 0;
            this->SetVisible(true); this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "commands status") == 0 || ::_stricmp(command, "theme status") == 0)
        {
            const auto& state = this->native_ui_;
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Native UI: Window Type %u (%s); command module %s; %u active command panels; anchor %s; movement %s; themed bars %s.",
                    state.window_type + 1, state.window_type_available ? "following game" : "unavailable", this->commands_.State().enabled ? "enabled" : "disabled",
                    state.menu_count, this->commands_.AnchorsTop() ? "Top" : "Bottom", state.menu_available ? "available" : "unavailable",
                    this->native_assets_.MeterSkin() && this->container_renderer_.MeterBackground().texture != nullptr ? "available" : "unavailable");
            return true;
        }
        else if (::_stricmp(command, "world status") == 0 || ::_stricmp(command, "compass status") == 0)
        {
            const auto& state = this->world_source_.State();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Time %s %02u:%02u; weather %s; heading %s %.0f; %zu markers; native assets %s; compass camera %s.",
                    state.time_valid ? "available" : "unavailable", state.time.hour, state.time.minute,
                    state.weather_valid ? weather_names[state.weather] : "unavailable", state.heading_valid ? HeadingName(state.heading) : "unavailable", HeadingDegrees(state.heading),
                    state.markers.size(), this->native_assets_.CompassAvailable() ? "available" : "unavailable", state.compass_rotation_valid ? "available" : "unavailable");
            return true;
        }
        else if (::_stricmp(command, "statuses") == 0 || ::_stricmp(command, "effects") == 0 ||
            ::_stricmp(command, "buffs") == 0 || ::_stricmp(command, "debuffs") == 0)
        {
            this->active_module_ = ::_stricmp(command, "debuffs") == 0 ? &this->debuffs_ : &this->buffs_;
            this->active_config_section_ = 0;
            this->option_scroll_ = 0;
            this->active_option_group_ = module_option_group::general;
            this->active_color_ = nullptr;
            this->SetVisible(true);
            this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "chat") == 0)
        {
            this->active_module_ = &this->chat_;
            this->active_option_group_ = module_option_group::general;
            this->active_color_ = nullptr;
            this->option_scroll_ = 0;
            this->SetVisible(true);
            this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "chat status") == 0)
        {
            const auto window1 = this->window_config_.ChatFilterValue(185);
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Chat: %s; %s; capture %s; %zu messages; game palette %s; tabs %s; second window %s (%s); native filters %s; Window 1 emotes %s.",
                    this->chat_.State().enabled ? "enabled" : "disabled", this->chat_.ViewName(), this->chat_capture_available_ ? "available" : "unavailable", this->chat_.Count(),
                    this->chat_palette_available_ ? "available" : "unavailable", this->chat_.TabsVisible() ? "shown" : "hidden",
                    this->chat_second_.State().enabled ? "enabled" : "disabled", this->chat_second_.ViewName(),
                    this->chat_filters_.Option(1).available ? "ready" : "unavailable", !window1 ? "unavailable" : (*window1 & 0x80) != 0 ? "shown" : "hidden");
            return true;
        }
        else if (::_strnicmp(command, "exp ", 4) == 0)
        {
            if (!this->experience_.HandleCommand(this->core_, command + 4)) return false;
            this->SaveSettings();
            return true;
        }
        else if (::_stricmp(command, "exp") == 0 || ::_stricmp(command, "experience") == 0 || ::_stricmp(command, "notifications") == 0)
        {
            this->active_module_ = ::_stricmp(command, "notifications") == 0 ? static_cast<hud_module*>(&this->notifications_) : &this->experience_;
            this->active_config_section_ = 0;
            this->active_option_group_ = module_option_group::general;
            this->option_scroll_ = 0;
            this->active_color_ = nullptr;
            this->active_slider_ = nullptr;
            this->SetVisible(true);
            this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "gil") == 0 || ::_stricmp(command, "equipment") == 0 || ::_stricmp(command, "gear") == 0 || ::_stricmp(command, "extras") == 0)
        {
            this->active_module_ = ::_stricmp(command, "gil") == 0 ? static_cast<hud_module*>(&this->gil_) :
                ::_stricmp(command, "extras") == 0 ? static_cast<hud_module*>(&this->compass_) : &this->equipment_;
            this->active_config_section_ = 0;
            this->active_option_group_ = module_option_group::general;
            this->option_scroll_ = 0;
            this->active_color_ = nullptr;
            this->active_slider_ = nullptr;
            this->SetVisible(true);
            this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "sounds status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Native UI sounds: %s.", this->ui_sounds_.Available() ? "ready" : "unavailable on this client");
            return true;
        }
        else if (::_stricmp(command, "features status") == 0)
        {
            const auto& progress = this->experience_.Progression();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Existing modules: progression %s %u/%u; combat records %zu; notifications %zu; party/enemy click targeting active outside edit mode.",
                    progress.Label(), progress.current, progress.needed, this->combat_source_.Count(), this->notifications_.Center().Entries().size());
            return true;
        }
        else if (::_stricmp(command, "party copy-alliance") == 0)
        {
            this->alliance_first_.CopyPreferences(this->party_);
            this->alliance_second_.CopyPreferences(this->party_);
            this->SaveSettings();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Copied current Party settings to Alliance 1 and Alliance 2; positions and visibility preserved.");
            return true;
        }
        else if (::_stricmp(command, "gil reset") == 0 || ::_stricmp(command, "gil status") == 0)
        {
            if (::_stricmp(command, "gil reset") == 0) { this->gil_source_.ResetSession(::GetTickCount64()); this->SaveSettings(); }
            const auto& session = this->gil_source_.Session();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Gil: %u; session %s; rate %s/hr; source %s; DAT icon %s.", session.balance,
                    FormatGil(session.Net(), true).c_str(), FormatGil(session.PerHour(::GetTickCount64()), true).c_str(),
                    session.ready ? "ready" : "loading", this->game_icons_.Item(65535) != nullptr ? "ready" : "unavailable");
            return true;
        }
        else if (::_stricmp(command, "equipment status") == 0)
        {
            size_t equipped{}, ready{};
            for (const auto& item : this->equipment_source_.Items()) { equipped += item.id != 0; ready += item.ready; }
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Equipment: %zu/16 equipped; %zu slots ready; source %s; module %s.",
                    equipped, ready, this->equipment_source_.Available() ? "live" : "unavailable", this->equipment_.State().enabled ? "enabled" : "disabled");
            return true;
        }
        else if (::_stricmp(command, "inventory") == 0)
        {
            this->active_module_ = &this->inventory_[0];
            this->active_config_section_ = 0;
            this->active_option_group_ = module_option_group::general;
            this->option_scroll_ = 0;
            this->active_color_ = nullptr;
            this->active_slider_ = nullptr;
            this->SetVisible(true);
            this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "inventory status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                for (const auto& inventory : this->inventory_)
                    this->core_->GetChatManager()->Writef(1, false, "[XIUI] %s.", inventory.Describe().c_str());
            return true;
        }
        else if (::_stricmp(command, "chat recover") == 0)
        {
            const bool recovered = this->chat_input_.Recover(this->core_ != nullptr ? this->core_->GetChatManager() : nullptr);
            if (recovered) this->ReleaseChatInput();
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Write(1, false, recovered ? "[XIUI] Native input restored after the tell handoff." : "[XIUI] Native input does not need this recovery.");
            return true;
        }
        else if (::_stricmp(command, "chat input") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            {
                const auto* input = this->core_->GetInputManager();
                const auto* keyboard = input != nullptr ? input->GetKeyboard() : nullptr;
                const auto* memory = this->core_->GetMemoryManager();
                const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
                const auto fullKeyboard = this->window_config_.FullKeyboard();
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Chat input: reply lookup %s; native chat handoff %s; native input status %u; editor %s; keyboard %s; layout %s.",
                    this->chat_input_.ReplyAvailable() ? "available" : "unavailable", this->chat_input_.HandoffAvailable() ? "available" : "unavailable",
                    this->core_->GetChatManager()->IsInputOpen(), this->ChatFocused() ? "focused" : "idle",
                    keyboard != nullptr && keyboard->GetBlockInput() ? "blocked" : "available", fullKeyboard ? (*fullKeyboard ? "Full" : "Compact") : "unavailable");
                this->core_->GetChatManager()->Writef(1, false, "[XIUI] Chat routing: replacement %s; native menu %s; subtarget %u; action target %u; target menu %u; action type %u; action ID %u.",
                    this->chat_capture_available_ && (this->game_hud_.HiddenMask() & NativeHudBit(native_hud_element::chat1)) != 0 ? "enabled" : "disabled",
                    this->native_ui_.menu_visible ? "visible" : "hidden", target != nullptr ? target->GetIsSubTargetActive() : 0,
                    target != nullptr ? target->GetActionTargetActive() : 0, target != nullptr ? target->GetIsMenuOpen() : 0,
                    target != nullptr ? target->GetActionType() : 0, target != nullptr ? target->GetActionId() : 0);
            }
            return true;
        }
        else if (::_stricmp(command, "chat colors") == 0)
        {
            const auto report = this->chat_.DescribeColors(this->chat_colors_);
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr) this->core_->GetChatManager()->Write(1, false, report.c_str());
            return true;
        }
        else if (::_stricmp(command, "chat clear") == 0) { this->chat_.Clear(); return true; }
        else if (::_strnicmp(command, "chat complete ", 14) == 0)
        {
            const auto report = this->chat_.DescribeCompletions(this->core_, command + 14);
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr) this->core_->GetChatManager()->Write(1, false, report.c_str());
            return true;
        }
        else if (::_stricmp(command, "chat show") == 0 || ::_stricmp(command, "chat hide") == 0)
        {
            this->chat_.State().enabled = ::_stricmp(command, "chat show") == 0;
            if (!this->chat_.State().enabled) this->ReleaseChatInput();
            this->SaveSettings();
            return true;
        }
        else if (::_stricmp(command, "enemylist") == 0)
        {
            this->active_module_ = &this->enemy_list_;
            this->active_option_group_ = module_option_group::general;
            this->active_color_ = nullptr;
            this->option_scroll_ = 0;
            this->SetVisible(true);
            this->SetEditing(true);
            return true;
        }
        else if (::_stricmp(command, "enemylist status") == 0)
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            {
                char text[128]{};
                std::snprintf(text, sizeof(text), "[XIUI] Enemy list: %zu tracked; %s.", this->enemy_list_.Count(), this->enemy_list_.State().enabled ? "enabled" : "disabled");
                this->core_->GetChatManager()->Write(1, false, text);
            }
            return true;
        }
        else if (::_strnicmp(command, "gamehud", 7) == 0 && (command[7] == '\0' || command[7] == ' '))
        {
            std::istringstream args{command + 7};
            std::string action, key;
            args >> action >> key;
            if (action.empty())
            {
                this->active_module_ = &this->game_hud_;
                this->active_option_group_ = module_option_group::general;
                this->active_config_section_ = 0;
                this->active_color_ = nullptr;
                this->option_scroll_ = 0;
                this->SetVisible(true);
                this->SetEditing(true);
                return true;
            }
            if (action == "show" && key == "all")
                this->game_hud_.ShowAll();
            else if (action == "hide" && key == "all")
                this->game_hud_.HideAll();
            else if ((action != "show" && action != "hide") || !this->game_hud_.SetShown(key, action == "show"))
                message = "[XIUI] Use /xiui gamehud or gamehud show|hide <element|all>.";
            if (message == nullptr)
            {
                this->SaveSettings();
                message = "[XIUI] Game HUD visibility updated.";
            }
        }
        else if (::_stricmp(command, "notify test") == 0)
        {
            this->notifications_.Demo(::GetTickCount64());
            message = "[XIUI] Notification samples queued (XP, item, skill).";
        }
        else if (::_stricmp(command, "notify clear") == 0)
        {
            this->notifications_.Center().Clear();
            message = "[XIUI] Notifications cleared.";
        }
        else if (::_stricmp(command, "notify status") == 0)
            message = this->notifications_.IsScreenAnchored() ? "[XIUI] Notifications use a stable screen-space anchor." : "[XIUI] Notifications use the saved manual position.";
        else if (::_stricmp(command, "container") == 0 || ::_stricmp(command, "container preview") == 0 || ::_stricmp(command, "container show") == 0 || ::_stricmp(command, "container hide") == 0)
        {
            message = "[XIUI] The container demo has been removed. Modules follow the game's Window Type setting.";
        }
        else
            return false;
        if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->Write(1, false, message);
        return true;
    }

    auto hud_runtime::ToggleEdit(void) -> bool
    {
        return this->SetEditing(!this->editing_);
    }

    auto hud_runtime::ExitEdit(void) -> void
    {
        this->ReleaseChatInput();
        this->SetEditing(false);
    }

    auto hud_runtime::SetVisible(const bool visible) -> void
    {
        this->visible_ = visible;
        if (!visible)
        {
            this->SuspendActionBarsInput();
            this->SuspendSatchelInput();
            if (this->editing_ && this->mouse_down_) this->visibility_pointer_release_ = true;
            this->ReleaseControllerFocus();
            this->hud_target_pressed_ = false;
            this->ReleaseChatInput();
            this->SetEditing(false);
        }
    }

    auto hud_runtime::ReloadSettings(void) -> void
    {
        startup_profile::scope timing(this->startup_profile_, "settings reload");
        this->CloseProfileControls();
        this->FinishConfigInput(false);
        this->FinishInteraction();
        if (!this->DrainSettings())
        {
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Write(1, false, "[XIUI] Pending settings could not be saved; reload cancelled.");
            return;
        }
        this->design_system_.ReloadTheme();
        for (auto* module : this->modules_.All())
        {
            module->ApplyMeterDefaults(this->design_system_.GetTheme().meter);
            module->ApplyTextDefaults(this->design_system_.GetTheme());
        }
        const auto selection = settings_store::FromIniText("");
        this->SaveConfigSelection(selection);
        this->LoadSettings();
        this->LoadConfigSelection(selection);
        this->profiles_.Initialize(this->settings_path_);
        if (this->profile_tracking_)
        {
            this->profile_character_ready_ = false;
            this->profile_character_.clear();
            this->profile_observed_character_.clear();
            this->next_profile_identity_update_ = 0;
            this->UpdateCharacterProfile(::GetTickCount64());
        }
        this->CloseProfileControls();
    }

    auto hud_runtime::ResetSettings(void) -> void
    {
        this->combat_settings_.ResetDefaults();
        this->heal_prediction_source_.Load(settings_store::FromIniText(""));
        this->ReleaseControllerFocus();
        this->CloseProfileControls();
        this->FinishConfigInput(false);
        this->ReleaseChatInput();
        this->CloseChoiceInput();
        this->notification_source_.Reset(::GetTickCount64());
        for (size_t index = 0; index < this->modules_.Count(); ++index)
        {
            auto* module = this->modules_.At(index);
            module->ResetDefaults(this->design_system_.GetTheme());
            module->State() = this->default_module_states_[index];
            module->ApplyMeterDefaults(this->design_system_.GetTheme().meter);
            module->ApplyTextDefaults(this->design_system_.GetTheme());
        }
        this->active_module_ = &this->general_;
        this->interaction_module_ = nullptr;
        this->interaction_ = interaction::none;
        this->config_collapsed_ = false;
        this->input_mode_menu_ = false;
        this->config_bounds_ = {-1.0f, 20.0f, ui::config_layout::default_width, ui::config_layout::default_height};
        this->config_sidebar_collapsed_ = false;
        this->active_color_ = nullptr;
        this->active_slider_ = nullptr;
        this->active_option_group_ = module_option_group::general;
        this->active_config_section_ = 0;
        this->option_scroll_ = 0;
        this->layout_initialized_ = false;
        if (!this->default_settings_text_.empty())
            this->LoadSettings(settings_store::FromIniText(this->default_settings_text_));
        this->SaveSettings();
    }

    auto hud_runtime::FlushSettings(void) const -> void
    {
        this->SaveSettings();
        if (!this->DrainSettings())
            throw std::runtime_error{"XIUI settings could not be saved"};
    }

    auto hud_runtime::IsEditing(void) const -> bool
    {
        return this->editing_;
    }

    auto hud_runtime::IsVisible(void) const -> bool
    {
        return this->visible_;
    }

    auto hud_runtime::IsGpuShaderActive(void) const -> bool
    {
        return this->design_system_.IsGpuShaderActive();
    }

    auto hud_runtime::SetEditing(const bool editing, const bool playSound) -> bool
    {
        if (editing) this->SuspendSatchelInput();
        if (!editing)
            this->cursor_renderer_.ReleaseNativeCursorOnOwnerThread();
        if (this->editing_ == editing)
            return true;
        this->SuspendActionBarsInput(false);
        if (editing && this->mouse_ == nullptr)
            return false;
        this->FinishConfigInput(false);
        this->ReleaseControllerFocus();
        this->ReleaseChatInput();

        this->CloseProfileControls();

        this->config_collapsed_ = false;
        this->interaction_ = interaction::none;
        this->hud_target_pressed_ = false;
        this->interaction_module_ = nullptr;
        this->active_color_ = nullptr;
        this->active_slider_ = nullptr;
        this->mouse_down_ = false;
        this->ReleasePointerCapture();
        this->mouse_captured_ = false;
        this->mouse_buttons_ = 0;

        if (editing)
        {
            this->previous_mouse_blocked_ = this->mouse_->GetBlockInput();
            this->mouse_block_state_saved_ = true;
            this->editing_ = true;
            this->ui_sounds_.MenuOpened(::GetTickCount64(), playSound);
        }
        else
        {
            this->editing_ = false;
            this->ui_sounds_.Hover(0, ::GetTickCount64(), false);
            if (this->mouse_block_state_saved_ && this->mouse_ != nullptr)
            {
                this->mouse_->SetBlockInput(this->previous_mouse_blocked_);
                this->mouse_block_state_saved_ = false;
            }
            this->SaveSettings();
        }
        return true;
    }

    auto hud_runtime::LoadSettings(void) -> void
    {
        this->skillchain_source_.Clear();
        if (this->settings_path_.empty())
            return;

        const auto attributes = ::GetFileAttributesA(this->settings_path_.c_str());
        const auto error = attributes == INVALID_FILE_ATTRIBUTES ? ::GetLastError() : ERROR_SUCCESS;
        if (!this->default_settings_text_.empty() && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND))
        {
            this->LoadSettings(settings_store::FromIniText(this->default_settings_text_));
            this->SaveSettings();
        }
        else
            this->LoadSettings(settings_store::CachedFile(this->settings_path_));
    }

    auto hud_runtime::LoadSettings(const settings_store& store) -> void
    {
        this->skillchain_source_.Clear();
        this->combat_settings_.Load(store);
        this->SuspendActionBarsInput();
        this->action_executor_.Cancel();
        this->action_store_.Load(store);
        this->SuspendSatchelInput();
        this->settings_search_revealed_module_ = nullptr;
        this->settings_search_highlight_module_ = nullptr;
        this->CloseSettingsSearch();
        this->heal_prediction_source_.Load(store);
        this->CloseProfileControls();
        ++this->controller_input_revision_;
        this->ReleaseControllerFocus();
        for (auto* module : this->modules_.All())
            module->Load(store);
        this->config_bounds_ = store.ReadRect("layout.config", {-1.0f, 20.0f, ui::config_layout::default_width, ui::config_layout::default_height});
        this->config_sidebar_collapsed_ = store.ReadBool("layout.config", "sidebar_collapsed", false);
        if (!store.ReadBool("layout.config", "sidebar_labels", false) && this->config_bounds_.width == 680.0f)
            this->config_bounds_.width = ui::config_layout::default_width;
        if (store.ReadInt("layout.config", "width_version", 0) < 1 && this->config_bounds_.width <= 820.0f)
            this->config_bounds_.width = ui::config_layout::default_width;
        if (store.ReadInt("layout.config", "height_version", 0) < 1 && this->config_bounds_.height == 430.0f)
            this->config_bounds_.height = ui::config_layout::default_height;
        this->active_color_ = nullptr;
        this->active_slider_ = nullptr;
        this->active_option_group_ = module_option_group::general;
        this->active_config_section_ = 0;
        this->option_scroll_ = 0;
        this->layout_initialized_ = false;
        this->LoadConfigSelection(store);
    }

    auto hud_runtime::CaptureSettings(void) const -> settings_store
    {
        settings_store store{this->settings_path_};
        this->action_store_.Save(store);
        this->heal_prediction_source_.Save(store);
        for (const auto* module : this->modules_.All())
            module->Save(store);
        this->combat_settings_.Save(store);
        if (this->controller_config_.moving)
            store.WriteRect((std::string{"layout."} + this->controller_config_.moving_id).c_str(), this->controller_config_.move_original);
        store.WriteRect("layout.config", this->config_bounds_);
        if (this->controller_config_.resizing)
            store.WriteRect("layout.config", this->controller_config_.resize_original);
        store.WriteBool("layout.config", "sidebar_labels", true);
        store.WriteBool("layout.config", "sidebar_collapsed", this->config_sidebar_collapsed_);
        store.WriteInt("layout.config", "width_version", 1);
        store.WriteInt("layout.config", "height_version", 1);
        this->SaveConfigSelection(store);
        return store;
    }

    auto hud_runtime::SaveSettings(void) const -> void
    {
        if (this->settings_path_.empty()) return;
        auto store = this->CaptureSettings();
        if (this->profiles_.Ready())
        {
            const auto& active = this->profiles_.Current();
            store.WriteString("profiles", "active", active.id);
            if (!this->profile_tracking_ || this->profile_character_ready_)
            {
                if (this->profile_character_ready_)
                    store.WriteString("profiles.characters", this->profile_character_.c_str(), active.id);
                auto profile = store.Preferences(active.path);
                profile.WriteString("profile", "name", active.name);
                profile.WriteInt("profile", "version", 1);
                this->profile_writer_.Queue(std::move(profile));
            }
        }
        this->settings_writer_.Queue(std::move(store));
    }

    auto hud_runtime::EnsureLayout(const D3DVIEWPORT8& viewport) -> void
    {
        if (!this->layout_initialized_)
        {
            if (this->config_bounds_.x < 0.0f)
                this->config_bounds_.x = std::max(8.0f, static_cast<float>(viewport.Width) - this->config_bounds_.width - 20.0f);
            if (this->chat_.State().bounds.y < 0.0f)
                this->chat_.State().bounds.y = std::max(0.0f, static_cast<float>(viewport.Height) - this->chat_.State().bounds.height - 18.0f);
            if (this->compass_.State().bounds.x < 0) this->compass_.State().bounds.x = std::max(0.0f, viewport.Width - this->compass_.State().bounds.width - 24.0f);
            if (this->minimap_.State().bounds.x < 0) this->minimap_.State().bounds.x = std::max(0.0f, viewport.Width - this->minimap_.State().bounds.width - 24.0f);
            if (this->minimap_.State().bounds.y < 0) this->minimap_.State().bounds.y = 90;
            if (this->compass_.State().bounds.y < 0) this->compass_.State().bounds.y = std::max(0.0f, viewport.Height - this->compass_.State().bounds.height - 24.0f);
            auto& satchel = this->satchel_.State().bounds;
            if (satchel.x < 0) satchel.x = std::max(8.0f, (viewport.Width - satchel.width) * 0.5f);
            if (satchel.y < 0) satchel.y = std::max(8.0f, (viewport.Height - satchel.height) * 0.5f);
            for (size_t i = 0; i <= action_bar_count; ++i)
            {
                auto& bar = this->actionbars_.BarState(i).bounds;
                if (bar.x < 0) bar.x = std::max(8.0f, (viewport.Width - bar.width) * 0.5f);
                if (bar.y < 0) bar.y = std::max(8.0f, viewport.Height * 0.76f - (i % 3) * 52.0f);
            }
            if (this->world_info_.State().bounds.x < 0) this->world_info_.State().bounds.x = std::max(0.0f, viewport.Width - this->world_info_.State().bounds.width - 24.0f);
            if (this->world_info_.State().bounds.y < 0) this->world_info_.State().bounds.y = std::max(0.0f, this->compass_.State().bounds.y - this->world_info_.State().bounds.height - 10.0f);
            this->world_info_.InitializeParts();
            for (size_t i = 0; i < this->inventory_.size(); ++i)
            {
                auto& bounds = this->inventory_[i].State().bounds;
                if (bounds.x < 0) bounds.x = std::max(16.0f, viewport.Width - bounds.width - 32);
                if (bounds.y < 0) bounds.y = std::min(80.0f + i * 100.0f, std::max(0.0f, viewport.Height - bounds.height - 16));
            }
            auto& equipment = this->equipment_.State().bounds;
            if (equipment.x < 0) equipment.x = std::max(16.0f, this->inventory_[0].State().bounds.x - equipment.width - 20);
            if (equipment.y < 0) equipment.y = this->inventory_[0].State().bounds.y;
            auto& gil = this->gil_.State().bounds;
            if (gil.x < 0) gil.x = std::max(16.0f, viewport.Width - gil.width - 32);
            if (gil.y < 0) gil.y = std::max(16.0f, viewport.Height - gil.height - 150);
            auto& readyCheck = this->ready_check_.State().bounds;
            if (readyCheck.x < 0) readyCheck.x = std::max(0.0f, (viewport.Width - readyCheck.width) * 0.5f);
            if (readyCheck.y < 0) readyCheck.y = std::max(0.0f, (viewport.Height - readyCheck.height) * 0.4f);
            auto& treasure = this->treasure_.State().bounds;
            if (treasure.x < 0) treasure.x = std::max(0.0f, viewport.Width - treasure.width - 24);
            if (treasure.y < 0) treasure.y = std::max(0.0f, viewport.Height * 0.2f);
            auto& blueMagic = this->blue_magic_.State().bounds;
            if (blueMagic.x < 0) blueMagic.x = std::max(0.0f, (viewport.Width - blueMagic.width) * 0.5f);
            if (blueMagic.y < 0) blueMagic.y = std::max(0.0f, (viewport.Height - blueMagic.height) * 0.25f);
            auto& phantomRoll = this->phantom_roll_.State().bounds;
            auto& magicBurst = this->magic_burst_.State().bounds;
            if (magicBurst.x < 0) magicBurst.x = std::max(0.0f, (viewport.Width - magicBurst.width) * 0.5f + 160);
            if (magicBurst.y < 0) magicBurst.y = std::max(0.0f, viewport.Height * 0.45f);
            if (phantomRoll.x < 0) phantomRoll.x = std::max(0.0f, (viewport.Width - phantomRoll.width) * 0.5f);
            if (phantomRoll.y < 0) phantomRoll.y = std::max(0.0f, (viewport.Height - phantomRoll.height) * 0.65f);
            auto& castBar = this->cast_bar_.State().bounds;
            if (castBar.x < 0) castBar.x = std::max(0.0f, (viewport.Width - castBar.width) * 0.5f);
            if (castBar.y < 0) castBar.y = std::max(0.0f, viewport.Height * 0.68f);
            auto& castCost = this->cast_cost_.State().bounds;
            if (castCost.x < 0) castCost.x = std::max(0.0f, (viewport.Width - castCost.width) * 0.5f);
            if (castCost.y < 0) castCost.y = castBar.y + castBar.height + 16;
            auto& recast = this->recast_.State().bounds;
            if (recast.x < 0) recast.x = std::max(16.0f, viewport.Width - recast.width - 32);
            if (recast.y < 0) recast.y = std::max(16.0f, viewport.Height * 0.35f);
            this->layout_initialized_ = true;
        }
        if (this->chat_second_.WindowVisible())
        {
            this->chat_second_.EnsureWindowDefaults();
            auto& second = this->chat_second_.State().bounds;
            const auto& main = this->chat_.State().bounds;
            if (second.x < 0) second.x = main.x;
            if (second.y < 0)
            {
                second.y = main.y - second.height - 32;
                if (second.y < 0 && main.y + main.height + second.height + 32 <= viewport.Height)
                    second.y = main.y + main.height + 32;
                else if (second.y < 0 && main.x + main.width + second.width + 16 <= viewport.Width)
                { second.x = main.x + main.width + 16; second.y = main.y; }
                second.y = std::max(28.0f, second.y);
            }
        }
        this->ClampLayout(viewport);
    }

    auto hud_runtime::ClampLayout(const D3DVIEWPORT8& viewport) -> void
    {
        const auto clampRect = [&viewport](rect& value, const float minWidth, const float minHeight) {
            value.width = std::clamp(value.width, minWidth, std::max(minWidth, static_cast<float>(viewport.Width)));
            value.height = std::clamp(value.height, minHeight, std::max(minHeight, static_cast<float>(viewport.Height)));
            value.x = std::clamp(value.x, 0.0f, std::max(0.0f, static_cast<float>(viewport.Width) - value.width));
            value.y = std::clamp(value.y, 0.0f, std::max(0.0f, static_cast<float>(viewport.Height) - value.height));
        };
        for (auto* module : this->modules_.All())
        {
            if (!module->HasEditableBounds())
                continue;
            auto& state = module->State();
            state.maximum_width = std::max(state.minimum_width, static_cast<float>(viewport.Width));
            state.bounds.width = std::clamp(state.bounds.width, state.minimum_width, state.maximum_width);
            auto* chat = module == &this->chat_ ? &this->chat_ : module == &this->chat_second_ ? &this->chat_second_ : nullptr;
            const float tabsHeight = chat != nullptr ? state.bounds.y - chat->VisualBounds(this->design_system_).y : 0.0f;
            state.bounds.height = std::clamp(state.bounds.height, state.minimum_height, std::max(state.minimum_height, static_cast<float>(viewport.Height) - tabsHeight));
            if (!module->PositionBoundsReady()) continue;
            const rect editBounds = this->MovementBounds(*module);
            state.bounds.x += std::clamp(editBounds.x, 0.0f, std::max(0.0f, static_cast<float>(viewport.Width) - editBounds.width)) - editBounds.x;
            state.bounds.y += std::clamp(editBounds.y, 0.0f, std::max(0.0f, static_cast<float>(viewport.Height) - editBounds.height)) - editBounds.y;
        }
        auto visibleConfig = this->ConfigVisibleBounds();
        clampRect(visibleConfig, std::min(this->ConfigLayout().MinimumWidth(), static_cast<float>(viewport.Width)),
            std::min(this->config_collapsed_ ? 32.0f : this->ConfigLayout().MinimumHeight(), static_cast<float>(viewport.Height)));
        this->config_bounds_.x = visibleConfig.x; this->config_bounds_.y = visibleConfig.y;
        this->config_bounds_.width = visibleConfig.width;
        if (!this->config_collapsed_) this->config_bounds_.height = visibleConfig.height;
    }

    auto hud_runtime::ToggleConfigSidebar(const D3DVIEWPORT8& viewport) -> void
    {
        this->FinishConfigInput(true, false);
        this->FinishConfigResize(false);
        this->CloseProfileControls();
        this->active_color_ = nullptr; this->active_slider_ = nullptr;
        const auto firstOption = this->GroupOptionIndex(static_cast<size_t>(std::max(0, this->option_scroll_)));
        this->config_sidebar_collapsed_ = !this->config_sidebar_collapsed_;
        const auto layout = this->ConfigLayout();
        if (!this->config_collapsed_ && viewport.Width != 0 && viewport.Height != 0)
            this->config_bounds_ = ui::ResizeConfigWindow(this->config_bounds_, ui::resize_bottom, 0, 0, layout.MinimumWidth(), layout.MinimumHeight(), viewport);
        if (!this->IsXIUIContributors())
        {
            const auto slots = this->GroupOptionSlots();
            const auto found = std::find(slots.begin(), slots.end(), firstOption);
            if (found != slots.end()) this->option_scroll_ = static_cast<int>(found - slots.begin());
        }
        this->ClampOptionScroll();
        if (this->controller_config_.active) this->RefreshControllerConfigFocus();
        this->config_tooltip_.clear();
        this->SaveSettings();
    }

    auto hud_runtime::ConfigResizeEdges(const float x, const float y) const -> uint8_t
    {
        if (!this->editing_ || this->config_collapsed_ || this->profile_name_.Focused() || this->profile_dropdown_ ||
            this->profile_actions_ || this->choice_input_.Opened() || this->input_mode_menu_) return 0;
        const auto& bounds = this->config_bounds_;
        if (!Contains(bounds, x, y)) return 0;
        if (this->active_color_ != nullptr && Contains(this->ConfigLayout().ColorEditor(this->controller_config_.viewport), x, y)) return 0;
        if (Contains(this->ConfigLayout().ResizeGrip(), x, y)) return ui::resize_right | ui::resize_bottom;
        constexpr float edge = 5;
        uint8_t result{};
        if (x < bounds.x + edge) result |= ui::resize_left;
        else if (x >= bounds.x + bounds.width - edge) result |= ui::resize_right;
        if (y < bounds.y + edge) result |= ui::resize_top;
        else if (y >= bounds.y + bounds.height - edge) result |= ui::resize_bottom;
        return result;
    }

    auto hud_runtime::ApplyConfigResize(const rect& original, const uint8_t edges, const float dx, const float dy,
        const D3DVIEWPORT8& viewport) -> void
    {
        const auto layout = this->ConfigLayout();
        const auto before = this->config_bounds_;
        const auto firstOption = this->GroupOptionIndex(static_cast<size_t>(std::max(0, this->option_scroll_)));
        this->config_bounds_ = ui::ResizeConfigWindow(original, edges, dx, dy, layout.MinimumWidth(), layout.MinimumHeight(), viewport);
        if (before.x == this->config_bounds_.x && before.y == this->config_bounds_.y &&
            before.width == this->config_bounds_.width && before.height == this->config_bounds_.height) return;
        if (layout.Columns() != this->ConfigLayout().Columns() && !this->IsXIUIContributors())
        {
            const auto slots = this->GroupOptionSlots();
            const auto found = std::find(slots.begin(), slots.end(), firstOption);
            if (found != slots.end()) this->option_scroll_ = static_cast<int>(found - slots.begin());
        }
        this->ClampOptionScroll();
        if (this->controller_config_.active) this->RefreshControllerConfigFocus();
        this->config_tooltip_.clear();
    }

    auto hud_runtime::ConfigSoundTarget(const D3DVIEWPORT8& viewport) -> uint64_t
    {
        if (!this->visible_ || !this->editing_) return 0;
        if (const auto target = this->SettingsSearchTarget(viewport); target != 0 || this->settings_search_open_) return target;
        this->NormalizeConfigSelection();
        if (this->choice_input_.Opened()) return this->ChoiceSoundTarget(viewport);
        if (const auto target = this->ProfileSoundTarget(viewport)) return target;
        if (this->ProfileSurface(viewport)) return 0;
        this->NormalizeConfigSelection();
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto layout = this->ConfigLayout();
        if (this->active_color_ != nullptr && Contains(layout.ColorEditor(viewport), x, y))
        {
            const auto part = ui::color_picker::HitTest(layout.ColorEditor(viewport), x, y);
            return part == ui::color_picker::part::none ? 0 : 0x10000 + static_cast<uint64_t>(part);
        }
        if (!Contains(this->ConfigVisibleBounds(), x, y)) return this->MovementToolbarTarget(viewport);
        if (Contains(layout.CloseButton(), x, y)) return 1;
        if (Contains(layout.HeaderAction(), x, y)) return 3;
        if (Contains(layout.HeaderAction(1), x, y)) return 4;
        if (Contains(layout.SidebarToggle(), x, y)) return 6;
        if (this->ConfigResizeEdges(x, y) != 0) return 5;
        if (const auto input = this->InputModeControlTarget(viewport); input != 0) return input;
        if (this->config_collapsed_) return 0;
        const size_t pageCount = this->VisibleConfigPages().size();
        for (size_t index = 0; index < pageCount; ++index)
            if (Contains(layout.ModuleTab(index), x, y)) return 0x100 + index;

        const auto page = this->ActiveConfigPage();
        if (this->HasConfigSections())
            for (size_t index = 0; index < page.SectionCount(); ++index)
                if (Contains(layout.Section(index, page.SectionCount()), x, y))
                    return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(page.primary)) << 32) | (0x200 + index);
        if (this->active_module_ == nullptr) return 0;
        const auto key = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->active_module_)) << 32;
        if (this->IsInventorySettings())
            for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, this->inventory_pages_.size()), x, y))
                    return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this->inventory_pages_[0])) << 32) | (0x42000 + index);
        const auto topics = this->active_module_->ConfigSections();
        if (!topics.empty())
        {
            for (size_t index = 0; index < topics.size(); ++index)
                if (Contains(layout.Submenu(index, topics.size()), x, y)) return key | (0x300 + index);
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
                if (Contains(layout.Category(index, categories.count), x, y)) return key | (0x400 + index);
        }
        if (this->IsExperienceDisplay())
            if (const auto target = this->experience_display_.Target(this->experience_, layout, x, y)) return key | (0x20000 + target);
        if (this->IsAttribution())
            for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, std::size(ui::attribution_views)), x, y)) return key | (0x40000 + index);
        if (this->IsGeneralAppearance())
            for (size_t index = 0; index < std::size(general_module::appearance_tabs); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, std::size(general_module::appearance_tabs)), x, y)) return key | (0x44000 + index);
        if (this->IsXIUIContributors())
        {
            const auto member = ui::contributor_list_layout{layout, this->option_scroll_}.MemberAt(x, y);
            return member ? key | (0x1000 + this->general_.ContributorsStart() + *member) : 0;
        }
        if (const auto target = this->ElementSelectorTarget()) return target;
        for (int index = 0; index < layout.VisibleSlots(); ++index)
        {
            if (!Contains(layout.OptionRow(index), x, y)) continue;
            const auto optionIndex = this->GroupOptionIndex(static_cast<size_t>(this->option_scroll_ + index));
            if (optionIndex >= this->active_module_->OptionCount()) return 0;
            const auto option = this->active_module_->Option(optionIndex);
            if (option.available && option.kind == module_option_kind::choice && option.choice_count > 1)
            {
                const auto row = layout.OptionRow(index);
                const uint64_t part = Contains(ui::config_layout::ChoicePrevious(row), x, y) ? 0 :
                    Contains(ui::config_layout::ChoiceNext(row), x, y) ? 2 : 1;
                return key | (0x50000 + optionIndex * 3 + part);
            }
            return option.available ? key | (0x1000 + optionIndex) : 0;
        }
        return 0;
    }

    auto hud_runtime::HudSoundTarget(void) -> uint64_t
    {
        if (!this->visible_) return 0;
        const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        if (this->TreasureInputAvailable())
            if (const auto target = this->treasure_.SoundTarget(x, y)) return 0x500000000ULL | target;
        if (!this->editing_ && this->ContextVisible(this->ready_check_) && !this->chat_.HistoryExpanded() && this->ready_check_.Contains(x, y))
        {
            const auto button = this->ready_check_.ButtonAt(x, y);
            return button != 0 ? 0x100000000ULL | button : 0;
        }
        if (const auto* chat = this->ChatAt(x, y))
        {
            const auto target = chat->SoundTarget(this->design_system_, x, y, this->editing_);
            return target != 0 ? (chat == &this->chat_ ? 0x200000000ULL : 0x300000000ULL) | target : 0;
        }
        if (this->ContextVisible(this->equipment_) && this->equipment_.State().enabled && (this->editing_ || this->equipment_.ShouldRender({})))
            for (size_t index = 0; index < equipment_grid_order.size(); ++index)
                if (Contains(this->equipment_.SlotBounds(index), x, y)) return 0x400000000ULL | (index + 1);
        if (this->editing_ || this->chat_.Focused()) return 0;
        const auto* memory = this->core_ != nullptr ? this->core_->GetMemoryManager() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (target == nullptr || (target->GetIsMenuOpen() != 0 && target->GetActionTargetActive() == 0 && target->GetIsSubTargetActive() == 0)) return 0;
        auto selected = this->ContextVisible(this->enemy_list_) ? this->enemy_list_.HitTarget(x, y, ::GetTickCount64()) : std::pair<uint32_t, uint32_t>{};
        for (const auto* party : {&this->alliance_second_, &this->alliance_first_, &this->party_})
        {
            if (selected.first != 0) break;
            if (this->ContextVisible(*party)) selected = party->HitTarget(x, y);
        }
        return selected.first != 0 ? 0x500000000ULL | selected.first : 0;
    }

    auto hud_runtime::UpdateSoundHover(const D3DVIEWPORT8& viewport, const bool active) -> void
    {
        uint64_t target{};
        this->chat_.SetHovered(false);
        this->chat_second_.SetHovered(false);
        if (active)
        {
            const auto x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
            const bool config = this->editing_ && (this->ProfileSurface(viewport) || this->choice_input_.Opened() || Contains(this->ConfigVisibleBounds(), x, y) ||
                (this->active_color_ != nullptr && Contains(this->ConfigLayout().ColorEditor(viewport), x, y)));
            if (!config)
                if (auto* chat = this->ChatAt(x, y); chat != nullptr) chat->SetHovered(Contains(chat->State().bounds, x, y));
            target = config ? this->ConfigSoundTarget(viewport) : this->HudSoundTarget();
        }
        const bool enabled = active && this->interaction_ == interaction::none && !this->mouse_down_ &&
            !this->ready_check_.PointerDown() && this->chat_pointer_ == nullptr && !this->hud_target_pressed_ &&
            (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0 && (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) == 0;
        this->ui_sounds_.Hover(target, ::GetTickCount64(), enabled);
    }

    auto hud_runtime::ProcessInput(const D3DVIEWPORT8& viewport) -> void
    {
        if (this->core_ == nullptr)
            return;

        auto* properties = this->core_->GetProperties();
        const HWND gameWindow = properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr;
        this->cursor_renderer_.SetNativeWindow(gameWindow);
        if (!this->editing_)
        {
            POINT cursor{};
            const bool active = gameWindow != nullptr && ::GetForegroundWindow() == gameWindow &&
                ::GetCursorPos(&cursor) != FALSE && ::ScreenToClient(gameWindow, &cursor) != FALSE;
            if (active) { this->mouse_x_ = cursor.x; this->mouse_y_ = cursor.y; }
            const bool satchel = active && this->SatchelInputAvailable();
            const bool actionbars = active && this->ActionBarsInputAvailable();
            this->actionbars_.SetHover(actionbars ? static_cast<float>(cursor.x) : -1, actionbars ? static_cast<float>(cursor.y) : -1);
            this->satchel_.SetHover(satchel ? static_cast<float>(cursor.x) : -1, satchel ? static_cast<float>(cursor.y) : -1);
            if (!active || !this->ContextVisible(this->ready_check_)) this->ready_check_.CancelPointer();
            if (this->ContextVisible(this->ready_check_)) this->ready_check_.SetHover(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
            this->SetChatMouseCapture(active && ((satchel && this->satchel_.PointerOwns(static_cast<float>(cursor.x), static_cast<float>(cursor.y))) ||
                (actionbars && this->actionbars_.PointerOwns(static_cast<float>(cursor.x), static_cast<float>(cursor.y))) ||
                this->treasure_.PointerDown() || (this->TreasureInputAvailable() && this->treasure_.Contains(static_cast<float>(cursor.x), static_cast<float>(cursor.y))) ||
                (this->ContextVisible(this->ready_check_) && (this->ready_check_.PointerDown() || this->ready_check_.Contains(static_cast<float>(cursor.x), static_cast<float>(cursor.y)))) ||
                this->ChatResizing() || this->chat_pointer_ != nullptr || this->ChatAt(static_cast<float>(cursor.x), static_cast<float>(cursor.y)) != nullptr));
            this->cursor_renderer_.SetNativeCursorHidden(this->chat_mouse_block_saved_);
            this->UpdateSoundHover(viewport, active);
            return;
        }
        if (gameWindow == nullptr || ::GetForegroundWindow() != gameWindow ||
            (this->mouse_capture_window_ != nullptr && ::GetCapture() != this->mouse_capture_window_))
        {
            this->input_mode_menu_ = false;
            this->FinishConfigInput(false);
            this->FinishInteraction();
            this->mouse_down_ = false;
            this->ReleasePointerCapture();
            this->mouse_captured_ = false;
            this->mouse_buttons_ = 0;
            this->UpdateSoundHover(viewport, false);
            this->cursor_renderer_.SetNativeCursorHidden(false);
            if (this->mouse_ != nullptr && this->mouse_block_state_saved_)
                this->mouse_->SetBlockInput(this->previous_mouse_blocked_);
            return;
        }

        if (gameWindow != nullptr)
        {
            POINT cursor{};
            if (::GetCursorPos(&cursor) != FALSE && ::ScreenToClient(gameWindow, &cursor) != FALSE)
            {
                this->mouse_x_ = cursor.x;
                this->mouse_y_ = cursor.y;
            }
        }

        const bool actionPointer = this->ActionBarsPointerOwns(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->actionbars_.SetHover(actionPointer ? static_cast<float>(this->mouse_x_) : -1, actionPointer ? static_cast<float>(this->mouse_y_) : -1);
        this->UpdateMouseCapture(viewport, this->mouse_buttons_);
        this->cursor_renderer_.SetNativeCursorHidden(this->mouse_captured_);
        if (!this->actionbars_.PointerDown()) this->UpdateInteraction(viewport);
        this->UpdateSoundHover(viewport, !actionPointer);
    }

    auto hud_runtime::ProcessPointerMessage(const uint32_t message, const uint32_t buttons, const D3DVIEWPORT8& viewport) -> bool
    {
        const bool captured = this->UpdateMouseCapture(viewport, buttons);
        if (this->choice_input_.HandleScrollPointer(message, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport))
        {
            if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) this->mouse_down_ = true;
            else if (message == WM_LBUTTONUP) { this->mouse_down_ = false; this->FinishInteraction(); }
            return captured;
        }
        // Window messages own button transitions; frame polling must never create clicks.
        if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && !this->mouse_down_)
        {
            const bool preview = this->choice_input_.Opened() && this->choice_input_.PreviewAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport);
            if (captured && !preview && this->ConfigSoundTarget(viewport) != 0) this->ui_sounds_.Select(::GetTickCount64());
            this->mouse_down_ = true;
            if (this->ProcessSettingsSearchClick(viewport)) return captured;
            if (this->ProcessChoiceClick(viewport)) return captured;
            if (this->slider_input_.Focused() && Contains(this->slider_input_bounds_, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
            { this->slider_input_.SelectAll(); return captured; }
            if (this->option_text_input_.Focused() && Contains(this->text_input_bounds_, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
            { this->option_text_input_.SelectAll(); return captured; }
            this->FinishConfigInput(true, false);
            if (captured && !this->ProcessConfigClick(viewport))
                this->BeginPanelInteraction(viewport);
            else if (!captured)
            {
                this->active_color_ = nullptr;
            }
        }

        this->UpdateInteraction(viewport);

        if (message == WM_LBUTTONUP && this->mouse_down_)
        {
            const bool closeConfig = this->interaction_ == interaction::close_config &&
                Contains(this->ConfigLayout().CloseButton(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->mouse_down_ = false;
            this->FinishInteraction();
            if (closeConfig) this->SetEditing(false);
        }
        return captured;
    }

    auto hud_runtime::FinishInteraction(void) -> void
    {
        const auto completed = this->interaction_;
        if (completed == interaction::color_pick) this->color_picker_.EndDrag();
        if (completed == interaction::drag_module && this->interaction_module_ != nullptr)
            this->interaction_module_->OnPositionChanged();
        this->interaction_ = interaction::none;
        this->interaction_module_ = nullptr;
        this->active_slider_ = nullptr;
        this->layout_snap_ = {};
        this->layout_snap_targets_.clear();
        if (completed != interaction::none && completed != interaction::close_config && completed != interaction::option_scrollbar)
            this->SaveSettings();
    }

    auto hud_runtime::ReleasePointerCapture(void) -> void
    {
        if (this->mouse_capture_window_ == nullptr)
            return;
        const HWND capturedWindow = this->mouse_capture_window_;
        this->mouse_capture_window_ = nullptr;
        if (::GetCapture() == capturedWindow)
            ::ReleaseCapture();
    }

    auto hud_runtime::UpdateMouseCapture(const D3DVIEWPORT8& viewport, const uint32_t buttons) -> bool
    {
        // Keep each button gesture with the UI or game that received its press.
        if (this->mouse_buttons_ == 0)
        {
            const float x = static_cast<float>(this->mouse_x_);
            const float y = static_cast<float>(this->mouse_y_);
            this->mouse_captured_ = this->ActionBarsPointerOwns(x, y) ||
                this->ProfileSurface(viewport) || this->choice_input_.Opened() || this->input_mode_menu_ || this->settings_search_open_ || this->interaction_ != interaction::none || Contains(this->ConfigVisibleBounds(), x, y) ||
                (this->active_color_ != nullptr && Contains(this->ConfigLayout().ColorEditor(viewport), x, y));
            if (const auto toolbar = this->MovementToolbar(viewport); toolbar && Contains(toolbar->bounds, x, y)) this->mouse_captured_ = true;
            for (const auto* module : this->modules_.All())
                this->mouse_captured_ = this->mouse_captured_ || (module->HasEditableBounds() && module->PreviewVisible() && Contains(module->State().EditBounds(), x, y));
        }
        this->mouse_buttons_ = buttons;
        if (this->mouse_ != nullptr && this->mouse_block_state_saved_)
            this->mouse_->SetBlockInput(this->previous_mouse_blocked_ || this->mouse_captured_);
        return this->mouse_captured_;
    }

    auto hud_runtime::ProcessConfigClick(const D3DVIEWPORT8& viewport) -> bool
    {
        if (this->ProcessInputModeClick(viewport)) return true;
        if (this->ProcessProfileClick(viewport)) return true;
        this->NormalizeConfigSelection();
        const float x = static_cast<float>(this->mouse_x_);
        const float y = static_cast<float>(this->mouse_y_);

        if (this->active_color_ != nullptr)
        {
            const auto editor = this->ConfigLayout().ColorEditor(viewport);
            if (Contains(editor, x, y))
            {
                const auto part = ui::color_picker::HitTest(editor, x, y);
                if (part == ui::color_picker::part::close) this->active_color_ = nullptr;
                else if (part == ui::color_picker::part::restore)
                {
                    this->color_picker_.Restore();
                    this->ApplyPickedColor();
                    this->SaveSettings();
                }
                else if (part == ui::color_picker::part::copy)
                {
                    const auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
                    this->color_picker_.CopyHex(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr);
                }
                else if (part == ui::color_picker::part::wheel || part == ui::color_picker::part::value || part == ui::color_picker::part::alpha)
                {
                    this->color_picker_.BeginDrag(part);
                    this->interaction_ = interaction::color_pick;
                }
                this->UpdateInteraction(viewport);
                return true;
            }
            this->active_color_ = nullptr;
        }

        const auto config = this->ConfigVisibleBounds();
        if (!Contains(config, x, y))
        {
            this->active_color_ = nullptr;
            return this->ProcessMovementToolbarClick(viewport);
        }

        if (const auto edges = this->ConfigResizeEdges(x, y); edges != 0)
        {
            this->CloseProfileControls(); this->CloseChoiceInput(); this->input_mode_menu_ = false;
            this->active_color_ = nullptr; this->active_slider_ = nullptr;
            this->interaction_ = interaction::resize_config;
            this->config_resize_edges_ = edges;
            this->config_resize_original_ = this->config_bounds_;
            this->config_resize_pointer_ = {x, y};
            return true;
        }

        if (Contains(this->ConfigLayout().SidebarToggle(), x, y))
        {
            this->ToggleConfigSidebar(viewport);
            return true;
        }

        if (Contains(this->ConfigLayout().HeaderAction(1), x, y))
        {
            this->FinishConfigInput(true, false);
            this->CloseProfileControls(); this->CloseChoiceInput(); this->ReleaseChatInput();
            this->active_color_ = nullptr; this->active_slider_ = nullptr;
            this->config_collapsed_ = !this->config_collapsed_;
            this->ClampLayout(viewport);
            return true;
        }

        if (Contains(this->ConfigLayout().HeaderAction(), x, y))
        {
            this->config_collapsed_ = false;
            this->FinishConfigInput(true, false);
            this->ReleaseChatInput();
            this->active_module_ = &this->general_;
            this->active_config_section_ = general_module::attribution_section;
            this->general_.SelectAttributionView(0);
            this->active_color_ = nullptr; this->active_slider_ = nullptr;
            this->option_scroll_ = 0;
            return true;
        }

        const rect close = this->ConfigLayout().CloseButton();
        if (Contains(close, x, y))
        {
            this->interaction_ = interaction::close_config;
            return true;
        }

        if (this->config_collapsed_) return false;

        if (this->ProcessConfigScrollbarClick()) return true;

        const auto pages = this->VisibleConfigPages();
        for (size_t index = 0; index < pages.size(); ++index)
        {
            const rect tab = this->ConfigLayout().ModuleTab(index);
            if (Contains(tab, x, y))
            {
                this->active_module_ = pages[index].SectionModule(0);
                this->active_module_->SelectConfigView(0);
                this->active_color_ = nullptr;
                this->active_option_group_ = module_option_group::general;
                this->active_config_section_ = 0;
                this->option_scroll_ = 0;
                return true;
            }
        }

        const auto page = this->ActiveConfigPage();
        if (page.SectionCount() > 1)
            for (size_t index = 0; index < page.SectionCount(); ++index)
                if (Contains(this->ConfigLayout().Section(index, page.SectionCount()), x, y))
                {
                    this->SelectConfigModule(page.SectionModule(index), page.SectionView(index));
                    return true;
                }

        if (this->IsInventorySettings())
            for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                if (Contains(ui::config_controls::Segment(this->ConfigLayout().ContentTabs(), index, this->inventory_pages_.size()), x, y))
                {
                    this->SelectConfigModule(this->inventory_pages_[index]);
                    return true;
                }

        const auto topics = this->active_module_ != nullptr ? this->active_module_->ConfigSections() : std::span<const module_config_section>{};
        if (!topics.empty())
        {
            for (size_t index = 0; index < topics.size(); ++index)
                if (Contains(this->ConfigLayout().Submenu(index, topics.size()), x, y))
                {
                    this->SelectConfigTopic(index);
                    return true;
                }
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
            {
                if (Contains(this->ConfigLayout().Category(index, categories.count), x, y))
                {
                    this->active_option_group_ = categories.groups[index];
                    this->active_color_ = nullptr;
                    this->option_scroll_ = 0;
                    return true;
                }
            }
        }

        if (this->IsExperienceDisplay())
        {
            const auto previous = this->experience_display_.View();
            const auto result = this->experience_display_.Click(this->experience_, this->ConfigLayout(), x, y);
            if (result != ui::experience_display_editor::result::none)
            {
                if (previous != this->experience_display_.View()) { this->option_scroll_ = 0; this->active_color_ = nullptr; }
                if (result == ui::experience_display_editor::result::changed) this->SaveSettings();
                return true;
            }
        }

        if (this->IsAttribution())
            for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                if (Contains(ui::config_controls::Segment(this->ConfigLayout().ContentTabs(), index, std::size(ui::attribution_views)), x, y))
                {
                    this->general_.SelectAttributionView(index);
                    this->option_scroll_ = 0; this->active_color_ = nullptr; this->active_slider_ = nullptr;
                    return true;
                }

        if (this->IsGeneralAppearance())
            for (size_t index = 0; index < std::size(general_module::appearance_tabs); ++index)
                if (Contains(ui::config_controls::Segment(this->ConfigLayout().ContentTabs(), index, std::size(general_module::appearance_tabs)), x, y))
                {
                    this->general_.SelectAppearanceTab(index);
                    this->option_scroll_ = 0; this->active_color_ = nullptr; this->active_slider_ = nullptr;
                    return true;
                }

        if (this->IsXIUIContributors())
        {
            if (const auto member = ui::contributor_list_layout{this->ConfigLayout(), this->option_scroll_}.MemberAt(x, y))
                this->general_.OnOptionChanged(this->general_.ContributorsStart() + *member);
            return true;
        }

        if (this->ProcessElementSelectorClick()) return true;

        if (y < config.y + 28.0f)
            return false;

        const int visibleOptions = this->ConfigLayout().VisibleSlots();
        int visibleIndex = -1;
        for (int index = 0; index < visibleOptions; ++index)
        {
            if (Contains(this->ConfigLayout().OptionRow(index), x, y))
            {
                visibleIndex = index;
                break;
            }
        }
        if (visibleIndex < 0)
            return true;

        const size_t optionIndex = this->GroupOptionIndex(static_cast<size_t>(this->option_scroll_ + visibleIndex));
        if (this->active_module_ == nullptr || optionIndex >= this->active_module_->OptionCount())
            return true;
        const module_option option = this->active_module_->Option(optionIndex);
        if (!option.available)
            return true;
        this->active_option_index_ = optionIndex;
        if (option.kind == module_option_kind::toggle && option.toggle != nullptr)
        {
            *option.toggle = !*option.toggle;
            this->active_module_->OnOptionChanged(optionIndex);
        }
        else if (option.kind == module_option_kind::color_override && option.color != nullptr && option.toggle != nullptr)
        {
            const auto row = this->ConfigLayout().OptionRow(visibleIndex);
            if (Contains(ui::config_layout::OverrideSwatch(row), x, y))
            {
                *option.toggle = true;
                this->active_color_ = option.color;
                this->color_picker_.Open(*option.color);
                this->active_module_->OnOptionChanged(optionIndex);
                this->SaveSettings();
                return true;
            }
            *option.toggle = !*option.toggle;
            this->active_module_->OnOptionChanged(optionIndex);
        }
        else if (option.kind == module_option_kind::color && option.color != nullptr)
        {
            this->active_color_ = option.color;
            this->color_picker_.Open(*option.color);
            return true;
        }
        else if (option.kind == module_option_kind::slider && option.slider != nullptr)
        {
            this->active_color_ = nullptr;
            const auto row = this->ConfigLayout().OptionRow(visibleIndex);
            if (Contains(ui::config_layout::SliderValue(row), x, y))
            {
                this->ReleaseChatInput();
                this->slider_input_.Open(option);
                this->slider_input_module_ = this->active_module_;
                this->slider_input_option_index_ = optionIndex;
                this->slider_input_bounds_ = ui::config_layout::SliderValue(row);
                return true;
            }
            this->active_slider_ = option.slider;
            this->active_slider_minimum_ = option.minimum;
            this->active_slider_maximum_ = option.maximum;
            this->active_slider_step_ = option.step;
            this->active_slider_row_ = row;
            this->interaction_ = interaction::option_slider;
            this->SetActiveSliderFromMouse();
            return true;
        }
        else if (option.kind == module_option_kind::dropdown && option.choice != nullptr && option.choice_count > 0)
        {
            this->active_color_ = nullptr;
            this->ReleaseChatInput();
            this->choice_input_.Open(option, ui::config_layout::ChoiceButton(this->ConfigLayout().OptionRow(visibleIndex)));
            this->choice_input_module_ = this->active_module_;
            this->choice_input_option_index_ = optionIndex;
            return true;
        }
        else if (option.kind == module_option_kind::choice && option.choice != nullptr && option.choice_count > 0)
        {
            const int count = static_cast<int>(option.choice_count);
            const int direction = Contains(ui::config_layout::ChoicePrevious(this->ConfigLayout().OptionRow(visibleIndex)), x, y) ? -1 : 1;
            *option.choice = (std::clamp(*option.choice, 0, count - 1) + direction + count) % count;
            this->active_color_ = nullptr;
            this->active_module_->OnOptionChanged(optionIndex);
        }
        else if (option.kind == module_option_kind::text_input && option.text != nullptr)
        {
            this->active_color_ = nullptr;
            this->ReleaseChatInput();
            this->option_text_input_.Open(*option.text, option.text_limit);
            this->text_input_module_ = this->active_module_;
            this->text_input_option_index_ = optionIndex;
            this->text_input_value_ = option.text;
            this->text_input_bounds_ = ui::config_layout::TextValue(this->ConfigLayout().OptionRow(visibleIndex));
            return true;
        }
        else if (option.kind == module_option_kind::action)
            return this->ActivateConfigAction(*this->active_module_, optionIndex);
        else
            return false;

        this->SaveSettings();
        return true;
    }

    auto hud_runtime::FocusModuleConfig(hud_module& module) -> void
    {
        for (size_t i = 0; i < this->actionbar_children_.size(); ++i)
            if (&module == &this->actionbar_children_[i])
            {
                this->FocusModuleConfig(this->actionbars_);
                this->actionbars_.SelectConfigView(i + 1);
                return;
            }
        for (size_t i = 0; i < this->world_parts_.size(); ++i)
            if (&module == &this->world_parts_[i])
            {
                this->FocusModuleConfig(this->world_info_);
                this->world_info_.SelectPart(i);
                this->active_config_section_ = 1;
                return;
            }
        if (&module == &this->notifications_) this->notifications_.SelectGroup(0);
        for (auto& group : this->notification_groups_)
            if (&module == &group)
            {
                this->FocusModuleConfig(this->notifications_);
                this->notifications_.SelectGroup(group.Index());
                this->active_config_section_ = 2;
                return;
            }
        if (&module == &this->target_cast_)
        {
            this->FocusModuleConfig(this->target_);
            this->target_.SelectConfigView(0);
            this->active_config_section_ = 9;
            return;
        }
        for (size_t i = 0; i < this->mob_info_groups_.size(); ++i)
            if (&module == &this->mob_info_groups_[i])
            {
                this->FocusModuleConfig(this->target_);
                this->target_.SelectConfigView(0);
                this->target_.Settings().mob_info.selected_group = static_cast<int>(i + 1);
                this->active_config_section_ = 11;
                return;
            }
        if (!this->editing_ || (this->active_module_ == &module && module.ConfigView() == 0)) return;
        this->FinishConfigInput(false);
        module.SelectConfigView(0);
        this->active_module_ = &module;
        this->active_config_section_ = 0;
        this->active_option_group_ = module_option_group::general;
        this->option_scroll_ = 0;
        this->active_color_ = nullptr;
        this->active_slider_ = nullptr;
    }

    auto hud_runtime::BeginPanelInteraction(const D3DVIEWPORT8& viewport) -> void
    {
        UNREFERENCED_PARAMETER(viewport);
        const float x = static_cast<float>(this->mouse_x_);
        const float y = static_cast<float>(this->mouse_y_);

        const auto tryPanel = [this, x, y](rect& bounds, hud_module* module) -> bool {
            const rect dragArea = module == nullptr ? this->ConfigLayout().HeaderDragArea() : module->State().EditBounds();
            if (Contains(dragArea, x, y))
            {
                if (module != nullptr)
                {
                    this->FocusModuleConfig(*module);
                    this->ui_sounds_.Select(::GetTickCount64());
                }
                this->interaction_module_ = module;
                this->interaction_ = module != nullptr ? interaction::drag_module : interaction::drag_config;
                this->drag_offset_x_ = x - bounds.x;
                this->drag_offset_y_ = y - bounds.y;
                this->drag_origin_ = {bounds.x, bounds.y};
                this->drag_moved_ = false;
                this->layout_snap_ = {};
                return true;
            }
            return false;
        };

        if (tryPanel(this->config_bounds_, nullptr))
            return;
        auto& modules = this->modules_.All();
        for (auto module = modules.rbegin(); module != modules.rend(); ++module)
        {
            if ((*module)->HasEditableBounds() && (*module)->PreviewVisible() && tryPanel((*module)->State().bounds, *module))
                return;
        }
    }

    auto hud_runtime::UpdateInteraction(const D3DVIEWPORT8& viewport) -> void
    {
        if (this->interaction_ == interaction::none)
            return;
        if (this->interaction_module_ != nullptr && !this->interaction_module_->PreviewVisible())
        {
            this->FinishInteraction();
            return;
        }

        if (this->interaction_ == interaction::resize_config)
        {
            if (this->mouse_down_)
                this->ApplyConfigResize(this->config_resize_original_, this->config_resize_edges_,
                    static_cast<float>(this->mouse_x_) - this->config_resize_pointer_.x,
                    static_cast<float>(this->mouse_y_) - this->config_resize_pointer_.y, viewport);
            return;
        }

        if (this->interaction_ == interaction::option_scrollbar)
        {
            if (this->mouse_down_) this->UpdateConfigScrollbar();
            return;
        }

        if (this->interaction_ == interaction::option_slider)
        {
            if (this->mouse_down_)
                this->SetActiveSliderFromMouse();
            return;
        }

        if (this->interaction_ == interaction::color_pick)
        {
            if (this->mouse_down_)
            {
                this->color_picker_.Update(this->ConfigLayout().ColorEditor(viewport), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                this->ApplyPickedColor();
            }
            return;
        }

        rect* bounds{};
        switch (this->interaction_)
        {
        case interaction::drag_module:
            bounds = this->interaction_module_ != nullptr ? &this->interaction_module_->State().bounds : nullptr;
            break;
        case interaction::drag_config:
            bounds = &this->config_bounds_;
            break;
        default:
            return;
        }

        if (!this->mouse_down_ || bounds == nullptr)
            return;

        const float x = static_cast<float>(this->mouse_x_) - this->drag_offset_x_;
        const float y = static_cast<float>(this->mouse_y_) - this->drag_offset_y_;
        if (this->interaction_ == interaction::drag_module)
        {
            this->drag_moved_ = this->drag_moved_ || std::abs(x - this->drag_origin_.x) >= 2 || std::abs(y - this->drag_origin_.y) >= 2;
            if (!this->drag_moved_) return;
        }
        bounds->x = x; bounds->y = y;
        if (this->interaction_ == interaction::drag_module)
            this->SnapModuleLayout(viewport);
        this->ClampLayout(viewport);
    }

    auto hud_runtime::RenderWorld(const hud_snapshot& snapshot) -> void
    {
        if (!this->visible_ || !this->device_) return;
        this->native_ui_=snapshot.native_ui;
        this->UpdateVisibilityContext();
        if (this->ContextVisible(this->target_) && this->target_.State().enabled)
            this->target_rings_.Draw(this->core_,this->device_,snapshot,this->target_.Settings().rings);
    }

    auto hud_runtime::Render(const hud_snapshot& incoming) -> void
    {
        startup_profile::scope timing(this->startup_profile_, "first HUD frame", !this->startup_render_recorded_ && this->device_ != nullptr);
        if (this->device_ != nullptr) this->startup_render_recorded_ = true;
        this->chat_alerts_ready_ = true;
        this->timing_ready_ = true;
        auto snapshot = incoming;
        this->native_ui_ = snapshot.native_ui;
        this->UpdateVisibilityContext();
        const bool settingsFailed = this->settings_writer_.HasFailed() || this->profile_writer_.HasFailed();
        if (settingsFailed && !this->settings_error_reported_ && this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->Write(1, false, "[XIUI] Settings save failed; changes will be retried on the next save.");
        this->settings_error_reported_ = settingsFailed;
        const uint64_t now = ::GetTickCount64();
        this->UpdateCharacterProfile(now);
        if (now >= this->next_chat_palette_update_)
        {
            this->chat_colors_ = this->chat_palette_.Read();
            this->chat_filters_.Refresh();
            const auto timestamps = this->window_config_.TimestampMode();
            this->chat_.SetInputContext(this->core_ != nullptr ? this->core_->GetChatManager() : nullptr);
            this->chat_.SetNativeTimestamps(timestamps);
            this->chat_second_.SetNativeTimestamps(timestamps);
            this->chat_.SetNativeWindowConfig(this->window_config_.ChatWindow());
            this->chat_second_.SetNativeWindowConfig(this->window_config_.ChatWindow(true));
            this->next_chat_palette_update_ = now + 500;
        }
        if (this->chat_colors_.available) snapshot.chat_colors = this->chat_colors_;
        this->notification_source_.Update(this->core_, this->notifications_.Center(), now);
        if (this->hud_target_pressed_ && (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
        { this->hud_target_pressed_ = false; this->hud_target_module_ = nullptr; }
        this->combat_source_.SetRules(this->combat_settings_.Rules());
        this->combat_source_.Update(this->core_, now);
        this->UpdateTreasure(now);
        this->skillchain_source_.SetRules(this->combat_settings_.Rules());
        if (this->magic_burst_.State().enabled || this->actionbars_.SkillchainPredictionsEnabled()) this->skillchain_source_.Update(this->core_, now);
        else this->skillchain_source_.Clear();
        this->actionbars_.SetSkillchains(&this->skillchain_source_);
        this->timing_source_.Update(this->core_, this->combat_source_, now);
        this->player_.SetTiming(&this->timing_source_); this->player_.SetSounds(&this->ui_sounds_);
        this->target_.SetTiming(&this->timing_source_); this->enemy_list_.SetTiming(&this->timing_source_);
        const auto completion = this->timing_source_.Recovery().Completion();
        const auto& timers = this->player_.TimingSettings();
        if (completion > this->recovery_completion_ && this->ContextVisible(this->player_) && this->player_.State().enabled && timers.resting &&
            timers.sound > 0 && timers.sound <= static_cast<int>(std::size(ui::sound_values)))
            this->ui_sounds_.Play(ui::sound_values[timers.sound - 1]);
        this->recovery_completion_ = completion;
        this->enemy_list_.Update(this->core_, now);
        this->experience_.Update(this->core_, now);
        this->party_source_.SetUtc(this->experience_.Utc(), now);
        this->party_source_.Update(this->core_, now);
        this->blue_magic_.Update(now, this->editing_);
        this->phantom_roll_.Update(now, this->editing_);
        this->game_icons_.SetThemes(this->general_.StatusIconTheme(), this->general_.JobIconTheme());
        this->party_timing_source_.Update(this->party_source_.Members(), this->party_source_.Members()[0].id, &this->timing_source_, now);
        for (auto* group : {&this->party_, &this->alliance_first_, &this->alliance_second_}) group->SetTiming(&this->party_timing_source_);
        this->UpdateControllerFocus();
        this->player_cast_source_.Update(this->core_, now);
        this->cast_cost_source_.Update(this->core_, now);
        this->heal_prediction_source_.Update(this->core_, this->party_source_, this->player_cast_source_, this->cast_cost_source_, now);
        this->recast_source_.Update(this->core_, now);
        this->pet_source_.Update(this->core_, this->combat_source_, now);
        this->ready_check_.Update(this->core_, now);
        this->inventory_source_.Update(this->core_, now);
        this->UpdateSatchel(now);
        this->UpdateActionBars(now);
        this->gil_source_.Update(this->core_, now);
        this->equipment_source_.Update(this->core_, this->inventory_source_.State(), now);
        this->target_.Update(this->core_);
        this->target_.UpdateDetails(this->core_, this->combat_source_, this->party_source_, now);
        if (this->experience_.NeedsSave(now)) this->SaveSettings();
        this->world_source_.Update(this->core_, now);
        if (this->minimap_.State().enabled && this->device_ != nullptr && this->ContextVisible(this->minimap_))
            this->minimap_source_.Update(this->core_, now, this->minimap_.NpcsEnabled(), this->minimap_.DistantPartyEnabled() && this->ContextVisible(this->minimap_) && this->GameWindowFocused() && !this->editing_);
        else this->minimap_source_.Clear();
        this->notifications_.Expire(now);
        this->game_hud_.SetAvailable(snapshot.native_hud_available_mask);
        if (const auto hidden = this->general_.TakeNameplatesRequest(); hidden && this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
            this->core_->GetChatManager()->QueueCommand(-1, *hidden ? "/names off" : "/names on");
        if (const auto requested = this->general_.TakeWindowTypeRequest(); requested && !this->window_config_.SetType(*requested))
            if (this->core_ != nullptr && this->core_->GetChatManager() != nullptr)
                this->core_->GetChatManager()->Write(1, false, "[XIUI] The client could not change Window Type.");
        const auto windowType = this->window_config_.Type();
        this->general_.UpdateWindowType(windowType);
        if (windowType) { this->native_ui_.window_type = *windowType; this->native_ui_.window_type_available = true; }
        this->chat_capture_available_ = snapshot.chat_capture_available;
        this->chat_palette_available_ = snapshot.chat_colors.available;
        auto* properties = this->core_ != nullptr ? this->core_->GetProperties() : nullptr;
        this->general_.SetClipboardOwner(properties != nullptr ? properties->GetFinalFantasyHwnd() : nullptr);
        if ((!this->chat_.State().enabled && (this->chat_.Focused() || this->chat_pointer_ != nullptr)) || (this->chat_pointer_ != nullptr && !this->chat_pointer_->WindowVisible()) ||
            (properties != nullptr && ::GetForegroundWindow() != properties->GetFinalFantasyHwnd()) ||
            (this->mouse_capture_window_ != nullptr && ::GetCapture() != this->mouse_capture_window_)) this->ReleaseChatInput(!this->GameWindowFocused());
        if (!this->visible_ || this->device_ == nullptr)
        {
            this->cursor_renderer_.SetNativeCursorHidden(false);
            return;
        }

        if (this->ContextVisible(this->chat_) && this->chat_.WindowVisible() && this->chat_capture_available_ &&
            (this->game_hud_.HiddenMask() & NativeHudBit(native_hud_element::chat1)) != 0 && properties != nullptr &&
            ::GetForegroundWindow() == properties->GetFinalFantasyHwnd())
        {
            const auto* memory = this->core_->GetMemoryManager();
            const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
            const bool targeting = target != nullptr && (target->GetIsSubTargetActive() != 0 || target->GetActionTargetActive() != 0);
            this->SyncNativeChatInput(this->core_->GetChatManager(), this->native_ui_.menu_visible, targeting);
        }

        D3DVIEWPORT8 viewport{};
        if (FAILED(this->device_->GetViewport(&viewport)))
            return;
        this->commands_.Update(snapshot.native_ui, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height));
        if (this->native_ui_.window_type_available) this->container_renderer_.SelectSkin(this->native_ui_.window_type);
        this->design_system_.SetWindowPalette(this->container_renderer_.BackgroundColor());
        this->EnsureLayout(viewport);
        this->chat_.SetStackPeerVisible(this->ContextVisible(this->chat_second_));
        this->chat_second_.SetStackPeerVisible(this->ContextVisible(this->chat_));
        for (auto* chat : {&this->chat_, &this->chat_second_})
        {
            chat->SetLayoutEditing(this->editing_);
            chat->UpdateCompactLayout(this->design_system_, now);
        }
        this->ProcessInput(viewport);
        this->chat_.SetHistoryViewport({static_cast<float>(viewport.X), static_cast<float>(viewport.Y), static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)});
        this->chat_second_.SetHistoryViewport({static_cast<float>(viewport.X), static_cast<float>(viewport.Y), static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)});
        this->design_system_.SetDarkBackingOpacity(this->general_.DarkBackingOpacity());

        if (!this->design_system_.BeginFrame())
        {
            this->cursor_renderer_.SetNativeCursorHidden(false);
            return;
        }
        [[maybe_unused]] const render_frame_guard frameGuard{this->design_system_};
#if defined(XIUI_ENABLE_TEST_FAULTS)
        if (this->raise_after_begin_for_testing_)
            throw std::runtime_error{"injected render failure"};
#endif

        const module_render_context context{this->design_system_, &this->container_renderer_, this->editing_, &this->game_icons_,
            {static_cast<float>(viewport.X), static_cast<float>(viewport.Y), static_cast<float>(viewport.Width), static_cast<float>(viewport.Height)}, &this->native_assets_, &this->general_.HealthFeedback()};
        if (this->editing_) this->DrawLayoutBackground(context.Viewport());
        this->target_.BeginRenderFrame();
        this->notifications_.BeginRenderFrame();
        for (auto* module : this->modules_.All())
        {
            if (!this->editing_ && module == &this->satchel_) continue;
            if (this->ContextVisible(*module) && module->HasEditableBounds() && (this->editing_ ? module->PreviewVisible() : module->ShouldRender(snapshot)))
            {
                this->design_system_.BeginContentBounds();
                module->Render(snapshot, context);
                const rect content = this->design_system_.EndContentBounds();
                auto& state = module->State();
                if (this->editing_)
                    state.content_bounds = {content.x - state.bounds.x, content.y - state.bounds.y, content.width, content.height};
                if (this->editing_)
                    this->design_system_.DrawEditFrame(state.EditBounds());
            }
        }
        if (this->ContextVisible(this->target_) && this->target_.State().enabled) this->target_.RenderWorldBar(snapshot, context);
        const bool floatingActions = this->actionbars_.FloatingWindowOpen() && !this->actionbars_.WindowFocused() && !this->actionbars_.ModalDialogOpen();
        if (floatingActions) this->actionbars_.RenderOverlay(context);
        if (this->editing_)
        {
            this->DrawMovementToolbar(viewport);
            this->DrawConfig(viewport);
            this->DrawColorEditor(viewport);
            this->DrawProfileOverlay(viewport);
            this->DrawChoiceInput(viewport);
            this->DrawInputModeControl(viewport);
            this->DrawSettingsSearch(viewport);
        }
        if (this->ContextVisible(this->equipment_) && this->equipment_.ShouldRender(snapshot) && !this->ProfileSurface(viewport) && properties != nullptr && ::GetForegroundWindow() == properties->GetFinalFantasyHwnd() &&
            (!this->editing_ || (!Contains(this->ConfigVisibleBounds(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)) &&
                (this->active_color_ == nullptr || !Contains(this->ConfigLayout().ColorEditor(viewport), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))))))
            this->equipment_.RenderTooltip(context, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        if (!this->editing_ && this->ContextVisible(this->chat_)) this->chat_.RenderHistory(snapshot, context);
        if (!this->editing_ && this->ContextVisible(this->chat_second_)) this->chat_second_.RenderHistory(snapshot, context);
        if (this->editing_)
        {
            this->DrawControllerConfig(viewport);
            this->DrawConfigTooltip(viewport);
        }
        this->DrawControllerOverlay(viewport, context);
        if (!this->editing_ && this->ContextVisible(this->notifications_) && this->controller_surface_ == controller_surface::none &&
            !this->ControllerNativeBusy() && !this->chat_.HistoryExpanded() && !this->chat_second_.HistoryExpanded() &&
            this->ChatAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)) == nullptr)
            if (const auto detail = this->notifications_.DetailAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                this->config_controls_.Tooltip(this->notifications_.Details(detail->id).c_str(), detail->bounds, viewport);
        this->DrawStatusTooltips(viewport, context);
        if (!this->editing_ && this->ContextVisible(this->satchel_) && this->satchel_.ShouldRender(snapshot)) this->satchel_.Render(snapshot, context);
        if (this->SatchelInputAvailable()) this->satchel_.RenderOverlay(context);
        if (!floatingActions && (this->ActionBarsInputAvailable() || this->actionbars_.FloatingWindowOpen())) this->actionbars_.RenderOverlay(context);
        if (this->editing_ || this->chat_mouse_block_saved_) this->DrawCursor(viewport);
    }

    auto hud_runtime::ConfigPages(void) -> std::array<config_page, config_page_count>
    {
        return {{{"general", "General", &this->general_},
            {"hotbar", "Action Bars", &this->actionbars_, nullptr, nullptr, this->actionbar_members_, 1, "Hotbars", 0, action_bar_count},
            {"casting", "Casting", &this->cast_bar_, &this->cast_cost_, &this->recast_},
            {"chat", "Chat", &this->chat_, &this->chat_second_},
            {"enemylist", this->enemy_list_.DisplayName(), &this->enemy_list_},
            {"experience", this->experience_.DisplayName(), &this->experience_},
            {"inventory", "Inventory", &this->inventory_[0], &this->satchel_, &this->equipment_, this->inventory_config_members_, this->inventory_.size(), "Dots", 1},
            {"minimap", "Minimap", &this->minimap_},
            {"notifications", this->notifications_.DisplayName(), &this->notifications_},
            {"party", "Party", &this->party_, &this->alliance_first_, &this->alliance_second_},
            {"pet", "Pet", &this->pet_, &this->pet_target_, nullptr, this->pet_config_members_},
            {"player", this->player_.DisplayName(), &this->player_},
            {"statuses", "Status Effects", &this->buffs_, &this->debuffs_},
            {"target", this->target_.DisplayName(), &this->target_, &this->target_recipient_, &this->subtarget_},
            {"extras", "Extras", &this->compass_, &this->world_info_, &this->commands_, this->extras_pages_}}};
    }

    auto hud_runtime::ConfigPageVisible(const config_page& page) const -> bool
    {
        if (this->settings_search_revealed_module_ == this->active_module_ && page.Contains(this->settings_search_revealed_module_)) return true;
        if (page.primary == &this->general_) return true;
        if (page.primary == &this->actionbars_) return true;
        if (page.primary == &this->party_) return this->general_.PartyEnabled();
        if (page.primary == &this->target_) return general_module::Enabled(*page.primary) ||
            general_module::Enabled(*page.secondary) || general_module::Enabled(*page.tertiary);
        for (size_t index = 0; index < page.SectionCount(); ++index)
            if (auto* module = page.SectionModule(index); module != nullptr &&
                (module == &this->inventory_[0] ? this->general_.InventoryEnabled() : general_module::Enabled(*module))) return true;
        return false;
    }

    auto hud_runtime::VisibleConfigPages(void) const -> std::vector<config_page>
    {
        std::vector<config_page> visible;
        visible.reserve(config_page_count);
        for (const auto& page : const_cast<hud_runtime*>(this)->ConfigPages())
            if (this->ConfigPageVisible(page)) visible.push_back(page);
        return visible;
    }

    auto hud_runtime::ActiveConfigPage(void) -> config_page
    {
        for (const auto& page : this->ConfigPages())
            if (page.Contains(this->active_module_)) return page;
        return {};
    }

    auto hud_runtime::SaveConfigSelection(const settings_store& store) const -> void
    {
        store.WriteString("session.config", "module", this->active_module_ != nullptr ? this->active_module_->Id() : "general");
        store.WriteInt("session.config", "view", this->active_module_ != nullptr ? static_cast<int>(this->active_module_->ConfigView()) : 0);
        store.WriteInt("session.config", "section", static_cast<int>(this->active_config_section_));
        const auto sections = this->active_module_ != nullptr ? this->active_module_->ConfigSections() : std::span<const module_config_section>{};
        store.WriteString("session.config", "topic_id", this->active_config_section_ < sections.size() ? std::string{sections[this->active_config_section_].Id()} : "");
        store.WriteInt("session.config", "party_presets_general", 1);
        store.WriteInt("session.config", "party_presets_location", 2);
        store.WriteInt("session.config", "actionbar_keybinds_topic", 1);
        store.WriteInt("session.config", "appearance_tab", static_cast<int>(this->general_.AppearanceTab()));
    }

    auto hud_runtime::LoadConfigSelection(const settings_store& store) -> void
    {
        this->active_module_ = &this->general_;
        const auto id = store.ReadString("session.config", "module", "general");
        for (auto* module : this->modules_.All())
            if (id == module->Id()) { this->active_module_ = module; break; }
        const auto views = this->active_module_->ConfigViews();
        const auto view = static_cast<size_t>(std::max(0, store.ReadInt("session.config", "view", 0)));
        this->active_module_->SelectConfigView(views.empty() ? 0 : std::min(view, views.size() - 1));
        auto section = static_cast<size_t>(std::max(0, store.ReadInt("session.config", "section", 0)));
        const auto topic = store.ReadString("session.config", "topic_id");
        if (topic.empty())
        {
            if (store.ReadInt("session.config", "party_presets_general", 0) == 0 &&
                (this->active_module_ == &this->party_ || this->active_module_ == &this->alliance_first_ || this->active_module_ == &this->alliance_second_))
            {
                if (section == 12) { this->active_module_ = &this->party_; section = 0; }
                else if (section == 13) section = 12;
            }
            if (this->active_module_ == &this->general_ && section == 6 && store.ReadInt("session.config", "party_presets_location", 0) < 2)
            { this->active_module_ = &this->party_; section = 0; }
            if (this->active_module_ == &this->actionbars_ && store.ReadInt("session.config", "actionbar_keybinds_topic", 0) == 0)
            {
                const size_t firstMoved = this->actionbars_.ConfigView() == action_bar_count ? 2 : 1;
                if (section >= firstMoved) ++section;
            }
        }
        const auto sections = this->active_module_->ConfigSections();
        if (!topic.empty())
        {
            const auto found = std::find_if(sections.begin(), sections.end(), [&](const auto& item) { return item.Id() == topic; });
            section = found != sections.end() ? static_cast<size_t>(found - sections.begin()) : 0;
        }
        this->active_config_section_ = section < sections.size() ? section : 0;
        this->general_.SelectAppearanceTab(static_cast<size_t>(std::max(0, store.ReadInt("session.config", "appearance_tab", 0))));
        this->NormalizeConfigSelection();
    }

    auto hud_runtime::SelectConfigTopic(const size_t index) -> void
    {
        this->active_config_section_ = index;
        this->active_color_ = nullptr;
        this->option_scroll_ = 0;
        if (this->active_module_ == &this->actionbars_ && this->actionbars_.OpenKeybindsSection(index))
            this->FinishActionBarsLaunch();
    }

    auto hud_runtime::SelectConfigModule(hud_module* module, const std::optional<size_t> view) -> void
    {
        const auto previousTopics = this->active_module_ != nullptr ? this->active_module_->ConfigSections() : std::span<const module_config_section>{};
        const auto previousTopic = this->active_config_section_ < previousTopics.size() ? std::string{previousTopics[this->active_config_section_].Id()} : std::string{};
        if (view && !module->ConfigViews().empty()) module->SelectConfigView(*view);
        this->active_module_ = module;
        const auto nextTopics = module->ConfigSections();
        this->active_config_section_ = 0;
        for (size_t topic = 0; !previousTopic.empty() && topic < nextTopics.size(); ++topic)
            if (previousTopic == nextTopics[topic].Id()) { this->active_config_section_ = topic; break; }
        this->active_color_ = nullptr; this->active_slider_ = nullptr;
        this->active_option_group_ = module_option_group::general;
        this->option_scroll_ = 0;
        if (module == &this->actionbars_ && view && *view > action_bar_count)
            this->FinishActionBarsLaunch();
    }

    auto hud_runtime::DrawConfig(const D3DVIEWPORT8& viewport) -> void
    {
        this->NormalizeConfigSelection();
        this->ClampOptionScroll();
        const auto layout = this->ConfigLayout();
        this->config_tooltip_.clear();
        const auto panel = this->ConfigVisibleBounds();
        const auto& theme = this->design_system_.GetTheme();
        const std::array<ui::container_fill, 4> fills{{
            {layout.HeaderBackground(), 0xD0000000 | ((theme.colors.chrome & 0xFEFEFE) >> 1)},
            {layout.SidebarBackground(), (theme.colors.surface_raised & 0xFFFFFF) | 0x60000000},
            {layout.SubmenuBackground(), (theme.colors.surface_raised & 0xFFFFFF) | 0x30000000},
            {layout.SectionBackground(), (theme.colors.surface_raised & 0xFFFFFF) | 0x30000000}}};
        this->config_controls_.Panel(panel, std::span{fills}.first(this->config_collapsed_ ? 1 : fills.size()));
        auto headerStyle = theme.accent_text;
        headerStyle.scale = 0.82f;
        this->config_controls_.Text("CONFIGURATION", layout.HeaderTitle(), headerStyle);
        const auto sidebarToggle = layout.SidebarToggle();
        const bool sidebarHovered = Contains(sidebarToggle, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(sidebarToggle, this->config_sidebar_collapsed_ ? ui::icon_glyph::arrow_right : ui::icon_glyph::arrow_left, sidebarHovered);
        const rect close = layout.CloseButton();
        const bool closeHovered = Contains(close, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(close, ui::icon_glyph::close, closeHovered);

        const auto attribution = layout.HeaderAction();
        const bool attributionHovered = Contains(attribution, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(attribution, ui::icon_glyph::attribution, attributionHovered ||
            (this->active_module_ == &this->general_ && this->active_config_section_ == general_module::attribution_section));
        const char* tooltip = attributionHovered ? "Attribution" : nullptr;
        rect tooltipAnchor = attribution;
        if (sidebarHovered)
        {
            tooltip = this->config_sidebar_collapsed_ ? "Expand sidebar" : "Collapse sidebar to icons";
            tooltipAnchor = sidebarToggle;
        }
        const auto eye = layout.HeaderAction(1);
        const bool eyeHovered = Contains(eye, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        this->config_controls_.IconButton(eye, this->config_collapsed_ ? ui::icon_glyph::eye_closed : ui::icon_glyph::eye, eyeHovered || this->config_collapsed_);
        if (eyeHovered)
        {
            tooltip = this->config_collapsed_ ? "Expand configuration" : "Collapse to title bar";
            tooltipAnchor = eye;
        }
        if (this->config_collapsed_)
        {
            if (tooltip != nullptr) this->config_controls_.Tooltip(tooltip, tooltipAnchor, viewport);
            return;
        }
        const auto pages = this->VisibleConfigPages();
        for (size_t index = 0; index < pages.size(); ++index)
        {
            const auto& page = pages[index];
            const rect bounds = layout.ModuleTab(index);
            const bool active = page.Contains(this->active_module_);
            const bool hovered = Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->config_controls_.NavigationButton(bounds, page.id, page.name, active, hovered, this->config_sidebar_collapsed_);
            if (this->config_sidebar_collapsed_ && hovered) { tooltip = page.name; tooltipAnchor = bounds; }
        }

        this->DrawProfileControls();

        const auto page = this->ActiveConfigPage();
        if (page.SectionCount() > 1)
        {
            constexpr const char* chatNames[]{"Window 1", "Window 2"};
            for (size_t index = 0; index < page.SectionCount(); ++index)
            {
                const auto bounds = layout.Section(index, page.SectionCount());
                const bool active = page.primary == &this->actionbars_ && this->actionbars_.FloatingWindowOpen() && this->actionbars_.WindowFocused() ?
                    page.SectionView(index) == this->actionbars_.WindowConfigView() : page.SectionContains(index, this->active_module_);
                const bool highlighted = active || Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                auto style = active ? theme.accent_text : theme.caption_text;
                style.scale = 0.72f; style.alignment = ui::text_alignment::center;
                this->config_controls_.Button(bounds, std::strcmp(page.id, "chat") == 0 ? chatNames[index] : page.SectionName(index), style, highlighted);
            }
        }

        static constexpr const char* categoryNames[] = {"GENERAL", "APPEARANCE", "EFFECTS", "COLORS", "TEXT"};
        const auto topics = this->active_module_ != nullptr ? this->active_module_->ConfigSections() : std::span<const module_config_section>{};
        if (!topics.empty())
        {
            this->active_config_section_ = std::min(this->active_config_section_, topics.size() - 1);
            for (size_t index = 0; index < topics.size(); ++index)
            {
                const auto bounds = layout.Submenu(index, topics.size());
                const bool active = this->active_config_section_ == index;
                const bool hovered = Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                this->config_controls_.NavigationItem(bounds, topics[index].name, active, hovered);
            }
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
            {
                const auto group = categories.groups[index];
                const rect bounds = layout.Category(index, categories.count);
                const bool active = this->active_option_group_ == group;
                const bool hovered = Contains(bounds, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                auto style = active ? theme.accent_text : theme.caption_text;
                style.scale = 0.72f;
                style.alignment = ui::text_alignment::center;
                this->config_controls_.Button(bounds, categoryNames[static_cast<size_t>(group)], style, active || hovered);
            }
        }

        if (this->active_module_ != nullptr)
        {
            if (this->IsInventorySettings())
            {
                std::array<const char*, inventory_groups.size()> names{};
                size_t selected{};
                for (size_t index = 0; index < names.size(); ++index)
                {
                    names[index] = this->inventory_pages_[index]->DisplayName();
                    if (this->active_module_ == this->inventory_pages_[index]) selected = index;
                }
                this->config_controls_.Segmented(layout.ContentTabs(), names, selected,
                    static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            }
            if (this->IsExperienceDisplay()) this->experience_display_.Draw(this->experience_, this->config_controls_, this->design_system_,
                layout, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            if (this->IsAttribution()) this->config_controls_.Segmented(layout.ContentTabs(), ui::attribution_views,
                this->general_.AttributionView(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            if (this->IsGeneralAppearance()) this->config_controls_.Segmented(layout.ContentTabs(), general_module::appearance_tabs,
                this->general_.AppearanceTab(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
            this->DrawElementSelector();
            if (this->IsXIUIContributors()) this->DrawContributorList();
            else
            {
                const int visibleOptions = layout.VisibleSlots();
                const auto slots = this->GroupOptionSlots();
                for (int visibleIndex = 0; visibleIndex < visibleOptions; ++visibleIndex)
                {
                    const auto slot = static_cast<size_t>(this->option_scroll_ + visibleIndex);
                    if (slot >= slots.size()) break;
                    const size_t optionIndex = slots[slot];
                    if (optionIndex >= this->active_module_->OptionCount())
                        continue;
                    const module_option option = this->active_module_->Option(optionIndex);
                    const rect optionRow = layout.OptionRow(visibleIndex);
                    const bool hovered = Contains(optionRow, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
                    const bool editingValue = this->slider_input_.Focused() && this->slider_input_module_ == this->active_module_ && this->slider_input_option_index_ == optionIndex;
                    if (editingValue) this->slider_input_bounds_ = ui::config_layout::SliderValue(optionRow);
                    const bool editingText = this->option_text_input_.Focused() && this->text_input_module_ == this->active_module_ && this->text_input_option_index_ == optionIndex;
                    if (editingText) this->text_input_bounds_ = ui::config_layout::TextValue(optionRow);
                    this->config_controls_.Option(optionRow, option, hovered, this->active_color_ == option.color,
                        editingValue ? &this->slider_input_ : nullptr, editingText ? &this->option_text_input_ : nullptr,
                        this->choice_input_.Opened() && this->choice_input_module_ == this->active_module_ && this->choice_input_option_index_ == optionIndex);
                    if (option.available && option.kind == module_option_kind::choice && option.choice_count > 1 && hovered &&
                        Contains(ui::config_layout::ChoiceButton(optionRow), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                    {
                        tooltip = Contains(ui::config_layout::ChoicePrevious(optionRow), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))
                            ? "Previous choice" : "Next choice";
                        tooltipAnchor = ui::config_layout::ChoiceButton(optionRow);
                    }
                    if (option.available && option.kind == module_option_kind::slider && hovered &&
                        Contains(ui::config_layout::SliderValue(optionRow), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                    {
                        tooltip = editingValue && this->slider_input_.Invalid() ? "Enter a valid number, or press Escape to cancel." : "Click to type a value. Enter applies; Escape cancels.";
                        tooltipAnchor = ui::config_layout::SliderValue(optionRow);
                    }
                    if (hovered && Contains(ui::config_layout::Description(optionRow), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
                    {
                        tooltip = option.description;
                        tooltipAnchor = optionRow;
                    }
                }

                const int totalOptions = static_cast<int>(slots.size());
                if (this->IsGeneralAppearance() && this->general_.AppearanceTab() == 1)
                {
                    const auto& sample = this->general_.GlobalTextStyle();
                    auto area = layout.ContentSubtabs();
                    area.height = 48;
                    this->design_system_.DrawText("Aa Bb Cc 0123456789 - XIUI", area, sample);
                }
                else if (this->IsGeneralAppearance() && this->general_.AppearanceTab() == 3)
                {
                    const auto area = layout.ContentSubtabs();
                    const rect sample{area.x + 16, area.y + 10, std::min(220.0f, area.width - 32), 28};
                    this->config_controls_.Button(sample, "Controller focus", theme.body_text, false);
                    this->design_system_.DrawFocusFrame(sample, this->general_.FocusStyle());
                }
                if (totalOptions > visibleOptions)
                {
                    char scrollText[32]{};
                    std::snprintf(scrollText, sizeof(scrollText), "%d-%d / %d", this->option_scroll_ + 1, std::min(totalOptions, this->option_scroll_ + visibleOptions), totalOptions);
                    auto scrollStyle = theme.caption_text;
                    scrollStyle.scale = 0.68f;
                    scrollStyle.alignment = ui::text_alignment::center;
                    this->config_controls_.Text(scrollText, layout.Footer(), scrollStyle);
                }
            }
        }

        this->DrawConfigScrollbar();
        this->DrawControllerInputStatus();
        const auto grip = layout.ResizeGrip();
        const bool resizeHovered = Contains(grip, static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        const auto gripColor = resizeHovered || this->interaction_ == interaction::resize_config || this->controller_config_.resizing
            ? theme.colors.control_selected : theme.colors.muted;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column <= row; ++column)
                this->FillRect({grip.x + grip.width - 5 - column * 4, grip.y + grip.height - 5 - (row - column) * 4, 2, 2}, gripColor);
        if (resizeHovered)
        {
            tooltip = "Drag an edge or corner to resize. Alt+F8: resize with arrow keys.";
            tooltipAnchor = grip;
        }
        if (tooltip != nullptr && this->interaction_ == interaction::none)
        {
            this->config_tooltip_ = tooltip;
            this->config_tooltip_anchor_ = tooltipAnchor;
        }
    }

    auto hud_runtime::DrawContributorList(void) -> void
    {
        this->ClampOptionScroll();
        const ui::contributor_list_layout layout{this->ConfigLayout(), this->option_scroll_};
        const auto& theme = this->design_system_.GetTheme();
        auto heading = theme.title_text;
        heading.top_color = heading.bottom_color = theme.colors.gold;
        heading.scale = 0.9f;
        heading.alignment = ui::text_alignment::left;
        for (size_t group = 0; group < std::size(ui::contributor_groups); ++group)
        {
            const auto header = layout.Header(group);
            if (layout.Visible(header)) this->config_controls_.Text(ui::contributor_groups[group], header, heading);
        }
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const auto hovered = layout.MemberAt(x, y);
        for (size_t index = 0; index < ui::xiui_contributors.size(); ++index)
        {
            const auto row = layout.Member(index);
            if (!layout.Visible(row)) continue;
            const auto option = this->general_.Option(this->general_.ContributorsStart() + index);
            this->config_controls_.Option(row, option, hovered && *hovered == index, false);
            if (Contains(ui::config_layout::Description(row), x, y))
            {
                this->config_tooltip_ = option.description;
                this->config_tooltip_anchor_ = row;
            }
        }
        if (layout.MaximumScroll() > 0)
        {
            auto style = theme.caption_text;
            style.scale = 0.68f; style.alignment = ui::text_alignment::center;
            this->config_controls_.Text("Scroll for more contributors", this->ConfigLayout().Footer(), style);
        }
    }

    auto hud_runtime::DrawConfigTooltip(const D3DVIEWPORT8& viewport) -> void
    {
        if (this->settings_search_open_) return;
        if (!this->config_tooltip_.empty() && this->interaction_ == interaction::none)
            this->config_controls_.Tooltip(this->config_tooltip_.c_str(), this->config_tooltip_anchor_, viewport);
    }

    auto hud_runtime::DrawColorEditor(const D3DVIEWPORT8& viewport) -> void
    {
        if (this->active_color_ == nullptr) return;
        const auto panel = this->ConfigLayout().ColorEditor(viewport);
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        this->color_picker_.Draw(this->design_system_, this->config_controls_, panel, x, y);
        if (!Contains(panel, x, y)) return;
        this->config_tooltip_.clear();
        const auto part = ui::color_picker::HitTest(panel, x, y);
        const ui::color_picker_layout layout{panel};
        if (part == ui::color_picker::part::restore)
        {
            this->config_tooltip_ = "Restore the color from when this picker opened.";
            this->config_tooltip_anchor_ = layout.Original();
        }
        else if (part == ui::color_picker::part::copy)
        {
            this->config_tooltip_ = "Copy this hex color. Transparent colors include the alpha value.";
            this->config_tooltip_anchor_ = layout.Hex();
        }
    }

    auto hud_runtime::DrawCursor(const D3DVIEWPORT8& viewport) -> void
    {
        if (!this->mouse_captured_ && !this->ChatResizing() && !this->chat_mouse_block_saved_)
            return;
        const float x = static_cast<float>(this->mouse_x_), y = static_cast<float>(this->mouse_y_);
        const bool actionPointer = this->ActionBarsPointerOwns(x, y);
        const auto actionEdges = actionPointer && !this->actionbars_.ModalDialogOpen() ? this->actionbars_.WindowResizeEdgesAt(x, y) : uint8_t{};
        const bool chatResize = !actionPointer && (this->ChatResizing() || (this->interaction_ == interaction::none &&
            !Contains(this->ConfigVisibleBounds(), static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)) &&
            this->ChatResizeAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_))));
        const auto edges = actionPointer ? actionEdges : this->editing_ ? this->interaction_ == interaction::resize_config ? this->config_resize_edges_ :
            this->ConfigResizeEdges(x, y) : uint8_t{};
        const bool mirror = chatResize || edges == (ui::resize_left | ui::resize_bottom) || edges == (ui::resize_right | ui::resize_top);
        this->cursor_renderer_.Draw(
            this->design_system_,
            actionPointer ? this->actionbars_.PointerCursor(x, y) : chatResize ? cursor_shape::resize_diagonal : this->editing_ ? this->ResolveCursorShape(viewport) : cursor_shape::arrow,
            x, y, mirror);
    }

    auto hud_runtime::FillRect(const rect& bounds, const D3DCOLOR color) const -> void
    {
        this->design_system_.FillRect(bounds, color);
    }

    auto hud_runtime::GradientRect(const rect& bounds, const D3DCOLOR leftColor, const D3DCOLOR rightColor) const -> void
    {
        this->design_system_.GradientRect(bounds, leftColor, rightColor);
    }

    auto hud_runtime::BorderRect(const rect& bounds, const D3DCOLOR color, const float thickness) const -> void
    {
        this->design_system_.BorderRect(bounds, color, thickness);
    }

    auto hud_runtime::ResolveCursorShape(const D3DVIEWPORT8& viewport) const -> cursor_shape
    {
        if (this->ActionBarsPointerOwns(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_)))
            return this->actionbars_.PointerCursor(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_));
        if (this->SettingsSearchTarget(viewport) != 0) return cursor_shape::hand;
        if (this->settings_search_open_) return cursor_shape::arrow;
        if (this->InputModeControlTarget(viewport) != 0) return cursor_shape::hand;
        if (this->choice_input_.Opened())
            return this->choice_input_.IndexAt(static_cast<float>(this->mouse_x_), static_cast<float>(this->mouse_y_), viewport) ? cursor_shape::hand : cursor_shape::arrow;
        if (this->profile_name_.Focused())
        {
            return this->ProfileSoundTarget(viewport) != 0 ? cursor_shape::hand : cursor_shape::arrow;
        }
        if (this->ProfileSoundTarget(viewport) != 0) return cursor_shape::hand;
        if (this->profile_dropdown_) return cursor_shape::arrow;
        switch (this->interaction_)
        {
        case interaction::drag_module:
        case interaction::drag_config:
            return cursor_shape::move;
        case interaction::resize_config:
            return (this->config_resize_edges_ & (ui::resize_left | ui::resize_right)) != 0
                ? ((this->config_resize_edges_ & (ui::resize_top | ui::resize_bottom)) != 0 ? cursor_shape::resize_diagonal : cursor_shape::resize_horizontal)
                : cursor_shape::resize_vertical;
        case interaction::color_pick:
        case interaction::module_toolbar:
        case interaction::option_scrollbar:
            return cursor_shape::hand;
        case interaction::option_slider:
            return cursor_shape::resize_horizontal;
        default:
            break;
        }

        const float x = static_cast<float>(this->mouse_x_);
        const float y = static_cast<float>(this->mouse_y_);
        if (const auto edges = this->ConfigResizeEdges(x, y); edges != 0)
            return (edges & (ui::resize_left | ui::resize_right)) != 0
                ? ((edges & (ui::resize_top | ui::resize_bottom)) != 0 ? cursor_shape::resize_diagonal : cursor_shape::resize_horizontal)
                : cursor_shape::resize_vertical;
        const auto previewCursor = [&] {
            if (const auto toolbar = this->MovementToolbar(viewport); toolbar && Contains(toolbar->bounds, x, y))
                return this->MovementToolbarTarget(viewport) != 0 ? cursor_shape::hand : cursor_shape::arrow;
            const auto& modules = this->modules_.All();
            for (auto module = modules.rbegin(); module != modules.rend(); ++module)
                if ((*module)->HasEditableBounds() && (*module)->PreviewVisible() && Contains((*module)->State().EditBounds(), x, y))
                    return cursor_shape::move;
            return cursor_shape::arrow;
        };
        const auto layout = this->ConfigLayout();
        if (this->config_collapsed_)
        {
            if (Contains(layout.CloseButton(), x, y) || Contains(layout.HeaderAction(), x, y) || Contains(layout.HeaderAction(1), x, y) || Contains(layout.SidebarToggle(), x, y)) return cursor_shape::hand;
            if (Contains(layout.HeaderDragArea(), x, y)) return cursor_shape::move;
            if (Contains(this->ConfigVisibleBounds(), x, y)) return cursor_shape::arrow;
            return previewCursor();
        }
        if (this->active_color_ != nullptr)
        {
            const auto editor = layout.ColorEditor(viewport);
            if (Contains(editor, x, y))
                return ui::color_picker::HitTest(editor, x, y) == ui::color_picker::part::none ? cursor_shape::arrow : cursor_shape::hand;
        }

        const auto scrollbar = this->ConfigScrollbar();
        if (scrollbar.Visible() && Contains(scrollbar.track, x, y)) return cursor_shape::hand;
        const rect close = layout.CloseButton();
        if (Contains(close, x, y) || Contains(layout.HeaderAction(), x, y) || Contains(layout.HeaderAction(1), x, y) || Contains(layout.SidebarToggle(), x, y))
            return cursor_shape::hand;

        const size_t pageCount = this->VisibleConfigPages().size();
        for (size_t index = 0; index < pageCount; ++index)
        {
            if (Contains(layout.ModuleTab(index), x, y))
                return cursor_shape::hand;
        }
        if (this->HasConfigSections())
        {
            const size_t count = const_cast<hud_runtime*>(this)->ActiveConfigPage().SectionCount();
            for (size_t index = 0; index < count; ++index)
                if (Contains(layout.Section(index, count), x, y)) return cursor_shape::hand;
        }
        if (this->HasConfigSubmenu())
        {
            const auto topics = this->active_module_->ConfigSections();
            for (size_t index = 0; index < topics.size(); ++index)
                if (Contains(layout.Submenu(index, topics.size()), x, y)) return cursor_shape::hand;
        }
        else
        {
            const auto categories = this->ConfigCategories();
            for (size_t index = 0; index < categories.count; ++index)
                if (Contains(layout.Category(index, categories.count), x, y)) return cursor_shape::hand;
        }

        if (this->IsInventorySettings())
            for (size_t index = 0; index < this->inventory_pages_.size(); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, this->inventory_pages_.size()), x, y)) return cursor_shape::hand;
        if (this->IsExperienceDisplay() && this->experience_display_.Target(this->experience_, layout, x, y) != 0) return cursor_shape::hand;
        if (this->IsAttribution())
            for (size_t index = 0; index < std::size(ui::attribution_views); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, std::size(ui::attribution_views)), x, y)) return cursor_shape::hand;
        if (this->IsGeneralAppearance())
            for (size_t index = 0; index < std::size(general_module::appearance_tabs); ++index)
                if (Contains(ui::config_controls::Segment(layout.ContentTabs(), index, std::size(general_module::appearance_tabs)), x, y)) return cursor_shape::hand;
        if (this->IsXIUIContributors())
            if (ui::contributor_list_layout{layout, this->option_scroll_}.MemberAt(x, y)) return cursor_shape::hand;

        if (const auto selector = this->ElementSelector(); selector.Enabled() && this->ElementSelectorLayout(selector, layout).IndexAt(x, y))
            return cursor_shape::hand;

        if (this->active_module_ != nullptr && !this->IsXIUIContributors())
        {
            const int visibleOptions = layout.VisibleSlots();
            const auto slots = this->GroupOptionSlots();
            for (int visibleIndex = 0; visibleIndex < visibleOptions; ++visibleIndex)
            {
                const auto slot = static_cast<size_t>(this->option_scroll_ + visibleIndex);
                if (slot >= slots.size()) break;
                const size_t optionIndex = slots[slot];
                if (optionIndex >= this->active_module_->OptionCount())
                    continue;
                const module_option option = this->active_module_->Option(optionIndex);
                const auto row = layout.OptionRow(visibleIndex);
                if (Contains(row, x, y))
                    return !option.available ? cursor_shape::arrow : (option.kind == module_option_kind::slider && !Contains(ui::config_layout::SliderValue(row), x, y) ? cursor_shape::resize_horizontal : cursor_shape::hand);
            }
        }

        if (Contains(layout.HeaderDragArea(), x, y))
            return cursor_shape::move;
        if (Contains(this->ConfigVisibleBounds(), x, y))
            return cursor_shape::arrow;

        return previewCursor();
    }

    auto hud_runtime::ApplyPickedColor(void) -> void
    {
        if (this->active_color_ == nullptr || *this->active_color_ == this->color_picker_.Color()) return;
        *this->active_color_ = this->color_picker_.Color();
        if (this->active_module_ != nullptr)
            this->active_module_->OnOptionChanged(this->active_option_index_);
    }

    auto hud_runtime::SetActiveSliderFromMouse(void) -> void
    {
        if (this->active_slider_ == nullptr)
            return;
        const rect track = ui::config_layout::SliderTrack(this->active_slider_row_);
        const float ratio = std::clamp((static_cast<float>(this->mouse_x_) - track.x) / std::max(1.0f, track.width), 0.0f, 1.0f);
        float value = this->active_slider_minimum_ + (this->active_slider_maximum_ - this->active_slider_minimum_) * ratio;
        *this->active_slider_ = ui::slider_value_input::Normalize(value, this->active_slider_minimum_, this->active_slider_maximum_, this->active_slider_step_);
        if (this->active_module_ != nullptr)
            this->active_module_->OnOptionChanged(this->active_option_index_);
    }

    auto hud_runtime::ConfigCategories(void) const -> config_categories
    {
        config_categories result{};
        if (this->active_module_ == nullptr || this->HasConfigSubmenu()) return result;
        std::array<bool, 5> populated{};
        for (size_t index = 0; index < this->active_module_->OptionCount(); ++index)
        {
            const auto option = this->active_module_->Option(index);
            if (!option.visible) continue;
            const auto group = static_cast<size_t>(option.group);
            if (group < populated.size()) populated[group] = true;
        }
        for (size_t group = 0; group < populated.size(); ++group)
            if (populated[group]) result.groups[result.count++] = static_cast<module_option_group>(group);
        return result;
    }

    auto hud_runtime::NormalizeConfigSelection(void) -> void
    {
        if ((this->active_color_ != nullptr || this->active_slider_ != nullptr) && this->active_module_ != nullptr && this->active_option_index_ < this->active_module_->OptionCount() &&
            !this->active_module_->Option(this->active_option_index_).visible)
        {
            this->active_color_ = nullptr;
            this->color_picker_.EndDrag();
            this->active_slider_ = nullptr;
            if (this->interaction_ == interaction::option_slider || this->interaction_ == interaction::color_pick) this->FinishInteraction();
        }
        if (!this->ConfigPageVisible(this->ActiveConfigPage()))
        {
            this->FinishConfigInput(false);
            this->active_module_ = &this->general_;
            this->active_config_section_ = general_module::enabled_modules_section;
            this->active_option_group_ = module_option_group::general;
            this->active_color_ = nullptr;
            this->active_slider_ = nullptr;
            this->option_scroll_ = 0;
        }
        if (this->choice_input_.Opened() && (this->active_module_ == nullptr || this->choice_input_module_ != this->active_module_ ||
            this->choice_input_option_index_ >= this->active_module_->OptionCount() || !this->MatchesConfigOption(this->choice_input_option_index_) ||
            !this->active_module_->Option(this->choice_input_option_index_).available ||
            this->active_module_->Option(this->choice_input_option_index_).choice != this->choice_input_.Option().choice)) this->CloseChoiceInput();
        if (this->option_text_input_.Focused() && (this->active_module_ == nullptr || this->text_input_module_ != this->active_module_ ||
            this->text_input_option_index_ >= this->active_module_->OptionCount() || !this->MatchesConfigOption(this->text_input_option_index_) ||
            !this->active_module_->Option(this->text_input_option_index_).available ||
            this->active_module_->Option(this->text_input_option_index_).text != this->text_input_value_)) this->FinishConfigInput(false);
        if (this->slider_input_.Focused() && (this->active_module_ == nullptr || this->slider_input_module_ != this->active_module_ ||
            this->slider_input_option_index_ >= this->active_module_->OptionCount() ||
            !this->MatchesConfigOption(this->slider_input_option_index_) ||
            !this->active_module_->Option(this->slider_input_option_index_).available ||
            this->active_module_->Option(this->slider_input_option_index_).slider != this->slider_input_.Option().slider)) this->FinishConfigInput(false);
        if (this->HasConfigSubmenu())
        {
            if (this->active_config_section_ < this->active_module_->ConfigSections().size()) return;
            this->active_config_section_ = 0;
        }
        else
        {
            const auto categories = this->ConfigCategories();
            if (categories.count == 0 || std::find(categories.groups.begin(), categories.groups.begin() + categories.count, this->active_option_group_) != categories.groups.begin() + categories.count)
                return;
            this->active_option_group_ = categories.groups[0];
        }
        this->active_color_ = nullptr;
        this->active_slider_ = nullptr;
        this->option_scroll_ = 0;
    }

    auto hud_runtime::MatchesConfigOption(const size_t index) const -> bool
    {
        if (this->active_module_ == nullptr || index >= this->active_module_->OptionCount() || !this->active_module_->Option(index).visible) return false;
        if (this->IsExperienceDisplay()) return this->experience_.DisplayOption(this->experience_display_.View(), index);
        if (this->IsAttribution()) return this->general_.AttributionOption(index);
        if (this->IsGeneralAppearance()) return this->general_.AppearanceOption(index);
        return this->HasConfigSubmenu() ? this->active_module_->ConfigSectionContains(this->active_config_section_, index) :
            this->active_module_->Option(index).group == this->active_option_group_;
    }

    auto hud_runtime::GroupOptionSlots(void) const -> std::vector<size_t>
    {
        if (this->active_module_ == nullptr) return {};
        std::vector<ui::config_option_entry> entries;
        for (size_t index = 0; index < this->active_module_->OptionCount(); ++index)
        {
            if (!this->MatchesConfigOption(index)) continue;
            const auto option = this->active_module_->Option(index);
            if (option.selector == module_selector::none) entries.push_back({index, option.display_order, option.pair_next});
        }
        std::vector<ui::config_option_entry> cards;
        std::erase_if(entries, [&](const auto& entry) {
            if (!this->active_module_->Option(entry.index).card) return false;
            cards.push_back(entry); return true;
        });
        const auto empty = this->active_module_->OptionCount();
        const int columns = ui::config_layout{this->config_bounds_, 0, this->HasConfigSections(), this->HasConfigSubmenu(), 0, 0, false, -1, 0, this->config_sidebar_collapsed_}.Columns();
        auto slots = ui::ArrangeConfigOptions(std::move(entries), empty, columns);
        if (!cards.empty() && slots.size() % columns != 0) slots.push_back(empty);
        for (const auto& card : cards) slots.push_back(card.index);
        return slots;
    }

    auto hud_runtime::GroupOptionCount(void) const -> size_t
    {
        return this->GroupOptionSlots().size();
    }

    auto hud_runtime::GroupOptionIndex(const size_t filteredIndex) const -> size_t
    {
        if (this->active_module_ == nullptr)
            return 0;
        const auto slots = this->GroupOptionSlots();
        return filteredIndex < slots.size() ? slots[filteredIndex] : this->active_module_->OptionCount();
    }

    auto hud_runtime::ClampOptionScroll(void) -> void
    {
        if (this->IsXIUIContributors())
        {
            this->option_scroll_ = std::clamp(this->option_scroll_, 0, ui::contributor_list_layout{this->ConfigLayout()}.MaximumScroll());
            return;
        }
        this->option_scroll_ = this->ConfigLayout().AlignScroll(this->option_scroll_, static_cast<int>(this->GroupOptionCount()));
    }

    auto hud_runtime::Contains(const rect& bounds, const float x, const float y) -> bool
    {
        return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
    }

} // namespace xiui
