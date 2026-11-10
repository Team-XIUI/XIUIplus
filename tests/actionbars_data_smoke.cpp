#include "modules/actionbars/action_import.hpp"
#include "modules/actionbars/action_executor.hpp"
#include "modules/actionbars/action_resource_names.hpp"
#include "native_target.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace xiui
{
    struct action_executor_test
    {
        static auto NativePalette(const action_executor& executor) -> bool { return executor.native_menu_allowed_; }
        static auto BeginWait(action_executor& executor) -> void
        { executor.pending_ = {{"/echo first", 4000, false}, {"/echo next", 0, false}}; executor.next_ = 1; executor.due_ = 5000; }
        static auto Waiting(const action_executor& executor) -> bool
        { return executor.pending_.size() == 2 && executor.next_ == 1 && executor.due_ == 5000; }
        static auto Ready(const action_executor& executor, uint64_t now) -> bool { return executor.StepReady(now); }
        static auto StandaloneWait(action_executor& executor) -> void
        { executor.pending_[1].command.clear(); executor.pending_[1].delay = 4000; executor.due_ = 1000; }
        static auto CompileWaits(const action_slot& slot) -> bool
        {
            action_executor executor; bool timed{};
            const auto steps = executor.BuildSteps(slot, action_executor::Commands(slot, {}), [](const action_slot&) { return std::string{}; }, timed);
            return timed && steps.size() == 4 && steps[0].command == "/targetnpc" && steps[0].mode == 2 &&
                steps[1].command == "/item \"Pickaxe\" <t>" && steps[1].delay == 4000 && steps[1].mode == 2 &&
                steps[2].command.empty() && steps[2].delay == 4000 && steps[3].command == "/echo done" && steps[3].mode == 2;
        }
    };
}

namespace
{
    auto Check(bool condition, const char* message) -> void
    { if (!condition) throw std::runtime_error{message}; }
    auto Spell(uint32_t id, std::string name) -> xiui::action_slot
    { xiui::action_slot slot{}; slot.kind = xiui::action_kind::spell; slot.id = id; slot.name = std::move(name); return slot; }

    auto Storage(void) -> void
    {
        using namespace xiui;
        action_store store;
        store.SetContext({1, 2, {}});
        const auto first = store.ActivePalette(action_surface::hotbar)->id;
        Check(store.Assign(action_surface::hotbar, 0, 0, Spell(144, "Fire")), "assign a typed action");
        store.SetContext({1, 3, {}});
        Check(store.Resolve(action_surface::hotbar, 0, 0).Empty(), "subjob contexts do not overwrite one another");
        store.Assign(action_surface::hotbar, 0, 0, Spell(145, "Fire II"));
        store.SetScope(action_surface::hotbar, 0, action_scope::shared_subjob);
        Check(store.Resolve(action_surface::hotbar, 0, 0).id == 145, "switching storage scope copies the current actions");
        store.SetContext({1, 4, {}});
        Check(store.Resolve(action_surface::hotbar, 0, 0).id == 145, "shared-subjob storage survives a subjob change");
        Check(store.Resolve(action_surface::hotbar, 1, 0).Empty(), "other bars retain their own slots");
        const auto shared = store.ActivePalette(action_surface::hotbar)->id;
        const auto second = store.Copy(shared, "Burst");
        Check(!second.empty() && second != shared, "copies receive stable independent ids");
        const auto previousCrossbar = store.ActivePalette(action_surface::crossbar)->id;
        Check(store.Select(action_surface::hotbar, second), "select a hotbar palette");
        Check(store.ActivePalette(action_surface::crossbar)->id == previousCrossbar, "crossbar selection is independent");
        Check(store.Rename(second, "Renamed") && store.ActivePalette(action_surface::hotbar)->id == second, "rename retains the selected stable id");
        Check(store.Move(second, -1), "palette order can be changed");
        Check(store.Cycle(action_surface::hotbar, 1), "palette cycling follows the current scope");
        Check(store.Delete(second) && !store.Delete(shared), "deletion preserves one usable palette in each scope");
        store.SetContext({1, 2, {}});
        store.SetScope(action_surface::hotbar, 0, action_scope::job_subjob);
        Check(store.ActivePalette(action_surface::hotbar)->id == first && store.Resolve(action_surface::hotbar, 0, 0).id == 144, "returning to an existing scope preserves its assignments");
        store.SetContext({1, 2, "Ifrit"});
        store.SetPetAware(action_surface::hotbar, 0, true);
        Check(store.Assign(action_surface::hotbar, 0, 0, Spell(146, "Fire III"), true), "create a pet variant");
        Check(store.Resolve(action_surface::hotbar, 0, 0).id == 146, "pet variant takes precedence");
        store.SetContext({1, 2, "Shiva"});
        Check(store.Resolve(action_surface::hotbar, 0, 0).id == 144, "unconfigured pets retain the ordinary palette");
        store.SetContext({1, 2, {}});
        const auto macro = store.CreateMacro("Example", {"/echo first", "/wait 2", "/echo second <wait 1>"});
        action_slot ref{}; ref.kind = action_kind::macro; ref.macro_id = macro;
        Check(!macro.empty() && store.Assign(action_surface::crossbar, 5, 7, ref), "stable macro references can be assigned");
        auto record = *store.Macro(macro); record.lines[0] = "/echo changed";
        Check(store.SaveMacro(record) && store.ExpandMacro(store.Resolve(action_surface::crossbar, 5, 7)).lines[0] == "/echo changed", "editing a shared macro updates its references");
        settings_store settings{""}; store.Save(settings);
        settings.WriteInt("controller.crossbar.device.xinput", "left_trigger", 3);
        const auto portable = settings.Preferences("");
        Check(portable.ReadInt("controller.crossbar.device.xinput", "left_trigger", -1) == -1, "portable profiles exclude local controller mappings");
        action_store restored; restored.SetContext({1, 2, "Ifrit"}); restored.Load(settings_store::FromIniText(portable.ToIniText()));
        Check(restored.Resolve(action_surface::hotbar, 0, 0).id == 146, "portable roundtrip preserves pet assignments");
        Check(restored.ExpandMacro(restored.Resolve(action_surface::crossbar, 5, 7)).lines == record.lines, "portable roundtrip preserves shared macros and multiline timing text");
        const auto imported = PreviewActionImport(portable.ToIniText());
        Check(imported.Valid(), "native palette exports can be previewed");
        action_store destination; Check(CommitActionImport(destination, imported) == imported.palettes.size(), "native imports create new palettes");
        Check(store.DeleteMacro(macro) && store.Resolve(action_surface::crossbar, 5, 7).macro_id.empty() && store.Resolve(action_surface::crossbar, 5, 7).lines == record.lines, "removing a library entry preserves assigned macro commands");
        Check(!store.Assign(action_surface::hotbar, 10, 0, Spell(1, "Invalid")), "out-of-range assignment is rejected");
    }
    auto Imports(void) -> void
    {
        using namespace xiui;
        const auto source = R"lua(-- literal-only tHotBar and tCrossBar bindings
return T{
 Default = { ['1'] = { ActionType = 'Spell', Label = 'Fire', Macro = { '/ma "Fire" <t>', '/wait 2', '/echo done' } } },
 Palettes = { { Name = 'Extra', Bindings = {
  ['C0'] = { ActionType = 'Ability', Label = 'Provoke' },
  ['LT:1'] = { ActionType = 'Spell', Label = 'Cure', Macro = { '/ma "Cure" <stpt>' } },
  ['RT2:8'] = { ActionType = 'Command', Macro = { '/echo last' } },
  ['A1'] = { ActionType = 'Unsupported', Label = 'Skip' },
 } } }
})lua";
        const auto preview = PreviewActionImport(source);
        Check(preview.Valid() && preview.palettes.size() == 2 && preview.actions == 4 && preview.unsupported == 1, "literal imports preview supported and unsupported actions");
        Check(preview.palettes[0].actions.hotbar[0][0].lines.size() == 3, "imports preserve every macro line");
        Check(preview.palettes[1].actions.hotbar[1][9].name == "Provoke", "tHotBar modifier and zero-slot mappings are preserved");
        Check(preview.palettes[1].actions.crossbar[0][0].lines[0] == "/ma \"Cure\" <stpt>" && preview.palettes[1].actions.crossbar[5][7].lines[0] == "/echo last", "crossbar trigger banks map consistently");
        Check(!PreviewActionImport("return { Default = os.execute('echo bad') }").Valid(), "expressions and imported code cannot execute");
        Check(!PreviewActionImport("return {}; os.execute('echo bad')").Valid(), "trailing executable Lua is rejected");
        Check(!PreviewActionImport("return { a = function() end }").Valid(), "functions are rejected");
        Check(!PreviewActionImport("return T ").Valid(), "incomplete table wrappers are rejected safely");
        Check(!PreviewActionImport(std::string(2 * 1024 * 1024 + 1, ' ')).Valid(), "oversized imports are rejected");
        const auto duplicate = PreviewActionImport("return { Default = { ['1'] = { ActionType='Command', Macro={'/echo a'} }, ['1'] = { ActionType='Command', Macro={'/echo b'} } } }");
        Check(duplicate.Valid() && duplicate.conflicts != 0 && duplicate.palettes[0].actions.hotbar[0][0].lines[0] == "/echo b", "duplicate literal bindings report conflicts without overwriting existing palettes");
        const auto xiui = PreviewActionImport(R"lua(return {
 globalScale=2, fontSize=18, playerBarEnabled=false, windowPositions={PlayerBar={x=900,y=800}},
 controller={enabled=true,triggerThreshold=99}, readyCheckSoundFile='unrelated.wav',
 hotbarBar1 = { x=1.5, slotActions={['15:0:palette:Test']={ [1]={macroRef=12,macroPaletteKey=15}, [2]={actionType='ws',action='Fast Blade',target='t'} }} },
 hotbarCrossbar = { slotActions={['15:0:palette:Test']={ L2x2={ [8]={actionType='pet', action='Assault',target='t'} } }} },
 macroDB={ [15]={ {id=12,actionType='macro',displayName='Summon',macroText='/ma "Ifrit" <me>\n/wait 1\n/echo ready'} } },
})lua");
        Check(xiui.Valid() && xiui.actions == 3 && xiui.palettes.size() == 2 && xiui.palettes[0].name == "Test", "XIUI+ keeps equally named hotbar and crossbar palettes independent");
        Check(xiui.palettes[1].actions.hotbar[0][0].lines.size() == 3 && xiui.palettes[0].actions.crossbar[4][7].name == "Assault", "XIUI+ macro references resolve from their source library");
        Check(xiui.source_context && xiui.palettes[0].job == 15 && xiui.palettes[0].subjob == 0, "XIUI+ imports preserve source job and shared-subjob context");
        action_store actionsOnly; actionsOnly.SetContext({15, 0, {}});
        const auto existing = actionsOnly.AllPalettes().front().id;
        auto settings = settings_store::FromIniText("[appearance.hud]\nscale=1.250\n[player]\nenabled=1\n[layout.player]\nx=42\ny=84\n[controller.crossbar]\nenabled=0\n");
        Check(CommitActionImport(actionsOnly, xiui) == 2, "XIUI settings files import through the action-only boundary");
        actionsOnly.Save(settings);
        Check(settings.ReadFloat("appearance.hud", "scale", 0) == 1.25f && settings.ReadBool("player", "enabled", false) &&
            settings.ReadRect("layout.player", {}).x == 42 && settings.ReadRect("layout.player", {}).y == 84 &&
            !settings.ReadBool("controller.crossbar", "enabled", true) &&
            std::any_of(actionsOnly.AllPalettes().begin(), actionsOnly.AllPalettes().end(), [&](const auto& palette) { return palette.id == existing; }),
            "action imports preserve HUD size, module visibility, positions, controller configuration and existing palettes");
        Check(!PreviewActionImport("return { globalScale=2, playerBarEnabled=false, windowPositions={PlayerBar={x=900,y=800}} }").Valid(),
            "settings-only XIUI Lua files cannot create a full profile or an empty action import");
        const auto jobFile = PreviewActionImport(source, "C:/example/bindings/WAR.lua");
        Check(jobFile.source_context && jobFile.palettes[0].job == 1 && jobFile.palettes[0].subjob == 0, "tBar job filenames preserve their source job context");
        const auto petImport = PreviewActionImport(R"lua(return {
 hotbarBar1 = {slotActions={['15:0:palette:Default']={ [1]={actionType='ma',action='Ifrit',target='me'} }, Ifrit={ [1]={actionType='pet',action='Assault',target='t'} }}},
})lua");
        Check(petImport.Valid() && petImport.palettes.size() == 1 && petImport.palettes[0].pets.size() == 1 && petImport.palettes[0].pets[0].pet == "Ifrit", "Lua pet storage becomes an automatic variant of the matching job palette");
    }
    auto MigrationData(void) -> void
    {
        using namespace xiui;
        const auto preview = PreviewActionImport(R"lua(local settings = {};
settings["userSettings"] = {};
settings["userSettings"]["macroDB"] = {[15]={{id=12,actionType='ma',action='Fire',target='me'}, {id=13,actionType='macro',displayName='Echo',macroText='/echo hi',recastSourceType='spell',recastSourceAction='Cure',alwaysAvailable=true}, {id=14,actionType='ja',action='Provoke',target='t'}}};
settings["userSettings"]["hotbarBar1"] = {slotActions={['15:0:palette:Default']={[1]={macroRef=12,macroPaletteKey=15},[2]={macroRef=13,macroPaletteKey=15}}}};
settings["userSettings"]["hotbarCrossbar"] = {slotActions={['15:0:palette:Default']={L2R2={[1]={actionType='command',action='/echo ordered'}},Shared={[1]={actionType='command',action='/echo shared'}}}}};
return settings;)lua");
        Check(preview.Valid() && preview.macros.size() == 3 && preview.actions == 4, "actual saved-profile assignments retain typed and unused library records");
        action_store store; store.SetContext({15,0,{}});
        Check(CommitActionImport(store, preview) == 2 && store.Macros().size() == 3, "shared library import is committed with both surface palettes");
        const action_palette* hotbar{}, *crossbar{};
        for (const auto& palette : store.AllPalettes()) if (palette.name == "Default" && palette.job == 15 && !palette.actions.hotbar[0][0].macro_id.empty()) hotbar = &palette;
        for (const auto& palette : store.AllPalettes()) if (!palette.actions.crossbar[6][0].Empty()) crossbar = &palette;
        Check(hotbar && crossbar && hotbar->id != crossbar->id && crossbar->actions.crossbar[2][0].lines[0] == "/echo ordered" && crossbar->actions.crossbar[6][0].lines[0] == "/echo shared", "Shared and ordered expanded data survive without overwrite");
        const auto action = store.ExpandMacro(hotbar->actions.hotbar[0][0]);
        const auto macro = store.ExpandMacro(hotbar->actions.hotbar[0][1]);
        Check(action.kind == action_kind::spell && action.id == 0 && action.name == "Fire" && action.target == "<me>", "a library ID is never mistaken for a spell resource ID");
        Check(macro.recast_kind == action_kind::spell && macro.recast_name == "Cure" && macro.always_available, "macro cooldown and availability metadata survive source expansion");
        auto record = *store.Macro(hotbar->actions.hotbar[0][0].macro_id); record.definition.name = "Fire II"; record.definition.label = "Shared Fire II"; record.icon = "spell:145";
        Check(store.SaveMacro(record) && store.ExpandMacro(hotbar->actions.hotbar[0][0]).name == "Fire II" && store.ExpandMacro(hotbar->actions.hotbar[0][0]).label == "Shared Fire II" && store.ExpandMacro(hotbar->actions.hotbar[0][0]).icon == "spell:145", "editing a typed shared definition updates its slot references");
        settings_store settings{""}; store.Save(settings); action_store loaded; loaded.SetContext({15,0,{}}); loaded.Load(settings);
        Check(loaded.Macros().size() == 3 && loaded.Macros()[0].definition.kind == action_kind::spell, "typed and unassigned definitions survive Native reload");
        auto reference = hotbar->actions.hotbar[0][0]; reference.inherit_appearance = false; reference.label = "Local label"; reference.icon = "spell:1";
        Check(store.ExpandMacro(reference).label == "Local label" && store.ExpandMacro(reference).icon == "spell:1", "legacy per-slot appearance overrides are preserved");
        const auto old = settings_store::FromIniText("[actionbars.data]\nversion=1\npalette_count=1\nscope.h0=0\nscope.c0=0\n[actionbars.palette.0]\nid=old\nname=44656661756c74\njob=0\nsubjob=0\n[actionbars.palette.0.h0.0]\nkind=1\nid=144\nname=46697265\n[actionbars.palette.0.c2.0]\nkind=1\nid=145\nname=46697265204949\n");
        action_store legacy; legacy.Load(old);
        Check(legacy.ActivePalette(action_surface::hotbar) && legacy.ActivePalette(action_surface::crossbar) && legacy.ActivePalette(action_surface::hotbar)->id != legacy.ActivePalette(action_surface::crossbar)->id, "legacy combined sets become independent without resetting profile data");
        Check(legacy.Resolve(action_surface::hotbar,0,0).id == 144 && legacy.Resolve(action_surface::crossbar,2,0).id == 145, "splitting legacy sets retains both surfaces' assignments");
    }
    auto JapaneseNames(void) -> void
    {
        using namespace xiui;
        std::string japanese;
        Check(ToGameText("ケアル", japanese) && FromGameText(japanese) == "ケアル", "Japanese resource names round trip through the game encoding");
        int cure{};
        const auto lookup = [&](const char* name, const uint32_t language) -> int* {
            return language == 2 && std::string_view{name} == "Cure" || language == 1 && name == japanese ? &cure : nullptr;
        };
        Check(FindActionResource("Cure", lookup) == &cure && FindActionResource(japanese, lookup) == &cure,
            "English and Japanese names resolve the same resource through their language IDs");
        Check(FindActionResource("Unknown", lookup) == nullptr, "unknown names remain unresolved");
        const char* names[]{japanese.c_str(), japanese.c_str(), "Cure"};
        Check(ActionResourceName(names) == "Cure" && ActionResourceName(names, 0) == "ケアル",
            "client command names are independent of the English catalog name");
        const auto english = [](const action_slot& action) {
            return action.kind == action_kind::spell && (action.name == "ケアル" || action.name == "Cure") ? std::string{"Cure"} : std::string{};
        };
        const std::string source = "/ma \"ケアル\" <stpc> <wait 2>";
        Check(action_executor::ResolveCommandName(source, english) == "/ma \"Cure\" <stpc> <wait 2>" && source == "/ma \"ケアル\" <stpc> <wait 2>",
            "dispatch resolves Japanese names without changing saved text, targets or inline waits");
        Check(action_executor::ResolveCommandName("  /MAGIC ケアル   <me>", english) == "  /MAGIC \"Cure\"   <me>",
            "unquoted Japanese names retain the command spelling and target spacing");
        const auto nativeJapanese = [](const action_slot& action) { return action.name == "Cure" ? std::string{"ケアル"} : std::string{}; };
        const auto command = action_executor::ResolveCommandName("/ma 'Cure' <stpt>", nativeJapanese);
        std::string encoded;
        Check(command == "/ma \"ケアル\" <stpt>" && ToGameText(command, encoded) && encoded == "/ma \"" + japanese + "\" <stpt>",
            "English names convert to the Japanese client's encoded command name");
        for (const auto* line : {"/echo ケアル", "/ma \"Unknown\" <me>", "/ma \"unterminated", "/ma \"Cure\" <t>"})
            Check(action_executor::ResolveCommandName(line, english) == line, "unrelated, unresolved and unchanged commands retain their original text");
        auto spell = Spell(1, "ケアル"); spell.label = "回復"; spell.target = "<stpc>";
        Check(action_executor::Commands(spell, "Cure") == std::vector<std::string>{"/ma \"Cure\" <stpc>"},
            "custom Japanese labels do not replace the executable resource name");
    }

    auto ExecutionBoundaries(void) -> void
    {
        using namespace xiui;
        auto spell = Spell(144, "Fire"); spell.target = "<stnpc>";
        Check(ValidateAction(spell).empty(), "native subtarget placeholders are supported");
        Check(action_executor::Commands(spell, "Fire") == std::vector<std::string>{"/ma \"Fire\" <stnpc>"}, "typed actions retain native target syntax");
        spell.target = "<t> /shutdown";
        Check(!ValidateAction(spell).empty() && action_executor::Commands(spell, "Fire").empty(), "typed targets cannot append commands");
        spell.target = "<t>";
        Check(action_executor::Commands(spell, "Fire\"\n/echo bad").empty(), "resource-name quoting rejects command separators");
        action_slot macro{}; macro.kind = action_kind::macro; macro.lines = {"/echo first", "/wait 0.5", "/echo next <wait 1>"};
        Check(ValidateAction(macro).empty() && action_executor::Commands(macro, {}) == macro.lines, "macro lines retain native wait directives for the sequencer");
        macro.lines = {"/echo " + std::string(240, 'x')};
        Check(!ValidateAction(macro).empty(), "overlong commands are rejected before queuing");
        action_executor executor;
        Check(!executor.Activate(nullptr, Spell(144, "Fire"), 0) && !executor.Status().empty(), "missing native services cannot dispatch an action");
        executor.SetRestricted(true);
        Check(!executor.Activate(nullptr, Spell(144, "Fire"), 0), "restricted activation cannot dispatch");
        const auto owned = executor.Inspect(nullptr, Spell(144, "Fire"));
        const auto preview = executor.Inspect(nullptr, Spell(144, "Fire"), true);
        Check(!owned.usable && owned.reason == "Close the editor or finish typing first.", "ordinary inspection retains input ownership restrictions");
        Check(!preview.known && !preview.usable && preview.reason == "Action services are unavailable.", "presentation skips input ownership without bypassing client availability");
        unsigned resolutions{};
        executor.SetMacroResolver([&](const action_slot&) { ++resolutions; return Spell(144, "Fire"); });
        action_slot reference{}; reference.kind = action_kind::macro; reference.macro_id = "shared-macro";
        Check(executor.Inspect(nullptr, reference, true).reason == preview.reason && resolutions == 1, "shared macro presentation propagates descriptive inspection");
        Check(!executor.Activate(nullptr, reference, 0) && resolutions == 1 && executor.Status() == owned.reason, "presentation never enables restricted macro activation");
        bool nativeRun{};
        executor.SetNativeRunner([&](const native_macro_ref&) { nativeRun = true; return true; });
        action_slot native{}; native.kind = action_kind::native_macro;
        Check(!executor.Activate(nullptr, native, 0) && !nativeRun, "restricted presentation cannot invoke the native macro runner");
        auto invalid = Spell(144, "Fire"); invalid.target = "<t> /echo injected";
        Check(!executor.Inspect(nullptr, invalid, true).usable && executor.Inspect(nullptr, invalid, true).reason == ValidateAction(invalid), "presentation retains action validation");
    }
    auto MacroPresentation(void) -> void
    {
        using namespace xiui;
        action_slot macro{}; macro.kind = action_kind::macro; macro.name = "Party heal"; macro.label = "Heal"; macro.icon = "custom.png";
        macro.lines = {"/equip main \"Light Staff\"", "/wait 1", "/echo Healing", "/ma \"Cure III\" <stpt> <wait 2>", "/ma \"Cure IV\" <stpt>"};
        const auto original = macro;
        const auto representative = action_executor::RepresentativeAction(macro);
        Check(representative && representative->kind == action_kind::spell && representative->name == "Cure III" && representative->target == "<stpt>", "macro presentation finds the first resource action after setup commands");
        Check(macro == original && action_executor::Commands(macro, {}) == original.lines, "representative metadata never rewrites macro labels, icons, commands or waits");
        for (const auto& [command, kind] : {std::pair{"/MAGIC Cure III <me>", action_kind::spell}, {"/ja Convert <me>", action_kind::ability}, {"/jobability \"Chainspell\" <me>", action_kind::ability}, {"/ws Fast Blade <t>", action_kind::weapon_skill}, {"/weaponskill \"Burning Blade\" <t>", action_kind::weapon_skill}, {"/pet 'Healing Ruby' <stpc>", action_kind::pet}, {"/item Echo Drops <me>", action_kind::item}})
        {
            action_slot commandSlot{}; commandSlot.kind = action_kind::command; commandSlot.lines = {command};
            const auto action = action_executor::RepresentativeAction(commandSlot);
            Check(action && action->kind == kind && !action->name.empty() && ValidateAction(*action).empty(), "command aliases and quoted or unquoted multiword names resolve for presentation");
        }
        macro.lines = {"/ma Mage's Ballad <me>"};
        const auto apostrophe = action_executor::RepresentativeAction(macro);
        Check(apostrophe && apostrophe->name == "Mage's Ballad", "unquoted resource names retain apostrophes");
        macro.lines = {"/ma Cure <wait 2>"};
        const auto wait = action_executor::RepresentativeAction(macro);
        Check(wait && wait->name == "Cure" && wait->target.empty(), "inline waits are not interpreted as a display target");
        macro.lines = {"/ma \"unterminated", "/ma \"\" <me>", "/ma <t>", "/echo /ma Cure <me>", "/equip main \"Light Staff\""};
        Check(!action_executor::RepresentativeAction(macro), "malformed commands and action text inside other commands do not create resource metadata");
        Check(!action_executor::RepresentativeAction(Spell(1, "Cure")), "typed actions keep their existing resource lookup");
        macro.lines = {"/ma \"Cure\" <stpt>"};
        action_executor executor; executor.SetRestricted(true);
        Check(!executor.Inspect(nullptr, macro, true).usable && !executor.Activate(nullptr, macro, 0) && executor.Status() == "Close the editor or finish typing first.", "representative display data cannot bypass restricted macro execution");
    }
    auto NativeEventWaits(void) -> void
    {
        using namespace xiui;
        action_slot timedMacro; timedMacro.kind = action_kind::macro;
        timedMacro.lines = {"/targetnpc", "/item \"Pickaxe\" <t> <wait 4>", "/wait 4", "/echo done"};
        const auto original = timedMacro;
        Check(action_executor_test::CompileWaits(timedMacro) && timedMacro == original,
            "both wait forms retain four-second delays and native macro mode for targeting, items and non-action lines without rewriting saved text");
        action_executor executor;
        action_executor_test::BeginWait(executor);
        executor.SetNativeInputBusy(true);
        Check(action_executor_test::Waiting(executor) && !action_executor_test::Ready(executor, 6000) && executor.Diagnostics(6000).find("native input") != std::string::npos,
            "native gathering events preserve the pending line and original wait deadline");
        action_slot macro; macro.kind = action_kind::macro; macro.lines = {"/echo replacement"};
        Check(!executor.Activate(nullptr, macro, 6000) && executor.Status() == "Finish the native menu, event or target selection first." && action_executor_test::Waiting(executor),
            "an event blocks new activations without replacing the waiting macro");
        executor.SetNativeInputBusy(false);
        Check(action_executor_test::Waiting(executor) && !action_executor_test::Ready(executor, 4999) && action_executor_test::Ready(executor, 5000),
            "event completion retains the elapsed wait and permits dispatch exactly at its deadline");
        executor.SetNativeInputBusy(true); action_executor_test::StandaloneWait(executor);
        Check(!action_executor_test::Ready(executor, 999) && action_executor_test::Ready(executor, 1000),
            "standalone wait lines can start their timers during a native item-trade menu without dispatching a command");
        executor.SetNativeInputBusy(true); executor.SetRestricted(true);
        Check(executor.Diagnostics(6000).starts_with("Macro trace: idle."), "explicit input restrictions still cancel a macro during an event");
        executor.SetRestricted(false); action_executor_test::BeginWait(executor); executor.Update(nullptr, 6000);
        Check(executor.Diagnostics(6000).starts_with("Macro trace: idle."), "character/service loss still cancels a paused macro");
        executor.Clear();
        Check(executor.Inspect(nullptr, macro).reason == "Action services are unavailable.", "clearing the executor removes transient event ownership");
    }
    auto MacroIconInference(void) -> void
    {
        using namespace xiui;
        action_slot macro; macro.kind = action_kind::macro;
        macro.lines = {"/ja \"Boost\" <me>", "/wait 2", "/ws \"Combo\" <t>"};
        const auto icon = action_executor::IconAction(macro);
        Check(icon && icon->kind == action_kind::weapon_skill && icon->name == "Combo", "the weapon skill supplies artwork instead of a preparatory Boost");
        Check(action_executor::RepresentativeAction(macro)->name == "Boost", "icon inference preserves existing automatic recast selection");
        macro.lines = {"/echo /ws Combo", "/wait 2", "/item \"Potion\" <me>"};
        const auto item = action_executor::IconAction(macro);
        Check(item && item->kind == action_kind::item && item->name == "Potion", "item commands supply artwork while text inside echo is ignored");
        macro.lines = {"/wait 2", "/echo hello"};
        Check(!action_executor::IconAction(macro), "non-action macros keep the neutral fallback icon");
        macro.icon = "game:spell:1"; action_executor executor;
        Check(executor.SuggestedIcon(nullptr, macro) == macro.icon, "an explicitly chosen game icon always overrides inference");
        macro.recast_kind = action_kind::weapon_skill; macro.recast_name = "Combo";
        Check(ValidateAction(macro).empty(), "the editor's weapon-skill recast source remains valid when saved");
        Check(ActionKindFromName("Game macro") == action_kind::native_macro && ActionKindFromName("Native macro") == action_kind::native_macro &&
            ActionKindFromName("Single command") == action_kind::command && ActionKindFromName("Command") == action_kind::command,
            "clearer create labels retain existing imported action type names");
    }

    auto NativePaletteBoundaries(void) -> void
    {
        using namespace xiui;
        action_guard_state ready{};
        ready.services = ready.logged_in = ready.has_character = ready.has_zone = ready.identity_valid = true;
        ready.menu_open = true;
        Check(!ActionGuardReason(ready).empty(), "an ordinary native menu prevents action execution");
        Check(ActionGuardReason(ready, false, true).empty(), "verified macro palette permission permits the menu flag before native targeting begins");
        auto denied = ready; denied.action_target = true;
        Check(!ActionGuardReason(denied, false, true).empty(), "macro palette permission never bypasses execution target ownership");
        Check(ActionGuardReason(denied, true, true).empty(), "descriptive presentation retains metadata while native targeting is active");
        denied = ready; denied.subtarget = true;
        Check(!ActionGuardReason(denied, false, true).empty(), "native subtarget ownership remains exclusive");
        struct target_state
        {
            bool action_target{}, subtarget{};
            uint32_t action_id{};
            uintptr_t action_callback{}, cancel_callback{};
            auto GetActionTargetActive() const { return this->action_target; }
            auto GetIsSubTargetActive() const { return this->subtarget; }
            auto GetActionId() const { return this->action_id; }
            auto GetActionCallback() const { return this->action_callback; }
            auto GetCancelCallback() const { return this->cancel_callback; }
        };
        struct memory_state
        {
            target_state target{};
            auto GetTarget() const { return &this->target; }
        } memory;
        for (const auto id : {0u, 1u, 16u, UINT32_MAX})
        {
            memory.target = {true, false, id};
            for (int activation = 0; activation < 3; ++activation)
            {
                denied = ready; denied.action_target = NativeTargetSelectionActive(&memory);
                Check(!denied.action_target && ActionGuardReason(denied, false, true).empty(),
                    "completed targeted macros permit repeated activation and other slots despite retained action flags and IDs");
            }
        }
        for (const auto state : {native_target_selection{true, 0, 0}, {false, 0x100, 0},
            {false, 0, 0x200}, {false, 0x100, 0x200}})
        {
            memory.target = {false, state.subtarget, UINT32_MAX, state.action_callback, state.cancel_callback};
            denied = ready; denied.action_target = NativeTargetSelectionActive(&memory);
            Check(denied.action_target && !ActionGuardReason(denied, false, true).empty(),
                "subtargets and either pending selection callback retain native input even before an action flag or ID is set");
        }
        memory.target = {true, false, 0, 0x100, 0x200};
        Check(NativeTargetSelectionActive(&memory), "a zero-ID action can still own native target selection");
        memory.target.action_callback = memory.target.cancel_callback = 0;
        Check(!NativeTargetSelectionActive(&memory), "completing selection releases its guard without clearing the hard target");
        Check(!NativeTargetSelectionActive(static_cast<const memory_state*>(nullptr)), "missing native target services never invent selection ownership");
        denied = ready; denied.dead = true;
        Check(!ActionGuardReason(denied, false, true).empty() && !ActionGuardReason(denied, true, true).empty(), "execution and presentation remain unavailable while KO'd");
        denied = ready; denied.identity_valid = false;
        Check(!ActionGuardReason(denied, false, true).empty() && !ActionGuardReason(denied, true, true).empty(), "execution and presentation retain character identity validation");
        denied = ready; denied.logged_in = false;
        Check(!ActionGuardReason(denied, false, true).empty(), "verified macro palettes cannot bypass login validation");
        denied = ready; denied.zoning = true;
        Check(!ActionGuardReason(denied, false, true).empty(), "verified macro palettes cannot permit actions during zoning");
        denied = ready; denied.has_zone = false;
        Check(!ActionGuardReason(denied, false, true).empty(), "verified macro palettes require a loaded zone");
        action_slot macro{}; macro.kind = action_kind::macro; macro.lines = {"/ma \"Cure\" <stpt>", "/echo after selection"};
        const auto scheduled = action_executor::Commands(macro, {});
        Check(scheduled.size() == 2 && scheduled.front() == macro.lines.front() && ActionGuardReason(ready, false, true).empty(), "the scheduler's shared guard admits a first subtarget command under a verified macro palette");
        ready.subtarget = true;
        Check(!ActionGuardReason(ready, false, true).empty(), "an opened selector is owned by the macro's existing wait path rather than menu permission");
        action_executor executor;
        Check(!action_executor_test::NativePalette(executor), "native macro palette permission starts disabled");
        executor.SetNativeMenuAllowed(true); executor.SetRestricted(true);
        Check(!executor.Activate(nullptr, macro, 0) && executor.Status() == "Close the editor or finish typing first.", "verified macro palette permission cannot enable restricted activation");
        Check(!executor.Inspect(nullptr, macro).usable, "ordinary inspection remains restricted despite macro palette permission");
        executor.Clear();
        Check(!action_executor_test::NativePalette(executor), "clearing the executor drops transient native macro palette permission");
    }
}

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::strcmp(argv[1], "--japanese-names-only") == 0)
        { JapaneseNames(); std::puts("Japanese/English resource lookup, command names and encoding checks passed."); return 0; }
        const bool importsOnly = argc > 1 && std::strcmp(argv[1], "--imports-only") == 0;
        for (int i = importsOnly ? 2 : 1; i < argc; ++i)
        {
            std::ifstream file{argv[i], std::ios::binary}; Check(static_cast<bool>(file), "open real writer fixture");
            const std::string text{std::istreambuf_iterator<char>{file}, {}};
            const auto preview = xiui::PreviewActionImport(text, argv[i]);
            Check(preview.Valid() && preview.unsupported == 0 && preview.macros.size() == 3 && preview.palettes.size() == 4 && preview.actions == 408, "populated real-writer fixture preserves all supported action records");
            xiui::action_store migrated; Check(xiui::CommitActionImport(migrated, preview) == 4, "real writer fixture commits atomically");
            std::printf("Saved-profile fixture: %s; %zu actions, %zu library entries, %zu sets.\n", argv[i], preview.actions, preview.macros.size(), preview.palettes.size());
        }
        if (importsOnly) { Imports(); MigrationData(); std::puts("XIUI, tHotBar and tCrossBar action-only imports and preservation checks passed."); return 0; }
        Storage(); Imports(); MigrationData(); ExecutionBoundaries(); MacroPresentation(); NativeEventWaits(); MacroIconInference(); JapaneseNames(); NativePaletteBoundaries(); std::puts("Action palette, macro, import and execution boundary checks passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
