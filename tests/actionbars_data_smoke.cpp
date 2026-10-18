#include "modules/actionbars/action_import.hpp"
#include "modules/actionbars/action_executor.hpp"

#include <cstdio>
#include <stdexcept>

namespace xiui
{
    struct action_executor_test
    {
        static auto NativePalette(const action_executor& executor) -> bool { return executor.native_macro_palette_; }
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
        Check(store.Select(action_surface::hotbar, second), "select a hotbar palette");
        Check(store.ActivePalette(action_surface::crossbar)->id == shared, "crossbar selection is independent");
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
 hotbarBar1 = { x=1.5, slotActions={['15:0:palette:Test']={ [1]={macroRef=12,macroPaletteKey=15}, [2]={actionType='ws',action='Fast Blade',target='t'} }} },
 hotbarCrossbar = { slotActions={['15:0:palette:Test']={ L2x2={ [8]={actionType='pet', action='Assault',target='t'} } }} },
 macroDB={ [15]={ {id=12,actionType='macro',displayName='Summon',macroText='/ma "Ifrit" <me>\n/wait 1\n/echo ready'} } },
})lua");
        Check(xiui.Valid() && xiui.actions == 3 && xiui.palettes[0].name == "Test", "XIUI storage keys group bars and crossbar into one named palette");
        Check(xiui.palettes[0].actions.hotbar[0][0].lines.size() == 3 && xiui.palettes[0].actions.crossbar[4][7].name == "Assault", "XIUI macro references resolve from their source library");
        Check(xiui.source_context && xiui.palettes[0].job == 15 && xiui.palettes[0].subjob == 0, "XIUI imports preserve source job and shared-subjob context");
        const auto jobFile = PreviewActionImport(source, "C:/example/bindings/WAR.lua");
        Check(jobFile.source_context && jobFile.palettes[0].job == 1 && jobFile.palettes[0].subjob == 0, "tBar job filenames preserve their source job context");
        const auto petImport = PreviewActionImport(R"lua(return {
 hotbarBar1 = {slotActions={['15:0:palette:Default']={ [1]={actionType='ma',action='Ifrit',target='me'} }, Ifrit={ [1]={actionType='pet',action='Assault',target='t'} }}},
})lua");
        Check(petImport.Valid() && petImport.palettes.size() == 1 && petImport.palettes[0].pets.size() == 1 && petImport.palettes[0].pets[0].pet == "Ifrit", "Lua pet storage becomes an automatic variant of the matching job palette");
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
        executor.SetNativeMacroPalette(true); executor.SetRestricted(true);
        Check(!executor.Activate(nullptr, macro, 0) && executor.Status() == "Close the editor or finish typing first.", "verified macro palette permission cannot enable restricted activation");
        Check(!executor.Inspect(nullptr, macro).usable, "ordinary inspection remains restricted despite macro palette permission");
        executor.Clear();
        Check(!action_executor_test::NativePalette(executor), "clearing the executor drops transient native macro palette permission");
    }
}

int main()
{
    try { Storage(); Imports(); ExecutionBoundaries(); MacroPresentation(); NativePaletteBoundaries(); std::puts("Action palette, macro, import and execution boundary checks passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
