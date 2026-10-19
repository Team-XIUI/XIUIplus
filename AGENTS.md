# XIUI native plugin

- Never add Codex co-author signatures to commits. Use `main` as the default branch name.
- Keep comments clear and concise.
- Keep routine validation focused. Do not run full suites, repeated builds, broad asset validation, or visual/gameplay checks unless requested or needed for a specific failure. The user checks visual and gameplay behavior. Stop checking when the user says a change works.
- Install new builds as one standalone `plugins/xiuinative.dll` in the active Phoenix setup, and use AgentLink to load/reload and confirm it is active. Preserve current settings and positions. The two-DLL host/runtime setup requires an explicitly selected isolated development install. Only leave builds uninstalled when the user explicitly requests it.
- The current development install is `C:/Games/PhoenixXI-dev/PhoenixXI`; AgentLink commands use `../phoenixlighting/scripts/game.ps1`. Confirm the active install through AgentLink rather than assuming the SDK directory is the running game.
- The client can run as `phoenix-loader` (window title is the character name). An AgentLink timeout or no `pol` process does not establish that the game is closed; retry and inspect the actual game window and AgentLink log.
- Browser automation uses `agent-browser`; load `agent-browser skills get core --full` before first use. Use another browser tool only when explicitly requested or the CLI is unavailable.
- Do not use follow-up input/question tools. Continue with available context; state any necessary manual command directly.
- Never use `.canvas.tsx` files for plans; use Markdown or HTML.

For HUD changes, follow [the shared foundations](docs/foundations.md), especially **HUD text**. Reuse the theme roles, module text profiles, rendering, controls, and persistence. Compact sizing must retain normal text effects. Apply game-state colors with `TintTextProfile` so gradients and effects survive. Plain configuration text is a separate presentation.

Start with [developer navigation](docs/README.md) and the relevant [short module note](docs/modules/README.md). [Development](docs/development.md) covers builds/install/default capture; [focused checks](tests/README.md) keeps validation proportional. Generated documentation uses `scripts/update-docs.ps1`; assets have a [generator index](assets/README.md).

For interactive feature changes, apply [the controller support skill](.agents/skills/xiui-controller-support/SKILL.md) and read [input ownership](docs/reference/input.md) first. Its [remaining acceptance](docs/reference/input.md#remaining-acceptance) distinguishes implemented behavior from hardware/native-keyboard limits. Old audits and implementation plans live in [the archive](docs/archive/README.md), not the routine reading path.

For releases, use the `xiui-native-release` skill and `scripts/package.ps1`. Ship one `dist/xiuinative.dll`; the host/runtime DLL pair is for development hot reload.
