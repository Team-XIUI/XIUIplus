# XIUI code organization and documentation audit

Snapshot: October 18, 2026. Reviewed the current working tree, including uncommitted work. This is an audit and cleanup recommendation, not an implementation contract.

## Cleanup follow-through

The recommended cleanup was implemented on October 18. The original findings below retain their snapshot measurements and old line references; use [developer navigation](../README.md) for current paths.

- Added task-to-file navigation, development/install/default-capture guidance, a focused-check guide and an asset/generator index. Split the player guide from the root README. Current controller guidance now leads; dated controller research and earlier minimap validation are preserved under `docs/history/`.
- Kept cross-module contracts in `foundations.md` and moved feature-specific detail into `reference/module-foundations.md`, retaining the original foundation headings as links. Updated agent routing and the local controller skill.
- Added `scripts/update-docs.ps1` and a local Markdown link checker. Regenerated the settings inventory and combined HTML with generation provenance. Personal HTML export is explicit through `--export-diagram`.
- Extracted 45 configuration state fields into `config_session`, with shared toggle activation. Split selection, pointer/cursor handling and drawing into three config translation units. `hud_runtime.cpp` decreased from 3,643 to 2,444 lines; lifecycle, module coordination and save callbacks remain in the runtime.
- Split the 5,254-line config fixture into support and responsibility-specific files; its dispatcher is 399 lines. Added a shared catalog of 72 focused commands and a wrapper that builds only the selected target. Replaced stale sidebar/row assumptions in the exercised fixtures with category and option identity checks. The shared description bounds now clear taller controls.
- Split the 823-line root CMake file into named build areas; the root is 103 lines. Reuse experience/notification models through separate libraries for dynamic/static MSVC runtimes. Removed asset/cache implementation includes from the common module header, moved button implementation out of its header, and separated the small selection style type. No build-time speedup is claimed without measurement.
- Replaced Minimap's option ordinals with named IDs while preserving identities and persistence. Added a defaults-capture utility using the existing portable preference boundary, a completed-save command, backups and capture metadata. Preserved the October 17 baseline and immutable sharing dictionary; the new metadata records that original capture rather than a new one.

Focused configuration, controller, Minimap, persistence/defaults, capture-encoding and standalone startup/resource checks cover the changed boundaries. No complete suite, asset regeneration or gameplay/physical-controller acceptance was performed for this cleanup. The client was closed during installation, so loading the installed standalone DLL remains a user step when the game is reopened.

The best first investment is an authoritative navigation guide and separation of current guidance from historical evidence. The largest code opportunity is configuration ownership inside `hud_runtime`, followed by focused test discovery and reducing shared-header dependencies. The existing module folders and shared rendering/input foundations should remain.

## Scope and evidence

Static review of source layout, representative implementation files, build/test entry points, settings workflows, and project documentation. No builds, test suites, asset regeneration, game commands, or live input were run for this audit. Native hook correctness and physical-controller acceptance were not evaluated.

- `src/`: 371 C++ source/header files, 72,679 lines, including generated resource tables. Generated data should not be counted as handwritten code complexity.
- `tests/`: 59 source/header files, 18,967 lines.
- `docs/`: 53 Markdown files, 6,805 lines; 2,795 lines belong to the generated settings inventory.
- The `hud_runtime*.cpp` family totals 7,332 lines. `hud_runtime.cpp` alone is 3,643 lines, with a 655-line class header.
- Checked local file targets in 56 project/agent Markdown documents. No missing local targets were found. External URLs and heading anchors were not checked.

## 1. Separate current behavior from historical plans

Priority: high. Effort: small to medium. Runtime risk: none for documentation changes.

There are concrete contradictions, even where an introductory warning correctly calls the document a proposal:

- [Controller plan](../controller-plan.md:33) labels a section “Current XIUI foundations and gaps,” then says there is no controller registration and the ABI is 8. [The actual ABI](../../src/shared/runtime_api.hpp:13) is 18, and [the host initializes controller input](../../src/host/plugin.cpp:118).
- [The minimap record](../implementation/28-minimap.md:44) still says later hover zoom/text changes await a standalone release after 1.0.2, alongside descriptions of newer implemented changes. It also [quotes a 37.3 MiB archive](../implementation/28-minimap.md:36); the current file is 40.43 MiB.
- [The implementation contract](../implementation/README.md:50) refers to the “current hot-reload workflow” without repeating the explicit isolated-install prerequisite in [AGENTS.md](../../AGENTS.md). This is easy to misread during a routine runtime edit.
- [The migration plan](../lua-profile-migration-plan.md:3) contains a particular task's scope and authorizations. That is useful history, but should not become implicit authorization for unrelated future tasks.

Recommended cleanup:

Make the current controller implementation guide authoritative. Move the obsolete controller-foundation snapshot into clearly dated research/history, preserving the useful binding research and unresolved hardware/software-keyboard evidence. Keep current feature behavior, source entry points, and remaining acceptance checks together; put old build timestamps and completed investigation narratives in dated records.

Do not archive every implementation record automatically. Some have useful current implementation summaries and explicit unverified acceptance criteria. Extract and preserve those before moving or shortening historical sections. Update inbound links when anything moves.

## 2. Add a small task-to-file navigation guide

Priority: high. Effort: small. Runtime risk: none.

There is no `docs/README.md`. The root [README](../../README.md) is 291 lines and combines player features, architecture, commands, editing instructions, installation, and development details. [Foundations](../foundations.md) is 573 lines and combines cross-module contracts with extensive feature histories and implementation notes.

The information exists, but an agent needs several broad reads to find the few files relevant to a small change.

Recommended cleanup:

- Add `docs/README.md` as the short entry point: task area, two to four starting files, authoritative guide, and the appropriate focused check.
- Add `docs/development.md` for standalone build/install, optional isolated hot reload, AgentLink's external script location, and SDK versus active-install discovery.
- Keep `AGENTS.md` concise: invariants and routing links. For controller work, link current implementation before historical research.
- Keep the shared contracts in `foundations.md`; move feature-specific narratives into existing feature guides or dated records. Preserve stable section anchors where practical.
- Prefer one useful source map over a README in every small module directory.

Useful routes include HUD rendering/text; settings and profiles; config controls; mouse/controller ownership; Action Bars; Chat; Satchel; Minimap; native hooks/ABI; assets; and release/default capture.

## 3. Make generated documentation visibly reproducible

Priority: high. Effort: small. Runtime risk: none.

[The generated feature page](../feature-parity.html) does not contain the recent Burnt minimap style documented in its Markdown inputs. [The settings inventory](../settings-audit.md:3) is dated October 15; more importantly, its [generator text is hardcoded to that date](../../tests/config_input_smoke.cpp:671), so regenerating it would still advertise the old date.

Recommended cleanup:

Mark generated pages with their input files, regeneration command, and source revision or generation date. Replace the hardcoded inventory date. Provide one documented update command that runs the existing settings-inventory exporter and [HTML renderer](../../scripts/render_feature_parity.py:353). Add a lightweight local link check when documentation is reorganized.

Treat generated HTML and the settings inventory as derived outputs, not separate places to manually edit feature descriptions. The link graph is currently healthy; preserve that during cleanup.

## 4. Give configuration its own owner

Priority: medium. Effort: medium, in small steps. Runtime risk: moderate because input ownership is involved.

Splitting runtime code into `hud_runtime_*.cpp` has helped, but these files still implement one large class and share its state. The main file contains [pointer activation](../../src/runtime/hud_runtime.cpp:2226), [configuration drawing](../../src/runtime/hud_runtime.cpp:3036), [cursor resolution](../../src/runtime/hud_runtime.cpp:3358), and option filtering/layout. [Controller config](../../src/runtime/hud_runtime_controller_config.cpp:74) separately builds focus targets and [activates those same kinds of controls](../../src/runtime/hud_runtime_controller_config.cpp:248).

This spreads changes to a shared control across drawing, pointer, controller, help, and focus code. The recently added multi-toggle row illustrates the number of integration points.

Recommended cleanup:

First move the remaining config drawing/pointer/cursor methods into consistently named config translation units. Then extract a configuration session/controller that owns selection, scrolling, open editors, and control activation, using the existing shared geometry and semantic actions. Let `hud_runtime` retain lifecycle and module coordination.

Move one complete responsibility at a time. Preserve exactly-once activation, press/release ownership, native input guards, save callbacks, visible focus, and editor cancellation. A file split alone improves navigation; a later ownership extraction reduces coupling.

## 5. Make focused checks easy to find, then reduce test coupling

Priority: medium. Effort: small for documentation; medium for extraction. Runtime risk: low if behavior stays unchanged.

[The config smoke file](../../tests/config_input_smoke.cpp) is 5,254 lines. It contains 68 distinct command-line switch strings; CMake registers only two argument-specific variants for that executable, in addition to its full run. Other useful focused paths, including [Minimap](../../tests/config_input_smoke.cpp:5126) and [defaults](../../tests/config_input_smoke.cpp:5160), require knowledge of the source or scattered feature docs.

Recommended cleanup:

Add `tests/README.md` or a small check catalog mapping change area to build target, executable arguments, and required input paths. Document the smallest useful check first and distinguish it from release packaging checks.

Then split unrelated config checks into feature files. Several checks are already separate, so continue that pattern. Keep a small dispatcher and reusable fixtures instead of expanding the single friend test struct. Prefer testing shared control behavior through a narrow interface as configuration ownership becomes clearer.

No new test framework or expanded mandatory full suite is needed for this cleanup.

## 6. Reduce build and header fan-out

Priority: medium. Effort: medium. Runtime risk: low to moderate.

[CMakeLists.txt](../../CMakeLists.txt) is 823 lines, and the full `XIUI_HUD_SOURCES` list is referenced in 14 places. Feature-oriented tests frequently compile the whole HUD under different definitions. The common [module header](../../src/runtime/modules/module.hpp:3) also includes renderer and native asset/cache headers, exposing broad implementation dependencies to every module.

Recommended cleanup:

Separate CMake source groups, resources, and tests into a few files under `cmake/`, keeping the top-level target relationships clear. Identify genuinely common pure model/data code that can be built independently. Trim common-header includes through forward declarations or smaller type-only headers where completeness is unnecessary.

Do not put every target onto one object library blindly. [The standalone release uses the static MSVC runtime](../../CMakeLists.txt:492), while test/runtime variants also have different feature definitions. Reuse must respect those differences.

There is a small interface ambiguity worth fixing separately: [build_all.ps1 exposes `-SkipTests`](../../scripts/build_all.ps1:5), but its standalone branch calls `package.ps1` before that flag is used. Document that it applies to development tests, rename it, or make the intended distinction explicit. Retain the required standalone packaging checks.

## 7. Use named option IDs consistently

Priority: medium. Effort: small per module. Runtime risk: low with focused coverage.

[Minimap's option count](../../src/runtime/modules/minimap/minimap_module.hpp:22), [section classification](../../src/runtime/modules/minimap/minimap_module.cpp:175), switch cases, and tests still coordinate through numeric ordinals and arithmetic such as `35 + text_profile_option_count`.

The project already has a better local precedent: [Action Bars uses named option IDs](../../src/runtime/modules/actionbars/actionbars_module.hpp:137).

Recommended cleanup:

Apply that approach incrementally to modules as they are edited: named IDs, named profile ranges, and counts derived from the last ID. Keep existing option identities and INI keys stable. Consolidate section membership metadata where it actually removes repeated range logic.

This does not require replacing the existing option system or rewriting every module at once.

## 8. Make default capture a maintained workflow

Priority: medium. Effort: small to medium. Runtime risk: low if capture remains separate from applying settings.

The latest defaults capture was done during this session with an ad hoc script. The repository has build/package scripts, but no maintained defaults-capture command. Future agents have to reconstruct save timing, active-install discovery, encoding, theme capture, transient exclusions, and documentation updates.

There is already an authoritative preference boundary in [settings_store](../../src/runtime/settings/settings_store.cpp:118), including exclusion of local controller-device data. Duplicating that policy in one-off parsers can drift.

Recommended cleanup:

Provide a capture command that confirms the active install, requests and observes a completed save, backs up the previous baseline, captures preferences/theme, and uses the existing preference filtering contract. Report the captured source and counts. Keep the immutable sharing preset unchanged. Run the existing defaults-only check when the baseline changes.

## What I would keep

- The domain-based `modules/` layout, source/model/render separation where it already exists, shared controls, text profiles, native containers, and semantic controller actions.
- The one-DLL release path and explicit isolated development mode.
- Pinned data sources, notices, bounded caches, asynchronous settings writes, and native ownership rules.
- Generated resource tables. Large generated headers such as action icons, timing rules, and skillchain data are not candidates for manual splitting. Give generated outputs a consistent generator notice and keep an asset-to-generator index instead.
- Historical native research that records offsets, packet evidence, failed probes, or acceptance limits. Label and relocate it; do not discard the evidence.

## Suggested cleanup order

1. Documentation-only: add the navigation/development/check guides, reconcile the controller and minimap contradictions, label historical records, and refresh derived pages.
2. Small maintenance changes: named option IDs, a repeatable defaults-capture command, and clearer build flags.
3. Code ownership: extract config responsibilities and split the large config test fixture in separate, reviewable changes.
4. Build optimization: measure affected builds, trim header dependencies, and reuse appropriate pure-code targets. Keep compiler/runtime variants intact.

Avoid a broad folder renaming exercise or a framework rewrite. The existing foundations are useful; the fastest improvement is making their ownership, entry points, and current contracts explicit.
