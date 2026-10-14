# XIUI native plugin

- Never add Codex co-author signatures to commits. Use `main` as the default branch name.
- Keep comments clear and concise.
- Keep routine validation focused. Do not run full suites, repeated builds, broad asset validation, or visual/gameplay checks unless requested or needed for a specific failure. The user checks visual and gameplay behavior. Stop checking when the user says a change works.
- Browser automation uses `agent-browser`; load `agent-browser skills get core --full` before first use. Use another browser tool only when explicitly requested or the CLI is unavailable.
- Do not use follow-up input/question tools. Continue with available context; state any necessary manual command directly.
- Never use `.canvas.tsx` files for plans; use Markdown or HTML.

For HUD changes, follow [the shared foundations](docs/foundations.md), especially **HUD text**. Reuse the theme roles, module text profiles, rendering, controls, and persistence. Compact sizing must retain normal text effects. Apply game-state colors with `TintTextProfile` so gradients and effects survive. Plain configuration text is a separate presentation.

For interactive feature changes, apply [the controller support skill](.agents/skills/xiui-controller-support/SKILL.md) and consult [the controller plan](docs/controller-plan.md). The plan is not implemented functionality.

For releases, use the `xiui-native-release` skill and `scripts/package.ps1`. Ship one `dist/xiuinative.dll`; the host/runtime DLL pair is for development hot reload.
