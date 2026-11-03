# XIUI+ localization

The globe icon in the top-right settings header selects English or 日本語 in both Basic and Advanced settings. Selection applies immediately and saves as `session.localization.language` in `config/XIUIplus/settings.ini`. English is the default and the fallback for unsupported language codes or new messages. Language is installation-local: profiles, imports, presets and resets do not replace it.

`en.json` and `ja.json` are UTF-8 message catalogs. English source text is the message key, following a gettext-style convention. Keep both files' keys identical; English values match their keys. Translate complete messages and format templates, preserving printf argument types and order. Keep product names, command syntax, button identifiers and license text intact.

Update translations by explicit message key, never by row number or positional alignment. HP, MP and TP deliberately retain the same abbreviations in both languages; the catalog validator enforces this. Player and Party resolve captions before rendering, fitting and measuring in both XI and XIV 1.0 appearances.

Numbered Hotbar labels share one translation across settings, movement, keybinds and dropdowns. The validator checks all ten labels against `Hotbar` and the dynamic `Hotbar %zu` template.

New features and changes to player-facing copy must update both catalogs in the same change. Include labels, descriptions, choices, tooltips, dialogs, errors, placeholders and keyboard/controller help. English fallback does not complete this requirement. Validate with `python scripts/build-locales.py`; the build also rejects catalog key mismatches, duplicate keys and incompatible format arguments.

`scripts/build-locales.py` validates duplicate keys, catalog parity, format arguments and label consistency. `scripts/localization_sources.py` checks explicit localization calls, shared option/section metadata and common help/control builders for missing catalog entries. It does not interpret client resources or user content as messages. CMake reruns validation when runtime sources or catalogs change, then generates `locale_catalog.hpp` in the build directory. Catalog data is compiled into the standalone DLL; no installed translation files or per-frame file access are required.

Use `i18n::Text` for XIUI+ copy and `i18n::Format` before inserting dynamic values. Resolve text before measuring, fitting, wrapping or drawing. Configuration rows and navigation localize their metadata at presentation; stable option labels, IDs, INI keys and selection logic stay in English. Settings search indexes translated text alongside original labels and descriptions, rebuilding after a language change.

Incoming chat, auto-translate tokens, client resource names/descriptions, character names, custom labels, commands and profile names remain verbatim. Never translate a general text-rendering function or a chat/resource buffer. Use `module_option::VerbatimChoices()` and the navigation control's `translate = false` for lists supplied by the client or the user. Native game UI language and developer diagnostics remain under their existing ownership.

Runtime exports scope the locale per callback and restore the previous thread-local locale afterward. A second runtime used to build the search catalog cannot change the live selection. Existing Unicode font fallback renders Japanese; `ui::WrapText` supports text without spaces in setup help and tooltips while preserving the configured text effects.

Focused check: `./scripts/check.ps1 -Name localization`. To validate catalogs alone, run `python scripts/build-locales.py`. Visual and gameplay acceptance remains in game.
