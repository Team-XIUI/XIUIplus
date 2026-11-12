#include "chat/chat_editor.hpp"
#include "chat/chat_encoding.hpp"
#include "chat/chat_model.hpp"
#include "chat/chat_token.hpp"
#include "chat/chat_text.hpp"
#include "resources/auto_translate.hpp"
#include "modules/chat/chat_module.hpp"
#include "modules/chat/chat_theme.hpp"
#include "hud_runtime.hpp"
#include "ui/clipped_quad.hpp"
#include "chat_stack_checks.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <fstream>

namespace
{
    auto Check(bool condition, const char* message) -> void { if (!condition) throw std::runtime_error{message}; }
    class status_packets final : public IPacketManager
    {
    public:
        size_t count{};
        uint16_t id{};
        std::vector<uint8_t> bytes;
        void AddIncomingPacket(uint16_t, uint32_t, uint8_t*) override {}
        void AddOutgoingPacket(const uint16_t packet, const uint32_t size, uint8_t* data) override
        { ++this->count; this->id = packet; this->bytes.assign(data, data + size); }
        bool QueueOutgoingPacket(uint16_t, uint16_t, uint16_t, uint32_t, uint32_t, queuepacketcallback_f, void*) const override { return false; }
    };
    class chat_manager final : public IChatManager
    {
    public:
        std::string sent; int32_t mode{}; size_t count{}; uint32_t input_limit{255};
        std::string translate_token, translation;
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
        int32_t ParseAutoTranslate(const char* text, char* buffer, int32_t size, bool) const override
        {
            const auto* expanded = text == this->translate_token ? this->translation.c_str() : text;
            strncpy_s(buffer, size, expanded, _TRUNCATE);
            return static_cast<int32_t>(strlen(buffer));
        }
        void ExecuteScript(const char*, const char*, bool) override {}
        void ExecuteScriptString(const char*, const char*, bool) override {}
        const char* GetInputTextRaw() const override { return this->native.data() + 16; }
        void SetInputTextRaw(const char*, uint32_t) const override {}
        uint32_t GetInputTextRawLength() const override { return static_cast<uint32_t>(std::strlen(this->GetInputTextRaw())); }
        uint32_t GetInputTextRawCaretPosition() const override { return this->GetInputTextRawLength(); }
        const char* GetInputTextParsed() const override { return ""; }
        void SetInputTextParsed(const char*) const override {}
        uint32_t GetInputTextParsedLength() const override { return 0; }
        uint32_t GetInputTextParsedLengthMax() const override { return this->input_limit; }
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
        static auto Emphasis(void) -> void
        {
            const auto formatted = [](const uint32_t mode, const char* text) {
                chat_model model;
                model.Add(chat_event{1, 1, mode, 0xFFFFFFFF, 1}, text);
                std::string value; bool bold{};
                for (const auto& span : model.Lines().front().spans)
                {
                    if (span.emphasis != bold) { value += "**"; bold = span.emphasis; }
                    value += span.text;
                }
                if (bold) value += "**";
                return value;
            };
            Check(formatted(20, "Shuu hits the Forest Hare for 384 points of damage.") ==
                "**Shuu** hits **the Forest Hare** for **384** points of damage.", "melee names and damage stand out from prose");
            Check(formatted(20, "The Forest Hare scores a critical hit!Shuu takes 1 point of damage.") ==
                "**The Forest Hare** scores a critical hit!**Shuu** takes **1** point of damage.", "concatenated critical-hit sentences retain their text and separate fields");
            Check(formatted(20, "The Forest Hare misses Shuu.") == "**The Forest Hare** misses **Shuu**.", "miss targets retain emphasis without a damage field");
            Check(formatted(20, "Shuu casts Cure IV on Phro.") == "**Shuu** casts **Cure IV** on **Phro**.", "multiword spell names and targets are emphasized");
            Check(formatted(20, "Shuu uses Fast Blade.The Forest Hare takes 1,234 points of damage.") ==
                "**Shuu** uses **Fast Blade**.**The Forest Hare** takes **1,234** points of damage.", "weaponskills and grouped damage values retain separate emphasis");
            Check(formatted(121, "Obtained: rabbit hide.") == "Obtained: **rabbit hide**.", "obtained item names are emphasized");
            Check(formatted(121, "Obtained: \x1e\x02" "rabbit hide\x1e\x01.") == "Obtained: **rabbit hide**.", "native inline item colors preserve semantic boundaries");
            Check(formatted(121, "Sloth caught a \x1e\x02" "Nosteau herring\x1e\x01!") ==
                "**Sloth** caught a **Nosteau herring**!", "fishing success emphasizes the player while retaining native item emphasis");
            Check(formatted(121, "Andurilus caught a tiger cod!") == "**Andurilus** caught a **tiger cod**!" &&
                formatted(121, "Phro caught an enormous fish!") == "**Phro** caught an **enormous fish**!",
                "fishing player and item fields are bold while articles and prose stay regular");
            for (const auto mode : {7u, 15u})
            {
                Check(formatted(mode, "Emilybydefault kneels respectfully before Shuu.") ==
                    "**Emilybydefault** kneels respectfully before **Shuu**.", "targeted emotes emphasize actor and target names");
                Check(formatted(mode, "Emilybydefault smiles happily.") == "**Emilybydefault** smiles happily." &&
                    formatted(mode, "Emilybydefault's pet says: hello 123.") == "**Emilybydefault**'s pet says: hello 123.",
                    "custom emotes emphasize their actor without interpreting authored prose or numbers");
            }
            Check(formatted(9, "Shuu: Sloth caught a tiger cod!") == "**Shuu**: Sloth caught a tiger cod!",
                "fishing and emote rules do not parse player-authored speech");
            Check(formatted(9, "Shuu: I hit for 384 points of damage.") == "**Shuu**: I hit for 384 points of damage.", "authored speech remains normal after the speaker prefix");
            Check(formatted(13, "(Shuu) Casting Cure IV.") == "(**Shuu**) Casting Cure IV." &&
                formatted(14, "[Linkshell] <Phro> Hello.") == "[Linkshell] <**Phro**> Hello.", "party and linkshell speaker brackets remain normal weight");
            Check(formatted(150, "Shantotto : Hello there.") == "**Shantotto** : Hello there.", "NPC dialogue emphasizes its speaker without parsing the prose as battle output");
            Check(formatted(20, "Unrecognized battle output.") == "Unrecognized battle output.", "unrecognized prose remains normal weight");
            const auto named = [](const uint32_t mode, const char* text) {
                chat_model model; model.Add(chat_event{1, 1, mode, 0xFFFFFFFF, 1}, text);
                std::string result;
                for (const auto& span : model.Lines().front().spans)
                    if (span.name) { Check(span.emphasis, "named fields retain bold emphasis"); result += span.text + "|"; }
                return result;
            };
            Check(named(20, "Shuu casts Cure IV on Phro.") == "Shuu|Phro|" &&
                named(121, "Sloth caught a tiger cod!") == "Sloth|" &&
                named(15, "Emilybydefault kneels respectfully before Shuu.") == "Emilybydefault|Shuu|" &&
                named(12, ">>Shuu: Cure IV for 123.") == "Shuu|",
                "only actor, recipient and speaker fields receive the name tint");
            chat_module tinted;
            chat_palette palette{}; palette.modes[12] = 0xCC8090B0;
            chat_module::rendered_row tell{1, 12, 0xFFFFFFFF};
            chat_module::rendered_span body{"hello"}, speaker{"Shuu", -1, 0, false, false, true, true};
            const auto bodyColor = tinted.SpanColor(tell, body, palette);
            const auto nameColor = tinted.SpanColor(tell, speaker, palette);
            const auto channel = [](const D3DCOLOR color, const unsigned shift) { return (color >> shift) & 255; };
            Check((nameColor & 0xFF000000) == (bodyColor & 0xFF000000) && channel(nameColor, 16) > channel(bodyColor, 16) &&
                std::abs(static_cast<float>(channel(nameColor, 16)) / channel(bodyColor, 16) -
                    static_cast<float>(channel(nameColor, 0)) / channel(bodyColor, 0)) < .01f &&
                ChatNameColor(0xCCFFFFFF) == 0xCCFFFFFF,
                "name brightness preserves message hue and alpha without clipping white");
            ui::design_system design; design.SetAppearanceTheme(ui::xiv1_appearance);
            auto selectedStyle = ui::theme::CreateDefault().body_text;
            for (int font = 1; font < ui::builtin_font_count; ++font)
            {
                selectedStyle.font = font;
                const auto resolved = ChatThemeStyle(design, selectedStyle);
                Check(resolved.font == font && ChatInputStyle(design, selectedStyle).font == font &&
                    ChatThemeStyle(design, selectedStyle, true).font == font &&
                    ChatThemeStyle(design, resolved).font == font && resolved.stroke.enabled && resolved.shadow.enabled,
                    "authored chat fonts survive XIV theme resolution in logs, typing and tabs while retaining text effects");
            }
            auto regular = ChatThemeStyle(design, ui::theme::CreateDefault().body_text);
            regular.weight = ui::font_weight::regular;
            auto bold = regular; bold.weight = ui::font_weight::bold;
            const auto normalWidth = MeasureChatText(design, "Damage 1234", regular);
            Check(MeasureChatText(design, "Damage 1234", bold) > normalWidth &&
                MeasureChatText(design, "Damage 1234", regular) == normalWidth, "regular and bold measurements use distinct font caches");
            ui::text_rasterizer rasterizer;
            const auto normalGlyph = rasterizer.Rasterize(ui::xiv1_font, 15, 'W', ui::font_weight::regular);
            const auto boldGlyph = rasterizer.Rasterize(ui::xiv1_font, 15, 'W', ui::font_weight::bold);
            Check(normalGlyph && boldGlyph && normalGlyph->masks[0] != boldGlyph->masks[0], "font weights rasterize distinct glyphs without simulated overdraw");
            const auto mediumGlyph = rasterizer.Rasterize(ui::xiv1_font, 15, 'W', ChatRunWeight(design, false));
            const auto ink = [](const ui::raster_glyph& glyph) {
                uint32_t total{}; for (const auto value : glyph.masks[0]) total += value; return total;
            };
            const bool mediumDistinct = mediumGlyph && ink(*mediumGlyph) > ink(*normalGlyph) && ink(*mediumGlyph) < ink(*boldGlyph);
            if (!mediumDistinct && mediumGlyph)
                std::fprintf(stderr, "Glyph coverage: regular %u, medium %u, bold %u\n", ink(*normalGlyph), ink(*mediumGlyph), ink(*boldGlyph));
            Check(mediumDistinct, "XIV chat body has a real coverage increase between regular and bold");
            ui::design_system native;
            Check(ChatRunWeight(native, false) == ui::font_weight::regular && ChatRunWeight(design, true) == ui::font_weight::bold,
                "body weight adjustment retains native chat typography and bold semantic fields");
            chat_module module;
            module.model_.Add(chat_event{1, 1, 20, 0xFFFFFFFF, 1}, "Shuu casts Cure IV on the Forest Hare.");
            module.RebuildRows(design, 160);
            Check(module.rows_.size() > 1, "mixed-weight messages wrap in a narrow log");
            for (const auto& row : module.rows_)
            {
                float width{};
                for (const auto& span : row.spans)
                {
                    auto style = ChatThemeStyle(design, module.text_.style);
                    style.weight = ChatRunWeight(design, span.emphasis);
                    Check(std::abs(span.width - MeasureChatText(design, span.text, style) - ChatThemeTracking(style)) < .01f,
                        "wrapped spans measure the same weight and tracking used to draw them");
                    width += span.width;
                }
                Check(width <= 160.01f, "mixed-weight rows stay within the viewport");
            }
            module.text_.style.font = ui::xiv1_italic_font;
            module.RebuildRows(design, 160);
            Check(module.cache_font_ == ui::xiv1_italic_font, "changing the chat face invalidates wrapped rows immediately");
            auto fontSettings = settings_store::FromIniText(""); module.Save(fontSettings);
            chat_module restoredFont; restoredFont.Load(fontSettings);
            Check(ChatThemeStyle(design, restoredFont.text_.style).font == ui::xiv1_italic_font,
                "the selected chat face remains authoritative after settings reload");
            chat_module narrow;
            narrow.model_.Add(chat_event{1, 1, 12, 0xFFFFFFFF, 1}, ">>Emilybydefault: hello.");
            narrow.RebuildRows(design, 35);
            std::string wrappedName;
            for (const auto& row : narrow.rows_) for (const auto& span : row.spans)
                if (span.name) wrappedName += span.text;
            Check(wrappedName == "Emilybydefault", "name tint survives character wrapping in a narrow chat window");
        }

        static auto StatusCancellation(void) -> void
        {
            const keyboard_restore neutral;
            hud_runtime runtime;
            std::array<std::array<uint32_t, 11>, 67> settings{};
            std::array<uintptr_t, 2> manager{0, reinterpret_cast<uintptr_t>(settings.data())};
            const auto config = reinterpret_cast<uintptr_t>(manager.data());
            runtime.window_config_.config_slot_ = reinterpret_cast<uintptr_t>(&config); runtime.window_config_.setter_ = 1;
            settings[66][1] = 66; settings[66][2] = 2; settings[66][3] = 4; settings[66][7] = 2;
            auto& player = const_cast<party_member&>(runtime.party_source_.Members()[0]);
            player.active = player.buffs_known = true; player.id = 50;
            player.buffs = {33, 33, 999, 2}; player.buff_count = 4;
            std::array<int16_t, 32> active; active.fill(-1);
            active[0] = active[1] = 33; active[2] = 999; active[3] = 2;
            IStatusIcon icon{}; icon.Index = 33; icon.CanCancel = 1;
            status_packets packets;
            auto& buffs = runtime.buffs_;
            Check(!buffs.CancelControllerEffect(&packets, &icon, 50, active), "unfocused inspection never requests cancellation");
            Check(runtime.HandleChatKey('F', 0, false, true, false, 0) && runtime.chat_.HistoryFocused() &&
                runtime.HandleChatKey('F', 0, false, true, false, 0) && buffs.ControllerEffect() == 33 &&
                buffs.CancelControllerEffect(&packets, &icon, 50, active) && packets.count == 1 && packets.id == 0x0F1 &&
                packets.bytes == std::vector<uint8_t>({0xF1, 0x04, 0, 0, 33, 0, 0, 0}) && player.buff_count == 4,
                "F then F focuses buffs and explicit cancellation sends the native request without removing an icon before server confirmation");
            icon.CanCancel = 0;
            Check(!buffs.CancelControllerEffect(&packets, &icon, 50, active), "client cancellation flags protect non-removable effects");
            icon.CanCancel = 1;
            Check(!buffs.CancelControllerEffect(&packets, &icon, 51, active), "a changed local player cannot cancel a stale selection");
            Check(buffs.MoveControllerBrowseHorizontal(1) && buffs.ControllerEffect() == 33, "duplicate effects keep their selected occurrence");
            active[1] = -1;
            Check(!buffs.CancelControllerEffect(&packets, &icon, 50, active), "a removed native duplicate invalidates cancellation before the HUD updates");
            Check(buffs.MoveControllerBrowseHorizontal(1) && buffs.ControllerEffect() == 999, "extended effect IDs remain selectable");
            Check(!buffs.CancelControllerEffect(&packets, &icon, 50, active), "mismatched metadata cannot grant cancellation to another effect");
            icon.Index = 999;
            Check(buffs.CancelControllerEffect(&packets, &icon, 50, active) && packets.count == 2 && packets.bytes[4] == 0xE7 && packets.bytes[5] == 3,
                "extended effect IDs retain both bytes in the cancellation request");
            active[2] = -1;
            Check(!buffs.CancelControllerEffect(&packets, &icon, 50, active) && packets.count == 2,
                "an effect already removed from the client cannot be cancelled from a stale HUD snapshot");
            Check(runtime.HandleChatKey(VK_RETURN, 0, false, true, false, 0) && runtime.confirmation_key_release_ == VK_RETURN,
                "keyboard Confirm shares cancellation and owns the Enter gesture");
            runtime.controller_hint_ = "unchanged";
            Check(runtime.HandleChatKey(VK_RETURN, 0x40000000, false, true, false, 0) && runtime.controller_hint_ == "unchanged" &&
                runtime.HandleKey(VK_RETURN, static_cast<LPARAM>(0xC0000000u), false) && runtime.confirmation_key_release_ == 0,
                "held Confirm cannot repeat cancellation and its release cannot open the native command menu");
            runtime.controller_hint_ = "unchanged";
            Check(runtime.HandleController({controller_action::confirm, controller_backend::xinput, 0, true, true, false, 100}) &&
                runtime.controller_hint_ == "unchanged", "held controller Confirm cannot request a second cancellation");
            runtime.ReleaseControllerFocus();
            auto& debuffs = runtime.debuffs_;
            icon.Index = 2; icon.CanCancel = 0;
            Check(debuffs.BeginControllerBrowse() && !debuffs.CancelControllerEffect(&packets, &icon, 50, active) && packets.count == 2,
                "non-cancellable debuffs remain inspection-only");
            runtime.ReleaseControllerFocus();

            // Full keyboard: numpad + cycles chat then Buffs, where F would start typing.
            settings[66][2] = 0;
            active[1] = 33; active[2] = 999;
            icon.Index = 33; icon.CanCancel = 1;
            Check(runtime.HandleChatKey(VK_ADD, 0, false, true, false, 0) && runtime.chat_.HistoryFocused() && !runtime.chat_.Focused() &&
                runtime.HandleChatKey(VK_ADD, 0, false, true, false, 0) && buffs.ControllerEffect() == 33,
                "numpad + then numpad + focuses Buffs in the Full keyboard layout");
            Check(runtime.KeyboardInputState() == keyboard_capture_status && KeyboardCapturesScan(runtime.KeyboardInputState(), 0x4E),
                "focused Buffs reserve numpad + from the native window cycle");
            const auto before = packets.count;
            buffs.CancelControllerEffect(&packets, &icon, 50, active);
            Check(packets.count == before + 1 && packets.bytes[4] == 33, "numpad-focused Buffs share the native cancellation request");
            runtime.ReleaseControllerFocus();

            // Mouse: the pressed and released effect must match, independent of tooltip settings.
            const auto first = buffs.Layout(buffs.Effects()).Icon(0);
            const auto second = buffs.Layout(buffs.Effects()).Icon(1);
            buffs.settings_.tooltips = false;
            const auto pointed = buffs.EffectAt(first.x + first.width / 2, first.y + first.height / 2);
            const auto duplicate = buffs.EffectAt(second.x + second.width / 2, second.y + second.height / 2);
            Check(pointed && pointed->id == 33 && pointed->occurrence == 0 && pointed->player == 50 &&
                duplicate && duplicate->id == 33 && duplicate->occurrence == 1 && !buffs.EffectAt(-100, -100),
                "pointer hit testing identifies duplicate effects by occurrence without tooltips");
            Check(buffs.CancelEffect(&packets, &icon, 50, active, *duplicate) && packets.count == before + 2,
                "a pointed duplicate effect sends one cancellation request");
            active[1] = -1;
            Check(!buffs.CancelEffect(&packets, &icon, 50, active, *duplicate) && !buffs.CancelEffect(&packets, &icon, 51, active, *pointed) &&
                packets.count == before + 2, "stale pointed effects and changed players cannot cancel");
            icon.CanCancel = 0;
            Check(!buffs.CancelEffect(&packets, &icon, 50, active, *pointed) && packets.count == before + 2,
                "client cancellation flags also protect pointer cancellation");
        }

        static auto Bubbles(void) -> void
        {
            const auto line = [](const uint64_t sequence, const uint32_t mode, const char* text) {
                return chat_line{sequence, 1000, mode, 0xFFFFFFFF, 1, {{text, -1}}};
            };
            const auto body = [](const bubble_message& message) {
                std::string text; for (const auto& span : message.spans) text += span.text; return text;
            };
            for (const auto mode : {1u, 9u, 2u, 10u, 3u, 11u})
            {
                const auto message = ParseBubbleMessage(line(1, mode, "Shuu[PortJeuno]: Hello there."));
                Check(message && message->speaker == "Shuu" && body(*message) == "Hello there.",
                    "say, shout and yell resolve the speaker without a remote-zone suffix");
            }
            for (const auto mode : {5u, 13u, 210u})
            {
                const auto message = ParseBubbleMessage(line(1, mode, "(Friend) Party message."));
                Check(message && message->channel == bubble_channel::party && message->speaker == "Friend", "party wrappers resolve their speaker");
            }
            for (const auto mode : {6u, 14u, 213u, 214u})
            {
                const auto message = ParseBubbleMessage(line(1, mode, "[2]<Friend> Linkshell message."));
                Check(message && message->speaker == "Friend" && body(*message) == "Linkshell message." &&
                    message->channel == (mode < 200 ? bubble_channel::linkshell1 : bubble_channel::linkshell2), "linkshell channels keep native membership routing");
            }
            for (const auto mode : {4u, 12u, 20u, 142u, 191u, 200u})
                Check(!ParseBubbleMessage(line(1, mode, "Friend : Choose an option.")), "tells, combat, native options and system messages cannot become speech bubbles");
            auto colored = line(1, 9, "Friend : "); colored.spans.push_back({"Colored speech.", 280});
            const auto parsed = ParseBubbleMessage(colored);
            Check(parsed && parsed->spans.size() == 1 && parsed->spans[0].color_index == 280 && body(*parsed) == "Colored speech.",
                "removing speaker prefixes preserves inline colors");
            const auto named = ParseBubbleMessage(line(1, 190, "Malene : A page of dialogue."));
            const auto continuation = ParseBubbleMessage(line(2, 190, "  Continued dialogue: still the same speaker."));
            Check(named && named->speaker == "Malene" && continuation && continuation->continuation &&
                body(*continuation) == "Continued dialogue: still the same speaker.", "indented NPC continuation is not mistaken for a new speaker");

            chat_bubbles_module bubbles;
            bubbles.ApplyTextDefaults(ui::theme::CreateDefault());
            bubbles.Add(line(1, 190, "Malene : A page of dialogue."), *named, 4, 100, 1000);
            bubbles.Add(line(2, 190, "  Continued dialogue."), *continuation, 4, 100, 1200);
            Check(bubbles.bubbles_.size() == 1 && bubbles.bubbles_[0].spans.size() == 3 && bubbles.bubbles_[0].spans[1].text == " " &&
                bubbles.bubbles_[0].started == 1000, "NPC fragments flow together without forcing a new line or restarting the entrance");
            ui::design_system design;
            bubbles.RebuildRows(bubbles.bubbles_[0], design, 10000);
            Check(bubbles.bubbles_[0].rows.size() == 1, "separate native chunks can share a single bubble row");
            bubbles.bubbles_[0].spans.push_back({"\nAn explicit paragraph.", -1});
            bubbles.bubbles_[0].cache_font = -1;
            bubbles.RebuildRows(bubbles.bubbles_[0], design, 10000);
            Check(bubbles.bubbles_[0].rows.size() == 2, "embedded paragraph breaks remain intact");
            const auto next = ParseBubbleMessage(line(3, 190, "Malene : Next page."));
            bubbles.Add(line(3, 190, "Malene : Next page."), *next, 4, 100, 1400);
            Check(bubbles.bubbles_.size() == 1 && bubbles.bubbles_[0].spans.size() == 1 && bubbles.bubbles_[0].spans[0].text == "Next page.",
                "a new NPC page replaces its speaker's previous bubble");
            const auto entering = chat_bubbles_module::Motion(bubbles.bubbles_[0], 1490);
            Check(chat_bubbles_module::Motion(bubbles.bubbles_[0], 1400).opacity == 0 && entering.opacity > 0 && entering.opacity < 1 && entering.offset_y > 0 &&
                chat_bubbles_module::Motion(bubbles.bubbles_[0], 1580).opacity == 1, "bubble entrance eases from transparent and below to fully visible");
            Check(bubbles.outgoing_.size() == 1 && chat_bubbles_module::Motion(bubbles.outgoing_[0], 1400).opacity == 1 &&
                chat_bubbles_module::Motion(bubbles.outgoing_[0], 1520).offset_y < 0 && chat_bubbles_module::Motion(bubbles.outgoing_[0], 1640).opacity == 0,
                "replaced dialogue fades and slides out while the new page enters");
            const auto expires = bubbles.bubbles_[0].expires;
            const auto leaving = chat_bubbles_module::Motion(bubbles.bubbles_[0], expires - 120);
            Check(leaving.opacity > 0 && leaving.opacity < 1 && leaving.offset_y < 0 &&
                chat_bubbles_module::Motion(bubbles.bubbles_[0], expires).opacity == 0, "natural expiration uses the same smooth exit without depending on frame rate");
            for (uint32_t i = 0; i < 20; ++i) bubbles.Add(line(i + 4, 9, "Friend : Hello."), *parsed, i + 10, i + 200, 1500);
            Check(bubbles.bubbles_.size() == 12, "busy chat keeps a bounded number of active speakers");
            bubbles.channels_[static_cast<size_t>(bubble_channel::say)] = false;
            bubbles.OnOptionChanged(8);
            Check(bubbles.bubbles_.empty(), "disabling a channel removes its existing bubbles");
            settings_store saved{".bubbles-check.ini"}; bubbles.range_ = 42; bubbles.Save(saved);
            chat_bubbles_module restored; restored.Load(saved);
            Check(restored.range_ == 42 && !restored.channels_[0] && !restored.CanMove() && !restored.PreviewVisible() &&
                restored.EditGrip() == edit_grip_kind::none, "bubble preferences persist without creating a movable HUD or edit grip");
            const auto oldProfile = settings_store::FromIniText("[chat.bubbles]\nopacity=0.900\nwidth=320\noffset_y=20\n");
            const auto stampedProfile = settings_store::FromIniText("[chat.bubbles]\nopacity=0.900\nwidth=320\noffset_y=20\nappearance_version=1\n");
            for (const auto* profile : {&oldProfile, &stampedProfile})
            {
                settings_store snapshot{""}; restored.Save(snapshot);
                OverlayProfilePreferences(snapshot, *profile);
                restored.Load(snapshot);
                Check(restored.opacity_ == 0 && restored.width_ == 360 && restored.offset_y_ == 28,
                    "loading an old character profile cannot inherit the current snapshot's migration marker");
            }
            restored.opacity_ = 0.9f; restored.width_ = 400; restored.offset_y_ = 36;
            settings_store customized{""}; restored.Save(customized);
            settings_store snapshot{""}; bubbles.Save(snapshot);
            OverlayProfilePreferences(snapshot, customized); restored.Load(snapshot);
            Check(restored.opacity_ == 0.9f && restored.width_ == 400 && restored.offset_y_ == 36,
                "later bubble appearance preferences survive profile handoff");
            hud_runtime runtime;
            const auto page = runtime.ConfigPages()[3];
            Check(page.SectionCount() == 3 && page.SectionModule(2) == &runtime.chat_bubbles_ && page.SectionContains(2, &runtime.chat_bubbles_),
                "Bubbles is a sibling category of both chat windows through shared configuration navigation");
            restored.Update(nullptr, false, 2000);
            Check(restored.bubbles_.empty(), "missing game state safely clears transient speech");
        }

        static auto DialoguePrompts(void) -> void
        {
            for (const auto code : {0x31, 0x32, 0x33, 0x37})
                Check(GameTextDialogueControl(std::string{"NPC text\x7F"} + static_cast<char>(code)) == dialogue_control::manual, "manual native continuation markers survive decoding as metadata");
            for (const auto code : {0x34, 0x35, 0x36})
                Check(GameTextDialogueControl(std::string{"NPC text\x7F"} + static_cast<char>(code) + '\x01') == dialogue_control::timed, "timed dialogue must not request a manual advance");
            Check(GameTextDialogueControl(ChatToken(0x7F31)) == dialogue_control::absent &&
                GameTextDialogueControl("\x1E\x7F" "1") == dialogue_control::absent &&
                GameTextDialogueControl("Text\x7F") == dialogue_control::absent, "tokens, palette operands and truncated controls are not dialogue prompts");

            chat_module chat, second{true};
            chat.AttachSecondWindow(second);
            chat.ApplyTextDefaults(ui::theme::CreateDefault());
            second.ApplyTextDefaults(ui::theme::CreateDefault());
            chat.SetDialogueActive(true);
            const auto append = [&](const uint64_t sequence, const uint32_t mode, const char* text, const uint32_t windows = 1) {
                chat_event event{}; event.sequence = sequence; event.timestamp = 1000; event.mode = mode; event.windows = windows;
                strcpy_s(event.text, text); chat.Receive(nullptr, event, false);
            };
            append(1, 150, "Malene : A longer dialogue line that wraps.\x7F\x31");
            ui::design_system design;
            chat.RebuildRows(design, 110);
            Check(chat.rows_.size() > 1 && chat.rows_.back().spans.back().dialogue_prompt &&
                chat.rows_.back().spans.back().text == "\xE2\x96\xBC", "one filled continuation arrow follows the final wrapped NPC row");
            size_t markers{};
            for (const auto& row : chat.rows_)
                for (const auto& span : row.spans) markers += span.dialogue_prompt;
            Check(markers == 1 && chat.DialoguePromptActive(1) && chat.CollapsePinned() && !chat.Focused(),
                "pending dialogue animates its current marker and stays readable without taking input focus");
            append(1, 150, "Malene : A longer dialogue line that wraps.\x7F\x31", 2);
            second.RebuildRows(design, 110);
            Check(chat.Count() == 1 && second.rows_.back().spans.back().dialogue_prompt && second.DialoguePromptActive(1),
                "native routing merges the marker once and supports the shared second chat window");
            append(2, 5, "Unrelated party chatter.");
            Check(chat.DialoguePromptActive(1), "unrelated chat cannot dismiss the pending NPC prompt");
            chat.tabs_[chat.active_tab_].scroll = 1;
            Check(!chat.DialoguePromptActive(1), "scrollback does not animate a historical prompt");
            chat.tabs_[chat.active_tab_].scroll = 0;
            append(3, 190, "<Sigh> What is it with you people and this \x87\xB2" "adventuring\x87\xB3 business?", 2);
            second.RebuildRows(design, 110);
            Check(!chat.DialoguePromptActive(1) && second.DialoguePromptActive(3) && second.rows_.back().spans.back().dialogue_prompt,
                "the captured native mode-190 page gets its arrow without a raw continuation control");
            const auto count = chat.Count();
            append(4, 151, "\x7F\x34\x01");
            Check(chat.Count() == count && !chat.DialoguePromptActive(3), "an empty auto-advance control clears the old prompt without adding a blank row");
            append(5, 1, "Regular chat.\x7F\x31");
            Check(!chat.model_.Lines().back().dialogue_prompt, "ordinary chat never acquires an NPC continuation marker");
            append(6, 150, "Final page.\x7F\x31");
            chat.SetDialogueActive(false);
            Check(!chat.DialoguePromptActive(6) && !second.DialoguePromptActive(6) && !chat.CollapsePinned(),
                "ending native dialogue stops both windows' animation and releases reactive sizing");
            chat.SetDialogueActive(true);
            append(7, 190, "Timed NPC redraw.\x7F\x34\x01");
            Check(!chat.model_.Lines().back().dialogue_prompt && !chat.DialoguePromptActive(7),
                "an explicit timed control overrides the mode-190 continuation fallback");
        }

        static auto ReactiveWindows(void) -> void
        {
            native_filter_records = {}; native_filter_writes = 0;
            const auto record = [](const uint32_t id, const int32_t value, const int32_t minimum, const int32_t maximum) {
                auto& entry = native_filter_records[id]; entry[1] = id; entry[2] = static_cast<uint32_t>(value);
                entry[3] = 4; entry[6] = static_cast<uint32_t>(minimum); entry[7] = static_cast<uint32_t>(maximum);
            };
            record(14, 0, 0, 1); record(15, 8, 4, 20); record(16, 2, 0, 20); record(17, 7, 3, 30);
            record(182, 1, 0, 1); record(183, 12, 4, 20); record(184, 0, 0, 20); record(198, -1, -1, 30);
            const std::array<uintptr_t, 2> manager{0, reinterpret_cast<uintptr_t>(native_filter_records.data())};
            const auto address = reinterpret_cast<uintptr_t>(manager.data());
            native_window_config native; native.config_slot_ = reinterpret_cast<uintptr_t>(&address); native.setter_ = 1;
            const auto first = native.ChatWindow(), second = native.ChatWindow(true);
            Check(first && first->reactive && first->minimum_lines == 2 && first->maximum_lines == 8 && first->resize_seconds == 7,
                "native Window 1 reactive sizing reads its validated settings");
            Check(second && !second->reactive && second->maximum_lines == 12 && second->resize_seconds == 7,
                "Window 2 inherits Window 1 resize delay when its native value is -1");
            native_filter_records[198][2] = 11;
            Check(native.ChatWindow(true)->resize_seconds == 11 && native_filter_writes == 0, "separate native Window 2 delay is read without changing the game");
            native_filter_records[184][7] = 99;
            Check(!native.ChatWindow(true), "unrecognized native record ranges disable following safely");

            ui::design_system design; chat_module chat;
            chat.ApplyTextDefaults(ui::theme::CreateDefault()); chat.state_.bounds = {20, 400, 560, 260};
            chat.collapse_mode_ = 2; chat.collapse_delay_ = 3; chat.collapse_max_lines_ = 8;
            const auto append = [&](const uint64_t sequence, const char* text, const uint32_t windows = 1) {
                chat_event event{}; event.sequence = sequence; event.timestamp = 1000; event.windows = windows; event.mode = 1;
                strcpy_s(event.text, text); chat.Receive(nullptr, event, false); chat.TrackCollapse(event, 0, 1000); return event;
            };
            append(1, "First message\nWrapped row"); append(2, "Second message"); auto third = append(3, "Third message");
            chat.UpdateCompactLayout(design, 1000);
            const auto original = chat.CompactBounds(design); const auto line = design.TextHeight(chat.text_.style) + chat.line_spacing_;
            Check(chat.collapse_visible_rows_ == 4 && original.height < 260 && original.y + original.height == 660,
                "reactive sizing uses live wrapped rows and preserves the saved bottom anchor");
            chat.UpdateCompactLayout(design, 3999);
            Check(chat.collapse_fade_sequence_ == 0 && chat.CompactBounds(design).height == original.height, "resize waits for the complete idle delay");
            chat.UpdateCompactLayout(design, 4110);
            Check(chat.collapse_fade_sequence_ == 1 && std::abs(chat.collapse_opacity_ - 0.5f) < 0.001f &&
                std::abs(chat.CompactBounds(design).height - (original.height - line)) < 0.01f,
                "all wrapped rows of the oldest message fade together while the window smoothly collapses");
            chat.UpdateCompactLayout(design, 4220);
            Check(chat.collapsed_through_ == 1 && chat.collapse_visible_rows_ == 2, "one whole message expires rather than the entire burst");
            const auto due = chat.collapse_due_; append(4, "Other native window", 2); chat.TrackCollapse(third, 1, 5000);
            Check(chat.collapse_due_ == due, "unmatched routes and duplicate delivery cannot revive expired messages or restart the delay");
            chat.UpdateCompactLayout(design, 7220);
            Check(chat.collapsed_through_ == 2 && chat.collapse_visible_rows_ == 1, "later messages retire one at a time at the configured interval");
            chat.Focus(true); chat.editor_.Set("Preserved draft"); chat.UpdateCompactLayout(design, 10000);
            Check(chat.CompactBounds(design).height == 260 && chat.editor_.Text() == "Preserved draft", "composition holds the full window without touching the draft");
            chat.Focus(false); chat.UpdateCompactLayout(design, 11000);
            Check(chat.collapsed_through_ == 2 && chat.collapse_due_ == 13000, "leaving composition restarts the idle wait");
            chat.history_view_.focused = true; chat.UpdateCompactLayout(design, 12000);
            Check(chat.FrameBounds(design).height == 260, "controller and keyboard history focus keep the window expanded");
            chat.history_view_ = {}; chat.tabs_[chat.active_tab_].scroll = 1; chat.UpdateCompactLayout(design, 13000);
            Check(chat.FrameBounds(design).height == 260, "scrollback stays open");
            chat.tabs_[chat.active_tab_].scroll = 0; chat.SetLayoutEditing(true); chat.UpdateCompactLayout(design, 14000);
            Check(chat.FrameBounds(design).height == 260, "layout editing exposes the complete saved frame");
            chat.SetLayoutEditing(false); chat.Clear(); chat.collapse_min_lines_ = 2; chat.UpdateCompactLayout(design, 15000);
            Check(std::abs(chat.collapse_height_rows_ - 2) < 0.001f && chat.FrameBounds(design).height > 2 * line,
                "minimum lines reserve space even with an empty history");
            append(5, "One message\nSecond wrapped row\nThird wrapped row"); chat.UpdateCompactLayout(design, 20000);
            Check(chat.collapse_visible_rows_ == 3 && chat.collapse_fade_sequence_ == 0,
                "a message crossing the minimum line boundary remains intact");
            chat.collapse_mode_ = 0; chat.SetNativeWindowConfig(first); chat.UpdateCompactLayout(design, 16000);
            const auto nativeDue = chat.collapse_due_; chat.SetNativeWindowConfig(first);
            Check(chat.collapse_due_ == nativeDue, "unchanged native polls cannot extend the idle deadline");
            chat.SetNativeWindowConfig(second); chat.UpdateCompactLayout(design, 17000);
            Check(chat.FrameBounds(design).height == 260, "native fixed sizing preserves the configured XIUI+ frame");
            const auto store = settings_store::FromIniText(""); chat.collapse_mode_ = 2; chat.collapse_delay_ = 9; chat.Save(store);
            chat_module restored; restored.Load(store);
            Check(restored.collapse_mode_ == 2 && restored.collapse_delay_ == 9 && restored.collapsed_through_ == 0 &&
                restored.ConfigSectionContains(2, restored.CollapseOptionStart()), "custom sizing persists without saving transient expiration state");
            RunChatStackChecks();
            chat_module lower, upper{true}; lower.AttachSecondWindow(upper);
            lower.ApplyTextDefaults(ui::theme::CreateDefault()); upper.ApplyTextDefaults(ui::theme::CreateDefault());
            lower.state_.bounds = {18, 400, 560, 260}; upper.state_.bounds = {18, 108, 560, 260};
            upper.state_.enabled = true; upper.defaults_initialized_ = true;
            lower.collapse_mode_ = 2; lower.collapse_min_lines_ = 1; lower.shared_windows_ = true;
            lower.UpdateCompactLayout(design, 1000);
            const auto lowerFrame = lower.FrameBounds(design), upperFrame = upper.FrameBounds(design);
            Check(std::abs(lowerFrame.y - (upperFrame.y + upperFrame.height) - 32) < 0.01f &&
                upperFrame.y > upper.state_.bounds.y && lower.state_.bounds.y == 400 && upper.state_.bounds.y == 108,
                "shared windows follow compact geometry while retaining the gap and saved positions");
            Check(upper.HitTest(design, upperFrame.x + 20, upperFrame.y + 20) &&
                !upper.HitTest(design, upperFrame.x + 20, 110), "pointer ownership follows the displayed stack");
            lower.SetLayoutEditing(true);
            Check(upper.FrameBounds(design).y == 108, "moving or resizing either chat window suspends shared stacking");
            lower.SetLayoutEditing(false); upper.SetStackPeerVisible(false);
            Check(upper.FrameBounds(design).y == 108, "a contextually hidden lower window cannot displace its peer");
            lower.Save(store); chat_module stackRestored; stackRestored.Load(store);
            Check(stackRestored.shared_windows_ && stackRestored.ConfigSectionContains(2, stackRestored.CollapseOptionStart() + 4),
                "shared stack preference persists in Layout without modifying chat positions");
        }

        static auto AnimationSpeed(void) -> void
        {
            chat_module primary, second{true};
            primary.message_entrances_.push_back({1, 1000});
            Check(std::abs(primary.MessageMotion(1, 1110).opacity - 0.5f) < 0.001f, "100% retains the original timing");
            const auto speedIndex = primary.AnimationOptionStart() + 1;
            auto option = primary.Option(speedIndex);
            Check(option.kind == module_option_kind::slider && option.minimum == 25 && option.maximum == 300 &&
                primary.ConfigSectionContains(8, speedIndex), "speed uses the shared slider in Animation");
            *option.slider = 200;
            Check(std::abs(primary.MessageMotion(1, 1055).opacity - 0.5f) < 0.001f && primary.MessageMotion(1, 1110).opacity == 1,
                "200% doubles animation speed");
            primary.message_animation_speed_ = 25;
            chat_event event{}; event.sequence = 2; event.mode = 1; event.windows = 1;
            primary.TrackMessageEntrance(event, 0, 1220);
            Check(primary.message_entrances_.size() == 2 && primary.MessageMotion(1, 1220).opacity < 1,
                "slower animations survive past the original expiration");
            primary.message_animation_ = 2; primary.OnOptionChanged(speedIndex);
            Check(primary.message_entrances_.empty() && primary.message_animation_ == 2, "speed edits preserve direction and clear active transitions");
            primary.message_animation_speed_ = 200; second.message_animation_speed_ = 50;
            const auto saved = settings_store::FromIniText(""); primary.Save(saved); second.Save(saved);
            chat_module loaded, loadedSecond{true}; loaded.Load(saved); loadedSecond.Load(saved);
            Check(loaded.message_animation_speed_ == 200 && loadedSecond.message_animation_speed_ == 50,
                "each window persists its own speed");
            loaded.ResetDefaults(ui::theme::CreateDefault());
            Check(loaded.message_animation_speed_ == 100, "defaults restore the original speed");
            loaded.Load(settings_store::FromIniText("[chat]\nmessage_animation_speed=nan\n"));
            Check(loaded.message_animation_speed_ == 100, "invalid speeds fall back to the original timing");
        }

        static auto MessageAnimation(void) -> void
        {
            chat_module primary, second{true}; primary.AttachSecondWindow(second);
            second.state_.enabled = true;
            chat_event event{}; event.sequence = 1; event.windows = 1; event.mode = 1;
            strcpy_s(event.text, "New message");
            primary.TrackMessageEntrance(event, 0, 1000);
            auto motion = primary.MessageMotion(1, 1000);
            Check(motion.opacity == 0 && motion.x == 0, "new messages start faded while the stack scrolls");
            motion = primary.MessageMotion(1, 1110);
            Check(std::abs(motion.opacity - 0.5f) < 0.001f, "all wrapped rows and spans share one fade curve");
            motion = primary.MessageMotion(1, 1220);
            Check(motion.opacity == 1 && motion.x == 0, "entrance settles at the normal text position");
            primary.message_animation_ = 2;
            motion = primary.MessageMotion(1, 1000);
            Check(motion.opacity == 0 && motion.x == -14, "slide right enters from the left");
            primary.message_animation_ = 0;
            Check(primary.MessageMotion(1, 1000).opacity == 1, "Off immediately restores normal presentation");
            primary.message_animation_ = 1; primary.message_entrances_.clear();
            primary.Receive(nullptr, event, false);
            Check(primary.message_entrances_.empty(), "restoring captured history never starts an animation");
            event.sequence = 2; primary.Receive(nullptr, event);
            Check(primary.message_entrances_.size() == 1 && second.message_entrances_.empty(), "new messages animate only in their matching visible window");
            const auto firstStart = primary.message_entrances_.front().started;
            primary.Receive(nullptr, event);
            Check(primary.message_entrances_.size() == 1 && primary.message_entrances_.front().started == firstStart, "duplicate delivery cannot restart an entrance");
            event.windows = 2; primary.Receive(nullptr, event);
            Check(primary.message_entrances_.size() == 1 && second.message_entrances_.size() == 1,
                "a second native-window delivery animates that window independently");
            primary.message_entrances_.clear(); primary.tabs_[primary.active_tab_].scroll = 1;
            event.sequence = 3; event.windows = 1; primary.TrackMessageEntrance(event, 0, 1000);
            Check(primary.message_entrances_.empty(), "scrolled history remains still while messages arrive");
            primary.tabs_[primary.active_tab_].scroll = 0; primary.history_view_.focused = primary.history_view_.expanded = true;
            primary.TrackMessageEntrance(event, 0, 1000);
            Check(primary.message_entrances_.empty(), "expanded history does not animate incoming lines");
            primary.history_view_ = {};
            for (uint64_t sequence = 4; sequence < 204; ++sequence) { event.sequence = sequence; primary.TrackMessageEntrance(event, 0, 1000); }
            Check(primary.message_entrances_.size() == 128, "chat bursts retain bounded animation state");
            event.sequence = 204; primary.TrackMessageEntrance(event, 0, 1220);
            Check(primary.message_entrances_.size() == 1, "expired animations leave the active set");
            primary.message_animation_ = 2; second.message_animation_ = 0;
            const auto saved = settings_store::FromIniText(""); primary.Save(saved); second.Save(saved);
            chat_module loaded, loadedSecond{true}; loaded.AttachSecondWindow(loadedSecond); loaded.Load(saved); loadedSecond.Load(saved);
            Check(loaded.message_animation_ == 2 && loadedSecond.message_animation_ == 0 && loaded.message_entrances_.empty(),
                "both windows persist independent choices with no transient animation state");
            loaded.ResetDefaults(ui::theme::CreateDefault());
            Check(loaded.message_animation_ == 1 && loaded.ConfigSectionContains(8, loaded.AnimationOptionStart()) &&
                loaded.Option(loaded.AnimationOptionStart()).choice_count == 3, "defaults and shared config expose all entrance choices");

            chat_module stack; stack.ApplyTextDefaults(ui::theme::CreateDefault());
            ui::design_system design;
            strcpy_s(event.text, "Old"); event.windows = 1;
            for (uint64_t sequence = 1; sequence <= 4; ++sequence) { event.sequence = sequence; stack.Receive(nullptr, event, false); }
            stack.RebuildRows(design, 100);
            const auto lineHeight = design.TextHeight(stack.text_.style) + stack.line_spacing_;
            const auto oldRows = stack.rows_.size();
            const auto oldTop = 200 - static_cast<float>(oldRows) * lineHeight;
            event.sequence = 5; strcpy_s(event.text, "New");
            stack.Receive(nullptr, event, false); stack.TrackMessageEntrance(event, 0, 1000); stack.RebuildRows(design, 100);
            const auto startOffset = stack.MessageScrollOffset(1000, lineHeight);
            Check(std::abs(startOffset - lineHeight) < 0.001f &&
                std::abs(200 - static_cast<float>(stack.rows_.size()) * lineHeight + startOffset - oldTop) < 0.001f,
                "a new message keeps existing rows at their previous positions when scrolling begins");
            Check(std::abs(stack.MessageScrollOffset(1110, lineHeight) - lineHeight * 0.5f) < 0.001f &&
                stack.MessageScrollOffset(1220, lineHeight) == 0, "the whole stack scrolls one row using the message fade timing");
            const auto beforeRows = stack.rows_.size();
            const auto beforeOffset = stack.MessageScrollOffset(1110, lineHeight);
            event.sequence = 6; strcpy_s(event.text, "A long incoming message that wraps across several rows of the chat window.");
            stack.Receive(nullptr, event, false); stack.TrackMessageEntrance(event, 0, 1110); stack.RebuildRows(design, 100);
            const auto wrappedRows = stack.rows_.size() - beforeRows;
            const auto burstOffset = stack.MessageScrollOffset(1110, lineHeight);
            Check(wrappedRows > 1 && std::abs(burstOffset - beforeOffset - wrappedRows * lineHeight) < 0.001f &&
                std::abs(-static_cast<float>(stack.rows_.size()) * lineHeight + burstOffset -
                    (-static_cast<float>(beforeRows) * lineHeight + beforeOffset)) < 0.001f,
                "wrapped arrivals preserve ongoing stack positions and scroll their full measured height");
            stack.message_animation_ = 2;
            Check(stack.MessageScrollOffset(1110, lineHeight) == burstOffset && stack.MessageMotion(6, 1110).x == -14,
                "horizontal entrances retain smooth vertical movement of the existing stack");
            stack.tabs_[stack.active_tab_].scroll = 1;
            Check(stack.MessageScrollOffset(1110, lineHeight) == 0, "browsing older messages holds the stack still");
            stack.tabs_[stack.active_tab_].scroll = 0; stack.history_view_.focused = stack.history_view_.expanded = true;
            Check(stack.MessageScrollOffset(1110, lineHeight) == 0, "expanded history holds the stack still");
            stack.history_view_ = {}; stack.message_animation_ = 0;
            Check(stack.MessageScrollOffset(1110, lineHeight) == 0, "Off keeps immediate message placement");

            const std::array<ui::textured_vertex, 4> quad{{{-10.5f, -10.5f, 0, 1, 0x00000000, 0, 0}, {9.5f, -10.5f, 0, 1, 0x00000000, 1, 0},
                {-10.5f, 9.5f, 0, 1, 0xFFFFFFFF, 0, 1}, {9.5f, 9.5f, 0, 1, 0xFFFFFFFF, 1, 1}}};
            std::vector<ui::textured_vertex> clipped;
            ui::AppendClippedQuad(clipped, quad, ui::rect{0, 0, 10, 10});
            Check(!clipped.empty() && clipped.size() % 3 == 0, "clipped glyphs and auto-translate art retain renderable geometry");
            for (const auto& vertex : clipped)
                Check(vertex.x >= -0.5f && vertex.y >= -0.5f && vertex.x <= 9.5f && vertex.y <= 9.5f && vertex.u >= 0.5f && vertex.v >= 0.5f,
                    "sliding glyphs and icons clip to the fixed log with matching texture coordinates");
            clipped.clear(); ui::AppendClippedQuad(clipped, quad, ui::rect{50, 50, 10, 10});
            Check(clipped.empty(), "art outside the log contributes no triangles");
            const std::array<ui::textured_vertex, 4> fading{{{-.5f, -.5f, 0, 1, 0xCC80A0C0, 0, 0}, {19.5f, -.5f, 0, 1, 0xCC80A0C0, 1, 0},
                {-.5f, 29.5f, 0, 1, 0xCC80A0C0, 0, 1}, {19.5f, 29.5f, 0, 1, 0xCC80A0C0, 1, 1}}};
            clipped.clear(); ui::AppendFadedQuad(clipped, fading, ui::rect{0, 0, 20, 30}, ui::vertical_fade{0, 12, .25f});
            bool fullOpacity{};
            for (const auto& vertex : clipped)
            {
                const float y = vertex.y + .5f;
                const auto alpha = vertex.color >> 24;
                Check((vertex.color & 0xFFFFFF) == 0x80A0C0 && std::abs(vertex.v - y / 30) < .001f &&
                    std::abs(static_cast<float>(alpha) - 204 * (.25f + .75f * std::clamp(y / 12, 0.0f, 1.0f))) < 1.1f,
                    "top fade changes only alpha and preserves glyph/icon UVs across its boundary");
                if (y >= 12) fullOpacity = fullOpacity || alpha == 204;
            }
            Check(fullOpacity, "messages below the top fade retain their full opacity");
        }

        inline static std::array<std::array<uint32_t, 11>, 207> native_filter_records{};
        inline static size_t native_filter_writes{};

        static auto __cdecl SetNativeFilter(const uint32_t id, const int32_t value) -> int32_t
        {
            if (id >= native_filter_records.size()) return -1;
            native_filter_records[id][2] = static_cast<uint32_t>(value);
            ++native_filter_writes;
            return 0;
        }

        static auto NativeFilters(void) -> void
        {
            native_filter_records = {}; native_filter_writes = 0;
            for (const auto id : {50u, 51u, 52u, 53u, 71u, 185u, 186u, 187u, 188u})
            {
                auto& record = native_filter_records[id];
                record[1] = id; record[3] = 4;
                record[7] = id == 53 ? 3 : id == 71 ? 1 : UINT32_MAX;
            }
            const std::array<uintptr_t, 2> configObject{0, reinterpret_cast<uintptr_t>(native_filter_records.data())};
            const auto configAddress = reinterpret_cast<uintptr_t>(configObject.data());
            native_window_config config;
            config.config_slot_ = reinterpret_cast<uintptr_t>(&configAddress);
            config.setter_ = reinterpret_cast<uintptr_t>(&SetNativeFilter);
            Check(config.ChatFilterValue(185) == 0 && !config.SetChatFilterValue(53, 4) && !config.SetChatFilterValue(71, 2) &&
                !config.ChatFilterValue(18) && native_filter_writes == 0, "native filter access validates supported records and ranges before writes");

            native_chat_filters filters;
            filters.config_ = &config; filters.definitions_available_ = true;
            for (auto& entry : filters.filters_) entry = {51, 0, 0};
            filters.filters_[0] = {0, 0, 0}; filters.filters_[1] = {51, 2, 2}; filters.filters_[8] = {3, 5, 9};
            filters.filters_[11] = {4, 11, 11}; filters.filters_[39] = {30, 11, 175};
            filters.filters_[52] = {51, -1, 33}; filters.filters_[53] = {51, -2, 34}; filters.filters_[54] = {51, -3, 35};
            filters.Refresh();
            chat_module module; module.SetNativeFilters(filters);
            const auto start = module.NativeFiltersStart();
            *module.Option(start).choice = 1; module.OnOptionChanged(start);
            Check(native_filter_writes == 0 && module.ConfigSectionContains(7, start + 66) && !module.ConfigSectionContains(7, start + 9),
                "native group selection changes visible controls without copying or changing filters");
            native_filter_records[185][2] = 1;
            filters.Refresh();
            native_filter_records[185][2] |= 1u << 4;
            *module.Option(start + 66).toggle = true; module.OnOptionChanged(start + 66);
            Check(native_filter_records[185][2] == (1u | 1u << 4 | 1u << 7) && native_filter_records[187][2] == 0,
                "enabling Window 1 emotes preserves newly changed neighboring bits and Window 2");
            native_filter_records[187][2] = 0x80000000;
            const auto writes = native_filter_writes;
            *module.Option(start).choice = 2; module.OnOptionChanged(start);
            Check(native_filter_writes == writes && !*module.Option(start + 66).toggle,
                "Window 2 selection reads its own native values without overwriting either window");

            *module.Option(start).choice = 0; module.OnOptionChanged(start);
            native_filter_records[50][2] = 1u << 29; native_filter_records[52][2] = 1u << 9;
            *module.Option(start + 9).choice = 1; module.OnOptionChanged(start + 9);
            Check(native_filter_records[50][2] == (1u << 29 | 1u << 3) && native_filter_records[52][2] == 1u << 9,
                "Hide maps to native filtering without touching unrelated mask or Hold bits");
            *module.Option(start + 9).choice = 2; module.OnOptionChanged(start + 9);
            Check(native_filter_records[50][2] == 1u << 29 && native_filter_records[52][2] == (1u << 9 | 1u << 5),
                "Hold clears the native hide bit and changes only that channel's Hold bit");
            *module.Option(start + 9).choice = 0; module.OnOptionChanged(start + 9);
            Check(native_filter_records[52][2] == 1u << 9, "Show clears the selected channel's Hide and Hold settings");
            *module.Option(start + 2).choice = 3; module.OnOptionChanged(start + 2);
            Check(native_filter_records[71][2] == 1 && (native_filter_records[52][2] & 4) != 0,
                "Tell combines native notification and Hold without inventing a suppression mode");
            *module.Option(start + 40).choice = 1; module.OnOptionChanged(start + 40);
            Check(native_filter_records[51][2] == 1, "native global mask two begins at filter bit thirty");
            *module.Option(start + 55).choice = 1; module.OnOptionChanged(start + 55);
            Check(native_filter_records[53][2] == 3 && filters.values_[52] == 1 && filters.values_[53] == 1,
                "system filter levels preserve the native cumulative behavior");
            *module.Option(start + 54).choice = 0; module.OnOptionChanged(start + 54);
            Check(native_filter_records[53][2] == 1 && filters.values_[54] == 0, "showing a system level also shows higher levels");
            native_filter_records[50][1] = 0;
            filters.Refresh();
            Check(!module.Option(start + 9).available, "unavailable native settings disable edits");

            chat_module second{true}; module.AttachSecondWindow(second); second.SetNativeFilters(filters);
            second.match_window1_ = true;
            const auto secondStart = second.NativeFiltersStart();
            Check(*second.Option(secondStart).choice == 2 && *module.Option(start).choice == 0,
                "Window 2 starts on its own routing group without changing Window 1's selected global filters");
            const auto firstMask = native_filter_records[185][2], secondMask = native_filter_records[187][2];
            const auto beforeGroups = native_filter_writes;
            *module.Option(start).choice = 1; module.OnOptionChanged(start);
            Check(*second.Option(secondStart).choice == 2 && native_filter_writes == beforeGroups,
                "switching Window 1's filter view never retargets Window 2 or changes native values");
            const auto party = secondStart + 1 + native_chat_filters::global_count + 4;
            *second.Option(party).toggle = true; second.OnOptionChanged(party);
            Check(native_filter_records[187][2] == (secondMask | 4) && native_filter_records[185][2] == firstMask &&
                *second.Option(party).toggle && *module.Option(start).choice == 1,
                "Window 2 Party edits reach only native Window 2, including when appearance follows Window 1");
            *second.Option(party).toggle = false; second.OnOptionChanged(party);
            Check(native_filter_records[187][2] == (secondMask & ~4u) && native_filter_records[185][2] == firstMask &&
                !*second.Option(party).toggle,
                "disabling a Window 2 category reads back immediately and preserves other routing bits");

            ui::design_system design;
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            for (const auto mode : {7u, 15u})
            {
                chat_event event{}; event.sequence = mode; event.windows = 1; event.mode = mode;
                strcpy_s(event.text, "Example waves."); module.Receive(nullptr, event, false);
                Check(ChatTypeIndex(mode) == 9 && ChatHistoryMatches(static_cast<size_t>(chat_history_tab::emotes), mode, 1) &&
                    !ChatHistoryMatches(static_cast<size_t>(chat_history_tab::emotes), mode, 0),
                    "incoming and outgoing emotes use their own history category and retain native admission");
            }
            module.RebuildRows(design, 1500);
            Check(module.Count() == 2 && module.rows_.size() == 2 && module.rows_[1].spans[0].text.starts_with("Example"),
                "both admitted native emote modes reach the normal XIUI+ log");
        }

        static auto ExpandedHistory(void) -> void
        {
            hud_runtime runtime; auto& chat = runtime.chat_;
            runtime.chat_second_.State().enabled = true;
            chat.ApplyTextDefaults(ui::theme::CreateDefault());
            chat.SetHistoryViewport({0, 0, 1600, 900});
            const auto initial = chat.State().bounds;
            chat.editor_.Set("unsent draft");
            const auto key = [&](const WPARAM value, const LPARAM flags = 0) {
                const bool consumed = runtime.HandleChatKey(value, flags, false, true, false, 0);
                if (consumed && value == VK_RETURN && (static_cast<uint32_t>(flags) & 0xC0000000u) == 0)
                    runtime.HandleKey(value, static_cast<LPARAM>(0xC0000000u), false);
                return consumed;
            };
            Check(!runtime.HandleChatKey('F', 0, true, true, false, 0) && !runtime.HandleChatKey('F', 0, false, false, false, 0) &&
                !chat.HistoryFocused(), "F respects existing input owners and native chat when replacement is off");
            Check(!key('F') && !chat.HistoryFocused() && !chat.HandleHistoryKey('F', 0, false, true, runtime.design_system_),
                "an unfocused F stays native through both runtime and module input routes");
            Check(chat.BeginHistory() && chat.HistoryFocused() && !chat.HistoryExpanded() && !chat.Focused() && runtime.ChatFocused(),
                "explicit history focus selects the log without opening a message editor");
            Check(key(VK_RETURN) && chat.HistoryExpanded() && chat.editor_.Text() == "unsent draft", "Enter expands history without submitting or changing a draft");
            const auto layout = chat.HistoryLayout(runtime.design_system_);
            Check(layout.frame.width > 1500 && layout.frame.height > 800 && layout.log.y < layout.footer.y && layout.tabs.size() == 12,
                "expanded history fills the viewport with all requested channel tabs above the frame");
            for (const auto& tab : layout.tabs)
                Check(tab.x >= 0 && tab.x + tab.width <= 1600 && tab.y + tab.height < layout.frame.y,
                    "all channel tab hit areas fit above the expanded log");
            {
                const auto compactTransform = chat.State().transform;
                const auto same = [](const ui::rect& a, const ui::rect& b) {
                    return std::abs(a.x - b.x) < .01f && std::abs(a.y - b.y) < .01f &&
                        std::abs(a.width - b.width) < .01f && std::abs(a.height - b.height) < .01f;
                };
                const auto inside = [](const ui::rect& outer, const ui::rect& inner) {
                    return inner.x >= outer.x - .01f && inner.y >= outer.y - .01f &&
                        inner.x + inner.width <= outer.x + outer.width + .01f && inner.y + inner.height <= outer.y + outer.height + .01f;
                };
                for (const auto viewport : {ui::rect{0, 0, 1600, 900}, ui::rect{40, 28, 960, 640}})
                    for (const float scale : {.75f, 1.0f, 1.5f})
                    {
                        ui::rect baseline{};
                        for (const auto position : {ui::point{-250, -100}, ui::point{1400, 800}, ui::point{5000, -3000}})
                        {
                            chat.State().bounds.x = position.x; chat.State().bounds.y = position.y;
                            chat.State().transform = ui::hud_transform::At(position.x, position.y, scale, scale);
                            chat.SetHistoryViewport(viewport);
                            chat.RenderHistory({}, module_render_context{runtime.design_system_, nullptr, false, nullptr, viewport});
                            const auto moved = chat.HistoryLayout(runtime.design_system_);
                            const auto frame = chat.State().transform.Map(moved.frame);
                            Check(same(chat.history_view_.viewport, chat.State().transform.Inverse(viewport)) && inside(viewport, frame),
                                "expanded rendering converts the viewport once and keeps its full frame on screen");
                            if (baseline.width == 0) baseline = frame;
                            Check(same(frame, baseline) && same(chat.ControllerBounds(runtime.design_system_), frame),
                                "moving compact chat never moves the expanded frame or its controller focus");
                            for (const auto& tab : moved.tabs)
                                if (tab.height > 0) Check(inside(viewport, chat.State().transform.Map(tab)), "expanded channel tabs stay on screen after moving chat");
                            const auto say = chat.State().transform.Map(moved.tabs[static_cast<size_t>(chat_history_tab::say)]);
                            bool save = false;
                            Check(chat.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, say.x + 2, say.y + 2, false, runtime.design_system_, viewport, save) &&
                                chat.HandleMouse(WM_LBUTTONUP, 0, say.x + 2, say.y + 2, false, runtime.design_system_, viewport, save) &&
                                chat.history_view_.tab == static_cast<size_t>(chat_history_tab::say) && !save,
                                "moved-chat history tabs remain clickable at their rendered screen positions");
                        }
                    }
                chat.State().bounds = initial; chat.State().transform = compactTransform;
                chat.SetHistoryViewport({0, 0, 1600, 900}); chat.SelectHistoryTab(0);
            }
            constexpr uint32_t modes[]{20, 20, 9, 12, 13, 14, 214, 222, 220, 212, 10, 15};
            for (size_t tab = 0; tab < std::size(chat_history_tabs); ++tab)
            {
                const uint32_t window = tab == static_cast<size_t>(chat_history_tab::window2) ? 2 : 1;
                Check(ChatHistoryMatches(tab, modes[tab], window) && !ChatHistoryMatches(tab, modes[tab], 0), "each history tab matches its admitted native channel");
                if (tab < 2) Check(!ChatHistoryMatches(tab, modes[tab], window ^ 3), "Window 1 and Window 2 retain separate native routing");
                else Check(!ChatHistoryMatches(tab, modes[tab] == 9 ? 12 : 9, 1), "channel histories remain separate");
                Check(key('F') && chat.history_view_.tab == (tab + 1) % std::size(chat_history_tabs), "F cycles the complete tab strip and wraps");
            }
            {
                const keyboard_restore shift{false, false, true};
                Check(key('F') && chat.history_view_.tab == static_cast<size_t>(chat_history_tab::emotes), "Shift+F wraps backward to Emotes");
            }
            Check(key('F') && chat.history_view_.tab == 0, "F wraps from Emotes to Window 1");
            {
                const auto window2 = static_cast<size_t>(chat_history_tab::window2), say = static_cast<size_t>(chat_history_tab::say);
                runtime.chat_second_.State().enabled = false;
                const auto hidden = chat.HistoryLayout(runtime.design_system_);
                Check(hidden.tabs[window2].height == 0 && hidden.tabs[say].x == layout.tabs[window2].x,
                    "disabled Window 2 has no tab or empty space in the expanded strip");
                Check(key('F') && chat.history_view_.tab == say, "F skips disabled Window 2");
                {
                    const keyboard_restore shift{false, false, true};
                    Check(key('F') && chat.history_view_.tab == 0, "Shift+F skips disabled Window 2 in reverse");
                }
                Check(chat.HandleControllerAction(controller_action::next_tab, false, runtime.design_system_) && chat.history_view_.tab == say &&
                    chat.HandleControllerAction(controller_action::previous_tab, false, runtime.design_system_) && chat.history_view_.tab == 0,
                    "controller navigation skips the same unavailable tab in both directions");
                Check(chat.HistorySoundTarget(runtime.design_system_, hidden.tabs[say].x + 2, hidden.tabs[say].y + 2) == 200 + say,
                    "mouse hit testing follows the compacted visible tab layout");
                runtime.chat_second_.State().enabled = true;
                std::array<std::array<uint32_t, 11>, 189> records{};
                for (const auto id : {187u, 188u}) { records[id][1] = id; records[id][3] = 4; records[id][7] = UINT32_MAX; }
                records[180][1] = 180; records[180][2] = 1; records[180][3] = 4; records[180][7] = 2;
                const std::array<uintptr_t, 2> object{0, reinterpret_cast<uintptr_t>(records.data())};
                const auto address = reinterpret_cast<uintptr_t>(object.data());
                native_window_config config; config.config_slot_ = reinterpret_cast<uintptr_t>(&address); config.setter_ = 1;
                native_chat_filters routing; routing.config_ = &config;
                chat.SetNativeFilters(routing);
                runtime.chat_second_.SetNativeFilters(routing);
                Check(routing.WindowHasRouting(true) == false && !chat.HistoryTabAvailable(window2),
                    "an enabled Window 2 with no configured message routing stays hidden");
                records[187][2] = 1;
                Check(routing.WindowHasRouting(true) == true && chat.Count() == 0 && chat.HistoryTabAvailable(window2),
                    "configured but quiet Window 2 remains available before receiving any messages");
                Check(key('F') && chat.history_view_.tab == window2, "newly configured Window 2 joins normal tab cycling");
                records[187][2] = 0;
                chat.HandleControllerAction(controller_action::next_tab, true, runtime.design_system_);
                Check(chat.history_view_.tab == 0, "removing selected Window 2 routing returns to Window 1 without repeating navigation");
                records[188][2] = 2;
                Check(chat.HistoryTabAvailable(window2), "battle routing in the high mask also configures Window 2");
                records[180][2] = 0;
                const auto secondBounds = runtime.chat_second_.State().bounds;
                Check(!chat.HistoryTabAvailable(window2) && !runtime.chat_second_.WindowVisible() && !runtime.chat_second_.ShouldRender({}) &&
                    !runtime.chat_second_.HasEditableBounds() && runtime.chat_second_.State().enabled && chat.ShouldRender({}),
                    "native single-window mode hides Window 2 rendering, browsing and edit controls without changing its saved enable choice");
                records[180][2] = 1;
                Check(runtime.chat_second_.WindowVisible() && runtime.chat_second_.ShouldRender({}) && chat.HistoryTabAvailable(window2) &&
                    runtime.chat_second_.State().bounds.x == secondBounds.x && runtime.chat_second_.State().bounds.y == secondBounds.y &&
                    runtime.chat_second_.State().bounds.width == secondBounds.width && runtime.chat_second_.State().bounds.height == secondBounds.height,
                    "restoring native multi-window restores Window 2 without changing its saved position or size");
                records[180][2] = 1; records[187][1] = 0;
                Check(!routing.WindowHasRouting(true).has_value() && chat.HistoryTabAvailable(window2),
                    "unavailable native routing does not hide an enabled user's window");
                chat.SetNativeFilters(runtime.chat_filters_);
                runtime.chat_second_.SetNativeFilters(runtime.chat_filters_);
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
            const auto say = full.tabs[static_cast<size_t>(chat_history_tab::say)];
            Check(chat.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, say.x + 2, say.y + 2, false, runtime.design_system_, chat.history_view_.viewport, save) &&
                chat.history_view_.tab == static_cast<size_t>(chat_history_tab::say) && chat.HandleMouse(WM_LBUTTONUP, 0, say.x + 2, say.y + 2, false, runtime.design_system_, chat.history_view_.viewport, save),
                "history tabs accept and retain a complete captured pointer click");
            const auto retainedTab = chat.history_view_.tab, retainedScroll = chat.CurrentScroll();
            Check(!key(VK_TAB, 0x20000000) && chat.HistoryExpanded(), "Alt+Tab leaves the expanded log visible and yields the shortcut");
            runtime.controller_surface_ = hud_runtime::controller_surface::chat;
            runtime.ReleaseControllerFocus(true); runtime.ReleaseChatInput(true);
            Check(chat.HistoryExpanded() && chat.history_view_.tab == retainedTab && chat.CurrentScroll() == retainedScroll &&
                chat.editor_.Text() == "unsent draft" && !runtime.KeyboardFocused() && !chat.Resizing() &&
                runtime.controller_surface_ == hud_runtime::controller_surface::none && runtime.chat_pointer_ == nullptr,
                "background cleanup releases input while retaining the expanded log, tab, scroll and draft");
            Check(key(VK_ESCAPE) && chat.HistoryFocused() && runtime.ChatFocused() && !chat.HistoryExpanded() && chat.editor_.Text() == "unsent draft" &&
                chat.State().bounds.x == initial.x && chat.State().bounds.y == initial.y && chat.State().bounds.width == initial.width && chat.State().bounds.height == initial.height,
                "Escape returns expanded history to the focused compact XIUI+ chat without altering its draft or layout");
            Check(key(VK_ESCAPE, 0x40000000) && chat.HistoryFocused(), "holding Escape cannot also leave compact chat focus");
            Check(key(VK_RETURN) && chat.HistoryExpanded() && chat.history_view_.tab == 0 && key(VK_ESCAPE) && chat.HistoryFocused(),
                "Enter reopens XIUI+ history on its first tab without focusing native chat");
            Check(key(VK_ESCAPE) && !runtime.ChatFocused(), "a second fresh Escape leaves compact chat focus");
            runtime.native_ui_.menu_visible = true;
            Check(!runtime.HandleChatKey('F', 0, false, true, true, 0) && !chat.HistoryFocused(), "an unfocused F stays native during target selection");
            Check(chat.BeginHistory(), "explicit history entry remains available");
            runtime.SyncNativeChatInput(nullptr, true, true);
            Check(chat.HistoryFocused() && runtime.HandleChatKey(VK_RETURN, 0, false, true, true, 0) && chat.HistoryExpanded(),
                "native menu polling cannot steal focused history or its expand key");
            key(VK_ESCAPE); runtime.native_ui_.menu_visible = false;
            Check(key(VK_OEM_2) && chat.Focused() && !chat.HandleHistoryKey('F', 0, false, true, runtime.design_system_),
                "F remains ordinary text when the message editor is focused");
            chat.Focus(false);
            auto& second = runtime.chat_second_;
            const auto cyclingDraft = chat.editor_.Text();
            second.State().enabled = true;
            second.history_view_.tab = 3; second.history_view_.scroll[3] = 7;
            Check(chat.BeginHistory() && chat.HistoryFocused() && !second.HistoryFocused(), "explicit entry selects the first visible chat window");
            Check(key('F', 0x40000000) && chat.HistoryFocused() && !second.HistoryFocused(), "held F cannot cycle windows repeatedly");
            chat.SelectHistoryTab(static_cast<size_t>(chat_history_tab::window1));
            chat_event windowTwo{101, ::GetTickCount64(), 20, 0xFFFFFFFF, 2}; strcpy_s(windowTwo.text, "Window two message");
            chat.Receive(nullptr, windowTwo, false);
            Check(key(VK_RETURN) && key('F') && chat.HistoryExpanded() && !second.HistoryFocused() &&
                chat.history_view_.tab == static_cast<size_t>(chat_history_tab::window2),
                "Window 2 is the second F tab in the expanded log, without changing focused HUD windows");
            chat.RebuildRows(runtime.design_system_, 800);
            Check(chat.rows_.size() == 1 && chat.rows_.front().sequence == windowTwo.sequence,
                "the expanded Window 2 tab shows only messages routed to the second native log");
            runtime.SyncNativeChatInput(nullptr, true, true);
            runtime.UpdateControllerFocus();
            Check(chat.HistoryExpanded() && chat.history_view_.tab == static_cast<size_t>(chat_history_tab::window2),
                "native combat-menu polling retains the expanded Window 2 tab");
            Check(key(VK_ESCAPE) && chat.HistoryFocused() && !chat.HistoryExpanded() && !second.HistoryFocused() && runtime.ChatFocused(),
                "Escape from the Window 2 tab returns to compact chat");
            Check(key(VK_ESCAPE, 0x40000000) && chat.HistoryFocused() && chat.editor_.Text() == cyclingDraft,
                "held Escape retains compact focus and the shared draft");
            Check(key(VK_ESCAPE) && !runtime.ChatFocused(), "a second fresh Escape releases compact chat focus");
            auto& player = const_cast<party_member&>(runtime.party_source_.Members()[0]);
            player.active = player.buffs_known = true; player.id = 50;
            player.buffs = {33, 40, 2}; player.buff_count = 3;
            Check(chat.BeginHistory() && key('F') && runtime.buffs_.ControllerFocused() && !second.HistoryFocused() &&
                !runtime.ChatFocused() && second.history_view_.tab == 3 && second.history_view_.scroll[3] == 7 && chat.editor_.Text() == cyclingDraft,
                "the second compact F selects Buffs directly, retaining both windows' tabs and the shared draft");
            Check(runtime.ControllerCapture() == controller_capture_mode::navigation &&
                (runtime.ControllerInputState() & controller_capture_mask) == static_cast<uint32_t>(controller_capture_mode::navigation) &&
                runtime.KeyboardInputState() == keyboard_capture_status && !runtime.KeyboardFocused() &&
                !key('W') && !key(VK_NUMPAD8) && runtime.buffs_.ControllerFocused(),
                "keyboard Buffs highlighting keeps controller sticks and keyboard movement available outside native menus");
            const auto firstBuff = runtime.buffs_.ControllerBounds();
            Check(key(VK_RIGHT) && runtime.buffs_.ControllerBounds().x > firstBuff.x,
                "keyboard arrows move between the actual XIUI+ effect icons");
            controller_status unavailable{}; unavailable.suspended = unavailable.enumeration_disabled = true;
            unavailable.connection_known = unavailable.configuration_known = true;
            runtime.UpdateControllerStatus(unavailable);
            runtime.native_ui_.menu_visible = true;
            runtime.UpdateControllerFocus();
            Check(runtime.buffs_.ControllerFocused() && runtime.KeyboardStatusFocusBounds().width > 0 &&
                !runtime.ControllerCaptured() && key(VK_LEFT) && runtime.buffs_.ControllerBounds().x == firstBuff.x,
                "keyboard status selection and arrows survive frame updates without controller capture");
            runtime.native_ui_.menu_visible = false;
            player.buffs = {40, 2}; player.buff_count = 2;
            runtime.UpdateControllerFocus();
            Check(runtime.buffs_.ControllerFocused() && runtime.KeyboardStatusFocusBounds().x == firstBuff.x,
                "an expired selected buff transfers focus to the remaining icon instead of restarting the window cycle");
            player.buffs = {33, 40, 2}; player.buff_count = 3;
            runtime.UpdateControllerFocus();
            Check(runtime.buffs_.ControllerBounds().x > firstBuff.x,
                "the replacement selection follows its effect when the list changes again");
            runtime.UpdateControllerStatus({});
            Check(key('F', 0x40000000) && runtime.buffs_.ControllerFocused(), "held F cannot skip a status group");
            Check(key('F') && runtime.debuffs_.ControllerFocused() && !runtime.buffs_.ControllerFocused(),
                "F continues from XIUI+ Buffs to XIUI+ Debuffs");
            Check(runtime.ControllerCapture() == controller_capture_mode::navigation && !runtime.KeyboardFocused(),
                "keyboard Debuffs highlighting also keeps movement available outside native menus");
            runtime.UpdateControllerStatus(unavailable);
            runtime.UpdateControllerFocus();
            Check(runtime.debuffs_.ControllerFocused() && runtime.KeyboardStatusFocusBounds().width > 0,
                "keyboard Debuffs focus survives the same unavailable-gamepad frame updates");
            runtime.UpdateControllerStatus({});
            Check(key(VK_ESCAPE) && !runtime.ControllerCaptured() && !runtime.ChatFocused(), "Escape releases keyboard status inspection");
            second.State().enabled = false;
            Check(chat.BeginHistory() && chat.HistoryFocused() && key('F') && runtime.buffs_.ControllerFocused(),
                "with Window 2 disabled, the second F selects XIUI+ Buffs");
            runtime.buffs_.State().enabled = false;
            Check(key('F') && runtime.debuffs_.ControllerFocused(), "disabled status modules are skipped without native focus");
            player.buff_count = 0;
            key(VK_ESCAPE);
            Check(chat.BeginHistory() && key('F') && chat.HistoryFocused() && !runtime.buffs_.ControllerFocused() && !runtime.debuffs_.ControllerFocused(),
                "empty status groups are skipped and focus wraps within XIUI+");
            key(VK_ESCAPE);
            player.buff_count = 3; runtime.buffs_.State().enabled = true;
            second.State().enabled = true;
            const auto cycle = [&](const controller_action action, const bool repeated = false) {
                return runtime.HandleController({action, controller_backend::xinput, 0, true, repeated, false, 100});
            };
            Check(cycle(controller_action::cycle_window) && chat.HistoryFocused(), "controller cycle starts with Window 1");
            Check(cycle(controller_action::cycle_window, true) && chat.HistoryFocused(), "held controller cycle cannot change windows");
            Check(cycle(controller_action::confirm) && chat.HistoryExpanded(), "controller Confirm expands the shared chat log");
            chat.SelectHistoryTab(static_cast<size_t>(chat_history_tab::window1));
            Check(cycle(controller_action::cycle_window) && chat.HistoryExpanded() && !second.HistoryFocused() &&
                chat.history_view_.tab == static_cast<size_t>(chat_history_tab::window2),
                "controller cycle selects Window 2 inside the expanded log");
            runtime.SyncNativeChatInput(nullptr, true);
            runtime.UpdateControllerFocus();
            Check(chat.HistoryExpanded(), "native combat-menu polling retains controller focus on the expanded log");
            for (size_t tab = 2; tab < std::size(chat_history_tabs); ++tab)
                Check(cycle(controller_action::cycle_window) && chat.HistoryExpanded() && chat.history_view_.tab == tab,
                    "controller tab navigation reaches every channel including Shout and Emotes");
            Check(cycle(controller_action::cycle_window) && chat.history_view_.tab == 0, "controller cycling wraps from Emotes to Window 1");
            chat.SelectHistoryTab(static_cast<size_t>(chat_history_tab::emotes));
            Check(cycle(controller_action::confirm, true) && chat.HistoryExpanded() &&
                chat.history_view_.tab == static_cast<size_t>(chat_history_tab::emotes), "held Confirm retains the current expanded tab");
            Check(cycle(controller_action::confirm) && !chat.HistoryExpanded() && cycle(controller_action::confirm) &&
                chat.HistoryExpanded() && chat.history_view_.tab == 0, "controller reopening also starts expanded history on its first tab");
            cycle(controller_action::cancel);
            runtime.native_ui_.menu_visible = true;
            Check(cycle(controller_action::cycle_window) && runtime.buffs_.ControllerFocused(), "controller cycle reaches Buffs directly after compact Chat");
            runtime.SyncNativeChatInput(nullptr, true);
            runtime.UpdateControllerFocus();
            const auto controllerBuff = runtime.buffs_.ControllerBounds();
            Check(runtime.ControllerCapture() == controller_capture_mode::navigation && controllerBuff.width > 0 &&
                cycle(controller_action::right) && runtime.buffs_.ControllerBounds().x > controllerBuff.x,
                "controller Buffs focus stays visible and navigable over the combat menu without capturing movement sticks");
            runtime.native_ui_.menu_visible = false;
            Check(runtime.ControllerCapture() == controller_capture_mode::navigation &&
                cycle(controller_action::left) && runtime.buffs_.ControllerBounds().x == controllerBuff.x,
                "closing the combat menu keeps Buffs navigation without taking movement sticks");
            runtime.native_ui_.menu_visible = true;
            Check(cycle(controller_action::cycle_window) && runtime.debuffs_.ControllerFocused(), "controller cycle continues to XIUI+ Debuffs");
            runtime.SyncNativeChatInput(nullptr, true);
            runtime.UpdateControllerFocus();
            Check(runtime.ControllerCapture() == controller_capture_mode::navigation && runtime.debuffs_.ControllerBounds().width > 0,
                "controller Debuffs focus retains navigation capture over the combat menu");
            runtime.native_ui_.menu_visible = false;
            Check(runtime.ControllerCapture() == controller_capture_mode::navigation && runtime.debuffs_.ControllerFocused(),
                "closing the combat menu keeps Debuffs highlighting without taking movement sticks");
            runtime.native_ui_.menu_visible = true;
            Check(cycle(controller_action::cycle_window) && chat.HistoryFocused() && !second.HistoryFocused(),
                "controller focus wraps from status effects to Chat 1");
            runtime.native_ui_.menu_visible = false;
            cycle(controller_action::cancel);
            Check(second.BeginHistory() && second.HistoryFocused() &&
                !runtime.HandleChatKey('F', 0, false, true, false, 0x11) && !runtime.ChatFocused() &&
                chat.editor_.Text() == cyclingDraft, "native text input releases both histories without changing the draft");
            Check(chat.BeginHistory() && key('F') && runtime.buffs_.ControllerFocused() &&
                !runtime.HandleChatKey(VK_RIGHT, 0, false, true, false, 0x11) && !runtime.ControllerCaptured(),
                "native text ownership releases status inspection before handling navigation");
            runtime.editing_ = true;
            Check(!key('F') && !chat.HistoryFocused() && !second.HistoryFocused(), "config editing keeps F away from both chat histories");
            runtime.editing_ = false;

            key(VK_ESCAPE); runtime.ReleaseControllerFocus();
            chat.State().enabled = second.State().enabled = false;
            runtime.native_ui_.menu_visible = false;
            runtime.game_hud_.Load(settings_store::FromIniText("[game_hud]\nenabled=1\nshow_chat1=1\nshow_status=0\n"));
            Check(runtime.ControllerNativeCycle() && (runtime.ControllerInputState() & controller_native_cycle) != 0 &&
                !cycle(controller_action::cycle_window) && !runtime.buffs_.ControllerFocused(),
                "with XIUI+ chat off and the native log shown, controller cycle leaves log selection to the game");
            Check(runtime.BeginControllerFocus(hud_runtime::controller_surface::buffs) && !runtime.ControllerNativeCycle() &&
                cycle(controller_action::cycle_window) && runtime.debuffs_.ControllerFocused(),
                "controller cycle still moves from XIUI+ Buffs to Debuffs beside the native log");
            Check(runtime.ControllerNativeCycle() && (runtime.ControllerInputState() & controller_native_cycle) != 0 &&
                !cycle(controller_action::cycle_window), "controller cycle from the last status group returns to the native log");
            runtime.ReleaseControllerFocus();
            runtime.game_hud_.Load(settings_store::FromIniText("[game_hud]\nenabled=1\nshow_chat1=0\nshow_status=0\n"));
            Check(!runtime.ControllerNativeCycle() && cycle(controller_action::cycle_window) && runtime.buffs_.ControllerFocused(),
                "with no chat log shown, controller cycle still reaches XIUI+ Buffs");
            runtime.ReleaseControllerFocus();
        }

        static auto SingleWindowHistory(void) -> void
        {
            {
                hud_runtime single;
                auto& combined = single.chat_;
                single.chat_second_.State().enabled = false;
                combined.ApplyTextDefaults(ui::theme::CreateDefault());
                combined.timestamp_display_ = 1;
                *combined.Option(8).choice = 2; combined.OnOptionChanged(8);
                combined.editor_.Set("keep this draft");
                chat_event message{}; message.sequence = 1; message.windows = 1; message.mode = 1;
                strcpy_s(message.text, "First log"); combined.Receive(nullptr, message, false);
                message.sequence = 2; message.windows = 2; strcpy_s(message.text, "Second log"); combined.Receive(nullptr, message, false);
                combined.history_view_.focused = combined.history_view_.expanded = true;
                combined.RebuildRows(single.design_system_, 500);
                Check(combined.rows_.size() == 2 && std::strcmp(combined.HistoryTabName(0), "All") == 0 &&
                    !combined.HistoryTabAvailable(1) && combined.editor_.Text() == "keep this draft",
                    "single-window All history combines both native logs without a second window or draft changes");
                *combined.Option(8).choice = 0; combined.OnOptionChanged(8);
                combined.RebuildRows(single.design_system_, 500);
                Check(combined.rows_.size() == 1 && std::strcmp(combined.HistoryTabName(0), "Window 1") == 0,
                    "changing the source invalidates the combined history cache");
            }
        }

        static auto WrappingWhitespace(void) -> void
        {
            ui::design_system design; chat_module chat;
            chat.ApplyTextDefaults(ui::theme::CreateDefault()); chat.timestamp_display_ = 1;
            chat_event event{}; event.sequence = 1; event.windows = 1; event.mode = 1;
            strcpy_s(event.text, "Filled\x1e\x50 \t  "); chat.Receive(nullptr, event, false);
            chat.RebuildRows(design, MeasureChatText(design, "Filled", chat.text_.style));
            Check(chat.rows_.size() == 1 && chat.rows_[0].spans[0].text == "Filled",
                "trailing colored whitespace cannot wrap into a blank message row");
            chat.Clear(); event.sequence = 2; strcpy_s(event.text, "First\n\n  Second"); chat.Receive(nullptr, event, false);
            chat.RebuildRows(design, 500);
            Check(chat.rows_.size() == 3 && chat.rows_[1].spans.empty() && chat.rows_[2].spans[0].text == "  Second",
                "explicit blank lines and intentional indentation are retained");
        }

        static auto Timestamps(void) -> void
        {
            constexpr uint32_t capturedSeconds = 12 * 3600 + 34 * 60 + 56;
            constexpr uint32_t capturedMilliseconds = capturedSeconds * 1000 + 900;
            for (const uint64_t age : {0ull, 50ull, 150ull, 999ull, 1001ull, 1500ull, 86400150ull})
                Check(ChatLocalSeconds(10000, 10000 + age, static_cast<uint32_t>((capturedMilliseconds + age) % 86400000)) == capturedSeconds,
                    "capture time stays fixed across fractional-second boundaries, animation duration and full days");
            Check(ChatLocalSeconds(10000, 10200, 50) == 86399 && ChatLocalSeconds(11000, 10000, 50) == 0,
                "timestamp capture wraps midnight and clamps a future monotonic timestamp");
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
            const auto captured = module.model_.Lines().back().local_seconds;
            char prefix[20]{}; std::snprintf(prefix, sizeof(prefix), "[%02u:%02u:%02u] ", captured / 3600, captured / 60 % 60, captured % 60);
            Check(text == std::string{prefix} + "Hello\n" + prefix + "  Indented\n", "all logical lines render the message's captured clock value");
            event.windows = 2; event.timestamp = ::GetTickCount64() + 86400000;
            module.Receive(nullptr, event, false);
            Check(module.model_.Lines().back().local_seconds == captured, "late routing updates retain the original message time");
            module.RebuildRows(design, 1499);
            Check(rows() == text, "full width reflow keeps timestamps identical to their first rendered value");
            chat_module shared{true}; module.AttachSecondWindow(shared); shared.EnsureWindowDefaults(); shared.timestamp_display_ = 3; shared.source_ = 0;
            shared.RebuildRows(design, 1500);
            Check(shared.rows_.front().spans.front().text == prefix, "both windows use the same captured timestamp");
            event.timestamp = ::GetTickCount64();
            event.windows = 3;
            module.second_window_ = nullptr;

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
            module.timestamp_display_ = 2;
            event.sequence = 3; event.mode = 5; strcpy_s(event.text, "Party message"); module.Clear(); module.Receive(nullptr, event, false);
            rows();
            const auto& row = module.rows_.front();
            Check(row.spans.size() == 2 && row.spans[0].timestamp && !row.spans[1].timestamp,
                "generated timestamps stay distinct from message text in cached rows");
            chat_palette palette; palette.modes[5] = 0xFF80E8FF;
            Check(module.SpanColor(row, row.spans[0], palette) == 0xFFB0B0B0 && module.Option(module.TimestampColorOptionStart() + 1).available,
                "timestamps use independent gray by default and the color picker is available");
            *module.Option(module.TimestampColorOptionStart()).toggle = true;
            *module.Option(module.TimestampColorOptionStart() + 1).color = 0xFFBBBBBB;
            Check(module.SpanColor(row, row.spans[0], palette) == 0xFFBBBBBB && module.SpanColor(row, row.spans[1], palette) == 0xFF80E8FF &&
                module.ConfigSectionContains(6, module.TimestampColorOptionStart()) && module.Option(module.TimestampColorOptionStart() + 1).available,
                "independent timestamp color changes immediately while Party messages retain turquoise");
            module.colors_.enabled[ChatTypeIndex(5)] = true; module.colors_.colors[ChatTypeIndex(5)] = 0xFF90E890;
            Check(module.SpanColor(row, row.spans[0], palette) == 0xFFBBBBBB && module.SpanColor(row, row.spans[1], palette) == 0xFF90E890,
                "custom message colors cannot override the separate timestamp color");
            auto colored = settings_store::FromIniText(""); module.Save(colored); chat_module loaded; loaded.Load(colored);
            Check(loaded.timestamp_color_enabled_ && loaded.timestamp_color_ == 0xFFBBBBBB,
                "timestamp color preference persists with its chat window");
            chat_module second{true}; module.AttachSecondWindow(second); second.EnsureWindowDefaults();
            Check(second.timestamp_color_enabled_ && second.timestamp_color_ == 0xFFBBBBBB,
                "new second windows inherit the timestamp presentation");
            second.timestamp_color_ = 0xFFEEEEEE; second.Save(colored);
            chat_module loadedSecond{true}; loadedSecond.Load(colored);
            Check(loadedSecond.timestamp_color_ == 0xFFEEEEEE && module.timestamp_color_ == 0xFFBBBBBB,
                "each window can retain a different timestamp color");
            loaded.ResetDefaults(ui::theme::CreateDefault());
            Check(loaded.timestamp_color_enabled_, "reset restores independent timestamp colors");
            loaded.Load(settings_store::FromIniText("[chat]\ntimestamp_color_enabled=0\ntimestamp_color=FFABCDEF\n"));
            Check(loaded.timestamp_color_enabled_ && loaded.timestamp_color_ == 0xFFABCDEF,
                "legacy settings enable separate color without replacing the chosen timestamp color");
            loaded.timestamp_color_enabled_ = false; loaded.Save(colored);
            chat_module follow; follow.Load(colored);
            Check(!follow.timestamp_color_enabled_, "a later choice to follow message color persists");
            Check(follow.SpanColor(row, row.spans[0], palette) == 0xFF80E8FF,
                "following the channel remains available explicitly");
            event.sequence = 4; event.mode = 6; strcpy_s(event.text, "[1]<Name> Linkshell message\n"); module.Clear(); module.Receive(nullptr, event, false);
            rows();
            Check(module.rows_.size() == 1 && module.rows_.front().spans.size() > 1 && module.rows_.front().spans[0].timestamp,
                "a trailing message newline does not add a timestamp-only row");
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
            const auto themedCombat = ChatThemeColor(20, game, true);
            Check(ChatThemeColor(20, game, false) == game && themedCombat != game &&
                module.colors_.Resolve(20, themedCombat, inlineColor) == inlineColor,
                "XIV chat defaults affect only the selected theme and retain authored inline highlights");
            module.colors_.enabled[11] = true; module.colors_.colors[11] = custom;
            Check(module.colors_.Resolve(20, themedCombat, inlineColor) == custom,
                "an explicit channel override remains authoritative over themed defaults");
            module.colors_.enabled[11] = false;
            ui::design_system themedDesign; themedDesign.SetAppearanceTheme(1);
            auto chatStyle = ui::theme::CreateDefault().body_text;
            chatStyle.font = ui::xiv1_font; chatStyle.scale = .85f; chatStyle.letter_spacing = .2f;
            const auto trackedStyle = ChatThemeStyle(themedDesign, chatStyle);
            Check(std::abs(trackedStyle.letter_spacing - .6f) < .001f &&
                ChatThemeStyle(themedDesign, trackedStyle).letter_spacing == trackedStyle.letter_spacing && chatStyle.letter_spacing == .2f,
                "XIV chat tracking is applied once without changing saved text preferences");
            Check(std::abs(MeasureChatText(themedDesign, "Five!", trackedStyle) - MeasureChatText(themedDesign, "Five!", chatStyle) -
                1.6f * ui::TextPixelScale(chatStyle.scale)) < .001f,
                "chat wrapping and caret measurement use the rendered tracking at fractional font sizes");
            for (size_t i = 0; i < std::size(modes); ++i)
            {
                Check(module.colors_.Resolve(modes[i], game, inlineColor) == inlineColor && module.colors_.Resolve(modes[i], game) == game,
                    "every chat type follows native and inline colors until explicitly overridden");
                module.colors_.enabled[i] = true; module.colors_.colors[i] = custom;
                Check(module.colors_.Resolve(modes[i], game, inlineColor) == custom, "XIUI+ overrides take precedence over game and inline colors");
                module.colors_.enabled[i] = false;
                Check(module.colors_.Resolve(modes[i], game, inlineColor) == inlineColor && module.colors_.colors[i] == custom,
                    "Game restores native colors while retaining the saved custom color");
            }
            module.colors_.enabled[0] = true;
            Check(module.colors_.Resolve(4, game) == game && second.Colors().Resolve(12, game) == custom,
                "incoming and outgoing tells are independent and overrides are shared by both XIUI+ windows");
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
                "color controls share preferences and visibly explain that XIUI+ overrides game settings colors");
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
                Check(alert_sounds == before + 1, "each chat type alerts once across both native and XIUI+ windows");
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
                "slash claims XIUI+ chat over a visible combat command menu");
            runtime.SyncNativeChatInput(&chat, true);
            Check(runtime.ChatFocused() && runtime.HandleChatKey(VK_BACK, 0, false, true, false, 0) &&
                runtime.chat_.editor_.Text().empty() && chat.IsInputOpen() == 0 && chat.count == 0,
                "frame synchronization and subsequent typing retain XIUI+ ownership without opening native chat");
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
            Check(runtime.HandleChatKey(VK_OEM_2, 0, false, true, false, 0), "slash can reopen XIUI+ chat over the same menu");
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
                        runtime.chat_.editor_.Text() == "/", "slash opens XIUI+ chat while selecting a target");
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
                    "hidden native slash drafts transfer into XIUI+ during targeting using native session cleanup");
                runtime.SyncNativeChatInput(&chat, true, true);
                Check(runtime.ChatFocused() && runtime.chat_native_context_suspended_ && chat.count == 0 && chat.input_updates == 0,
                    "adopted commands retain focus without sending or rewriting the closed native editor");
                runtime.ReleaseChatInput();
            }
        }

        static auto SentHistory(void) -> void
        {
            chat_module module; chat_manager chat;
            module.editor_.Set("/p first message"); Check(module.Submit(&chat), "remember first sent command");
            module.editor_.Set("/s latest message"); Check(module.Submit(&chat), "remember latest sent command");
            native_chat_input input; input.raw_offset_ = 16; input.state_offset_ = 0;
            input.reset_chat_ = reinterpret_cast<uintptr_t>(&ResetNative);
            input.close_menu_ = reinterpret_cast<uintptr_t>(&CloseMenu);
            input.menu_manager_ = reinterpret_cast<uintptr_t>(&input);
            chat.OpenNative("");
            auto draft = input.TakeInput(&chat);
            Check(draft && draft->text.empty() && module.AdoptDraft(std::move(*draft)) && chat.IsInputOpen() == 0,
                "Space-opened empty native chat transfers to the editor before the first typed character");
            module.HandleKey(nullptr, VK_UP, 0, false, true);
            Check(module.editor_.Text() == "/s latest message", "Space-opened chat recalls the latest sent command");
            module.HandleKey(nullptr, VK_UP, 0, false, true);
            Check(module.editor_.Text() == "/p first message", "Up traverses older sent commands");
            module.Focus(false);
            module.HandleKey(nullptr, VK_OEM_2, 0, false, true);
            module.HandleKey(nullptr, VK_UP, 0, false, true);
            Check(module.editor_.Text() == "/s latest message", "slash restarts at the newest entry after dismissing recalled history");
            module.HandleKey(nullptr, VK_DOWN, 0, false, true);
            Check(module.editor_.Text() == "/", "Down restores the unsent slash draft without sending it");
            Check(chat.count == 2, "history browsing never resubmits commands");
            module.Focus(false); chat.OpenNative("search", 5);
            Check(!input.TakeInput(&chat), "native search editors retain their own history");
        }

        static auto ManualHeight(void) -> void
        {
            ui::design_system design; chat_module module;
            module.ApplyTextDefaults(ui::theme::CreateDefault()); module.state_.bounds = {20, 300, 560, 350};
            module.SetNativeWindowConfig(native_chat_window_settings{true, 0, 2, 5});
            for (uint64_t i = 1; i <= 30; ++i)
            {
                chat_event event{}; event.sequence = i; event.mode = 1; event.windows = 1;
                strcpy_s(event.text, "Message"); module.Receive(nullptr, event, false);
            }
            module.UpdateCompactLayout(design, 1000);
            Check(module.collapse_visible_rows_ == 2, "unmodified chat follows the native maximum line setting");
            module.OnOptionChanged(2); module.UpdateCompactLayout(design, 1001);
            Check(module.height_limits_lines_ && module.collapse_visible_rows_ > 2 && module.collapse_active_,
                "manual height overrides the native line cap while preserving reactive collapse");
            const auto store = settings_store::FromIniText(""); module.Save(store);
            chat_module restored; restored.Load(store);
            Check(restored.height_limits_lines_, "manual maximum height survives profile reload");
            module.OnOptionChanged(module.CollapseOptionStart()); module.UpdateCompactLayout(design, 1002);
            Check(!module.height_limits_lines_ && module.collapse_visible_rows_ == 2,
                "choosing reactive sizing again restores the explicit native line limit");
        }

        static auto FullKeyboardInputs(void) -> void
        {
            const keyboard_restore neutral;
            hud_runtime runtime; chat_manager chat;
            std::array<std::array<uint32_t, 11>, 67> table{};
            std::array<uintptr_t, 2> manager{0, reinterpret_cast<uintptr_t>(table.data())};
            const auto config = reinterpret_cast<uintptr_t>(manager.data());
            runtime.window_config_.config_slot_ = reinterpret_cast<uintptr_t>(&config); runtime.window_config_.setter_ = 1;
            auto& record = table[66]; record[1] = 66; record[3] = 4; record[7] = 2;
            Check(runtime.window_config_.FullKeyboard() == true && runtime.window_config_.KeyboardLayout() == native_keyboard_layout::full,
                "Full keyboard mode reads the native configuration");
            Check(runtime.HandleChatKey('F', 0, false, true, false, 0) && runtime.chat_.Focused() && runtime.chat_.editor_.Text() == "f" &&
                !runtime.chat_input_.HandoffAvailable() && chat.count == 0,
                "Full-mode first-character typing opens XIUI+ directly without a native editor or sending");
            Check(runtime.HandleChatKey('R', 0, false, true, false, 0) && runtime.chat_.editor_.Text() == "fr", "following letters enter the same draft once");
            runtime.ReleaseChatInput();
            Check(!runtime.HandleChatKey(VK_NUMPAD7, 0, false, true, false, 0) && !runtime.HandleChatKey(VK_F1, 0, false, true, false, 0) &&
                !runtime.HandleChatKey('F', 0x40000000, false, true, false, 0) && !runtime.ChatFocused(),
                "numpad movement, function keys and an already-held letter cannot start composition");
            Check(!runtime.HandleChatKey('F', 0, true, true, false, 0) && !runtime.HandleChatKey('F', 0, false, false, false, 0) &&
                !runtime.HandleChatKey('F', 0, false, true, false, 0x21), "blocked input, native chat and other native editors keep their keys");
            runtime.native_ui_.menu_visible = true;
            Check(!runtime.HandleChatKey('F', 0, false, true, false, 0), "native menus retain regular letters");
            runtime.native_ui_.menu_visible = false;
            Check(!runtime.HandleChatKey('F', 0, false, true, true, 0), "native targeting retains regular letters");
            Check(runtime.HandleChatKey(VK_SPACE, 0, false, true, false, 0) && runtime.chat_.editor_.Text() == "fr",
                "Space opens composition while preserving the unsent draft without adding a leading space");
            runtime.ReleaseChatInput();
            auto& input = runtime.chat_input_;
            input.raw_offset_ = 16; input.state_offset_ = 0;
            input.reset_chat_ = reinterpret_cast<uintptr_t>(&ResetNative);
            input.close_menu_ = reinterpret_cast<uintptr_t>(&CloseMenu);
            input.menu_manager_ = reinterpret_cast<uintptr_t>(&input);
            chat.OpenNative("f");
            runtime.SyncNativeChatInput(&chat, false);
            Check(runtime.ChatFocused() && runtime.chat_.editor_.Text() == "f" && runtime.chat_.editor_.Caret() == 1 &&
                runtime.chat_.Layout(runtime.design_system_).input.height > 0 && chat.IsInputOpen() == 0 && chat.count == 0,
                "the first native character opens visible XIUI+ input and transfers once without sending");
            Check(runtime.ChatFocusBounds().width == 0, "typing a message does not outline the chat history window");
            runtime.SyncNativeChatInput(&chat, false);
            Check(runtime.chat_.editor_.Text() == "f" && chat.input_updates == 0 && chat.count == 0,
                "frame synchronization cannot duplicate or rewrite transferred input");
            runtime.ReleaseChatInput();
            chat.OpenNative("test"); runtime.chat_activation_enter_down_ = true;
            runtime.SyncNativeChatInput(&chat, false);
            Check(!runtime.ChatFocused() && chat.IsInputOpen() == 0x11, "native activation retains its Enter release before handoff");
            runtime.chat_activation_enter_down_ = false; runtime.SyncNativeChatInput(&chat, false);
            Check(runtime.ChatFocused() && runtime.chat_.editor_.Text() == "test" && runtime.chat_.editor_.Caret() == 4 && chat.count == 0,
                "ordinary native text becomes visible with its full body and caret intact");
            Check(runtime.ChatFocusBounds().width == 0, "native chat composition keeps its caret without a history focus ring");
            runtime.ReleaseChatInput(); chat.OpenNative("native search", 5);
            runtime.SyncNativeChatInput(&chat, false);
            Check(!runtime.ChatFocused() && chat.IsInputOpen() == 0x21 && std::strcmp(chat.GetInputTextRaw(), "native search") == 0,
                "non-chat native editors retain their own text and ownership");
            chat.OpenNative(""); runtime.SyncNativeChatInput(&chat, false);
            Check(runtime.ChatFocused() && chat.IsInputOpen() == 0 && runtime.chat_.editor_.Text() == "test",
                "an empty native editor opens XIUI+ input while preserving the saved unsent draft");
            runtime.ReleaseChatInput();
            auto& compactMember = const_cast<party_member&>(runtime.party_source_.Members()[0]);
            compactMember.active = compactMember.buffs_known = true; compactMember.id = 50;
            compactMember.buffs = {33}; compactMember.buff_count = 1;
            for (const uint32_t compact : {1u, 2u})
            {
                record[2] = compact;
                Check(runtime.window_config_.KeyboardLayout() == static_cast<native_keyboard_layout>(compact), "Compact 1 and Compact 2 remain distinct native layouts");
                Check(runtime.window_config_.FullKeyboard() == false &&
                    runtime.HandleChatKey('F', 0, false, true, false, 0) && runtime.chat_.HistoryFocused(),
                    "both Compact layouts retain F-to-focus without opening the message editor");
                Check(runtime.keyboard_window_focus_ && runtime.controller_surface_ == hud_runtime::controller_surface::chat &&
                    runtime.ChatFocusBounds().width > 0 && !runtime.KeyboardFocused(),
                    "F establishes visible chat browsing without capturing typing or movement");
                controller_status unavailable{}; unavailable.suspended = unavailable.enumeration_disabled = true;
                unavailable.configuration_known = unavailable.connection_known = true;
                runtime.UpdateControllerStatus(unavailable);
                Check(runtime.chat_.HistoryFocused() && runtime.ChatFocusBounds().width > 0,
                    "disabled or disconnected gamepads cannot clear keyboard chat focus");
                runtime.native_ui_.menu_visible = true;
                Check(runtime.ControllerCapture() == controller_capture_mode::navigation && runtime.ChatFocusBounds().width > 0,
                    "chat browsing retains navigation over native menus without taking movement input");
                runtime.native_ui_.menu_visible = false;
                Check(runtime.HandleChatKey('F', 0x40000000, false, true, false, 0) && runtime.chat_.HistoryFocused(),
                    "holding F cannot skip over a focused window");
                runtime.chat_second_.State().enabled = true;
                Check(runtime.HandleChatKey('F', 0, false, true, false, 0) && runtime.buffs_.ControllerFocused() &&
                    !runtime.chat_second_.HistoryFocused() && runtime.controller_surface_ == hud_runtime::controller_surface::buffs,
                    "the next compact F selects status effects even with Window 2 enabled");
                Check(runtime.HandleChatKey(VK_ESCAPE, 0, false, true, false, 0) && runtime.ChatFocusBounds().width == 0 &&
                    runtime.controller_surface_ == hud_runtime::controller_surface::none && !runtime.keyboard_window_focus_,
                    "Escape immediately releases the outline and keyboard browsing owner");
                runtime.native_ui_.menu_visible = true;
                Check(!runtime.HandleChatKey('F', 0, false, true, false, 0), "unfocused F yields to native menus");
                runtime.native_ui_.menu_visible = false;
                Check(!runtime.HandleChatKey('F', 0, false, true, true, 0), "unfocused F yields to native action targeting");
                runtime.ReleaseChatInput();
                runtime.UpdateControllerStatus({});
            }
            record[2] = 3;
            Check(!runtime.window_config_.FullKeyboard().has_value(), "unknown native keyboard layouts remain unavailable");
            Check(!runtime.HandleChatKey('F', 0, false, true, false, 0) && !runtime.ChatFocused(),
                "unavailable native keyboard settings cannot turn F into an implicit XIUI+ shortcut");

            runtime.actionbars_.Load(settings_store::FromIniText(
                "[actionbars.hotbar]\nenabled=1\nslots=8\n"
                "[actionbars.hotbar.keys]\nslot1=81\nslot2=49\nslot3=186\nslot4=32\nslot5=112\nslot6=82\nslot7=69\nslot8=97\n"
                "[actionbars.hotbar.modifiers]\nslot2=4\nslot6=1\nslot7=2\n"));
            record[2] = 0;
            for (const WPARAM key : {WPARAM{'Q'}, WPARAM{VK_OEM_1}, WPARAM{VK_SPACE}, WPARAM{VK_NUMPAD1}})
                Check(!runtime.HandleActionBarsKey(key, 0, false) && !runtime.KeyboardFocused(),
                    "Full mode leaves bound letters, punctuation, Space and numpad input available for native typing");
            BYTE keys[256]{}; keys[VK_SHIFT] = keys[VK_LSHIFT] = 0x80;
            Check(::SetKeyboardState(keys) != FALSE && !runtime.HandleActionBarsKey('1', 0, false),
                "Shift-only hotbar bindings cannot steal Full-mode punctuation");
            keys[VK_SHIFT] = keys[VK_LSHIFT] = 0; Check(::SetKeyboardState(keys) != FALSE, "clear Shift modifier");
            Check(runtime.HandleActionBarsKey(VK_F1, 0, false) && runtime.HandleActionBarsKey(VK_F1, static_cast<LPARAM>(0x80000000u), false),
                "Full mode retains explicit function-key bindings and drains their releases");
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80;
            Check(::SetKeyboardState(keys) != FALSE && runtime.HandleActionBarsKey('R', 0, false) &&
                runtime.HandleActionBarsKey('R', static_cast<LPARAM>(0x80000000u), false), "Full mode retains Ctrl bindings");
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0; keys[VK_MENU] = keys[VK_LMENU] = 0x80;
            Check(::SetKeyboardState(keys) != FALSE && runtime.HandleActionBarsKey('E', 0x20000000u, false) &&
                runtime.HandleActionBarsKey('E', static_cast<LPARAM>(0xA0000000u), false), "Full mode retains Alt bindings");
            keys[VK_MENU] = keys[VK_LMENU] = 0; Check(::SetKeyboardState(keys) != FALSE, "clear command modifiers");
            record[2] = 1;
            Check(runtime.HandleActionBarsKey('Q', 0, false), "Compact mode retains ordinary hotbar bindings");
            record[2] = 0;
            Check(runtime.HandleActionBarsKey('Q', 0x40000000u, false) &&
                runtime.HandleActionBarsKey('Q', static_cast<LPARAM>(0x80000000u), false) && !runtime.HandleActionBarsKey('Q', 0, false),
                "changing to Full mode drains an owned key before yielding a fresh text press");
            runtime.actionbars_.OpenEditor();
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80; Check(::SetKeyboardState(keys) != FALSE, "hold search modifier");
            Check(runtime.HandleActionBarsKey('F', 0, false) && runtime.HandleActionBarsKey('F', static_cast<LPARAM>(0x80000000u), false), "explicitly focus the action search field");
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0; Check(::SetKeyboardState(keys) != FALSE, "release search modifier");
            Check(runtime.HandleActionBarsKey('Q', 0, false) && runtime.HandleActionBarsKey('Q', static_cast<LPARAM>(0x80000000u), false),
                "an explicitly opened action editor retains its text input in Full mode");
            runtime.actionbars_.SuspendInput();
        }

        static auto __stdcall LatestReply(void) -> const char* { return "Alice"; }

        static auto __fastcall ReplyAt(void* object, void*, const uint32_t index) -> const char*
        {
            const auto& targets = *static_cast<const std::vector<std::string>*>(object);
            return targets.empty() ? nullptr : targets[index % targets.size()].c_str();
        }

        struct keyboard_restore
        {
            BYTE keys[256]{};
            explicit keyboard_restore(const bool control = false, const bool alt = false, const bool shift = false)
            {
                Check(::GetKeyboardState(this->keys) != FALSE, "capture thread-local keyboard state");
                BYTE neutral[256]{};
                if (control) neutral[VK_CONTROL] = neutral[VK_LCONTROL] = 0x80;
                if (alt) neutral[VK_MENU] = neutral[VK_LMENU] = 0x80;
                if (shift) neutral[VK_SHIFT] = neutral[VK_LSHIFT] = 0x80;
                Check(::SetKeyboardState(neutral) != FALSE, "set thread-local reply modifiers");
            }
            ~keyboard_restore(void) { ::SetKeyboardState(this->keys); }
        };

        static auto ReplyCycling(void) -> void
        {
            std::vector<std::string> targets{"Alice", "Bob", "Carol"};
            void* manager = &targets;
            native_chat_input input;
            Check(input.ReplyTargets().empty(), "missing native reply history leaves the shortcut unclaimed");
            input.last_teller_ = reinterpret_cast<uintptr_t>(&LatestReply);
            Check(input.ReplyTargets() == std::vector<std::string>{"Alice"}, "latest-teller fallback remains available");
            input.reply_teller_ = reinterpret_cast<uintptr_t>(&ReplyAt);
            input.reply_manager_ = reinterpret_cast<uintptr_t>(&manager);
            Check(input.ReplyTargets() == targets, "native reply history is read in order without duplicating wrapped entries");
            chat_module module;
            constexpr LPARAM alt = 0x20000000;
            Check(module.HandleKey(nullptr, 'R', alt, false, true, targets) && module.editor_.Text() == "/t Alice ",
                "first reply shortcut opens the latest native recipient");
            module.editor_.Insert("unsent body");
            for (const auto* name : {"Bob", "Carol", "Alice"})
            {
                Check(module.HandleKey(nullptr, 'R', alt, false, true, targets) &&
                    module.editor_.Text() == std::string{"/t "} + name + " unsent body",
                    "fresh Alt+R presses cycle all recipients and wrap while preserving the body");
            }
            module.HandleKey(nullptr, 'R', 0x60000000, false, true, targets);
            module.HandleKey(nullptr, 'R', static_cast<LPARAM>(0xA0000000u), false, true, targets);
            Check(module.editor_.Text() == "/t Alice unsent body", "held and released reply keys cannot advance the history");
            module.Focus(false);
            module.editor_.Set("/tell Carol retained body");
            module.HandleKey(nullptr, 'R', alt, false, true, targets);
            Check(module.editor_.Text() == "/t Alice retained body", "reply after leaving composition restarts at the latest recipient");
            {
                const keyboard_restore control{true};
                Check(module.HandleKey(nullptr, 'R', 0, false, true, targets) && module.editor_.Text() == "/t Bob retained body",
                    "Ctrl+R continues the same reply cycle as Alt+R");
                module.HandleKey(nullptr, 'R', 0x40000000, false, true, targets);
                Check(module.editor_.Text() == "/t Bob retained body", "held Ctrl+R cannot advance the recipient");
            }
            module.HandleKey(nullptr, 'R', alt, false, true, targets);
            Check(module.editor_.Text() == "/t Carol retained body", "switching back to Alt+R continues the shared cycle");
            chat_manager chat;
            Check(chat.count == 0 && module.Submit(&chat) && chat.sent == "/t Carol retained body" && chat.count == 1,
                "cycling never sends and explicit submission sends only the chosen recipient once");
            targets = {"Alice", "Bob", "Carol", "Dave", "Erin", "Frank", "Grace", "Heidi"};
            Check(input.ReplyTargets() == targets, "all eight native reply slots remain available");
            targets.clear(); input.last_teller_ = 0;
            Check(input.ReplyTargets().empty(), "an empty native history stays empty");
        }

        static auto IncomingReplyOrder(void) -> void
        {
            constexpr LPARAM alt = 0x20000000;
            const std::array<std::string, 2> native{"Mai", "Phro"};
            const auto tell = [](std::string_view name) {
                std::array<uint8_t, 24> packet{};
                packet[4] = 3;
                std::copy(name.begin(), name.end(), packet.begin() + 8);
                return packet;
            };
            chat_module module;
            module.ObserveTellPacket(0x17, tell("Mai"));
            module.ObserveTellPacket(0x17, tell("Phro"));
            module.OpenTell("Mai");
            module.editor_.Insert("unsent body");
            Check(module.HandleKey(nullptr, 'R', alt, false, true, native) && module.editor_.Text() == "/t Phro unsent body",
                "reply starts with the latest received teller despite stale native order and an already-open tell");
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(module.editor_.Text() == "/t Mai unsent body", "further reply presses cycle received tellers in arrival order");
            {
                const keyboard_restore control{true};
                module.HandleKey(nullptr, 'R', 0, false, true, native);
                Check(module.editor_.Text() == "/t Phro unsent body", "Ctrl+R shares the received-tell cycle and wraps");
            }
            module.ObserveTellPacket(0x17, tell("Mai"));
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(module.editor_.Text() == "/t Mai unsent body", "a new tell restarts an active reply cycle without replacing the body");
            module.ObserveTellPacket(0x17, tell("Mai"));
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(module.editor_.Text() == "/t Mai unsent body", "another tell from the current recipient restarts at that recipient");

            auto other = tell("Phro"); other[4] = 4;
            module.ObserveTellPacket(0x17, other);
            chat_event outgoing{1, 1000, 4, 0xFFFFFFFF, 1}; strcpy_s(outgoing.text, ">>Phro: outgoing");
            module.Receive(nullptr, outgoing, false);
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(module.editor_.Text() == "/t Phro unsent body", "party packets and outgoing tells neither reorder nor restart reply history");
            const auto expected = module.recent_tellers_;
            module.ObserveTellPacket(0x18, tell("Other"));
            module.ObserveTellPacket(0x17, std::span<const uint8_t>{other}.first(12));
            module.ObserveTellPacket(0x17, tell("bad name"));
            module.ObserveTellPacket(0x17, tell(""));
            Check(module.recent_tellers_ == expected, "unrelated, truncated and invalid tell packets cannot change reply history");
            module.ObserveTellPacket(0x17, tell("phro"));
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(module.recent_tellers_.size() == 2 && module.editor_.Text() == "/t phro unsent body",
                "case variations share one reply slot across received and native history");
            chat_manager chat;
            Check(chat.count == 0 && module.Submit(&chat) && chat.count == 1 && chat.sent == "/t phro unsent body",
                "tracking and cycling never send, and explicit submission sends the selected recipient once");
            module.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(module.editor_.Text() == "/t phro ", "a fresh reply after sending starts with the latest incoming teller");

            chat_module replay;
            chat_event first{1, 1000, 12, 0xFFFFFFFF, 2}; strcpy_s(first.text, "[16:00] Mai>> earlier");
            replay.Receive(nullptr, first, false);
            chat_event latest{2, 1001, 12, 0xFFFFFFFF, 2}; latest.indent = true;
            strcpy_s(latest.text, "\x1e\x51[16:01:02]\x1e\x01 \x1e\xc8Phro\x1e\x01>> latest");
            replay.Receive(nullptr, latest, false);
            replay.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(replay.editor_.Text() == "/t Phro ", "replayed window-two tells restore incoming recency across reload, colors, indentation and timestamps");
            latest.windows = 1; replay.Receive(nullptr, latest, false);
            replay.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(replay.editor_.Text() == "/t Mai ", "the same tell routed to both native windows cannot restart reply cycling");
            replay.AdoptTell({"/tell Phro retained", 19});
            replay.HandleKey(nullptr, 'R', alt, false, true, native);
            Check(replay.editor_.Text() == "/t Phro retained", "native tell handoff starts a fresh reply cycle rather than skipping the newest sender");

            for (const auto* name : {"A", "B", "C", "D", "E", "F", "G", "H", "I"})
                module.ObserveTellPacket(0x17, tell(name));
            Check(module.recent_tellers_ == std::vector<std::string>{"I", "H", "G", "F", "E", "D", "C", "B"},
                "received reply history remains bounded to the eight most recent distinct senders");
        }

        static auto ReplyKeyboardOwnership(void) -> void
        {
            for (const bool nativeMenu : {false, true})
                for (const bool targeting : {false, true})
                {
                    hud_runtime runtime;
                    chat_manager chat;
                    runtime.native_ui_.menu_visible = nativeMenu;
                    runtime.chat_.ObserveTeller("Alice");
                    Check(runtime.HandleChatKey('R', 0x20000000, false, true, targeting, 0) && runtime.ChatFocused(),
                        "Alt+R explicitly opens a reply over native menus and targeting");
                    runtime.SyncNativeChatInput(&chat, nativeMenu, targeting);
                    Check(runtime.ChatFocused() && runtime.chat_.editor_.Text() == "/t Alice ",
                        "native menu synchronization retains an explicitly opened reply");
                    runtime.chat_.editor_.Set("/t Alice reply body");
                    Check(runtime.HandleChatKey(VK_RETURN, 0, false, true, targeting, 0),
                        "the reply editor owns its send key");
                    Check(runtime.chat_.Submit(&chat) && chat.count == 1 && chat.sent == "/t Alice reply body" &&
                        !runtime.ChatFocused() && runtime.KeyboardFocused(),
                        "sending a reply retains keyboard capture until the send key is released");
                    runtime.chat_activation_enter_down_ = true;
                    Check(runtime.HandleKey(VK_RETURN, 0x40000000, false) && runtime.KeyboardFocused() && chat.count == 1,
                        "held send cannot reach another input owner or submit twice");
                    Check(runtime.HandleKey(VK_RETURN, static_cast<LPARAM>(0xC0000000u), false) &&
                        !runtime.KeyboardFocused() && !runtime.chat_activation_enter_down_ && chat.count == 1,
                        "the send release is consumed and cannot activate native chat or commands");
                    Check(!runtime.HandleKey(VK_RETURN, 0, false), "a new Enter gesture returns to native gameplay");
                }
            hud_runtime runtime;
            runtime.chat_.Focus(true);
            runtime.chat_.editor_.Set({});
            Check(runtime.HandleChatKey(VK_RETURN, 0, false, true, false, 0) && !runtime.ChatFocused() && runtime.KeyboardFocused(),
                "closing an empty chat also drains its Enter gesture");
            runtime.SetVisible(false); runtime.SetVisible(true);
            Check(!runtime.KeyboardFocused(), "hiding XIUI+ clears pending chat keyboard capture");
        }

        static auto TellInputs(void) -> void
        {
            const keyboard_restore neutral;
            ReplyCycling();
            IncomingReplyOrder();
            ReplyKeyboardOwnership();
            constexpr LPARAM alt = 0x20000000, repeated = 0x60000000;
            const std::array<std::string, 1> alice{"Alice"}, bob{"Bob"}, carol{"Carol"};
            chat_module module;
            chat_manager chat;
            Check(!module.HandleKey(nullptr, 'R', alt, false, true) && !module.Focused(), "reply with no previous teller keeps chat closed");
            Check(module.HandleKey(nullptr, 'R', alt, false, true, alice) && module.Focused() && module.editor_.Text() == "/t Alice ",
                "Alt+R opens an explicitly addressed tell to the latest sender");
            module.HandleKey(nullptr, 'R', repeated, false, true, bob);
            Check(module.editor_.Text() == "/t Alice " && chat.count == 0, "holding Alt+R neither changes recipient nor sends");
            module.editor_.Set("/party hello");
            module.HandleKey(nullptr, 'R', alt, false, true, bob);
            Check(module.editor_.Text() == "/t Bob hello", "reply switching preserves the message body");
            module.HandleKey(nullptr, 'R', alt, false, true, carol);
            Check(module.editor_.Text() == "/t Carol hello" && module.Submit(&chat) && chat.sent == "/t Carol hello" && chat.count == 1,
                "the recipient is bound when opening and the normal submit path sends once");
            Check(!module.HandleKey(nullptr, 'R', alt, true, true, alice) && !module.Focused(), "other input owners retain Alt+R");
            Check(!module.HandleKey(nullptr, 'R', alt, false, false, alice) && !module.Focused(), "native chat keeps its shortcut when replacement is inactive");
            module.State().enabled = false;
            Check(!module.HandleKey(nullptr, 'R', alt, false, true, alice), "disabled XIUI+ chat cannot claim reply");
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
            Check(!input.TakeTell(&chat) && chat.IsInputOpen() == 0x11 && !module.Focused(), "ordinary Enter does not open XIUI+ input");
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
                "native CHAT keeps its Enter release before XIUI+ takes keyboard capture");
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

        static auto DestinationIndicator(void) -> void
        {
            std::array<char, 264> object{};
            uintptr_t current = reinterpret_cast<uintptr_t>(object.data());
            native_chat_input native;
            native.send_prefix_slot_ = reinterpret_cast<uintptr_t>(&current);
            const auto prefix = [&](const char* text) { object.fill(0); strcpy_s(object.data() + 8, object.size() - 8, text); };
            chat_module module; ui::design_system design;
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            prefix("/say "); module.SetNativeDestination(native.SendPrefix(), "Shio");
            Check(module.SendChannelLabel() == "Say" && !module.Focused() && module.editor_.Text().empty(),
                "the native default channel is readable without opening composition or changing the draft");
            const auto idle = module.Layout(design);
            Check(idle.destination.height == 0 && idle.input.height == 0 && idle.destination.y == idle.input_text.y &&
                idle.destination.x + idle.destination.width < idle.input_text.x && idle.input_text.x > idle.content.x + 5,
                "idle chat reclaims the typing row while retaining the fixed horizontal columns");
            module.UpdateInputAnimation(1000); module.Focus(true); module.UpdateInputAnimation(1120);
            Check(module.destination_reveal_ > 0 && module.destination_reveal_ < 1 && module.destination_reveal_ == module.input_reveal_,
                "opening an empty draft reveals the channel pill together with the typing row");
            module.editor_.Set("x"); module.UpdateInputAnimation(1240);
            Check(module.destination_reveal_ == 1, "typing retains the already-visible channel pill");
            module.UpdateInputAnimation(1360);
            Check(module.destination_reveal_ == 1, "a nonempty draft completes the channel pill entrance");
            module.editor_.Set({}); module.UpdateInputAnimation(1480);
            Check(module.destination_reveal_ == 1, "clearing a focused draft keeps its channel pill visible");
            module.UpdateInputAnimation(1600);
            Check(module.destination_reveal_ == 1 && module.Layout(design).input_text.x == idle.input_text.x,
                "an empty focused draft preserves the pill and reserved input columns");
            module.Focus(false);
            Check(idle.destination.width < 60 && std::string_view{ChatDestinationTag(module.InputDestination())} == "SAY",
                "the abbreviated channel badge gives unused label space back to the draft");
            for (const auto command : {"/s", "/p", "/l", "/l2", "/u", "/sh", "/y", "/t Cody", "/r", "/em", "/ae", "/aj", "/fishwatch", ""})
                Check(std::strlen(ChatDestinationTag(DescribeChatDestination(command))) <= 3, "every destination tag is at most three characters");
            prefix("/unity "); module.SetNativeDestination(native.SendPrefix(), "Shio");
            Check(module.SendChannelLabel() == "Unity", "native mode changes refresh without observing a typed /cm command");
            prefix("/tell Cody "); module.SetNativeDestination(native.SendPrefix(), "Shio");
            Check(module.SendChannelLabel() == "Tell: Cody", "native locked tells include the actual recipient");
            Check(std::string_view{ChatDestinationTag(module.InputDestination())} == "TEL", "tell badges stay compact while retaining the full recipient label");
            module.Focus(true); const auto original = module.Layout(design);
            Check(original.input_text.x == idle.input_text.x && original.input_text.width == idle.input_text.width &&
                original.destination.width == idle.destination.width && original.input.height > idle.input.height,
                "opening composition animates its row without shifting the reserved horizontal columns");
            module.editor_.Set("/p one message");
            Check(module.SendChannelLabel() == "Party" && module.InputDestination().mode == 5 &&
                module.Layout(design).input_text.x == original.input_text.x && module.Layout(design).input_text.width == original.input_text.width,
                "typed channel commands update the indicator without shifting the draft or character counter");
            module.editor_.Set("/t Another hello");
            Check(module.SendChannelLabel() == "Tell: Another", "an explicit tell replaces the locked native recipient in the indicator");
            module.editor_.Set("/r hello");
            Check(module.SendChannelLabel() == "Tell: Shio", "reply indicators use the native reply recipient");
            Check(std::string_view{ChatDestinationTag(module.InputDestination())} == "REP" && module.InputDestination().mode == 4,
                "typed replies show REP with the tell channel's text color");
            chat_module reply;
            const std::array<std::string, 1> targets{"Shio"};
            Check(reply.OpenReply(targets) && std::string_view{ChatDestinationTag(reply.InputDestination())} == "REP" && reply.InputDestination().mode == 4,
                "reply shortcuts retain REP when composition expands to a tell command");
            reply.ObserveTeller("Another");
            Check(std::string_view{ChatDestinationTag(reply.InputDestination())} == "REP", "new incoming tells cannot relabel the reply being composed");
            reply.OpenTell("Shio");
            Check(std::string_view{ChatDestinationTag(reply.InputDestination())} == "TEL", "explicit tells keep their own TEL indicator");
            module.editor_.Set("/fishwatch");
            Check(module.SendChannelLabel() == "Command: /fishwatch" && module.InputDestination().mode == 0,
                "non-chat commands are not labeled as messages to the default channel");
            module.editor_.Set("ordinary message"); module.channel_ = 2;
            Check(module.SendChannelLabel() == "Linkshell 1" && module.InputCommand(module.editor_.Text()) == "/linkshell ordinary message",
                "XIUI+ channel overrides describe the unchanged actual send command");
            module.editor_.Set({});
            Check(module.SendChannelLabel() == "Linkshell 1", "clearing a draft does not reset an explicitly selected channel");
            module.editor_.Set("ordinary message");
            module.channel_ = chat_module::game_channel;
            module.active_tab_ = static_cast<int>(chat_tab::party);
            Check(module.SendChannelLabel() == "Tell: Cody", "history filters do not select an outgoing channel");
            prefix("/unknown "); module.SetNativeDestination(native.SendPrefix(), {});
            module.editor_.Set({});
            Check(module.SendChannelLabel() == "Say" && std::string_view{ChatDestinationTag(module.InputDestination())} == "SAY",
                "an empty draft without a selected or readable native channel defaults its pill to SAY");
            object.fill('x'); Check(!native.SendPrefix(), "unterminated native prefixes are rejected");
            current = 0; Check(!native.SendPrefix(), "unavailable native state is safe during client transitions");
            chat_module secondary{true}; secondary.ApplyTextDefaults(ui::theme::CreateDefault());
            Check(secondary.Layout(design).destination.height == 0, "the shared composition destination is not duplicated in Window 2");
            Check(DescribeChatDestination(" /L2 hello").mode == 213 && DescribeChatDestination("/aj hello").mode == 219 &&
                DescribeChatDestination("/em waves").mode == 7 && DescribeChatDestination("/t").label == "Tell",
                "channel aliases, casing, emotes and incomplete tell drafts retain truthful labels");
            module.editor_.Set("animation"); module.Focus(true); module.UpdateInputAnimation(2000);
            const auto composing = module.Layout(design);
            module.Focus(false); module.UpdateInputAnimation(2120);
            const auto closing = module.Layout(design);
            Check(closing.input.height > 0 && closing.input.height < composing.input.height &&
                closing.log.height > composing.log.height && closing.log.height < idle.log.height,
                "closing composition smoothly returns the typing row's height to the log");
            module.UpdateInputAnimation(2240);
            Check(module.Layout(design).input.height == 0 && module.Layout(design).destination.height == 0 &&
                module.Layout(design).counter.height == 0 && module.Layout(design).log.height == idle.log.height,
                "the completed exit leaves no typing row, pill, counter or reserved vertical gap");
        }

        static auto DestinationPicker(void) -> void
        {
            chat_module module; chat_manager chat; ui::design_system design; bool save{};
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.SetNativeDestination("/say ", {});
            module.editor_.Set("unsent message", 4);
            module.Focus(true); module.UpdateInputAnimation(1); module.UpdateInputAnimation(241);
            const ui::rect viewport{0, 0, 1280, 720};
            const auto mouse = [&](const uint32_t message, const ui::rect& bounds) {
                return module.HandleMouse(message, 0, bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f,
                    false, design, viewport, save, &chat);
            };
            const auto pill = module.Layout(design).destination;
            const ui::rect outside{1000, 20, 10, 10};
            Check(mouse(WM_LBUTTONDOWN, pill) && !module.ChannelPickerOpen(), "channel picker waits for its matching click release");
            Check(mouse(WM_LBUTTONUP, outside) && !module.ChannelPickerOpen(), "releasing outside the pill cancels opening and owns the release");
            const auto open = [&] {
                mouse(WM_LBUTTONDOWN, pill); mouse(WM_LBUTTONUP, pill);
                Check(module.ChannelPickerOpen(), "clicking the channel pill opens its picker");
            };
            open();
            Check(module.editor_.Text() == "unsent message" && module.editor_.Caret() == 4 && chat.count == 0,
                "opening the picker preserves the draft and caret without sending");
            const auto party = module.ChannelPickerLayout(design).rows[1];
            mouse(WM_LBUTTONDOWN, party);
            Check(module.channel_ == chat_module::game_channel, "pressing a channel choice alone cannot change the destination");
            mouse(WM_LBUTTONUP, party);
            Check(module.channel_ == 1 && save && !module.ChannelPickerOpen() && module.editor_.Caret() == 4 &&
                module.InputCommand(module.editor_.Text()) == "/party unsent message" && chat.count == 0,
                "a released channel choice changes the actual outgoing command once and requests persistence");
            module.editor_.Set("/p hello all");
            Check(module.ChooseSendChannel(2, save) && module.editor_.Text() == "hello all" && module.InputCommand(module.editor_.Text()) == "/linkshell hello all",
                "changing channels replaces an explicit chat prefix while preserving its message");
            module.editor_.Set("/p /echo literal");
            Check(module.ChooseSendChannel(0, save) && module.InputCommand(module.editor_.Text()) == "/say /echo literal",
                "changing a chat destination cannot turn a literal slash-leading body into a command");
            module.editor_.Set("/fishwatch");
            Check(!module.ChooseSendChannel(1, save) && module.editor_.Text() == "/fishwatch", "utility commands are never rewritten by channel selection");
            module.editor_.Set("keep my message");
            Check(!module.ChooseSendChannel(7, save) && module.editor_.Text() == "keep my message", "Tell selection without a recipient leaves the message intact");
            module.SetNativeDestination("/say ", "Shio");
            Check(module.ChooseSendChannel(7, save) && module.InputCommand(module.editor_.Text()) == "/t Shio keep my message" && chat.count == 0,
                "Tell selection uses a known recipient and never treats message text as a name");
            Check(module.ChooseSendChannel(chat_module::game_channel, save) && module.editor_.Text() == "keep my message" && module.SendChannelLabel() == "Say",
                "Game selection restores native routing without retaining a tell recipient in the body");
            open(); mouse(WM_LBUTTONDOWN, outside);
            Check(!module.ChannelPickerOpen() && mouse(WM_LBUTTONUP, outside), "clicking outside dismisses the picker and drains the owned release");
            open(); module.HandleKey(nullptr, VK_ESCAPE, 0, false, true);
            Check(!module.ChannelPickerOpen() && module.Focused() && module.editor_.Text() == "keep my message", "Escape dismisses the picker without closing composition");
            module.editor_.Set({}); module.Focus(false);
            module.UpdateInputAnimation(2000);
            Check(module.Layout(design).input.height == 0, "closing composition removes the entire typing row");
            module.Focus(true); module.UpdateInputAnimation(2120);
            mouse(WM_MOUSEMOVE, module.Layout(design).destination); module.UpdateInputAnimation(2240);
            Check(module.destination_reveal_ > 0, "hover reveals the channel slot in an active empty composition");
            open(); module.CancelPointer();
            Check(!module.ChannelPickerOpen(), "input suspension closes the picker without sending a message");
        }

        static auto ChannelShortcuts(void) -> void
        {
            chat_module module;
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.SetNativeDestination("/say ", {});
            const auto alt = [](const uint32_t scan) { return static_cast<LPARAM>(0x20000001u | scan << 16); };
            Check(!module.HandleKey(nullptr, 'P', alt(0x19), false, true) && !module.Focused() && module.channel_ == chat_module::game_channel,
                "following the game leaves gameplay Alt+P with the native chat mode");
            module.editor_.Set("/l this is an example", 21); module.Focus(true);
            Check(module.HandleKey(nullptr, 'M', alt(0x27), false, true) && module.channel_ == 3 && module.editor_.Text() == "this is an example" &&
                module.InputCommand(module.editor_.Text()) == "/linkshell2 this is an example" && module.TakeChannelSave() && !module.TakeChannelSave(),
                "Alt on the physical semicolon key (AZERTY M) moves a Linkshell draft to Linkshell 2 and requests a save");
            Check(module.HandleKey(nullptr, 'P', static_cast<LPARAM>(0x40000000u) | alt(0x19), false, true) && module.channel_ == 3,
                "a held shortcut does not switch again");
            Check(module.HandleKey(nullptr, 'P', alt(0x19), false, true) && module.channel_ == 1 && module.editor_.Text() == "this is an example" &&
                module.Focused(), "Alt+P switches to Party and keeps the draft open");
            Check(module.HandleKey(nullptr, 'S', alt(0x1F), true, true) && module.channel_ == 1 && module.editor_.Text() == "this is an example",
                "a key bound by Ashita neither switches channels nor types");
            module.Focus(false);
            Check(module.HandleKey(nullptr, 'L', alt(0x26), false, true) && module.Focused() && module.channel_ == 2 &&
                module.InputCommand(module.editor_.Text()) == "/linkshell this is an example",
                "with an XIUI+ channel selected, gameplay shortcuts open composition on the chosen channel");
        }

        static auto NativeSendChannel(void) -> void
        {
            chat_module module; chat_manager chat; ui::design_system design;
            module.ApplyTextDefaults(ui::theme::CreateDefault());
            Check(module.channel_ == chat_module::game_channel && std::strcmp(module.Option(7).choices[module.channel_], "Game") == 0,
                "new chat instances follow the native chat channel by default");
            module.Load(settings_store::FromIniText("[chat]\nchannel=0\n"));
            Check(module.channel_ == chat_module::game_channel, "the old default Say channel migrates to Game");
            auto snapshot = settings_store::FromIniText(""); module.Save(snapshot);
            OverlayProfilePreferences(snapshot, settings_store::FromIniText("[chat]\nchannel=0\n[chat2]\nchannel=0\n"));
            module.Load(snapshot);
            Check(module.channel_ == chat_module::game_channel && snapshot.ReadInt("chat2", "channel_version", -1) == 0,
                "legacy character profiles cannot inherit a snapshot marker that skips native-channel migration");
            OverlayProfilePreferences(snapshot, settings_store::FromIniText("[chat]\nchannel=0\nchannel_version=1\n"));
            module.Load(snapshot);
            Check(module.channel_ == 0, "modern character profiles preserve deliberate Say overrides");
            module.Load(settings_store::FromIniText("[chat]\nchannel=1\n"));
            Check(module.channel_ == 1, "an existing non-default channel override is preserved");
            module.channel_ = 0;
            auto saved = settings_store::FromIniText(""); module.Save(saved);
            chat_module restored; restored.Load(saved);
            Check(restored.channel_ == 0, "an explicit Say override survives later saves without repeating migration");
            module.ResetDefaults(ui::theme::CreateDefault());
            module.Focus(true); module.editor_.Set("hello native channel");
            const auto ordinary = module.Layout(design);
            Check(ordinary.input.x == ordinary.content.x && ordinary.input.width == ordinary.content.width,
                "the channel indicator shares the input row without changing its outer bounds");
            Check(module.Submit(&chat) && chat.mode == static_cast<int32_t>(Ashita::CommandMode::Typed) && chat.sent == "hello native channel" && chat.count == 1,
                "default messages reach the native typed-chat path once without forcing a channel or recipient");
            module.Focus(true); module.editor_.Set("/l explicit linkshell");
            const auto command = module.Layout(design);
            Check(command.input.x == ordinary.input.x && command.input.width == ordinary.input.width,
                "starting a slash command does not shift the input or caret area");
            Check(module.Submit(&chat) && chat.mode == static_cast<int32_t>(Ashita::CommandMode::AshitaParse) && chat.sent == "/l explicit linkshell" && chat.count == 2,
                "explicit slash commands retain their original parser route and destination");
            chat.input_limit = 18; module.SetInputContext(&chat); module.Focus(true);
            module.editor_.Set({}); module.editor_.Insert(std::string(100, 'x'));
            Check(module.editor_.Text().size() == 18 && module.InputUsage(module.editor_.Text()) == std::pair<size_t, size_t>{18, 18},
                "native default input counts actual text without reserving a hidden Say prefix");
            bool save{}; const auto input = module.Layout(design).input;
            module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, input.x + 2, input.y + 4, false, design, {0, 0, 1280, 720}, save);
            module.HandleMouse(WM_LBUTTONUP, 0, input.x + 2, input.y + 4, false, design, {0, 0, 1280, 720}, save);
            Check(module.channel_ == chat_module::game_channel && !save, "clicking the input start cannot cycle an invisible channel prefix");
            module.Save(saved); restored.Load(saved);
            Check(restored.channel_ == chat_module::game_channel, "native channel following persists normally");
        }

        static auto Encoding(void) -> void
        {
            const std::string accents = "éèàçñäöü ÉÈÀÇÑÄÖÜ œŒß";
            const std::string native = "\x85\xC8\x85\xC7\x85\xBF\x85\xC6\x85\xD0\x85\xC3\x85\xD5\x85\xDB "
                "\x85\xA8\x85\xA7\x85\x9F\x85\xA6\x85\xB0\x85\xA3\x85\xB5\x85\xBB \x85\x5C\x85\x4C\x85\xBE";
            std::string encoded;
            Check(ToGameText(accents, encoded) && encoded == native && FromGameText(native) == accents,
                "French, Spanish and German accents and ligatures use FFXI's Western character page in both directions");
            for (wchar_t character = 0xC0; character <= 0xFF; ++character)
            {
                if (character == 0xD7 || character == 0xF7) continue;
                const auto text = ToUtf8({&character, 1});
                Check(ToGameText(text, encoded) && encoded == std::string{static_cast<char>(0x85), static_cast<char>(character - 0x21)} &&
                    FromGameText(encoded) == text, "all Latin-1 accented letters round trip without transliteration");
            }
            const auto token = ChatToken(0x85C8);
            const auto mixed = std::string{"\x1E\x85"} + native + " \x82\xB1\x82\xF1 " + token + "\xEF\x27ok\xEF\x28";
            Check(FromGameText(mixed) == std::string{"\x1E\x85"} + accents + " こん " + token + std::string{chat_cap_open} + "ok" + std::string{chat_cap_close},
                "accent decoding preserves Japanese, color operands, raw tokens and auto-translate caps");
            Check(ToGameText(accents + " こん " + token, encoded) && encoded == native + " \x82\xB1\x82\xF1 " + token,
                "outbound accents coexist with Japanese and opaque auto-translate bytes");
            Check(ToGameText("é±×÷", encoded) && encoded == "\x85\xC8\x81\x7D\x81\x7E\x81\x80",
                "existing Shift-JIS symbol encodings remain preferred");
            Check(!ToGameText("é😀", encoded) && !ToGameText("\xC3", encoded), "unsupported characters and incomplete UTF-8 remain rejected");

            chat_manager chat; chat_module module;
            chat_event event{}; event.sequence = 1; event.windows = 1; event.mode = 1;
            strcpy_s(event.text, native.c_str()); module.Receive(nullptr, event, false);
            std::string received;
            for (const auto& span : module.model_.Lines().front().spans) received += span.text;
            Check(received == accents, "received native accents reach the shared chat model as readable UTF-8");
            chat.input_limit = 10; module.channel_ = 0; module.SetInputContext(&chat);
            module.editor_.Insert("éèà");
            Check(module.editor_.Text() == "éè" && module.InputUsage(module.editor_.Text()) == std::pair<size_t, size_t>{4, 5},
                "accented input respects the native byte budget and retains complete characters");
            Check(module.Submit(&chat) && chat.count == 1 && chat.sent == "/say \x85\xC8\x85\xC7" && !module.Focused(),
                "accented drafts submit once through the normal chat command path");
        }

        static auto InputLimits(void) -> void
        {
            chat_manager chat; chat.input_limit = 18;
            chat_module module; module.channel_ = 0; module.SetInputContext(&chat);
            module.editor_.Insert(std::string(100, 'a'));
            Check(module.editor_.Text() == std::string(13, 'a') && module.InputUsage(module.editor_.Text()) == std::pair<size_t, size_t>{13, 13},
                "typing and paste stop at the native budget after accounting for the selected channel");
            module.editor_.Insert("b");
            Check(module.editor_.Text().size() == 13 && module.Submit(&chat) && chat.sent.size() == 18,
                "a full input rejects growth and the same limit permits sending");
            module.editor_.Set("/t Test "); module.editor_.Insert(std::string(100, 'x'));
            Check(module.editor_.Text().size() == 18 && module.InputUsage(module.editor_.Text()).first == 18,
                "explicit commands and tell recipients use the native budget");
            module.editor_.SelectAll(); module.editor_.Insert("ok");
            Check(module.editor_.Text() == "ok", "replacement remains available when an input was full");
            const auto token = ChatToken(0x0E06);
            module.editor_.Set(std::string(8, 'a')); module.editor_.Insert(token);
            Check(module.editor_.Text() == std::string(8, 'a'), "a token that cannot fit is not split");
            module.editor_.Set(std::string(7, 'a')); module.editor_.Insert(token);
            Check(module.editor_.Text() == std::string(7, 'a') + token, "whole auto-translate tokens fit at the boundary");
            const std::string japanese = "\xE3\x81\x82";
            module.editor_.Set({});
            for (size_t i = 0; i < 10; ++i) module.editor_.Insert(japanese);
            Check(module.editor_.Text().size() == 18 && module.InputUsage(module.editor_.Text()).first == 12,
                "Japanese input is capped by encoded game length without splitting Unicode characters");
            module.editor_.Set("abc"); module.editor_.Complete({"abcdefghijklmnop"}, false);
            Check(module.editor_.Text() == "abc", "completion cannot insert an over-limit value");
            module.editor_.ResetCompletion();
            module.auto_translate_labels_[token] = "/wave";
            module.editor_.Set({}); module.editor_.Insert(token + " " + std::string(30, 'x'));
            Check(module.InputCommand(module.editor_.Text()).size() == 18 && module.InputUsage(module.editor_.Text()).first == 18,
                "resolved auto-translate commands share the exact submission budget");
            module.editor_.Set({}); module.Focus(true);
            module.UpdateInputAnimation(1000); module.UpdateInputAnimation(1240);
            ui::design_system design;
            design.SetAppearanceTheme(ui::xiv1_appearance);
            const auto layout = module.Layout(design);
            Check(module.input_counter_ && layout.counter.height > 0 && layout.counter.y == layout.input_text.y &&
                layout.counter.height == layout.input_text.height && layout.input_text.x + layout.input_text.width + 8 <= layout.counter.x &&
                layout.counter.x + layout.counter.width <= layout.input.x + layout.input.width,
                "counter shares the typing row with reserved space that prevents overlap");
            Check(ChatInputStyle(design, module.text_.style).weight == ChatRunWeight(design, false),
                "typing text uses the same weight as ordinary chat prose");
            *module.Option(module.AnimationOptionStart() + 2).toggle = false;
            const auto withoutCounter = module.Layout(design);
            Check(module.ConfigSectionContains(0, module.AnimationOptionStart() + 2) && withoutCounter.counter.height == 0 &&
                withoutCounter.input_text.width > layout.input_text.width && withoutCounter.log.height == layout.log.height,
                "hiding the inline counter returns typing space without moving the log");
            module.editor_.Insert(std::string(30, 'x'));
            Check(module.editor_.Text().size() == 13, "hiding the counter does not disable the input limit");
            auto saved = settings_store::FromIniText(""); module.Save(saved); chat_module restored; restored.Load(saved);
            Check(!restored.input_counter_, "counter visibility persists");
            restored.ResetDefaults(ui::theme::CreateDefault());
            Check(restored.input_counter_, "reset restores the default-on counter");
        }

        static auto AutoTranslateCommands(void) -> void
        {
            const auto wave = ChatToken(0x0701), south = ChatToken(0x0E06);
            chat_module module;
            chat_manager chat;
            module.auto_translate_labels_[wave] = "/wave";
            module.auto_translate_labels_[south] = "South";
            module.Focus(true); module.editor_.Set("/wa");
            module.editor_.CompleteCandidates({{"/wave", wave, chat_completion_kind::auto_translate}}, false);
            module.HandleKey(nullptr, VK_RETURN, 0, false, true);
            Check(chat.count == 0 && module.editor_.Text() == wave, "choosing an auto-translated command does not execute it");
            Check(module.Submit(&chat) && chat.sent == "/wave" && chat.mode == -1 && chat.count == 1,
                "auto-translated emotes execute as commands instead of Say messages");
            module.channel_ = 1;
            module.editor_.Set(wave + " <t> " + south);
            Check(module.Submit(&chat) && chat.sent == "/wave <t> " + south,
                "emote command routing preserves arguments and body tokens regardless of selected channel");
            module.editor_.Set("hello " + wave);
            Check(module.Submit(&chat) && chat.sent == "/party hello " + wave,
                "an emote token in message text stays an auto-translate phrase");
            module.editor_.Set(south);
            Check(module.Submit(&chat) && chat.sent == "/party " + south, "ordinary leading phrases keep their selected chat channel");
            module.editor_.Set("/em waves " + south);
            Check(module.Submit(&chat) && chat.sent == "/em waves " + south, "typed emotes preserve their body tokens");
            module.auto_translate_labels_.erase(wave);
            chat.translate_token = wave; chat.translation = "/wave";
            module.editor_.Set(wave);
            Check(module.Submit(&chat) && chat.sent == "/wave", "pasted command tokens can resolve through the native parser");
            const auto count = chat.count;
            module.editor_.Set(wave + " " + std::string(250, 'x'));
            Check(!module.Submit(&chat) && chat.count == count && module.editor_.Text().starts_with(wave),
                "expanded commands obey the native input limit and preserve rejected drafts");
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
            Check(module.Submit(&chat) && chat.sent == south && chat.mode == static_cast<int32_t>(Ashita::CommandMode::Typed),
                "selected phrases retain their native token and the game's selected send channel");
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
            Check(std::abs(MeasureChatText(design, text, style) - design.MeasureText("South", style) - 16 * style.scale - 4 * ui::TextPixelScale(style.scale)) < .001f,
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

        static auto MatchWindowSettings(void) -> void
        {
            chat_module first, second{true};
            first.AttachSecondWindow(second);
            Check(first.background_ && first.container_.border && first.ConfigSectionContains(3, first.ContainerOptionStart()) &&
                first.ConfigSectionContains(3, first.ContainerOptionStart() + 1),
                "chat starts framed and exposes background/frame controls under Appearance");
            const auto settings = settings_store::FromIniText(
                "[chat]\nlayout_version=4\nopacity=0.8\npadding=12\nline_spacing=4\nmessage_animation=2\nmessage_animation_speed=150\n"
                "collapse_mode=2\ncollapse_delay=8\ncollapse_min_lines=2\ncollapse_max_lines=18\nheight_limits_lines=1\ntimestamp_display=3\n"
                "[layout.chat]\nx=20\ny=500\nwidth=600\nheight=300\n"
                "[chat2]\nenabled=1\ndefaults_initialized=1\nlayout_version=4\nopacity=0.3\nbackground=0\nborders=0\nline_spacing=1\nsource=1\ntab_window2=1\n"
                "[layout.chat2]\nx=650\ny=200\nwidth=420\nheight=180\n");
            first.Load(settings); second.Load(settings);
            Check(first.background_ && first.container_.border && !second.background_ && !second.container_.border,
                "legacy settings retain a background while explicit transparent choices load independently");
            first.text_.style.scale = 1.2f; first.text_.style.top_color = 0xFF123456;
            first.chrome_.style.scale = 0.7f; second.text_.style.scale = 0.8f;
            const auto match = second.MatchWindow1Option();
            auto toggle = second.Option(match);
            Check(toggle.available && !*toggle.toggle && second.ConfigSectionContains(0, match), "Window 2 exposes an opt-in matching toggle under General");
            *toggle.toggle = true; second.OnOptionChanged(match);
            Check(second.State().bounds.x == 650 && second.State().bounds.y == 200 && second.State().bounds.width == 600 &&
                second.State().bounds.height == 300 && second.state_.enabled && second.source_ == 1 && second.visible_tabs_[1] && !second.visible_tabs_[0],
                "matching copies dimensions while retaining position, visibility, message source and tabs");
            Check(second.background_ && second.container_.border && second.container_.opacity == 0.8f && second.container_.padding == 12 && second.line_spacing_ == 4 &&
                second.text_.style.scale == 1.2f && second.text_.style.top_color == 0xFF123456 && second.chrome_.style.scale == 0.7f &&
                second.message_animation_ == 2 && second.message_animation_speed_ == 150 && second.timestamp_display_ == 3 &&
                second.height_limits_lines_ && second.CollapseSettings()->maximum_lines == 18,
                "matching retains complete text profiles and follows appearance, animation, timestamps and reactive sizing");
            for (size_t index = 0; index < second.OptionCount(); ++index)
                if (second.InheritedWindowOption(index)) Check(!second.Option(index).available, "inherited controls cannot receive pointer or controller edits");
            Check(second.Option(0).available && second.Option(7).available && second.Option(8).available && second.Option(9).available &&
                second.Option(match).available, "visibility, routing, tabs and unlink remain editable");
            Check(!second.InheritedWindowOption(second.ContentOptionCount()) && second.Option(second.ContentOptionCount()).available,
                "the linked window retains its independent element scale");
            native_chat_filters filters;
            first.SetNativeFilters(filters); second.SetNativeFilters(filters);
            Check(!second.InheritedWindowOption(second.NativeFiltersStart()), "native filters remain independent when matching");
            first.container_.opacity = 0.65f; first.message_animation_speed_ = 200; first.OnOptionChanged(3);
            Check(second.container_.opacity == 0.65f && second.message_animation_speed_ == 200, "Window 1 edits update the linked window immediately");
            *first.Option(first.ContainerOptionStart()).toggle = false;
            first.OnOptionChanged(first.ContainerOptionStart());
            Check(!second.background_ && second.container_.border, "linked chat follows background changes while retaining the frame");
            first.State().bounds.width = 620; second.EnsureWindowDefaults();
            Check(second.State().bounds.width == 620, "direct resizing of Window 1 reaches the linked window");
            second.State().bounds.height = 280; second.collapse_due_ = 1234; second.EnsureWindowDefaults();
            Check(second.State().bounds.height == 280 && second.collapse_due_ == 1234, "viewport clamping does not repeatedly reset linked collapse state");
            ui::design_system design;
            Check(second.Grip(design).width == 0, "matched dimensions have no independent resize grip");
            second.State().bounds.x = 700;
            auto saved = settings_store::FromIniText(""); first.Save(saved); second.Save(saved);
            Check(saved.ReadBool("chat2", "match_window1", false) && saved.ReadFloat("chat2", "opacity", 0) == 0.3f &&
                !saved.ReadBool("chat2", "background", true) && !saved.ReadBool("chat2", "borders", true) &&
                saved.ReadFloat("layout.chat2", "width", 0) == 420 && saved.ReadFloat("layout.chat2", "x", 0) == 700,
                "saving retains independent overrides and the current position alongside the link");
            chat_module reloadedFirst, reloadedSecond{true}; reloadedFirst.AttachSecondWindow(reloadedSecond);
            reloadedFirst.Load(saved); reloadedSecond.Load(saved);
            Check(reloadedSecond.MatchingWindow1() && reloadedSecond.container_.opacity == 0.65f && reloadedSecond.text_.style.scale == 1.2f &&
                !reloadedSecond.background_ && reloadedSecond.container_.border &&
                reloadedSecond.State().bounds.width == 620 && reloadedSecond.State().bounds.x == 700 && reloadedSecond.source_ == 1,
                "ordinary profile reload restores the live link without changing its independent routing or position");
            *reloadedSecond.Option(reloadedSecond.MatchWindow1Option()).toggle = false;
            reloadedSecond.OnOptionChanged(reloadedSecond.MatchWindow1Option());
            Check(reloadedSecond.container_.opacity == 0.3f && reloadedSecond.text_.style.scale == 0.8f &&
                !reloadedSecond.background_ && !reloadedSecond.container_.border &&
                reloadedSecond.State().bounds.width == 420 && reloadedSecond.State().bounds.height == 180 && reloadedSecond.State().bounds.x == 700 &&
                reloadedSecond.source_ == 1 && reloadedSecond.visible_tabs_[1], "unlink restores prior settings while preserving movement and filters");
            first.collapse_mode_ = 0; first.SetNativeWindowConfig(native_chat_window_settings{true, 1, 14, 5});
            second.SetNativeWindowConfig(native_chat_window_settings{true, 2, 6, 10});
            second.EnsureWindowDefaults();
            Check(second.CollapseSettings()->maximum_lines == 14, "Follow game uses Window 1's native line settings while matched");
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
            module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, module.Grip(design).x + 9, module.Grip(design).y + 4, false, design, viewport, save);
            module.HandleMouse(WM_LBUTTONUP, 0, module.Grip(design).x + 9, -500, false, design, viewport, save);
            Check(module.VisualBounds(design).y >= viewport.y && module.State().bounds.y + module.State().bounds.height == 660,
                "resize keeps optional tabs on screen and bottom edge anchored");
            module.state_.bounds = {20, 350, 660, 310};
            module.active_tab_ = 0; module.source_ = 2;
            Check(layout.scrollbar.width == 6 && layout.scrollbar.x > layout.log.x + layout.log.width &&
                layout.scrollbar.x + layout.scrollbar.width <= layout.content.x + layout.content.width, "contrasting scrollbar has reserved space");
            Check(module.chrome_.style.scale >= 0.85f && module.chrome_.style.stroke.enabled == theme.meter_text.stroke.enabled, "tab labels use readable shared meter text");
            module.editor_.Set("/tell");
            Check(module.Layout(design).input.width == module.Layout(design).content.width, "explicit commands retain full borderless input width");
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
            animated.UpdateInputAnimation(1120);
            Check(animated.input_reveal_ > 0 && animated.input_reveal_ < 1 && animated.Layout(design).log.height < idleLogHeight, "input animates into the space reserved for typing");
            animated.UpdateInputAnimation(1240); animated.Focus(false); animated.UpdateInputAnimation(1360);
            Check(animated.input_reveal_ > 0 && animated.input_reveal_ < 1, "input animates out after focus is released");
            animated.UpdateInputAnimation(1480);
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
            secondView.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, secondView.Grip(design).x + 9, secondView.Grip(design).y + 4, false, design, viewport, save);
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
        if (argc == 2 && std::strcmp(argv[1], "--emphasis-only") == 0)
        {
            xiui::chat_test::Emphasis();
            std::puts("Chat semantic emphasis, native colors, real font weights and mixed-weight wrapping passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--match-window-only") == 0)
        {
            xiui::chat_test::MatchWindowSettings();
            std::puts("Chat Window 2 settings inheritance, independent routing, disabled controls and reversible profile persistence passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--bubbles-only") == 0)
        {
            xiui::chat_test::Bubbles();
            std::puts("Chat bubble channels, speaker parsing, NPC pages, bounded speakers, persistence and category navigation passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--dialogue-prompt-only") == 0)
        {
            xiui::chat_test::DialoguePrompts();
            std::puts("NPC continuation markers, wrapping, shared-window routing, animation lifetime and passive input passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--native-channel-only") == 0)
        {
            xiui::chat_test::DestinationIndicator();
            xiui::chat_test::DestinationPicker();
            xiui::chat_test::ChannelShortcuts();
            xiui::chat_test::NativeSendChannel();
            xiui::chat_test::InputLimits();
            xiui::chat_test::AutoTranslateCommands();
            std::puts("Native default send channel, explicit overrides, prefix-free input and encoded limits passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--input-limit-only") == 0)
        {
            xiui::chat_test::InputLimits();
            std::puts("Chat native limits, paste, token/Unicode boundaries, completion and counter settings passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--encoding-only") == 0)
        {
            xiui::chat_test::Encoding();
            std::puts("FFXI accent encoding, received text, mixed Japanese/tokens and bounded submission passed."); return 0;
        }

        using namespace xiui;
        if (argc == 2 && std::strcmp(argv[1], "--collapse-only") == 0)
        {
            chat_test::ReactiveWindows();
            std::puts("Native chat sizing, idle collapse, wrapped messages, minimum lines, focused history, persistence and shared stacking passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--native-filters-only") == 0)
        {
            chat_test::NativeFilters();
            chat_test::AutoTranslateCommands();
            std::puts("Native filter masks, per-window routing, Hold/Notify, group selection and emote history/commands passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--history-only") == 0)
        {
            chat_test::ExpandedHistory();
            chat_test::SingleWindowHistory();
            chat_test::WrappingWhitespace();
            std::puts("Chat history focus, expansion, channel tabs, wrapped-message/page/wheel scrolling and keyboard ownership passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--status-cancel-only") == 0)
        {
            chat_test::StatusCancellation();
            std::puts("Local effect cancellation flags, native requests, stale identities and Confirm gesture ownership passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--animation-speed-only") == 0)
        {
            chat_test::AnimationSpeed();
            std::puts("Chat animation speed, duration/expiration, independent persistence and reset checks passed.");
            return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--message-animation-only") == 0)
        {
            chat_test::MessageAnimation();
            std::puts("Chat message stack scrolling, wrapped/burst arrivals, entrance direction/fade, history suppression and clipped art checks passed.");
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
        if (argc == 2 && std::strcmp(argv[1], "--auto-translate-commands-only") == 0)
        {
            chat_test::AutoTranslateCommands();
            std::puts("Auto-translated emote command routing, body tokens, native fallback and input limit checks passed.");
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
        if (argc == 2 && std::strcmp(argv[1], "--sent-history-only") == 0)
        {
            chat_test::SentHistory(); chat_test::ManualHeight();
            std::puts("Space/slash sent history and manual chat height overrides passed."); return 0;
        }
        if (argc == 2 && std::strcmp(argv[1], "--full-keyboard-only") == 0)
        {
            chat_test::FullKeyboardInputs();
            std::puts("Full keyboard native draft visibility, first-character ownership, Compact shortcuts and editor isolation checks passed.");
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
        chat_test::Encoding();
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
