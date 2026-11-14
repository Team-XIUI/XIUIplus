# XIUI+ plugin

Guidance for AI coding agents working in this repository.

- Use **XIUI+** in display text and **XIUIplus** in filenames and data directories. `/xiuiplus` is the documented command; old aliases and import formats remain compatible. Preserve original XIUI credits.
- Use `main` as the default branch name. Do not add AI co-author signatures to commits.
- Keep comments clear and concise.
- Localize all new or changed player-facing text in both `locales/en.json` and `locales/ja.json` in the same change, including labels, choices, tooltips, dialogs, errors and controller help. Follow [localization](locales/README.md); English fallback does not count as a translation. Preserve client-provided and user-entered text verbatim. Run `python scripts/build-locales.py` when changing localized text.
- Keep validation focused. Run the [focused check](tests/README.md) for the boundary you changed rather than full suites, repeated builds or broad asset validation. Automated checks do not establish visual or gameplay behavior.
- Installing a build must preserve the player's settings and positions. Ship one standalone `XIUIplus.dll`; the two-DLL host/runtime pair is for development hot reload only.

If `docs/development.md` exists, read it too. It is a local, untracked file with the maintainer's install, live-verification and release steps.

## Where to start

Start with the [developer docs](docs/README.md) and the relevant [module note](docs/modules/README.md).

- HUD changes follow the [foundations](docs/foundations.md), especially **HUD text**. Reuse the theme roles, module text profiles, rendering, controls and persistence. Compact sizing must keep normal text effects. Apply game-state colors with `TintTextProfile` so gradients and effects survive.
- Interactive changes follow [controller support](docs/reference/controller-support.md) and [input ownership](docs/reference/input.md). Its [remaining acceptance](docs/reference/input.md#remaining-acceptance) section lists what automated checks do not cover.
- Generated documentation uses `scripts/update-docs.ps1`. Embedded asset inputs are defined in [cmake/resources.cmake](cmake/resources.cmake).

## Releases

`VERSION` holds the release name (`MAJOR.MINOR.PATCH` plus an optional channel, e.g. `0.2.1-alpha`). Tags, titles and displayed numbers derive from it, so never write them by hand. Change the version only when asked. Releases need short, player-facing patch notes for every feature and fix since the last published release.
