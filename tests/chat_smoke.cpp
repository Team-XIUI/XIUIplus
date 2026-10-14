#include "chat/chat_editor.hpp"
#include "chat/chat_encoding.hpp"
#include "chat/chat_model.hpp"
#include "chat/chat_token.hpp"
#include "chat/chat_text.hpp"
#include "resources/auto_translate.hpp"
#include "modules/chat/chat_module.hpp"
#include "hud_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <fstream>

namespace
{
    auto Check(bool condition, const char* message) -> void { if (!condition) throw std::runtime_error{message}; }
    class chat_manager final : public IChatManager
    {
    public:
        std::string sent; int32_t mode{}; size_t count{};
        mutable size_t input_updates{};
        mutable std::array<char, 512> native{};
        auto OpenNative(const char* text, const uint8_t state = 4) -> void
        {
            this->native[0] = static_cast<char>(state);
            strncpy_s(this->native.data() + 16, this->native.size() - 16, text, _TRUNCATE);
        }
        void ParseCommand(int32_t, const char*) override {}
        void QueueCommand(int32_t value, const char* text) override { this->mode = value; this->sent = text; ++this->count; }
        void Write(int32_t, bool, const char*) override {}
        void Writef(int32_t, bool, const char*, ...) override {}
        void AddChatMessage(int32_t, bool, const char*) override {}
        int32_t ParseAutoTranslate(const char* text, char* buffer, int32_t size, bool) const override { strncpy_s(buffer, size, text, _TRUNCATE); return static_cast<int32_t>(strlen(buffer)); }
        void ExecuteScript(const char*, const char*, bool) override {}
        void ExecuteScriptString(const char*, const char*, bool) override {}
        const char* GetInputTextRaw() const override { return this->native.data() + 16; }
        void SetInputTextRaw(const char*, uint32_t) const override {}
        uint32_t GetInputTextRawLength() const override { return static_cast<uint32_t>(std::strlen(this->GetInputTextRaw())); }
        uint32_t GetInputTextRawCaretPosition() const override { return this->GetInputTextRawLength(); }
        const char* GetInputTextParsed() const override { return ""; }
        void SetInputTextParsed(const char*) const override {}
        uint32_t GetInputTextParsedLength() const override { return 0; }
        uint32_t GetInputTextParsedLengthMax() const override { return 255; }
        const char* GetInputTextDisplay() const override { return ""; }
        void SetInputTextDisplay(const char*) const override {}
        void SetInputText(const char* text) const override { ++this->input_updates; strncpy_s(this->native.data() + 16, this->native.size() - 16, text, _TRUNCATE); }
        uint8_t IsInputOpen() const override { return this->native[0] == 4 ? 0x11 : this->native[0] == 5 ? 0x21 : 0; }
        bool GetSilentAliases() const override { return false; }
        void SetSilentAliases(bool) override {}
    };
}

namespace xiui
{
    struct chat_test
    {
        static auto ExpandedHistory(void) -> void
        {
            hud_runtime runtime; auto& chat = runtime.chat_;
            chat.ApplyTextDefaults(ui::theme::CreateDefault());
            chat.SetHistoryViewport({0, 0, 1600, 900});
            const auto initial = chat.State().bounds;
            chat.editor_.Set("unsent draft");
            const auto key = [&](const WPARAM value, const LPARAM flags = 0) { return runtime.HandleChatKey(value, flags, false, true, false, 0); };
            Check(!runtime.HandleChatKey('F', 0, true, true, false, 0) && !runtime.HandleChatKey('F', 0, false, false, false, 0) &&
                !chat.HistoryFocused(), "F respects existing input owners and native chat when replacement is off");
            Check(key('F') && chat.HistoryFocused() && !chat.HistoryExpanded() && !chat.Focused() && runtime.KeyboardFocused(),
                "F focuses the log and captures game keys without opening a message editor");
            Check(key(VK_RETURN) && chat.HistoryExpanded() && chat.editor_.Text() == "unsent draft", "Enter expands history without submitting or changing a draft");
            const auto layout = chat.HistoryLayout(runtime.design_system_);
            Check(layout.frame.width > 1500 && layout.frame.height > 800 && layout.log.y < layout.footer.y && layout.tabs.size() == 10,
                "expanded history fills the viewport with all requested channel tabs above the frame");
            for (const auto& tab : layout.tabs)
                Check(tab.x >= 0 && tab.x + tab.width <= 1600 && tab.y + tab.height < layout.frame.y,
                    "all channel tab hit areas fit above the expanded log");
            constexpr uint32_t modes[]{20, 9, 12, 13, 14, 214, 222, 220, 212, 10};
            for (size_t tab = 0; tab < std::size(chat_history_tabs); ++tab)
            {
                Check(ChatHistoryMatches(tab, modes[tab], 1) && !ChatHistoryMatches(tab, modes[tab], 0), "each history tab matches its admitted native channel");
                if (tab > 0) Check(!ChatHistoryMatches(tab, modes[tab % 9 + 1], 1), "channel histories remain separate");
                Check(key('F') && chat.history_view_.tab == (tab + 1) % std::size(chat_history_tabs), "F cycles the complete tab strip and wraps");
            }
            chat.SetHistoryViewport({0, 0, 420, 520});
            const auto narrow = chat.HistoryLayout(runtime.design_system_);
            Check(narrow.tabs.back().y > narrow.tabs.front().y && narrow.frame.y > narrow.tabs.back().y,
                "small viewports wrap every channel tab without hiding or overlapping it");
            chat.SetHistoryViewport({0, 0, 800, 600});
            for (uint64_t sequence = 1; sequence <= 100; ++sequence)
            {
                chat_event event{}; event.sequence = sequence; event.timestamp = ::GetTickCount64(); event.windows = 1; event.mode = 9;
                const std::string text = "message " + std::to_string(sequence) + " " + std::string(150, 'A');
                strcpy_s(event.text, text.c_str()); chat.Receive(nullptr, event, false);
            }
            key(VK_UP);
            const size_t oneMessage = chat.CurrentScroll();
            Check(oneMessage > 1 && chat.rows_[chat.rows_.size() - oneMessage - 1].sequence == 99,
                "Up scrolls a whole wrapped message rather than one visual row");
            key(VK_DOWN);
            Check(chat.CurrentScroll() == 0, "Down returns one message toward the newest entry");
            key(VK_LEFT);
            Check(chat.CurrentScroll() == chat.visible_rows_ - 1, "Left pages by the visible history height with one row of context");
            key(VK_RIGHT);
            Check(chat.CurrentScroll() == 0, "Right pages back toward the newest messages");
            key(VK_END);
            bool save = false;
            const auto full = chat.HistoryLayout(runtime.design_system_);
            Check(chat.HandleMouse(WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), full.log.x + 10, full.log.y + 10, false,
                runtime.design_system_, chat.history_view_.viewport, save) && chat.CurrentScroll() == 3 && !save,
                "expanded history retains normal mouse-wheel scrolling without changing window preferences");
            const auto say = full.tabs[1];
            Check(chat.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, say.x + 2, say.y + 2, false, runtime.design_system_, chat.history_view_.viewport, save) &&
                chat.history_view_.tab == 1 && chat.HandleMouse(WM_LBUTTONUP, 0, say.x + 2, say.y + 2, false, runtime.design_system_, chat.history_view_.viewport, save),
                "history tabs accept and retain a complete captured pointer click");
            Check(key(VK_ESCAPE) && !runtime.KeyboardFocused() && !chat.HistoryExpanded() && chat.editor_.Text() == "unsent draft" &&
                chat.State().bounds.x == initial.x && chat.State().bounds.y == initial.y && chat.State().bounds.width == initial.width && chat.State().bounds.height == initial.height,
                "Escape restores gameplay and the original chat window without altering its draft or layout");
            runtime.native_ui_.menu_visible = true;
            Check(runtime.HandleChatKey('F', 0, false, true, true, 0) && chat.HistoryFocused(), "F can focus history during target selection");
            runtime.SyncNativeChatInput(nullptr, true, true);
            Check(chat.HistoryFocused() && runtime.HandleChatKey(VK_RETURN, 0, false, true, true, 0) && chat.HistoryExpanded(),
                "native menu polling cannot steal focused history or its expand key");
            key(VK_ESCAPE); runtime.native_ui_.menu_visible = false;
            Check(key(VK_OEM_2) && chat.Focused() && !chat.HandleHistoryKey('F', 0, false, true, runtime.design_system_),
                "F remains ordinary text when the message editor is focused");
            chat.Focus(false);
            runtime.editing_ = true;
            Check(!key('F') && !chat.HistoryFocused(), "config editing keeps F away from chat history");
            runtime.editing_ = false;
        }

        static auto Timestamps(void) -> void
        {
            std::array<uint32_t, 0xB6 * 11> table{};
            std::array<uintptr_t, 2> manager{0, reinterpret_cast<uintptr_t>(table.data())};
            const uintptr_t config = reinterpret_cast<uintptr_t>(manager.data());
            native_window_config native;
            native.config_slot_ = reinterpret_cast<uintptr_t>(&config); native.setter_ = 1;
            auto* record = table.data() + 0xB5 * 11; record[1] = 0xB5; record[7] = 2;
            Check(native.TimestampMode() == 0, "native Windows timestamp record reports Off");
            record[2] = 2;
            Check(native.TimestampMode() == 2, "native timestamp record retains seconds format");
            record[2] = 3;
            Check(!native.TimestampMode(), "invalid native timestamp formats fail safely");
            record[2] = 0;

            ui::design_system design; chat_module module;
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.SetNativeTimestamps(native.TimestampMode());
            chat_event event{}; event.sequence = 1; event.timestamp = ::GetTickCount64(); event.windows = 3; event.mode = 1;
            strcpy_s(event.text, "\x1e\x51[12:34:56]\x1e\x01 Hello\n\x1e\x51[12:34:56]\x1e\x01   Indented");
            module.Receive(nullptr, event, false);
            const auto rows = [&] {
                module.RebuildRows(design, 1500);
                std::string text;
                for (const auto& row : module.rows_) { for (const auto& span : row.spans) text += span.text; text += '\n'; }
                return text;
            };
            Check(rows() == "Hello\n  Indented\n", "game Off suppresses injected addon timestamps on each logical line without losing indentation");
            record[2] = 1; module.SetNativeTimestamps(native.TimestampMode());
            auto text = rows();
            Check(text[0] == '[' && text[3] == ':' && text[6] == ']' && text.substr(7, 7) == " Hello\n" &&
                std::count(text.begin(), text.end(), '[') == 2, "native timestamp changes rebuild cached history with one HH:MM prefix per line");
            record[2] = 2; module.SetNativeTimestamps(native.TimestampMode()); text = rows();
            Check(text[6] == ':' && text[9] == ']' && std::count(text.begin(), text.end(), '[') == 2,
                "Follow game retains the native seconds format without duplicate addon times");
            *module.Option(5).choice = 1;
            Check(rows() == "Hello\n  Indented\n", "explicit Off overrides native timestamps for this window");
            auto store = settings_store::FromIniText(""); module.Save(store); chat_module restored; restored.Load(store);
            Check(restored.timestamp_display_ == 1, "timestamp override persists independently of the game setting");
            restored.Load(settings_store::FromIniText("[chat]\ntimestamps=0\n"));
            Check(restored.timestamp_display_ == 0, "previous disabled defaults follow game timestamps");
            restored.Load(settings_store::FromIniText("[chat]\ntimestamps=1\n"));
            Check(restored.timestamp_display_ == 2, "existing enabled timestamp overrides retain HH:MM");
            event.sequence = 2; strcpy_s(event.text, "[12:34] literal message"); module.Clear(); module.Receive(nullptr, event, false);
            Check(rows() == "[12:34] literal message\n", "ordinary message text resembling a timestamp is preserved");
        }

        inline static size_t confirm_sounds{};
        static auto __cdecl ConfirmSound(void) -> void { ++confirm_sounds; }
        inline static size_t alert_sounds{};
        static auto __cdecl AlertSound(void) -> void { ++alert_sounds; }
        inline static size_t cursor_sounds{};
        static auto __cdecl CursorSound(void) -> void { ++cursor_sounds; }
        inline static std::array<size_t, 5> chatmon_sounds{};
        static auto __cdecl TellClip(void) -> void { ++chatmon_sounds[0]; }
        static auto __cdecl TalkClip(void) -> void { ++chatmon_sounds[1]; }
        static auto __cdecl EmoteClip(void) -> void { ++chatmon_sounds[2]; }
        static auto __cdecl ExamineClip(void) -> void { ++chatmon_sounds[3]; }
        static auto __cdecl InviteClip(void) -> void { ++chatmon_sounds[4]; }
        static auto ChatMonSounds(void) -> void
        {
            ui::native_ui_sounds sounds; sounds.Initialize();
            for (size_t i = 0; i < 5; ++i) Check(sounds.clips_.Available(i), "each bundled ChatMon resource decodes as a playable PCM clip");
            constexpr void(__cdecl* callbacks[])(void){&TellClip, &TalkClip, &EmoteClip, &ExamineClip, &InviteClip};
            for (size_t i = 0; i < 5; ++i) sounds.sounds_[i + 3].function = reinterpret_cast<uintptr_t>(callbacks[i]);
            chatmon_sounds = {};
            chat_module module; module.SetSounds(&sounds);
            const auto option = module.SoundsOptionStart() + 1;
            Check(module.Option(option).choice_preview && module.Option(option).preview_first == 1,
                "chat sound lists offer previews for audible choices and exclude Off");
            for (size_t i = 0; i < 5; ++i)
            {
                module.alerts_.channels[0] = static_cast<int>(i + 4);
                chat_event event{i + 1, ::GetTickCount64(), 12, 0xFFFFFFFF, 1}; strcpy_s(event.text, "Tell");
                module.Receive(nullptr, event);
                Check(module.PreviewChoice(option, i + 4) && module.alerts_.channels[0] == static_cast<int>(i + 4) && chatmon_sounds[i] == 2,
                    "each ChatMon choice routes to its own clip and preview leaves its selection intact");
            }
            Check(!module.PreviewChoice(option, 0) && !module.PreviewChoice(module.ColorsOptionStart(), 4), "Off and unrelated options cannot play previews");
            module.alerts_.keyword_sound = 4;
            auto store = settings_store::FromIniText(""); module.Save(store);
            chat_module restored; restored.Load(store);
            Check(restored.alerts_.channels[0] == 8 && restored.alerts_.keyword_sound == 4,
                "new sound selections survive persistence without being clamped to the old list");
            sounds.Shutdown();
        }
        static auto Customization(void) -> void
        {
            ui::native_ui_sounds sounds;
            sounds.sounds_[0].function = reinterpret_cast<uintptr_t>(&CursorSound);
            sounds.sounds_[1].function = reinterpret_cast<uintptr_t>(&ConfirmSound);
            sounds.sounds_[2].function = reinterpret_cast<uintptr_t>(&AlertSound);
            chat_module module, second{true}; module.AttachSecondWindow(second); module.SetSounds(&sounds);
            auto legacy = settings_store::FromIniText("[chat.sounds]\nincoming_tell=1\nparty=1\nsay=0\n");
            module.Load(legacy);
            Check(module.alerts_.channels[0] == 1 && module.alerts_.channels[3] == 1 && module.alerts_.channels[2] == 0,
                "old sound switches retain menu chimes or Off when loaded as sound selections");
            module.alerts_.channels[0] = 2; module.alerts_.channels[3] = 3;
            module.alerts_.keywords = "priority"; module.alerts_.UpdateKeywords();
            confirm_sounds = alert_sounds = cursor_sounds = 0;
            uint64_t sequence{};
            const auto receive = [&](uint32_t mode, const char* text) {
                chat_event event{++sequence, ::GetTickCount64(), mode, 0xFFFFFFFF, 1}; strcpy_s(event.text, text);
                module.Receive(nullptr, event);
            };
            receive(12, "Tell"); receive(13, "Party"); receive(13, "priority"); receive(9, "Say");
            Check(confirm_sounds == 1 && cursor_sounds == 1 && alert_sounds == 1,
                "per-type selections use their native sound and keyword sound takes priority once");
            module.alerts_.keyword_sound = 0; receive(13, "priority");
            Check(cursor_sounds == 2 && alert_sounds == 1, "disabling keyword sound retains the channel sound");
            constexpr uint32_t modes[]{12, 4, 9, 13, 14, 214, 10, 11, 212, 15, 219, 20, 200};
            constexpr D3DCOLOR game = 0xFF80A0C0, inlineColor = 0xFFE08060, custom = 0xCC60C090;
            for (size_t i = 0; i < std::size(modes); ++i)
            {
                Check(module.colors_.Resolve(modes[i], game, inlineColor) == inlineColor && module.colors_.Resolve(modes[i], game) == game,
                    "every chat type follows native and inline colors until explicitly overridden");
                module.colors_.enabled[i] = true; module.colors_.colors[i] = custom;
                Check(module.colors_.Resolve(modes[i], game, inlineColor) == custom, "XIUI overrides take precedence over game and inline colors");
                module.colors_.enabled[i] = false;
                Check(module.colors_.Resolve(modes[i], game, inlineColor) == inlineColor && module.colors_.colors[i] == custom,
                    "Game restores native colors while retaining the saved custom color");
            }
            module.colors_.enabled[0] = true;
            Check(module.colors_.Resolve(4, game) == game && second.Colors().Resolve(12, game) == custom,
                "incoming and outgoing tells are independent and overrides are shared by both XIUI windows");
            auto profile = module_text_profile{ui::theme::CreateDefault().body_text};
            profile.style.top_color = 0xFFFFFFFF; profile.style.bottom_color = 0xFF808080;
            const auto tinted = TintTextProfile(profile, module.colors_.Resolve(12, game));
            Check(tinted.style.top_color != tinted.style.bottom_color && tinted.style.stroke.enabled == profile.style.stroke.enabled &&
                tinted.style.shadow.enabled == profile.style.shadow.enabled && tinted.style.glow.enabled == profile.style.glow.enabled,
                "chat overrides preserve text gradients and effects through the shared tint path");
            auto store = settings_store::FromIniText(""); module.Save(store); second.Save(store);
            chat_module restored; restored.Load(store);
            Check(restored.alerts_.channels == module.alerts_.channels && restored.alerts_.keyword_sound == 0 &&
                restored.colors_.enabled == module.colors_.enabled && restored.colors_.colors == module.colors_.colors,
                "sound selections, override flags and custom colors persist without changing the native palette");
            Check(second.Option(second.ColorsOptionStart()).toggle == &module.colors_.enabled[0] &&
                std::string_view{module.ConfigSections()[6].description}.find("override your game settings colors") != std::string_view::npos,
                "color controls share preferences and visibly explain that XIUI overrides game settings colors");
            restored.ResetDefaults(ui::theme::CreateDefault());
            Check(std::none_of(restored.colors_.enabled.begin(), restored.colors_.enabled.end(), [](bool value) { return value; }),
                "reset returns every channel to game colors");
        }
        static auto Alerts(void) -> void
        {
            ui::native_ui_sounds sounds;
            sounds.sounds_[2].function = reinterpret_cast<uintptr_t>(&AlertSound);
            chat_module module, second{true};
            module.AttachSecondWindow(second);
            module.SetSounds(&sounds); second.SetSounds(&sounds);
            alert_sounds = 0;
            uint64_t sequence{};
            const auto receive = [&](uint32_t mode, const char* text = "Message") {
                chat_event event{++sequence, ::GetTickCount64(), mode, 0xFFFFFFFF, 1};
                strcpy_s(event.text, text);
                module.Receive(nullptr, event);
                return event;
            };
            constexpr uint32_t modes[]{12, 4, 9, 13, 14, 214, 10, 11, 212, 15, 219, 20, 200};
            for (const auto mode : modes) receive(mode);
            Check(alert_sounds == 1, "only incoming tells alert by default");
            for (size_t i = 0; i < std::size(modes); ++i)
            {
                module.alerts_.channels = {};
                module.alerts_.channels[i] = true;
                const auto before = alert_sounds;
                auto event = receive(modes[i]);
                event.windows = 2;
                module.Receive(nullptr, event); second.Receive(nullptr, event);
                Check(alert_sounds == before + 1, "each chat type alerts once across both native and XIUI windows");
            }
            module.alerts_.channels = {};
            module.alerts_.keywords = "  hello, rare item, ,HELLO  ";
            module.alerts_.UpdateKeywords();
            const auto before = alert_sounds;
            receive(9, "HELLO there");
            receive(200, "A rare item appeared");
            receive(9, "he\x1e\x54llo through colors");
            receive(9, "No match");
            Check(alert_sounds == before + 3, "comma-separated keywords trim spaces, ignore case and cross inline color spans");
            module.alerts_.channels[0] = true;
            receive(12, "hello");
            Check(alert_sounds == before + 4, "a channel and keyword matching together still play only one sound");
            module.State().enabled = false;
            receive(12, "hello");
            module.State().enabled = true;
            chat_event replay{++sequence, ::GetTickCount64(), 12, 0xFFFFFFFF, 1}; strcpy_s(replay.text, "hello");
            module.Receive(nullptr, replay, false);
            Check(alert_sounds == before + 4, "disabled chat and history replay stay silent");
            module.alerts_.channels = {};
            module.alerts_.keywords = "caf\xC3\x89, \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
            module.alerts_.UpdateKeywords();
            chat_line unicode{}; unicode.mode = 9; unicode.spans = {{"CAF\xC3\xA9", -1}};
            Check(module.alerts_.Matches(unicode), "Unicode keyword case matching is independent of keyboard locale");
            unicode.spans = {{"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", -1}};
            Check(module.alerts_.Matches(unicode), "Japanese keywords match displayed Unicode text");
            module.alerts_.keywords = " , , "; module.alerts_.UpdateKeywords();
            Check(!module.alerts_.Matches(unicode), "empty keyword entries do not match every message");
            module.alerts_.keywords = "hello, rare item"; module.alerts_.UpdateKeywords();
            module.alerts_.channels[3] = true;
            auto store = settings_store::FromIniText(""); module.Save(store); second.Save(store);
            chat_module restored; restored.Load(store);
            Check(restored.alerts_.keywords == module.alerts_.keywords && restored.alerts_.channels == module.alerts_.channels,
                "chat sound preferences and keywords persist together");
            const auto keyword = second.SoundsOptionStart();
            Check(second.Option(keyword).text == &module.alerts_.keywords && second.ConfigSectionContains(5, keyword),
                "both config windows edit the same shared alert preferences");
            restored.ResetDefaults(ui::theme::CreateDefault());
            Check(restored.alerts_.channels[0] && std::count(restored.alerts_.channels.begin(), restored.alerts_.channels.end(), 1) == 1 &&
                restored.alerts_.keywords.empty(), "reset restores incoming-tell-only alerts");
            hud_runtime runtime; runtime.chat_.SetSounds(&sounds);
            auto event = replay; event.sequence = 1;
            const auto replayBefore = alert_sounds;
            runtime.HandleChat(event);
            runtime.chat_alerts_ready_ = true; ++event.sequence; runtime.HandleChat(event);
            runtime.visible_ = false; ++event.sequence; runtime.HandleChat(event);
            Check(alert_sounds == replayBefore + 1, "runtime history replay and hidden HUDs are silent while live incoming tells alert");
        }
        static auto Sounds(void) -> void
        {
            ui::native_ui_sounds sounds;
            sounds.sounds_[1].function = reinterpret_cast<uintptr_t>(&ConfirmSound);
            chat_module module;
            module.SetSounds(&sounds);
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            chat_manager chat;
            confirm_sounds = 0;
            module.editor_.Set("hello");
            Check(module.Submit(&chat) && chat.count == 1 && confirm_sounds == 1, "valid chat send plays one native confirm sound");
            Check(module.Submit(&chat) && confirm_sounds == 1, "empty draft is silent");
            module.editor_.Set(std::string(256, 'a'));
            Check(!module.Submit(&chat) && chat.count == 1 && confirm_sounds == 1, "oversized message is silent");
            module.editor_.Set("/tell Someone ");
            Check(!module.Submit(&chat) && confirm_sounds == 1, "recipient-only draft is silent");
            module.editor_.Set("\xF0\x9F\x98\x80");
            Check(!module.Submit(&chat) && confirm_sounds == 1, "unencodable draft is silent");
            module.editor_.Set("hello");
            Check(!module.Submit(nullptr) && confirm_sounds == 1, "unavailable chat is silent");
            module.ResetDefaults(ui::theme::CreateDefault());
            module.editor_.Set("again");
            Check(module.Submit(&chat) && chat.count == 2 && confirm_sounds == 2, "reset retains shared sound feedback");
            module.State().bounds = {20, 400, 560, 260};
            module.visible_tabs_[0] = module.visible_tabs_[1] = true;
            ui::design_system design;
            const auto layout = module.Layout(design);
            const auto tab = layout.tabs[static_cast<size_t>(chat_tab::window1)];
            Check(module.SoundTarget(design, tab.x + 2, tab.y + 2, false) != 0 &&
                module.SoundTarget(design, tab.x + 2, tab.y + 2, true) == 0, "chat tab hover follows actual interaction mode");
            bool save{};
            const ui::rect viewport{0, 0, 1280, 720};
            module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, tab.x + 2, tab.y + 2, false, design, viewport, save);
            module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, tab.x + 2, tab.y + 2, false, design, viewport, save);
            module.HandleMouse(WM_LBUTTONUP, 0, tab.x + 2, tab.y + 2, false, design, viewport, save);
            Check(confirm_sounds == 3, "chat button confirms once per physical press");
        }

        static auto MenuInputs(void) -> void
        {
            hud_runtime runtime;
            chat_manager chat;
            runtime.native_ui_.menu_visible = true;
            runtime.chat_.editor_.Set("previous draft");
            Check(!runtime.HandleChatKey(VK_RETURN, 0, false, true, false, 0) &&
                !runtime.HandleChatKey(VK_DOWN, 0, false, true, false, 0) && !runtime.ChatFocused(),
                "command menu confirmation and navigation keep their native input");
            Check(runtime.HandleChatKey(VK_OEM_2, 0, false, true, false, 0) && runtime.ChatFocused() &&
                runtime.chat_.editor_.Text() == "/" && runtime.chat_native_context_suspended_,
                "slash claims XIUI chat over a visible combat command menu");
            runtime.SyncNativeChatInput(&chat, true);
            Check(runtime.ChatFocused() && runtime.HandleChatKey(VK_BACK, 0, false, true, false, 0) &&
                runtime.chat_.editor_.Text().empty() && chat.IsInputOpen() == 0 && chat.count == 0,
                "frame synchronization and subsequent typing retain XIUI ownership without opening native chat");
            Check(runtime.HandleChatKey(VK_ESCAPE, 0, false, true, false, 0) && !runtime.ChatFocused(),
                "Escape cancels chat without reaching the underlying command menu");
            runtime.SyncNativeChatInput(&chat, true);
            Check(!runtime.chat_native_context_suspended_ && !runtime.HandleChatKey(VK_DOWN, 0, false, true, false, 0),
                "the command menu regains keyboard input after chat closes");
            for (const auto flags : {LPARAM{0x40000000}, static_cast<LPARAM>(0x80000000u)})
                Check(!runtime.HandleChatKey(VK_OEM_2, flags, false, true, false, 0) && !runtime.ChatFocused(),
                    "held and released slash do not claim menu input");
            Check(!runtime.HandleChatKey(VK_OEM_2, 0, true, true, false, 0) &&
                !runtime.HandleChatKey(VK_OEM_2, 0, false, false, false, 0) &&
                !runtime.HandleChatKey(VK_OEM_2, 0, false, true, false, 0x21) &&
                !runtime.HandleChatKey(VK_OEM_2, 0, false, true, false, 0x11) && !runtime.ChatFocused(),
                "other input owners, disabled replacement and native editors retain slash");
            Check(runtime.HandleChatKey(VK_OEM_2, 0, false, true, false, 0), "slash can reopen XIUI chat over the same menu");
            runtime.SyncNativeChatInput(&chat, true, true);
            Check(runtime.ChatFocused() && runtime.chat_native_context_suspended_, "target selection cannot interrupt explicitly opened chat");
            runtime.ReleaseChatInput();
            runtime.chat_.Focus(true);
            runtime.SyncNativeChatInput(&chat, false, true);
            Check(!runtime.ChatFocused() && !runtime.chat_native_context_suspended_, "a new targeting prompt retains priority over ordinary chat focus");
        }

        inline static bool player_menu_closed{};
        static auto __fastcall CloseMenu(void*, void*, const char* name) -> void
        { player_menu_closed = std::strcmp(name, "menu    playermo") == 0; }

        static auto __fastcall ResetNative(void* object, void*) -> int32_t
        {
            *static_cast<uint32_t*>(object) = 3;
            static_cast<char*>(object)[16] = '\0';
            return 1;
        }

        static auto TargetInputs(void) -> void
        {
            hud_runtime runtime;
            chat_manager chat;
            for (const bool nativeMenu : {false, true})
            {
                runtime.native_ui_.menu_visible = nativeMenu;
                for (const WPARAM slash : {VK_OEM_2, VK_DIVIDE})
                {
                    Check(!runtime.HandleChatKey(VK_RETURN, 0, false, true, true, 0) &&
                        !runtime.HandleChatKey(VK_DOWN, 0, false, true, true, 0), "idle target confirmation and navigation stay native");
                    Check(!runtime.HandleChatKey(slash, 0, true, true, true, 0) &&
                        !runtime.HandleChatKey(slash, 0, false, false, true, 0) &&
                        !runtime.HandleChatKey(slash, 0x40000000, false, true, true, 0) &&
                        !runtime.HandleChatKey(slash, static_cast<LPARAM>(0x80000000u), false, true, true, 0) &&
                        !runtime.HandleChatKey(slash, 0, false, true, true, 0x21) && !runtime.ChatFocused(),
                        "targeting does not bypass blocked shortcuts, key repeats, disabled replacement or other native editors");
                    Check(runtime.HandleChatKey(slash, 0, false, true, true, 0) && runtime.ChatFocused() &&
                        runtime.chat_.editor_.Text() == "/", "slash opens XIUI chat while selecting a target");
                    runtime.SyncNativeChatInput(&chat, nativeMenu, true);
                    Check(runtime.ChatFocused() && runtime.HandleChatKey(VK_BACK, 0, false, true, true, 0) &&
                        runtime.chat_.editor_.Text().empty(), "frame synchronization and editing retain chat focus during target selection");
                    Check(runtime.HandleChatKey(VK_ESCAPE, 0, false, true, true, 0) && !runtime.ChatFocused(),
                        "cancelling chat consumes Escape before the target selection");
                    runtime.SyncNativeChatInput(&chat, nativeMenu, true);
                    Check(!runtime.chat_native_context_suspended_ && !runtime.HandleChatKey(VK_DOWN, 0, false, true, true, 0),
                        "target navigation resumes after chat closes");
                }
            }

            auto& input = runtime.chat_input_;
            input.raw_offset_ = 16;
            input.state_offset_ = 0;
            input.reset_chat_ = reinterpret_cast<uintptr_t>(&ResetNative);
            input.close_menu_ = reinterpret_cast<uintptr_t>(&CloseMenu);
            input.menu_manager_ = reinterpret_cast<uintptr_t>(&input);
            for (const auto* text : {"", "ordinary native message"})
            {
                chat.OpenNative(text);
                Check(!input.TakeCommand(&chat) && chat.IsInputOpen() == 0x11 && std::strcmp(chat.GetInputTextRaw(), text) == 0,
                    "empty and non-command native drafts remain untouched");
            }
            chat.OpenNative("/p hello", 5);
            Check(!input.TakeCommand(&chat) && chat.IsInputOpen() == 0x21, "slash text in another native editor stays native");
            for (const auto* text : {"/", "/p unsent message"})
            {
                player_menu_closed = false;
                chat.OpenNative(text);
                runtime.SyncNativeChatInput(&chat, true, true);
                Check(runtime.ChatFocused() && runtime.chat_.editor_.Text() == text &&
                    runtime.chat_.editor_.Caret() == std::strlen(text) && chat.native[0] == 3 &&
                    chat.GetInputTextRawLength() == 0 && !player_menu_closed,
                    "hidden native slash drafts transfer into XIUI during targeting using native session cleanup");
                runtime.SyncNativeChatInput(&chat, true, true);
                Check(runtime.ChatFocused() && runtime.chat_native_context_suspended_ && chat.count == 0 && chat.input_updates == 0,
                    "adopted commands retain focus without sending or rewriting the closed native editor");
                runtime.ReleaseChatInput();
            }
        }

        static auto TellInputs(void) -> void
        {
            constexpr LPARAM alt = 0x20000000, repeated = 0x60000000;
            chat_module module;
            chat_manager chat;
            Check(!module.HandleKey(nullptr, 'R', alt, false, true) && !module.Focused(), "reply with no previous teller keeps chat closed");
            Check(module.HandleKey(nullptr, 'R', alt, false, true, "Alice") && module.Focused() && module.editor_.Text() == "/t Alice ",
                "Alt+R opens an explicitly addressed tell to the latest sender");
            module.HandleKey(nullptr, 'R', repeated, false, true, "Bob");
            Check(module.editor_.Text() == "/t Alice " && chat.count == 0, "holding Alt+R neither changes recipient nor sends");
            module.editor_.Set("/party hello");
            module.HandleKey(nullptr, 'R', alt, false, true, "Bob");
            Check(module.editor_.Text() == "/t Bob hello", "reply switching preserves the message body");
            module.HandleKey(nullptr, 'R', alt, false, true, "Carol");
            Check(module.editor_.Text() == "/t Carol hello" && module.Submit(&chat) && chat.sent == "/t Carol hello" && chat.count == 1,
                "the recipient is bound when opening and the normal submit path sends once");
            Check(!module.HandleKey(nullptr, 'R', alt, true, true, "Alice") && !module.Focused(), "other input owners retain Alt+R");
            Check(!module.HandleKey(nullptr, 'R', alt, false, false, "Alice") && !module.Focused(), "native chat keeps its shortcut when replacement is inactive");
            module.State().enabled = false;
            Check(!module.HandleKey(nullptr, 'R', alt, false, true, "Alice"), "disabled XIUI chat cannot claim reply");
            module.State().enabled = true;
            Check(!module.OpenTell("Alice /echo") && !module.OpenTell(""), "invalid recipients cannot enter the command prefix");
            for (const auto* draft : {"/tell Alice unsent message", "previous draft"})
            {
                module.editor_.Set(draft);
                module.Focus(false);
                Check(!module.HandleKey(nullptr, VK_OEM_2, 0, true, true) && module.editor_.Text() == draft,
                    "another input owner keeps a dismissed draft intact");
                Check(module.HandleKey(nullptr, VK_OEM_2, 0, false, true) && module.Focused() &&
                    module.editor_.Text() == "/" && module.editor_.Caret() == 1,
                    "slash starts a fresh command after a previous tell or dismissed draft");
                module.HandleKey(nullptr, VK_ESCAPE, 0, false, true);
            }

            native_chat_input input;
            input.raw_offset_ = 16;
            input.state_offset_ = 0;
            input.reset_chat_ = reinterpret_cast<uintptr_t>(&ResetNative);
            input.close_menu_ = reinterpret_cast<uintptr_t>(&CloseMenu);
            input.menu_manager_ = reinterpret_cast<uintptr_t>(&input);
            chat.OpenNative("/tell Alice ", 5);
            Check(!input.TakeTell(&chat) && chat.IsInputOpen() == 0x21, "search and other native editors are never closed or adopted");
            chat.OpenNative("");
            Check(!input.TakeTell(&chat) && chat.IsInputOpen() == 0x11 && !module.Focused(), "ordinary Enter does not open XIUI input");
            chat.OpenNative("/s hello");
            Check(!input.TakeTell(&chat) && std::strcmp(chat.GetInputTextRaw(), "/s hello") == 0, "handoff is limited to explicitly addressed tells");
            chat.OpenNative("/tell Selectedplayer ");
            auto selected = input.TakeTell(&chat);
            Check(selected && chat.native[0] == 3 && chat.IsInputOpen() == 0 && chat.GetInputTextRawLength() == 0 && module.AdoptTell(std::move(*selected)) &&
                module.Focused() && module.editor_.Text() == "/tell Selectedplayer " && player_menu_closed,
                "the native CHAT action transfers its recipient and closes both native input and its player command window");
            Check(!input.TakeTell(&chat) && chat.count == 1, "handoff happens once and does not send a message");
            Check(!module.Submit(&chat) && module.Focused() && module.error_.empty() && chat.count == 1,
                "an address-only tell remains editable without submitting an invalid game command");
            for (const auto* recipient : {"Selectedplayer", "Anotherplayer", "Selectedplayer"})
            {
                module.HandleKey(nullptr, VK_ESCAPE, 0, false, true);
                chat.OpenNative((std::string{"/tell "} + recipient + " ").c_str());
                auto next = input.TakeTell(&chat);
                Check(next && module.AdoptTell(std::move(*next)) && module.editor_.Text() == std::string{"/tell "} + recipient + " " &&
                    chat.native[0] == 3 && chat.GetInputTextRawLength() == 0 && chat.input_updates == 0 && chat.count == 1,
                    "repeated CHAT actions use native session cleanup without rewriting closed input or sending messages");
            }
            module.Focus(false);
            for (const WPARAM key : {VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_ESCAPE})
                Check(!module.HandleKey(nullptr, key, 0, false, true), "camera and untarget keys pass through after a tell loses focus");
            hud_runtime runtime;
            runtime.chat_input_ = input;
            runtime.chat_.OpenTell("Previousplayer");
            runtime.SyncNativeChatInput(&chat, true);
            Check(!runtime.chat_.Focused() && runtime.chat_.editor_.Text() == "/t Previousplayer " && chat.count == 1,
                "the native command window owns input while the previous tell draft is preserved");
            runtime.chat_.Focus(true);
            chat.OpenNative("/tell Previousplayer ");
            runtime.chat_native_menu_recipient_ = "Curly";
            runtime.chat_native_menu_confirmed_at_ = ::GetTickCount64();
            runtime.chat_activation_enter_down_ = true;
            runtime.SyncNativeChatInput(&chat, true);
            Check(!runtime.chat_.Focused() && chat.native[0] == 4 && chat.GetInputTextRawLength() != 0 && chat.count == 1,
                "native CHAT keeps its Enter release before XIUI takes keyboard capture");
            runtime.HandleKey(VK_RETURN, static_cast<LPARAM>(0xC0000000u), false);
            runtime.SyncNativeChatInput(&chat, true);
            Check(runtime.chat_.Focused() && runtime.chat_.editor_.Text() == "/tell Curly " && chat.native[0] == 3 && chat.count == 1,
                "the native CHAT action replaces an already-focused tell recipient without sending the old draft");
            runtime.SyncNativeChatInput(&chat, true);
            Check(runtime.chat_.Focused() && runtime.chat_native_context_suspended_,
                "the native tell editor's parent command window cannot steal focus back from the adopted draft");
            const std::string retained = "/tell Previousplayer unsent body";
            Check(module.AdoptTell({retained, retained.size()}, "Curly") &&
                module.editor_.Text() == "/tell Curly unsent body" && module.editor_.Caret() == module.editor_.Text().size(),
                "binding CHAT to the selected player retains the message body and adjusts the caret");
            module.Focus(false);
            Check(!input.Recover(&chat) && chat.native[0] == 3, "recovery leaves healthy native input unchanged");
            chat.OpenNative("", 0);
            Check(input.Recover(&chat) && chat.native[0] == 3 && chat.count == 1, "recovery restores the previous invalid handoff without sending");
            chat.OpenNative("/t Alice ", 0);
            Check(!input.Recover(&chat) && chat.native[0] == 0, "recovery never overwrites a native draft");
            chat.OpenNative("", 5);
            Check(!input.Recover(&chat) && chat.native[0] == 5, "recovery never closes another native editor");
            chat.OpenNative("/t Alice \x82\xb1\x82\xf1");
            auto japanese = input.TakeTell(&chat);
            Check(japanese && japanese->caret == japanese->text.size() && japanese->text.size() == 15,
                "native tell text and caret retain Shift-JIS conversion");
            input.state_offset_ = 1;
            chat.OpenNative("/t Alice ");
            Check(!input.TakeTell(&chat) && chat.IsInputOpen() == 0x11 && chat.GetInputTextRawLength() > 0,
                "an unvalidated native input state leaves the original draft intact");
            input.state_offset_ = 0;
            input.reset_chat_ = 0;
            Check(!input.TakeTell(&chat) && chat.native[0] == 4, "missing native close support preserves the game's tell input");
        }

        static auto Palette(void) -> void
        {
            struct
            {
                struct color_entry { uint32_t index, color; } colors[5]{
                    {0x54, 0x80808080}, {0x55, 0x80A05040}, {0x6C, 0x80A04030}, {0x6C, 0x50F0F050}, {}};
                struct mode_entry { uint16_t mode; uint8_t color, category; } modes[6]{
                    {1, 0x54, 1}, {2, 0x55, 1}, {3, 0x6C, 1}, {11, 0x6C, 1}, {3, 0x55, 1}, {}};
            } native;
            native_chat_palette source;
            source.module_ = reinterpret_cast<uintptr_t>(&native);
            source.size_ = sizeof(native);
            source.colors_ = reinterpret_cast<uintptr_t>(native.colors);
            source.modes_ = reinterpret_cast<uintptr_t>(native.modes);
            const auto palette = source.Read();
            Check(palette.available && palette.colors[0x6C] == 0xFFFF664C,
                "yell uses the first native color entry instead of the later duplicate yellow entry");
            Check(palette.modes[3] == palette.colors[0x6C] && palette.modes[11] == palette.modes[3] &&
                palette.colors[256 + 3] == palette.modes[3] && palette.colors[256 + 11] == palette.modes[3],
                "incoming, outgoing, inline and input yell colors share native first-match lookup");
            Check(palette.modes[2] == 0xFFFF7F66, "shout retains its separate configured color");
            native.colors[2].color = 0x80603070;
            Check(source.Read().modes[11] == ChatColor(0x80603070), "native color setting changes refresh without reusing the duplicate entry");
        }

        static auto AutoTranslate(const char* path) -> void
        {
            const auto south = ChatToken(0x0E06);
            std::string encoded;
            Check(ToGameText("go " + south + " now", encoded) && encoded == "go " + south + " now", "native auto-translate bytes survive outbound encoding");
            chat_editor atom;
            atom.Set("go " + south);
            atom.Move(-1, false); Check(atom.Caret() == 3, "left moves over a complete phrase token");
            atom.Move(1, false); Check(atom.Caret() == 9, "right moves over a complete phrase token");
            atom.Erase(true); Check(atom.Text() == "go ", "backspace removes a complete phrase token");
            atom.Insert(south); Check(atom.Text() == "go " + south, "pasting retains phrase control bytes");
            atom.Set("go " + south, 5); Check(atom.Caret() == 3, "restored carets cannot enter a phrase token");
            atom.Erase(false); Check(atom.Text() == "go ", "delete removes a complete phrase token");
            atom.Set(std::string(509, 'a') + south); Check(atom.Text().size() == 509, "draft truncation never splits a phrase token");
            atom.Insert(south); Check(atom.Text().size() == 509, "pasted phrases respect draft capacity without partial tokens");
            atom.Set(ChatToken(0x0E20) + " south"); Check(atom.CompletionPrefix() == "south", "phrase payload bytes are excluded from prefix parsing");
            std::vector<chat_completion> choices{{"South", south, chat_completion_kind::auto_translate}, {"Southern San d'Oria", ChatToken(0x1201), chat_completion_kind::auto_translate}};
            for (uint16_t i = 2; i < 37; ++i) choices.push_back({"South place " + std::to_string(i), ChatToken(static_cast<uint16_t>(0x1200 + i)), chat_completion_kind::auto_translate});
            choices.push_back(choices.front());
            chat_module module;
            module.ApplyTextDefaults(ui::theme::CreateDefault()); module.State().bounds = {20, 400, 560, 260};
            module.auto_translate_labels_[south] = "South";
            module.Focus(true); module.editor_.Set("south"); module.editor_.CompleteCandidates(choices, false);
            Check(module.editor_.Completions().size() == 37 && module.editor_.Text() == south && PlainChatText(module.DisplayDraft(module.editor_.Text())) == "{South}", "all matching phrases remain available and render as readable tokens");
            ui::design_system design;
            auto layout = module.Layout(design);
            Check(layout.completion_rows > 0 && layout.completion_rows <= 8 && layout.completion.y >= layout.content.y &&
                layout.completion.y + layout.completion.height < layout.input.y, "completion popup fits above the input without covering it");
            module.HandleKey(nullptr, VK_NEXT, 0, false, true);
            Check(module.editor_.CompletionIndex() == 8, "Page Down reaches matches beyond the visible completion rows");
            module.HandleKey(nullptr, VK_UP, 0, false, true); Check(module.editor_.CompletionIndex() == 7, "arrows browse completion choices instead of history");
            module.HandleKey(nullptr, VK_ESCAPE, 0, false, true);
            Check(module.Focused() && module.editor_.Text() == "south" && module.editor_.Completions().empty(), "Escape closes the picker and restores the typed prefix");
            module.editor_.CompleteCandidates(choices, false);
            module.HandleKey(nullptr, VK_RETURN, 0, false, true);
            Check(module.Focused() && module.editor_.Completions().empty() && module.editor_.Text() == south && module.error_.empty(), "Enter chooses a phrase without sending or leaving input");
            chat_manager chat;
            Check(module.Submit(&chat) && chat.sent == "/say " + south, "selected phrases send their native token rather than quoted text");
            module.Focus(true); module.editor_.Set("south"); module.editor_.CompleteCandidates(choices, false);
            layout = module.Layout(design); bool save{};
            const ui::rect viewport{0, 0, 1280, 720};
            Check(module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, layout.completion.x + 10, layout.completion.y + 25, true, design, viewport, save) &&
                module.editor_.Completions().empty() && module.Focused(), "completion choices can be clicked in edit mode");
            module.HandleMouse(WM_LBUTTONUP, 0, layout.completion.x + 10, layout.completion.y + 25, true, design, viewport, save);
            runtime_state state{sizeof(runtime_state)}; module.Capture(state);
            chat_module restored; restored.Restore(state); Check(restored.editor_.Text() == south, "native phrase tokens survive draft transfer");
            atom.Set(""); atom.CompleteCandidates(choices, false); Check(atom.Completions().size() == 37, "empty-prefix completion exposes the whole catalog");
            if (path == nullptr) return;
            std::ifstream file{path, std::ios::binary};
            const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
            const auto entries = resources::DecodeAutoTranslate(bytes);
            Check(entries && entries->size() > 2000, "decode the installed complete phrase dictionary");
            Check(std::any_of(entries->begin(), entries->end(), [](const auto& entry) { return entry.id == 0x0E06 && entry.text == "South"; }) &&
                std::any_of(entries->begin(), entries->end(), [](const auto& entry) { return entry.text.starts_with("@A"); }), "dictionary includes South and linked zone names");
            Check(!resources::DecodeAutoTranslate(std::span{bytes}.first(bytes.size() - 1)), "truncated phrase dictionaries fail cleanly");
            std::printf("Installed auto-translate dictionary: %zu phrases.\n", entries->size());
        }

        static auto AutoTranslateCaps(const char* path) -> void
        {
            const auto text = std::string{chat_cap_open} + "South" + std::string{chat_cap_close};
            Check(FromGameText("\xEF\x27South\xEF\x28") == text && PlainChatText(text) == "{South}", "native incoming caps remain explicit and clipboard text stays readable");
            const auto spans = ParseChatText(text);
            Check(spans.size() == 1 && spans[0].text == text, "native caps survive incoming span parsing");
            ui::design_system design;
            auto style = ui::theme::CreateDefault().body_text; style.scale = 1.25f; style.letter_spacing = 2;
            Check(MeasureChatText(design, text, style) == design.MeasureText("South", style) + (16 + 4) * style.scale,
                "cap advances use the native eight pixels and configured text spacing");
            if (path == nullptr) return;
            std::ifstream file{path, std::ios::binary};
            const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
            size_t definitions{}, textures{};
            for (size_t at = 0; at + 32 <= bytes.size();)
            {
                uint32_t meta{}; std::memcpy(&meta, bytes.data() + at + 4, 4);
                const size_t size = ((meta >> 7) & 0xFFFFF) * size_t{16};
                if (size < 16 || size > bytes.size() - at) break;
                const auto payload = std::span{bytes}.subspan(at + 16, size - 16);
                if ((meta & 0x7F) == 0x31 && std::string_view{reinterpret_cast<const char*>(payload.data()), 16} == "font    usgaiji ")
                    for (size_t side = 0; side < 2; ++side)
                    {
                        const auto sprite = resources::DecodeMenuSprite(payload, 8 + side);
                        Check(sprite && sprite->front().size() == 2, "native cap definition includes its bounds and visible strip");
                        const auto& q = sprite->front()[1];
                        Check(q.texture == (side == 0 ? "markgree" : "markred ") && q.u == side * 4 && q.v == 0 && q.width == 4 && q.height == 8 &&
                            q.points == std::array<int16_t, 8>{2, 0, 6, 0, 2, 16, 6, 16} && q.colors[0] == 0x7F7F7F7F && !q.additive,
                            "native green left and red right caps retain their UV halves, stretched geometry and modulation");
                        ++definitions;
                    }
                if ((meta & 0x7F) == 0x20 && payload.size() >= 0x45)
                {
                    const std::string_view name{reinterpret_cast<const char*>(payload.data() + 9), 8};
                    if (name == "markgree" || name == "markred ")
                    {
                        const auto image = resources::DecodeMenuTexture(payload);
                        Check(image && image->width == 8 && image->height == 8, "native cap marker textures decode at their original size");
                        uint32_t red{}, green{};
                        for (const auto color : image->pixels) { red += (color >> 16) & 255; green += (color >> 8) & 255; }
                        Check(name == "markgree" ? green > red : red > green, "cap colors come from the native texture pixels");
                        ++textures;
                    }
                }
                at += size;
            }
            Check(definitions == 2 && textures == 2, "both cap definitions and textures exist in the installed DAT");
            std::puts("Native auto-translate cap geometry and texture checks passed.");
        }

        static auto Run(void) -> void
        {
            chat_module module;
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            chat_manager chat;
            module.editor_.Set("hello");
            module.channel_ = 1;
            module.Focus(true);
            Check(module.Submit(&chat) && chat.sent == "/party hello" && chat.mode == -1 && chat.count == 1 && !module.Focused(), "typed channel sends through command parser once");
            module.editor_.Set("/echo local command");
            Check(module.Submit(&chat) && chat.sent == "/echo local command", "slash commands stay intact");
            module.editor_.Set(std::string(256, 'a'));
            Check(!module.Submit(&chat) && module.editor_.Text().size() == 256 && chat.count == 2, "oversized input stays editable and is not sent");
            module.editor_.Set("draft"); module.Focus(true);
            runtime_state state{sizeof(runtime_state)}; module.Capture(state);
            chat_module restored; restored.Restore(state);
            Check(restored.Focused() && restored.editor_.Text() == "draft" && restored.editor_.Caret() == 5, "draft transfer");
            restored.Focus(false);
            Check(!restored.HandleKey(nullptr, VK_OEM_2, 0, true, true) && !restored.Focused(), "other input owner prevents slash focus");
            Check(!restored.HandleKey(nullptr, VK_RETURN, 0x01000000, false, true) && !restored.Focused(), "numpad Enter remains available to game menus");
            Check(!restored.HandleKey(nullptr, VK_RETURN, 0, false, true) && !restored.Focused() && restored.editor_.Text() == "draft", "main Enter passes to the game without opening chat or changing the draft");
            Check(!restored.HandleKey(nullptr, VK_RETURN, 0x40000000, false, true) && !restored.Focused(), "held Enter cannot open chat");
            Check(restored.HandleKey(nullptr, VK_OEM_2, 0, false, true) && restored.Focused() && restored.editor_.Text() == "/", "slash opens chat with a fresh command prefix");
            Check(restored.HandleKey(nullptr, VK_RETURN, 0x40000000, false, true) && restored.Focused(), "held Enter cannot immediately submit or close chat");
            Check(!restored.HandleKey(nullptr, 'A', static_cast<LPARAM>(0x80000000u), false, true), "key releases cannot insert text");
            restored.HandleKey(nullptr, VK_ESCAPE, 0, false, true);
            Check(!restored.Focused(), "Escape releases focus");
            ui::design_system design;
            const ui::rect viewport{0, 0, 1280, 720};
            chat_module editingChat;
            editingChat.ApplyTextDefaults(ui::theme::CreateDefault());
            editingChat.State().bounds = {20, 400, 560, 260};
            Check(editingChat.HandleKey(nullptr, VK_OEM_2, 0, false, true) && editingChat.Focused(), "slash opens input while the layout editor is open");
            editingChat.editor_.Set("draft");
            const auto editingInput = editingChat.Layout(design).input;
            bool editingSave{};
            Check(editingChat.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, editingInput.x + 6, editingInput.y + 10, true, design, viewport, editingSave) &&
                editingChat.Focused() && editingChat.editor_.Caret() == 0, "edit mode input clicks move the caret instead of dragging the module");
            Check(editingChat.HandleMouse(WM_LBUTTONUP, 0, editingInput.x + 6, editingInput.y + 10, true, design, viewport, editingSave), "edit mode retains the input click release");
            Check(!editingChat.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, 30, 420, true, design, viewport, editingSave) && !editingChat.Focused(), "edit mode log presses release typing and allow dragging");
            module.state_.bounds = {20, 400, 560, 260};
            bool save{};
            Check(module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, 575, 403, false, design, viewport, save), "top-right grip starts resizing");
            module.HandleMouse(WM_LBUTTONUP, 0, 675, 353, false, design, viewport, save);
            Check(save && !module.Resizing() && module.state_.bounds.x == 20 && module.state_.bounds.y == 350 && module.state_.bounds.width == 660 && module.state_.bounds.height == 310, "resize preserves bottom-left and includes release position");
            const auto theme = ui::theme::CreateDefault();
            Check(module.text_.style.shadow.enabled == theme.body_text.shadow.enabled && module.text_.style.glow.enabled == theme.body_text.glow.enabled, "chat preserves shared text effects");
            module.editor_.Set("");
            module.Focus(false);
            chat_module second{true}; second.ApplyTextDefaults(theme); module.AttachSecondWindow(second);
            const auto layout = module.Layout(design);
            Check(std::all_of(layout.tabs.begin(), layout.tabs.end(), [](const auto& tab) { return tab.height == 0; }) && !second.State().enabled,
                "tabs and second window are hidden by default");
            Check(layout.log.y == layout.content.y && layout.input.height == 0 && module.VisualBounds(design).y == module.State().bounds.y &&
                !module.HitTest(design, module.State().bounds.x + module.State().bounds.width - 10, module.State().bounds.y - 12), "idle chat has no input row or second-window control above its frame");
            module.Focus(true);
            *module.Option(11).toggle = true; module.OnOptionChanged(11);
            Check(second.State().enabled && second.State().bounds.width == module.State().bounds.width && second.State().bounds.height == module.State().bounds.height && module.Focused(),
                "config creates the second window at the current main window size");
            *module.Option(11).toggle = false; module.OnOptionChanged(11);
            Check(!second.State().enabled, "second window closes through config");
            *module.Option(9).toggle = true; *module.Option(10).toggle = true;
            module.state_.bounds.width = module.state_.minimum_width;
            const auto tabs = module.Layout(design);
            const auto& firstTab = tabs.tabs[static_cast<size_t>(chat_tab::window1)];
            const auto& lastTab = tabs.tabs[static_cast<size_t>(chat_tab::window2)];
            Check(firstTab.y == lastTab.y && lastTab.x + lastTab.width <= module.State().bounds.x + module.State().bounds.width && firstTab.y + firstTab.height < module.State().bounds.y,
                "both optional native-window tabs fit on one row at minimum width");
            Check(module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, lastTab.x + 1, lastTab.y + 1, false, design, viewport, save) &&
                module.source_ == 1 && module.active_tab_ == static_cast<int>(chat_tab::window2) && module.Focused(), "optional window tab switches the message source");
            module.HandleMouse(WM_LBUTTONUP, 0, lastTab.x + 1, lastTab.y + 1, false, design, viewport, save);
            module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, module.Grip().x + 9, module.Grip().y + 4, false, design, viewport, save);
            module.HandleMouse(WM_LBUTTONUP, 0, module.Grip().x + 9, -500, false, design, viewport, save);
            Check(module.VisualBounds(design).y >= viewport.y && module.State().bounds.y + module.State().bounds.height == 660,
                "resize keeps optional tabs on screen and bottom edge anchored");
            module.state_.bounds = {20, 350, 660, 310};
            module.active_tab_ = 0; module.source_ = 2;
            const auto prefix = module.ChannelProfile();
            Check(prefix.style.scale < module.text_.style.scale && prefix.style.alignment == ui::text_alignment::center &&
                prefix.style.shadow.enabled == module.text_.style.shadow.enabled, "compact centered channel retains text effects");
            Check(layout.scrollbar.width == 6 && layout.scrollbar.x > layout.log.x + layout.log.width &&
                layout.scrollbar.x + layout.scrollbar.width <= layout.content.x + layout.content.width, "contrasting scrollbar has reserved space");
            Check(module.chrome_.style.scale >= 0.85f && module.chrome_.style.stroke.enabled == theme.meter_text.stroke.enabled, "tab labels use readable shared meter text");
            module.editor_.Set("/tell");
            Check(module.Layout(design).channel.width == 0, "explicit commands retain full borderless input width");
            hud_snapshot snapshot{};
            for (uint64_t sequence = 1; sequence <= 1024; ++sequence)
            {
                chat_event event{}; event.sequence = sequence; event.windows = 1; event.mode = 1;
                strcpy_s(event.text, "A message with words and a long segment for wrapping.");
                module.Receive(nullptr, event);
            }
            module.RebuildRows(design, 450);
            const auto existing = module.rows_.size();
            const auto start = std::chrono::steady_clock::now();
            for (uint64_t sequence = 1025; sequence <= 1224; ++sequence)
            {
                chat_event event{}; event.sequence = sequence; event.windows = 1; event.mode = 1; strcpy_s(event.text, "A message with words and a long segment for wrapping.");
                module.Receive(nullptr, event); module.RebuildRows(design, 450);
            }
            Check(module.Count() == chat_history_capacity && module.rows_.size() == existing && module.rows_.front().sequence == 201, "incremental wrapping evicts old history");
            std::printf("200 incremental chat updates: %.2f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
            module.tabs_[0].scroll = 20;
            chat_event event{}; event.sequence = 1225; event.windows = 1; event.mode = 1; strcpy_s(event.text, "new");
            module.Receive(nullptr, event); module.RebuildRows(design, 450);
            const auto anchored = module.tabs_[0].scroll;
            event.windows = 3;
            module.Receive(nullptr, event); module.RebuildRows(design, 450);
            Check(module.tabs_[0].scroll == anchored, "window-routing merge does not move scrollback");
            Check(module.ScrollbarOpacity(::GetTickCount64()) == 0, "scrollbar stays hidden before scrolling");
            module.Scroll(1);
            Check(module.ScrollbarOpacity(module.scrollbar_until_ - 1200) == 1 && module.ScrollbarOpacity(module.scrollbar_until_ - 100) > 0 &&
                module.ScrollbarOpacity(module.scrollbar_until_ - 100) < 1 && module.ScrollbarOpacity(module.scrollbar_until_) == 0,
                "scrolling reveals the thumb briefly, then fades it away");
            module.Clear(); module.RebuildRows(design, 450);
            Check(module.rows_.empty() && module.Count() == 0, "clear removes cached rows");
            chat_module animated; animated.ApplyTextDefaults(theme);
            animated.UpdateInputAnimation(1000);
            const float idleLogHeight = animated.Layout(design).log.height;
            Check(animated.Layout(design).input.height == 0 && animated.HandleKey(nullptr, VK_OEM_2, 0, false, true) && animated.Focused() && animated.editor_.Text() == "/",
                "slash starts typing from a hidden input");
            animated.UpdateInputAnimation(1070);
            Check(animated.input_reveal_ > 0 && animated.input_reveal_ < 1 && animated.Layout(design).log.height < idleLogHeight, "input animates into the space reserved for typing");
            animated.UpdateInputAnimation(1140); animated.Focus(false); animated.UpdateInputAnimation(1210);
            Check(animated.input_reveal_ > 0 && animated.input_reveal_ < 1, "input animates out after focus is released");
            animated.UpdateInputAnimation(1280);
            Check(animated.Layout(design).input.height == 0 && animated.Layout(design).log.height == idleLogHeight && animated.editor_.Text() == "/", "idle input disappears, restores message space, and keeps its draft");
            chat_module templateView, copiedView{true}; templateView.AttachSecondWindow(copiedView);
            templateView.ApplyTextDefaults(theme); copiedView.ApplyTextDefaults(theme);
            templateView.State().bounds = {18, 400, 603, 221};
            templateView.container_.opacity = 0.65f; templateView.container_.padding = 6; templateView.container_.scan_lines = false;
            templateView.text_.style.scale = 1.2f; templateView.text_.style.stroke.enabled = templateView.text_.style.glow.enabled = true;
            templateView.text_.style.bottom_color = 0xFFAABBDD; templateView.text_.offset_x = 3;
            templateView.line_spacing_ = 4; templateView.timestamp_display_ = 2; templateView.visible_tabs_[0] = true;
            *templateView.Option(11).toggle = true; templateView.OnOptionChanged(11);
            Check(copiedView.State().bounds.width == 603 && copiedView.State().bounds.height == 221 && copiedView.container_.opacity == 0.65f &&
                copiedView.container_.padding == 6 && !copiedView.container_.scan_lines && copiedView.text_.style.scale == 1.2f && copiedView.text_.style.stroke.enabled &&
                copiedView.text_.style.glow.enabled && copiedView.text_.style.bottom_color == 0xFFAABBDD && copiedView.text_.offset_x == 3 && copiedView.line_spacing_ == 4 &&
                copiedView.timestamp_display_ == 2 && copiedView.visible_tabs_[0] && copiedView.source_ == 1, "second window inherits size and all presentation settings while keeping native Window 2 routing");
            copiedView.State().bounds = {650, 350, 500, 180}; copiedView.text_.style.scale = 0.8f;
            copiedView.State().enabled = false; templateView.State().bounds.width = 700;
            *templateView.Option(11).toggle = true; templateView.OnOptionChanged(11);
            Check(copiedView.State().bounds.x == 650 && copiedView.State().bounds.width == 500 && copiedView.State().bounds.height == 180 && copiedView.text_.style.scale == 0.8f,
                "reopening preserves independent size, position, and style");
            chat_module primaryView, secondView{true};
            primaryView.ApplyTextDefaults(theme); secondView.ApplyTextDefaults(theme); primaryView.AttachSecondWindow(secondView);
            for (uint32_t window = 1; window <= 3; ++window)
            {
                chat_event line{}; line.sequence = window; line.windows = window; line.mode = 1; strcpy_s(line.text, "shared capture");
                primaryView.Receive(nullptr, line);
            }
            primaryView.RebuildRows(design, 500); secondView.RebuildRows(design, 500);
            Check(primaryView.rows_.size() == 2 && primaryView.rows_[0].sequence == 1 && secondView.rows_.size() == 2 && secondView.rows_[0].sequence == 2 &&
                &primaryView.History() == &secondView.History(), "both native windows filter one shared history, including while the second view is closed");
            secondView.EnsureWindowDefaults();
            secondView.State().enabled = true; secondView.Focus(true);
            Check(!secondView.Focused() && secondView.Layout(design).input.height == 0 && secondView.HasEditableBounds(), "second window shares the main input and has independent bounds");
            primaryView.State().bounds = {25, 350, 600, 260}; secondView.State().bounds = {650, 350, 500, 180};
            secondView.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, secondView.Grip().x + 9, secondView.Grip().y + 4, false, design, viewport, save);
            secondView.State().enabled = false;
            Check(!secondView.HandleMouse(WM_MOUSEMOVE, 0, 800, 200, false, design, viewport, save) && !secondView.Resizing(), "hiding the second view cancels an active resize");
            secondView.State().enabled = true;
            *primaryView.Option(8).choice = 2; primaryView.OnOptionChanged(8);
            *primaryView.Option(9).toggle = true; *secondView.Option(8).toggle = true;
            settings_store saved{""}; primaryView.Save(saved); secondView.Save(saved);
            chat_module loadedPrimary, loadedSecond{true}; loadedPrimary.AttachSecondWindow(loadedSecond);
            loadedPrimary.Load(saved); loadedSecond.Load(saved);
            Check(loadedPrimary.State().bounds.x == 25 && loadedSecond.State().bounds.x == 650 && loadedSecond.State().enabled &&
                loadedPrimary.source_ == 2 && loadedSecond.source_ == 1 && loadedPrimary.visible_tabs_[0] && loadedSecond.visible_tabs_[0] && !loadedSecond.visible_tabs_[1],
                "both view layouts, native sources, visibility, and selected tabs persist independently");
            settings_store legacy{""}; legacy.WriteInt("chat", "active_tab", 4); legacy.WriteRect("layout.chat", {18, 1200, 603, 221});
            chat_module migrated; migrated.Load(legacy);
            Check(migrated.State().bounds.y == 1200 && migrated.State().bounds.width == 603 && migrated.source_ == 0 &&
                migrated.active_tab_ == static_cast<int>(chat_tab::window1) && !migrated.visible_tabs_[0] && !migrated.visible_tabs_[1], "existing chat keeps its position and adopts tabless native routing");
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.RestoreChat(state);
            Check(runtime.ChatFocused() && runtime.chat_.editor_.Text() == "draft", "edit mode retains restored drafts and captures focused keyboard input");
            runtime.visible_ = false;
            runtime.RestoreChat(state);
            Check(!runtime.ChatFocused() && !runtime.chat_.Focused(), "hidden HUD still releases restored chat input");
            runtime.visible_ = true; runtime.editing_ = false;
            runtime.chat_.State().bounds = {20, 400, 560, 260}; runtime.chat_second_.State().bounds = {600, 400, 560, 180};
            Check(runtime.ChatAt(610, 410) == nullptr, "disabled second window cannot capture input");
            runtime.chat_second_.State().enabled = true;
            Check(runtime.ChatAt(610, 410) == &runtime.chat_second_ && runtime.ChatAt(30, 410) == &runtime.chat_, "input routes to the correct visible window");
            runtime.chat_.State().enabled = false;
            Check(runtime.ChatAt(610, 410) == nullptr, "disabling chat hides both views and releases their hit areas");
            runtime.chat_.State().enabled = true;
            runtime.chat_.State().bounds = {20, 30, 560, 260}; runtime.chat_second_.State().bounds = {-1, -1, 560, 180};
            D3DVIEWPORT8 screen{}; screen.Width = 1280; screen.Height = 720;
            runtime.EnsureLayout(screen);
            Check(runtime.chat_second_.State().bounds.y > runtime.chat_.State().bounds.y + runtime.chat_.State().bounds.height, "second view opens below a chat window near the screen top");

        }
    };
}

int main(int argc, char** argv)
{
    try
    {
        using namespace xiui;
        if (argc == 2 && std::strcmp(argv[1], "--history-only") == 0)
        {
            chat_test::ExpandedHistory();
            std::puts("Chat history focus, expansion, channel tabs, wrapped-message/page/wheel scrolling and keyboard ownership passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--timestamps-only") == 0)
        {
            chat_test::Timestamps();
            std::puts("Native timestamp settings, live history refresh, addon prefix suppression, overrides and persistence passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--chatmon-only") == 0)
        {
            chat_test::ChatMonSounds();
            chat_test::Customization();
            std::puts("Embedded ChatMon clips, alert/preview routing, sound persistence and existing chat customization checks passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--customization-only") == 0)
        {
            chat_test::Customization();
            chat_test::Alerts();
            std::puts("Per-type sound selection, keyword priority, legacy preferences and optional chat color override checks passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--alerts-only") == 0)
        {
            chat_test::Alerts();
            std::puts("Chat channel defaults, keyword matching, duplicate/replay silence and shared alert persistence checks passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--menu-input-only") == 0)
        {
            chat_test::MenuInputs();
            chat_test::TargetInputs();
            chat_test::TellInputs();
            std::puts("Command-menu and target-selection slash ownership, hidden command recovery, native input priority and tell handoff checks passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--sounds-only") == 0)
        {
            chat_test::Sounds();
            std::puts("Chat send sound, rejected-draft silence and chat control feedback checks passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--tell-only") == 0)
        {
            chat_test::TellInputs();
            std::puts("Reply shortcuts and native CHAT tell handoff checks passed.");
            return 0;
        }
        Check(ChatTabMatches(chat_tab::all, 1, 1) && ChatTabMatches(chat_tab::window2, 5, 2) && !ChatTabMatches(chat_tab::window1, 5, 2) && !ChatTabMatches(chat_tab::all, 1, 0), "native window routing and rejected messages");
        Check(ChatTabMatches(chat_tab::party, 5, 1) && ChatTabMatches(chat_tab::tell, 12, 1) && ChatTabMatches(chat_tab::linkshell, 213, 1) && ChatTabMatches(chat_tab::chat, 211, 1), "channel classification");
        const auto encoded = std::string{"base\x1e\x54"} + "party\x1e\x01 reset\x1f\x05" + "party mode";
        const auto spans = ParseChatText(encoded);
        Check(spans.size() == 4 && spans[1].color_index == 0x54 && spans[2].color_index == -1 && spans[3].color_index == 261, "both native inline color formats and reset");
        const auto japanese = std::string{"\x82\xb1\x82\xf1\x82\xc9\x82\xbf\x82\xcd"};
        const auto decoded = FromGameText(japanese);
        std::string roundtrip;
        Check(ToGameText(decoded, roundtrip) && roundtrip == japanese && decoded.size() == 15, "Shift-JIS and UTF-8 round trip");
        Check(FromGameText(std::string{"\x1e\xe0"} + japanese).substr(0, 2) == std::string{"\x1e\xe0"}, "palette indices are not decoded as Shift-JIS");
        Check(!ToGameText("\xF0\x9F\x98\x80", roundtrip), "unrepresentable outbound text rejected");
        Check(ChatColor(0x80808080) == 0xFFFFFFFF && ChatColor(0x80404040) == 0xFF7F7F7F, "native white point and lower intensities");
        Check(ChatColor(0x8020C0A0) == 0xFF2AFFD4 && ChatColor(0x80A05040) == 0xFFFF7F66 && ChatColor(0x80A04030) == 0xFFFF664C,
            "configured party, shout and yell colors retain their RGB proportions without channel clipping");
        chat_editor editor;
        editor.Set("/ma Cu"); editor.Complete({"Cure", "Cure II", "Curaga"}, false);
        Check(editor.Text() == "/ma Curaga", "prefix completion");
        editor.Complete({}, false); Check(editor.Text() == "/ma Cure", "Tab cycles");
        editor.Complete({}, true); Check(editor.Text() == "/ma Curaga", "Shift+Tab reverses");
        editor.Set("/ma \"Cure I"); editor.Complete({"Cure II"}, false);
        Check(editor.Text() == "/ma \"Cure II\"", "quoted completion");
        editor.Set("/ma Cure", 6); editor.Complete({"Curaga"}, false);
        Check(editor.Text() == "/ma Curaga", "completion replaces token after caret");
        editor.Set(decoded); editor.Erase(true);
        Check(editor.Text().size() == 12 && editor.Caret() == 12, "UTF-8 backspace");
        editor.SelectAll(); editor.Insert("draft"); editor.Remember(); editor.Set("unsent"); editor.History(-1);
        Check(editor.Text() == "draft", "command history"); editor.History(1); Check(editor.Text() == "unsent", "unsent draft restored");
        editor.Set(std::string(510, 'a') + decoded); Check(editor.Text().size() == 510, "UTF-8 capacity boundary");
        chat_test::Palette();
        chat_test::Run();
        chat_test::AutoTranslate(argc > 1 ? argv[1] : nullptr);
        chat_test::AutoTranslateCaps(argc > 2 ? argv[2] : nullptr);
        std::puts("chat smoke passed"); return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "chat smoke failed: %s\n", error.what()); return 1; }
}
